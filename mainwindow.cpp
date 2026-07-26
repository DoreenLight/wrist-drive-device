#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "protocolworker.h"
#include "floatiddefs.h"

#include <QByteArray>
#include <QDateTime>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QRegularExpression>
#include <QSerialPortInfo>
#include <QSplitter>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , stackedWidget(nullptr)
    , homePage(nullptr)
    , serialPage(nullptr)
    , protocolPage(nullptr)
    , motorPage(nullptr)
    , rehabPage(nullptr)
    // Page 1
    , portComboBox(nullptr)
    , baudComboBox(nullptr)
    , dataBitsComboBox(nullptr)
    , parityComboBox(nullptr)
    , stopBitsComboBox(nullptr)
    , flowControlComboBox(nullptr)
    , refreshPortsButton(nullptr)
    , openCloseButton(nullptr)
    , sendButton(nullptr)
    , backButton(nullptr)
    , receiveEdit(nullptr)
    , transmitEdit(nullptr)
    , sendLineEdit(nullptr)
    , hexSendCheckBox(nullptr)
    , hexReceiveCheckBox(nullptr)
    , autoSendCheckBox(nullptr)
    , autoIntervalSpinBox(nullptr)
    , statusLabel(nullptr)
    , serialPort(new QSerialPort(this))
    , autoSendTimer(new QTimer(this))
    // Page 2
    , protoPortCombo(nullptr)
    , protoBaudCombo(nullptr)
    , protoOpenBtn(nullptr)
    , protoRefreshBtn(nullptr)
    , pingBtn(nullptr)
    , startBtn(nullptr)
    , stopBtn(nullptr)
    , floatIdCombo(nullptr)
    , floatValueEdit(nullptr)
    , floatWriteBtn(nullptr)
    , batchIdCombo(nullptr)
    , batchValueEdit(nullptr)
    , batchAddBtn(nullptr)
    , batchPreviewEdit(nullptr)
    , batchSendBtn(nullptr)
    , statRxFrames(nullptr)
    , statChkErr(nullptr)
    , statTelemetry(nullptr)
    , statAck(nullptr)
    , telemetryTable(nullptr)
    , protocolLogEdit(nullptr)
    , protocolStatusLabel(nullptr)


    // Page 3: 电机控制
    , m1AngleLabel(nullptr), m1SpeedLabel(nullptr), m1CurrentLabel(nullptr)
    , m1KpLabel(nullptr), m1KiLabel(nullptr)
    , m2PosLabel(nullptr), m2SpdLabel(nullptr), m2TrqLabel(nullptr)
    , m1ToggleBtn(nullptr), m2ToggleBtn(nullptr)
    , m1LastAngleCmd(0), m1Running(false), m2Running(false)
    , motorModeCombo(nullptr), m2ModeCombo(nullptr)
    , motorStatusLabel(nullptr)
    // Page 4: 康复治疗
    , treatModeCombo(nullptr)
    , treatAngleEdit(nullptr), treatSpeedEdit(nullptr), treatCyclesSpin(nullptr)
    , treatStartBtn(nullptr), treatPauseBtn(nullptr), treatStopBtn(nullptr)
    , treatTimeLabel(nullptr)
    , treatM1AngleLabel(nullptr), treatM2AngleLabel(nullptr), treatTorqueLabel(nullptr)
    , treatLogEdit(nullptr), treatStatusLabel(nullptr)
    , treatTimer(nullptr), treatElapsedSec(0), treatRunning(false)
    , protoWorker(nullptr)
    , protoThread(nullptr)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("腕关节康复设备串口控制台"));
    resize(1060, 680);

    stackedWidget = new QStackedWidget(this);
    setCentralWidget(stackedWidget);

    // ── 创建工作线程 ─────────────────────────────────
    protoThread = new QThread(this);
    protoWorker = new ProtocolWorker();   // no parent — will be moved
    protoWorker->moveToThread(protoThread);

    // 连接 worker 信号 → 主线程槽 (自动 queued)
    connect(protoWorker, &ProtocolWorker::frameParsed,
            this, &MainWindow::onProtocolFrameParsed, Qt::QueuedConnection);
    connect(protoWorker, &ProtocolWorker::telemetryReceived,
            this, &MainWindow::onTelemetryReceived);
    connect(protoWorker, &ProtocolWorker::ackReceived,
            this, &MainWindow::onAckReceived);
    connect(protoWorker, &ProtocolWorker::ioReportReceived,
            this, &MainWindow::onIoReportReceived);
    connect(protoWorker, &ProtocolWorker::rawDataReceived,
            this, &MainWindow::onProtocolRawReceived);
    connect(protoWorker, &ProtocolWorker::rawDataSent,
            this, &MainWindow::onProtocolRawSent);
    connect(protoWorker, &ProtocolWorker::statsChanged,
            this, &MainWindow::onProtocolStatsChanged);
    connect(protoWorker, &ProtocolWorker::parseError,
            this, &MainWindow::onProtocolParseError);
    connect(protoWorker, &ProtocolWorker::portError,
            this, &MainWindow::onProtocolPortError);
    connect(protoWorker, &ProtocolWorker::portOpened,
            this, &MainWindow::onProtocolPortOpened);
    connect(protoWorker, &ProtocolWorker::portClosed,
            this, &MainWindow::onProtocolPortClosed);

    // 清理线程
    connect(protoThread, &QThread::finished,
            protoWorker, &QObject::deleteLater);

    protoThread->start();

    // ── 创建页面 ─────────────────────────────────────
    createHomePage();
    createSerialPage();
    createProtocolDebugPage();
    createMotorControlPage();
    createRehabPage();

    stackedWidget->addWidget(homePage);     // index 0
    stackedWidget->addWidget(serialPage);   // index 1
    stackedWidget->addWidget(protocolPage); // index 2
    stackedWidget->addWidget(motorPage);    // index 3
    stackedWidget->addWidget(rehabPage);    // index 4
    stackedWidget->setCurrentWidget(homePage);

    // 遥测队列定时刷新 (50ms, 低频批量更新图表)
    mChartTimer = new QTimer(this);
    mChartTimer->setInterval(50);
    connect(mChartTimer, &QTimer::timeout, this, [this]() {
        QMutexLocker lock(&mTelemetryMutex);
        if (mTelemetryQueue.isEmpty()) return;
        // 批量取走队列中所有数据
        QList<QMap<uint8_t, float>> batch = mTelemetryQueue;
        mTelemetryQueue.clear();
        lock.unlock();
        // 合并多帧数据后统一更新
        QMap<uint8_t, float> merged;
        for (const auto &frame : batch) {
            for (auto it = frame.constBegin(); it != frame.constEnd(); ++it) {
                merged.insert(it.key(), it.value());  // 同 ID 取最后一帧
            }
        }
        feedMotorChart(m1ChartCtx, merged);
        feedMotorChart(m2ChartCtx, merged);
    });
    mChartTimer->start();

    // 康复治疗计时器
    treatTimer = new QTimer(this);
    treatTimer->setInterval(1000);
    connect(treatTimer, &QTimer::timeout, this, [this]() {
        if (treatRunning) {
            treatElapsedSec++;
            int h = treatElapsedSec / 3600;
            int m = (treatElapsedSec % 3600) / 60;
            int s = treatElapsedSec % 60;
            treatTimeLabel->setText(QStringLiteral("%1:%2:%3")
                .arg(h, 2, 10, QChar('0'))
                .arg(m, 2, 10, QChar('0'))
                .arg(s, 2, 10, QChar('0')));
        }
    });

    autoSendTimer->setInterval(1000);
    connect(autoSendTimer, &QTimer::timeout, this, &MainWindow::onAutoSendTimeout);
    connect(serialPort, &QSerialPort::readyRead, this, &MainWindow::onSerialReadyRead);
    connect(serialPort, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error != QSerialPort::NoError) {
            statusLabel->setText(QStringLiteral("串口错误: %1").arg(serialPort->errorString()));
        }
    });

    onRefreshPorts();
}

MainWindow::~MainWindow()
{
    // 停止协议 worker 线程
    if (protoThread->isRunning()) {
        QMetaObject::invokeMethod(protoWorker, "closePort", Qt::QueuedConnection);
        protoThread->quit();
        protoThread->wait(3000);
    }

    if (serialPort->isOpen()) {
        serialPort->close();
    }
    delete ui;
}

// ═══════════════════════════════════════════════════════════
// Page 0: 首页
// ═══════════════════════════════════════════════════════════

void MainWindow::createHomePage()
{
    homePage = new QWidget(this);
    auto *layout = new QVBoxLayout(homePage);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(18);

    auto *title = new QLabel(QStringLiteral("腕关节康复设备控制台"), homePage);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: bold;"));

    auto *subtitle = new QLabel(QStringLiteral("请选择要进入的功能模块"), homePage);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet(QStringLiteral("font-size: 13px; color: #666;"));

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(18);
    buttonLayout->setAlignment(Qt::AlignCenter);

    auto *serialButton = new QPushButton(QStringLiteral("串口配置与调试"), homePage);
    serialButton->setFixedSize(200, 60);
    auto *motionButton = new QPushButton(QStringLiteral("协议调试"), homePage);
    motionButton->setFixedSize(200, 60);
    auto *motorButton = new QPushButton(QStringLiteral("电机控制调试"), homePage);
    motorButton->setFixedSize(200, 60);
    auto *rehabButton = new QPushButton(QStringLiteral("康复治疗"), homePage);
    rehabButton->setFixedSize(200, 60);

    connect(serialButton, &QPushButton::clicked, this, &MainWindow::showSerialPage);
    connect(motionButton, &QPushButton::clicked, this, &MainWindow::showProtocolPage);
    connect(motorButton, &QPushButton::clicked, this, &MainWindow::showMotorPage);
    connect(rehabButton, &QPushButton::clicked, this, &MainWindow::showRehabPage);

    buttonLayout->addWidget(serialButton);
    buttonLayout->addWidget(motionButton);
    buttonLayout->addWidget(motorButton);
    buttonLayout->addWidget(rehabButton);

    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addLayout(buttonLayout);
}

// ═══════════════════════════════════════════════════════════
// Page 1: 串口配置与调试 (保持不变)
// ═══════════════════════════════════════════════════════════

void MainWindow::createSerialPage()
{
    serialPage = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(serialPage);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    auto *topLayout = new QHBoxLayout;
    backButton = new QPushButton(QStringLiteral("返回首页"), serialPage);
    backButton->setMinimumHeight(34);
    auto *title = new QLabel(QStringLiteral("串口配置与调试"), serialPage);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold;"));
    topLayout->addWidget(backButton);
    topLayout->addWidget(title, 1);
    topLayout->addStretch();
    rootLayout->addLayout(topLayout);

    auto *configBox = new QGroupBox(QStringLiteral("串口配置"), serialPage);
    auto *configLayout = new QGridLayout(configBox);
    configLayout->setSpacing(10);

    configLayout->addWidget(new QLabel(QStringLiteral("端口"), serialPage), 0, 0);
    portComboBox = new QComboBox(serialPage);
    configLayout->addWidget(portComboBox, 0, 1);

    configLayout->addWidget(new QLabel(QStringLiteral("波特率"), serialPage), 0, 2);
    baudComboBox = new QComboBox(serialPage);
    baudComboBox->addItem(QStringLiteral("1200"), 1200);
    baudComboBox->addItem(QStringLiteral("2400"), 2400);
    baudComboBox->addItem(QStringLiteral("4800"), 4800);
    baudComboBox->addItem(QStringLiteral("9600"), 9600);
    baudComboBox->addItem(QStringLiteral("19200"), 19200);
    baudComboBox->addItem(QStringLiteral("38400"), 38400);
    baudComboBox->addItem(QStringLiteral("57600"), 57600);
    baudComboBox->addItem(QStringLiteral("115200"), 115200);
    baudComboBox->addItem(QStringLiteral("230400"), 230400);
    baudComboBox->addItem(QStringLiteral("460800"), 460800);
    baudComboBox->addItem(QStringLiteral("921600"), 921600);
    baudComboBox->addItem(QStringLiteral("1000000"), 1000000);
    baudComboBox->addItem(QStringLiteral("1500000"), 1500000);
    baudComboBox->addItem(QStringLiteral("2000000"), 2000000);
    baudComboBox->setCurrentIndex(7);
    configLayout->addWidget(baudComboBox, 0, 3);

    configLayout->addWidget(new QLabel(QStringLiteral("数据位"), serialPage), 1, 0);
    dataBitsComboBox = new QComboBox(serialPage);
    dataBitsComboBox->addItem(QStringLiteral("5"), 5);
    dataBitsComboBox->addItem(QStringLiteral("6"), 6);
    dataBitsComboBox->addItem(QStringLiteral("7"), 7);
    dataBitsComboBox->addItem(QStringLiteral("8"), 8);
    dataBitsComboBox->setCurrentIndex(3);
    configLayout->addWidget(dataBitsComboBox, 1, 1);

    configLayout->addWidget(new QLabel(QStringLiteral("校验位"), serialPage), 1, 2);
    parityComboBox = new QComboBox(serialPage);
    parityComboBox->addItem(QStringLiteral("无"), int(QSerialPort::NoParity));
    parityComboBox->addItem(QStringLiteral("偶校验"), int(QSerialPort::EvenParity));
    parityComboBox->addItem(QStringLiteral("奇校验"), int(QSerialPort::OddParity));
    parityComboBox->addItem(QStringLiteral("空格"), int(QSerialPort::SpaceParity));
    parityComboBox->addItem(QStringLiteral("标记"), int(QSerialPort::MarkParity));
    parityComboBox->setCurrentIndex(0);
    configLayout->addWidget(parityComboBox, 1, 3);

    configLayout->addWidget(new QLabel(QStringLiteral("停止位"), serialPage), 2, 0);
    stopBitsComboBox = new QComboBox(serialPage);
    stopBitsComboBox->addItem(QStringLiteral("1"), int(QSerialPort::OneStop));
    stopBitsComboBox->addItem(QStringLiteral("1.5"), int(QSerialPort::OneAndHalfStop));
    stopBitsComboBox->addItem(QStringLiteral("2"), int(QSerialPort::TwoStop));
    stopBitsComboBox->setCurrentIndex(0);
    configLayout->addWidget(stopBitsComboBox, 2, 1);

    configLayout->addWidget(new QLabel(QStringLiteral("流控"), serialPage), 2, 2);
    flowControlComboBox = new QComboBox(serialPage);
    flowControlComboBox->addItem(QStringLiteral("无"), int(QSerialPort::NoFlowControl));
    flowControlComboBox->addItem(QStringLiteral("硬件流控"), int(QSerialPort::HardwareControl));
    flowControlComboBox->addItem(QStringLiteral("软件流控"), int(QSerialPort::SoftwareControl));
    flowControlComboBox->setCurrentIndex(0);
    configLayout->addWidget(flowControlComboBox, 2, 3);

    refreshPortsButton = new QPushButton(QStringLiteral("刷新端口"), serialPage);
    openCloseButton = new QPushButton(QStringLiteral("打开串口"), serialPage);
    configLayout->addWidget(refreshPortsButton, 0, 4);
    configLayout->addWidget(openCloseButton, 1, 4, 2, 1);
    rootLayout->addWidget(configBox);

    auto *debugBox = new QGroupBox(QStringLiteral("发送与接收"), serialPage);
    auto *debugLayout = new QVBoxLayout(debugBox);
    debugLayout->setSpacing(10);

    auto *sendTopLayout = new QHBoxLayout;
    sendLineEdit = new QLineEdit(serialPage);
    sendLineEdit->setPlaceholderText(QStringLiteral("输入要发送的内容"));
    sendButton = new QPushButton(QStringLiteral("发送"), serialPage);
    hexSendCheckBox = new QCheckBox(QStringLiteral("HEX发送"), serialPage);
    autoSendCheckBox = new QCheckBox(QStringLiteral("自动发送"), serialPage);
    autoIntervalSpinBox = new QSpinBox(serialPage);
    autoIntervalSpinBox->setRange(100, 10000);
    autoIntervalSpinBox->setValue(1000);
    autoIntervalSpinBox->setSuffix(QStringLiteral(" ms"));
    sendTopLayout->addWidget(sendLineEdit, 1);
    sendTopLayout->addWidget(sendButton);
    sendTopLayout->addWidget(hexSendCheckBox);
    sendTopLayout->addWidget(autoSendCheckBox);
    sendTopLayout->addWidget(autoIntervalSpinBox);
    debugLayout->addLayout(sendTopLayout);

    auto *actionLayout = new QHBoxLayout;
    hexReceiveCheckBox = new QCheckBox(QStringLiteral("HEX接收"), serialPage);
    auto *clearReceiveButton = new QPushButton(QStringLiteral("清空接收"), serialPage);
    auto *clearTransmitButton = new QPushButton(QStringLiteral("清空发送"), serialPage);
    actionLayout->addWidget(hexReceiveCheckBox);
    actionLayout->addStretch();
    actionLayout->addWidget(clearReceiveButton);
    actionLayout->addWidget(clearTransmitButton);
    debugLayout->addLayout(actionLayout);

    auto *editLayout = new QHBoxLayout;
    auto *receivePanel = new QWidget(serialPage);
    auto *receivePanelLayout = new QVBoxLayout(receivePanel);
    receiveEdit = new QTextEdit(serialPage);
    receiveEdit->setReadOnly(true);
    receiveEdit->setPlaceholderText(QStringLiteral("接收到的数据会显示在这里"));
    receivePanelLayout->addWidget(new QLabel(QStringLiteral("接收区"), serialPage));
    receivePanelLayout->addWidget(receiveEdit, 1);
    editLayout->addWidget(receivePanel, 1);

    auto *transmitPanel = new QWidget(serialPage);
    auto *transmitPanelLayout = new QVBoxLayout(transmitPanel);
    transmitEdit = new QTextEdit(serialPage);
    transmitEdit->setReadOnly(true);
    transmitEdit->setPlaceholderText(QStringLiteral("发送记录会显示在这里"));
    transmitPanelLayout->addWidget(new QLabel(QStringLiteral("发送区"), serialPage));
    transmitPanelLayout->addWidget(transmitEdit, 1);
    editLayout->addWidget(transmitPanel, 1);
    debugLayout->addLayout(editLayout, 1);

    statusLabel = new QLabel(QStringLiteral("等待串口配置..."), serialPage);
    statusLabel->setWordWrap(true);
    debugLayout->addWidget(statusLabel);

    rootLayout->addWidget(debugBox, 1);

    connect(backButton, &QPushButton::clicked, this, &MainWindow::showHomePage);
    connect(refreshPortsButton, &QPushButton::clicked, this, &MainWindow::onRefreshPorts);
    connect(openCloseButton, &QPushButton::clicked, this, &MainWindow::onOpenClosePort);
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::onSendData);
    connect(clearReceiveButton, &QPushButton::clicked, this, &MainWindow::onClearReceive);
    connect(clearTransmitButton, &QPushButton::clicked, this, &MainWindow::onClearTransmit);
    connect(autoSendCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && serialPort->isOpen()) {
            autoSendTimer->start(autoIntervalSpinBox->value());
        } else {
            autoSendTimer->stop();
        }
    });
    connect(autoIntervalSpinBox, &QSpinBox::valueChanged, this, [this](int value) {
        if (autoSendCheckBox->isChecked()) {
            autoSendTimer->start(value);
        }
    });
}

// ═══════════════════════════════════════════════════════════
// Page 2: 通信协议调试界面
// ═══════════════════════════════════════════════════════════

void MainWindow::createProtocolDebugPage()
{
    protocolPage = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(protocolPage);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(8);

    // ── 顶部: 返回 + 标题 ────────────────────────────
    auto *topLayout = new QHBoxLayout;
    auto *protoBackBtn = new QPushButton(QStringLiteral("返回首页"), protocolPage);
    protoBackBtn->setMinimumHeight(34);
    auto *protoTitle = new QLabel(QStringLiteral("通信协议调试 (UART3)"), protocolPage);
    protoTitle->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold;"));
    topLayout->addWidget(protoBackBtn);
    topLayout->addWidget(protoTitle, 1);
    topLayout->addStretch();
    rootLayout->addLayout(topLayout);
    connect(protoBackBtn, &QPushButton::clicked, this, &MainWindow::showHomePage);

    // ── 主布局: 左侧面板 + 右侧面板 ──────────────────
    auto *mainLayout = new QHBoxLayout;
    mainLayout->setSpacing(12);

    // ═══════════════════════════════════════════════════
    // 左侧面板
    // ═══════════════════════════════════════════════════
    auto *leftPanel = new QWidget(protocolPage);
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);
    leftPanel->setFixedWidth(320);

    // ── 串口配置 ─────────────────────────────────────
    auto *cfgBox = new QGroupBox(QStringLiteral("串口配置"), leftPanel);
    auto *cfgLayout = new QGridLayout(cfgBox);
    cfgLayout->setSpacing(6);

    cfgLayout->addWidget(new QLabel(QStringLiteral("端口"), cfgBox), 0, 0);
    protoPortCombo = new QComboBox(cfgBox);
    protoPortCombo->setMinimumWidth(100);
    cfgLayout->addWidget(protoPortCombo, 0, 1);

    cfgLayout->addWidget(new QLabel(QStringLiteral("波特率"), cfgBox), 1, 0);
    protoBaudCombo = new QComboBox(cfgBox);
    protoBaudCombo->addItem(QStringLiteral("9600"), 9600);
    protoBaudCombo->addItem(QStringLiteral("19200"), 19200);
    protoBaudCombo->addItem(QStringLiteral("38400"), 38400);
    protoBaudCombo->addItem(QStringLiteral("57600"), 57600);
    protoBaudCombo->addItem(QStringLiteral("115200"), 115200);
    protoBaudCombo->addItem(QStringLiteral("230400"), 230400);
    protoBaudCombo->addItem(QStringLiteral("460800"), 460800);
    protoBaudCombo->addItem(QStringLiteral("921600"), 921600);
    protoBaudCombo->setCurrentIndex(5);  // 默认 230400
    cfgLayout->addWidget(protoBaudCombo, 1, 1);

    protoRefreshBtn = new QPushButton(QStringLiteral("刷新"), cfgBox);
    protoOpenBtn = new QPushButton(QStringLiteral("打开串口"), cfgBox);
    protoOpenBtn->setMinimumHeight(30);
    cfgLayout->addWidget(protoRefreshBtn, 0, 2);
    cfgLayout->addWidget(protoOpenBtn, 1, 2);

    leftLayout->addWidget(cfgBox);

    // ── 快速命令 ─────────────────────────────────────
    auto *cmdBox = new QGroupBox(QStringLiteral("快速命令"), leftPanel);
    auto *cmdLayout = new QVBoxLayout(cmdBox);
    cmdLayout->setSpacing(6);

    auto *quickRow = new QHBoxLayout;
    pingBtn = new QPushButton(QStringLiteral("Ping 心跳"), cmdBox);
    startBtn = new QPushButton(QStringLiteral("启动电机"), cmdBox);
    stopBtn = new QPushButton(QStringLiteral("停止电机"), cmdBox);
    pingBtn->setStyleSheet(QStringLiteral("QPushButton { background-color: #e3f2fd; }"));
    startBtn->setStyleSheet(QStringLiteral("QPushButton { background-color: #e8f5e9; }"));
    stopBtn->setStyleSheet(QStringLiteral("QPushButton { background-color: #ffebee; }"));
    quickRow->addWidget(pingBtn);
    quickRow->addWidget(startBtn);
    quickRow->addWidget(stopBtn);
    cmdLayout->addLayout(quickRow);

    // Float 写入
    auto *floatBox = new QGroupBox(QStringLiteral("Float 写入"), cmdBox);
    auto *floatLayout = new QGridLayout(floatBox);
    floatLayout->setSpacing(4);

    floatLayout->addWidget(new QLabel(QStringLiteral("ID:"), floatBox), 0, 0);
    floatIdCombo = new QComboBox(floatBox);
    for (int i = 0; i < floatIDTableSize; ++i) {
        if (floatIDTable[i].writable) {
            floatIdCombo->addItem(
                QStringLiteral("0x%1 %2").arg(floatIDTable[i].id, 2, 16, QChar('0')).arg(
                    QString::fromUtf8(floatIDTable[i].name)),
                floatIDTable[i].id);
        }
    }
    floatLayout->addWidget(floatIdCombo, 0, 1, 1, 2);

    floatLayout->addWidget(new QLabel(QStringLiteral("值:"), floatBox), 1, 0);
    floatValueEdit = new QLineEdit(floatBox);
    floatValueEdit->setPlaceholderText(QStringLiteral("浮点数，如 3.5"));
    floatWriteBtn = new QPushButton(QStringLiteral("写入"), floatBox);
    floatLayout->addWidget(floatValueEdit, 1, 1);
    floatLayout->addWidget(floatWriteBtn, 1, 2);
    cmdLayout->addWidget(floatBox);

    // 批量写入
    auto *batchBox = new QGroupBox(QStringLiteral("批量写入"), cmdBox);
    auto *batchLayout = new QGridLayout(batchBox);
    batchLayout->setSpacing(4);

    batchLayout->addWidget(new QLabel(QStringLiteral("ID:"), batchBox), 0, 0);
    batchIdCombo = new QComboBox(batchBox);
    for (int i = 0; i < floatIDTableSize; ++i) {
        if (floatIDTable[i].writable) {
            batchIdCombo->addItem(
                QStringLiteral("0x%1 %2").arg(floatIDTable[i].id, 2, 16, QChar('0')).arg(
                    QString::fromUtf8(floatIDTable[i].name)),
                floatIDTable[i].id);
        }
    }
    batchLayout->addWidget(batchIdCombo, 0, 1);

    batchLayout->addWidget(new QLabel(QStringLiteral("值:"), batchBox), 1, 0);
    batchValueEdit = new QLineEdit(batchBox);
    batchValueEdit->setPlaceholderText(QStringLiteral("浮点数"));
    batchAddBtn = new QPushButton(QStringLiteral("添加"), batchBox);
    batchLayout->addWidget(batchValueEdit, 1, 1);
    batchLayout->addWidget(batchAddBtn, 1, 2);

    batchPreviewEdit = new QTextEdit(batchBox);
    batchPreviewEdit->setReadOnly(true);
    batchPreviewEdit->setMaximumHeight(80);
    batchPreviewEdit->setPlaceholderText(QStringLiteral("已添加的批量项..."));
    batchLayout->addWidget(batchPreviewEdit, 2, 0, 1, 3);

    batchSendBtn = new QPushButton(QStringLiteral("批量发送"), batchBox);
    batchSendBtn->setMinimumHeight(30);
    batchLayout->addWidget(batchSendBtn, 3, 0, 1, 3);

    cmdLayout->addWidget(batchBox);
    leftLayout->addWidget(cmdBox);

    // ── 帧统计 ───────────────────────────────────────
    auto *statsBox = new QGroupBox(QStringLiteral("帧统计"), leftPanel);
    auto *statsLayout = new QGridLayout(statsBox);
    statsLayout->setSpacing(4);

    statsLayout->addWidget(new QLabel(QStringLiteral("接收帧:"), statsBox), 0, 0);
    statRxFrames = new QLabel(QStringLiteral("0"), statsBox);
    statRxFrames->setAlignment(Qt::AlignRight);
    statsLayout->addWidget(statRxFrames, 0, 1);

    statsLayout->addWidget(new QLabel(QStringLiteral("校验错:"), statsBox), 0, 2);
    statChkErr = new QLabel(QStringLiteral("0"), statsBox);
    statChkErr->setAlignment(Qt::AlignRight);
    statChkErr->setStyleSheet(QStringLiteral("QLabel { color: #c62828; }"));
    statsLayout->addWidget(statChkErr, 0, 3);

    statsLayout->addWidget(new QLabel(QStringLiteral("遥测帧:"), statsBox), 1, 0);
    statTelemetry = new QLabel(QStringLiteral("0"), statsBox);
    statTelemetry->setAlignment(Qt::AlignRight);
    statsLayout->addWidget(statTelemetry, 1, 1);

    statsLayout->addWidget(new QLabel(QStringLiteral("ACK帧:"), statsBox), 1, 2);
    statAck = new QLabel(QStringLiteral("0"), statsBox);
    statAck->setAlignment(Qt::AlignRight);
    statsLayout->addWidget(statAck, 1, 3);

    leftLayout->addWidget(statsBox);
    leftLayout->addStretch();

    // ═══════════════════════════════════════════════════
    // 右侧面板: 遥测数据
    // ═══════════════════════════════════════════════════
    auto *rightPanel = new QWidget(protocolPage);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);

    // ── 遥测数据表格 ────────────────────────────────
    auto *teleBox = new QGroupBox(QStringLiteral("遥测数据 (MCU 上传 10 floats)"), rightPanel);
    auto *teleLayout = new QVBoxLayout(teleBox);

    telemetryTable = new QTableWidget(floatIDTableSize, 3, teleBox);
    telemetryTable->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("名称"), QStringLiteral("值")});
    telemetryTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    telemetryTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    telemetryTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    telemetryTable->verticalHeader()->setVisible(false);
    telemetryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    telemetryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    telemetryTable->setAlternatingRowColors(true);

    for (int i = 0; i < floatIDTableSize; ++i) {
        auto *idItem = new QTableWidgetItem(
            QStringLiteral("0x%1").arg(floatIDTable[i].id, 2, 16, QChar('0')).toUpper());
        idItem->setTextAlignment(Qt::AlignCenter);
        telemetryTable->setItem(i, 0, idItem);

        auto *nameItem = new QTableWidgetItem(
            QString::fromUtf8(floatIDTable[i].name));
        telemetryTable->setItem(i, 1, nameItem);

        auto *valItem = new QTableWidgetItem(QStringLiteral("—"));
        valItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valItem->setForeground(QColor(0x15, 0x65, 0xc0));
        QFont f = valItem->font(); f.setBold(true); f.setPointSize(10); valItem->setFont(f);
        telemetryTable->setItem(i, 2, valItem);
    }

    teleLayout->addWidget(telemetryTable);
    rightLayout->addWidget(teleBox, 2);

    // ── 诊断日志 (仅帧事件 + 错误) ───────────────────
    auto *diagBox = new QGroupBox(QStringLiteral("诊断日志"), rightPanel);
    auto *diagLayout = new QVBoxLayout(diagBox);
    protocolLogEdit = new QTextEdit(rightPanel);
    protocolLogEdit->setReadOnly(true);
    protocolLogEdit->setMaximumHeight(120);
    protocolLogEdit->setFont(QFont(QStringLiteral("Consolas"), 8));
    diagLayout->addWidget(protocolLogEdit);
    auto *diagBtnRow = new QHBoxLayout;
    diagBtnRow->addStretch();
    auto *clearDiagBtn = new QPushButton(QStringLiteral("清空"), rightPanel);
    connect(clearDiagBtn, &QPushButton::clicked, protocolLogEdit, &QTextEdit::clear);
    diagBtnRow->addWidget(clearDiagBtn);
    diagLayout->addLayout(diagBtnRow);
    rightLayout->addWidget(diagBox);

    // ── 状态栏 ───────────────────────────────────────
    protocolStatusLabel = new QLabel(QStringLiteral("请先打开串口。"), protocolPage);
    protocolStatusLabel->setWordWrap(true);
    protocolStatusLabel->setStyleSheet(QStringLiteral("color: #555; padding: 4px; border-top: 1px solid #ddd;"));
    rightLayout->addWidget(protocolStatusLabel);

    // ── 装配主布局 ───────────────────────────────────
    mainLayout->addWidget(leftPanel);
    mainLayout->addWidget(rightPanel, 1);
    rootLayout->addLayout(mainLayout, 1);

    // ── 连接信号 ─────────────────────────────────────
    connect(protoRefreshBtn, &QPushButton::clicked,
            this, &MainWindow::onProtocolRefreshPorts);
    connect(protoOpenBtn, &QPushButton::clicked,
            this, &MainWindow::onProtocolOpenClose);
    connect(pingBtn, &QPushButton::clicked,
            this, &MainWindow::onSendPing);
    connect(startBtn, &QPushButton::clicked,
            this, &MainWindow::onSendStart);
    connect(stopBtn, &QPushButton::clicked,
            this, &MainWindow::onSendStop);
    connect(floatWriteBtn, &QPushButton::clicked,
            this, &MainWindow::onSendFloatWrite);
    connect(batchAddBtn, &QPushButton::clicked,
            this, &MainWindow::onSendBatchWrite);
    connect(batchSendBtn, &QPushButton::clicked, this, [this]() {
        // 发送已添加的批量项
        if (!protoWorker->isOpen()) {
            protocolStatusLabel->setText(QStringLiteral("请先打开串口。"));
            return;
        }
        // 解析 batchPreviewEdit 中的内容并发送
        QString text = batchPreviewEdit->toPlainText().trimmed();
        if (text.isEmpty()) {
            protocolStatusLabel->setText(QStringLiteral("批量列表为空。"));
            return;
        }
        BatchWriteItems items;
        const QStringList lines = text.split('\n');
        for (const QString &line : lines) {
            const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")));
            if (parts.size() >= 2) {
                bool okId = false, okVal = false;
                uint8_t id = static_cast<uint8_t>(parts[0].toUInt(&okId, 0));
                float val = parts[1].toFloat(&okVal);
                if (okId && okVal) {
                    items.append({id, val});
                }
            }
        }
        if (!items.isEmpty()) {
            QMetaObject::invokeMethod(protoWorker, "sendBatchWrite",
                                      Qt::QueuedConnection,
                                      Q_ARG(BatchWriteItems, items));
            appendProtocolLog(QStringLiteral("→ 批量发送 %1 个浮点数").arg(items.size()));
        }
    });
    // 初始化端口列表
    onProtocolRefreshPorts();
}

// ═══════════════════════════════════════════════════════════
// 页面切换
// ═══════════════════════════════════════════════════════════

void MainWindow::showHomePage()
{
    stackedWidget->setCurrentWidget(homePage);
}

void MainWindow::showSerialPage()
{
    stackedWidget->setCurrentWidget(serialPage);
}

void MainWindow::showProtocolPage()
{
    stackedWidget->setCurrentWidget(protocolPage);
}

void MainWindow::showMotorPage()
{
    stackedWidget->setCurrentWidget(motorPage);
}

void MainWindow::showRehabPage()
{
    stackedWidget->setCurrentWidget(rehabPage);
}

// ═══════════════════════════════════════════════════════════
// Page 1: 串口调试 — 槽函数 (保持不变)
// ═══════════════════════════════════════════════════════════

void MainWindow::onRefreshPorts()
{
    portComboBox->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        QString displayName = info.portName();
        if (!info.description().isEmpty()) {
            displayName += QStringLiteral(" - %1").arg(info.description());
        } else if (!info.manufacturer().isEmpty()) {
            displayName += QStringLiteral(" - %1").arg(info.manufacturer());
        }
        portComboBox->addItem(displayName, info.portName());
    }

    if (portComboBox->count() == 0) {
        portComboBox->addItem(QStringLiteral("无可用串口"));
        portComboBox->setEnabled(false);
        openCloseButton->setEnabled(false);
        statusLabel->setText(QStringLiteral("当前没有检测到串口，请连接设备后刷新。"));
    } else {
        portComboBox->setEnabled(true);
        openCloseButton->setEnabled(true);
        statusLabel->setText(QStringLiteral("已检测到可用串口。"));
    }
}

void MainWindow::onOpenClosePort()
{
    if (serialPort->isOpen()) {
        serialPort->close();
        openCloseButton->setText(QStringLiteral("打开串口"));
        statusLabel->setText(QStringLiteral("串口已关闭。"));
        autoSendCheckBox->setEnabled(false);
        return;
    }

    serialPort->setPortName(portComboBox->currentData().toString());
    serialPort->setBaudRate(baudRateFromComboBox());
    serialPort->setDataBits(dataBitsFromComboBox());
    serialPort->setParity(parityFromComboBox());
    serialPort->setStopBits(stopBitsFromComboBox());
    serialPort->setFlowControl(flowControlFromComboBox());

    if (serialPort->open(QIODevice::ReadWrite)) {
        openCloseButton->setText(QStringLiteral("关闭串口"));
        statusLabel->setText(QStringLiteral("串口已打开，准备发送和接收数据。"));
        autoSendCheckBox->setEnabled(true);
    } else {
        statusLabel->setText(QStringLiteral("打开串口失败: %1").arg(serialPort->errorString()));
    }
}

void MainWindow::onSendData()
{
    if (!serialPort->isOpen()) {
        statusLabel->setText(QStringLiteral("请先打开串口再发送数据。"));
        return;
    }

    QString payload = sendLineEdit->text();
    QByteArray data;
    if (hexSendCheckBox->isChecked()) {
        QString compact = payload.remove(QRegularExpression(QStringLiteral("\\s")));
        data = QByteArray::fromHex(compact.toLatin1());
    } else {
        data = payload.toUtf8();
    }

    serialPort->write(data);
    appendTransmitData(payload);
    sendLineEdit->clear();
    statusLabel->setText(QStringLiteral("数据已发送。"));
}

void MainWindow::onClearReceive()
{
    receiveEdit->clear();
}

void MainWindow::onClearTransmit()
{
    transmitEdit->clear();
}

void MainWindow::onSerialReadyRead()
{
    QByteArray data = serialPort->readAll();
    appendReceiveData(data);
}

void MainWindow::onAutoSendTimeout()
{
    if (serialPort->isOpen() && autoSendCheckBox->isChecked()) {
        onSendData();
    }
}

// ═══════════════════════════════════════════════════════════
// Page 2: 协议调试 — 槽函数
// ═══════════════════════════════════════════════════════════

void MainWindow::onProtocolRefreshPorts()
{
    protoPortCombo->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        QString displayName = info.portName();
        if (!info.description().isEmpty()) {
            displayName += QStringLiteral(" - %1").arg(info.description());
        } else if (!info.manufacturer().isEmpty()) {
            displayName += QStringLiteral(" - %1").arg(info.manufacturer());
        }
        protoPortCombo->addItem(displayName, info.portName());
    }

    if (protoPortCombo->count() == 0) {
        protoPortCombo->addItem(QStringLiteral("无可用串口"));
        protoOpenBtn->setEnabled(false);
    } else {
        protoOpenBtn->setEnabled(true);
    }
}

void MainWindow::onProtocolOpenClose()
{
    if (protoWorker->isOpen()) {
        QMetaObject::invokeMethod(protoWorker, "closePort", Qt::QueuedConnection);
        return;
    }

    if (protoPortCombo->count() == 0 || protoPortCombo->currentData().toString().isEmpty()) {
        protocolStatusLabel->setText(QStringLiteral("没有可用的串口。"));
        return;
    }

    QString portName = protoPortCombo->currentData().toString();
    int baud = protoBaudCombo->currentData().toInt();

    // 协议页使用 8N1 固定参数
    QMetaObject::invokeMethod(protoWorker, "openPort", Qt::QueuedConnection,
                              Q_ARG(QString, portName),
                              Q_ARG(int, baud),
                              Q_ARG(int, int(QSerialPort::Data8)),
                              Q_ARG(int, int(QSerialPort::NoParity)),
                              Q_ARG(int, int(QSerialPort::OneStop)),
                              Q_ARG(int, int(QSerialPort::NoFlowControl)));
}

void MainWindow::onSendPing()
{
    if (!protoWorker->isOpen()) {
        protocolStatusLabel->setText(QStringLiteral("请先打开串口。"));
        return;
    }
    QMetaObject::invokeMethod(protoWorker, "sendPing", Qt::QueuedConnection);
    appendProtocolLog(QStringLiteral("→ 发送 PING"));
}

void MainWindow::onSendStart()
{
    if (!protoWorker->isOpen()) {
        protocolStatusLabel->setText(QStringLiteral("请先打开串口。"));
        return;
    }
    QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
                              Q_ARG(uint8_t, IO_M1_START_STOP),
                              Q_ARG(uint8_t, IO_VAL_START));
    appendProtocolLog(QStringLiteral("→ 发送 IO_WRITE: START_STOP = 启动"));
}

void MainWindow::onSendStop()
{
    if (!protoWorker->isOpen()) {
        protocolStatusLabel->setText(QStringLiteral("请先打开串口。"));
        return;
    }
    QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
                              Q_ARG(uint8_t, IO_M1_START_STOP),
                              Q_ARG(uint8_t, IO_VAL_STOP));
    appendProtocolLog(QStringLiteral("→ 发送 IO_WRITE: START_STOP = 停止"));
}

void MainWindow::onSendFloatWrite()
{
    if (!protoWorker->isOpen()) {
        protocolStatusLabel->setText(QStringLiteral("请先打开串口。"));
        return;
    }

    bool ok = false;
    float val = floatValueEdit->text().toFloat(&ok);
    if (!ok) {
        protocolStatusLabel->setText(QStringLiteral("请输入有效的浮点数值。"));
        return;
    }

    uint8_t id = static_cast<uint8_t>(floatIdCombo->currentData().toUInt());
    QMetaObject::invokeMethod(protoWorker, "sendFloatWrite", Qt::QueuedConnection,
                              Q_ARG(uint8_t, id),
                              Q_ARG(float, val));

    QString name = floatIdCombo->currentText();
    appendProtocolLog(QStringLiteral("→ 发送 FLOAT_WRITE: %1 = %2").arg(name).arg(val));
    floatValueEdit->clear();
}

void MainWindow::onSendBatchWrite()
{
    // 添加一项到批量列表
    bool ok = false;
    float val = batchValueEdit->text().toFloat(&ok);
    if (!ok) {
        protocolStatusLabel->setText(QStringLiteral("请输入有效的浮点数值。"));
        return;
    }

    uint8_t id = static_cast<uint8_t>(batchIdCombo->currentData().toUInt());
    QString entry = QStringLiteral("%1 %2").arg(id).arg(val, 0, 'f', 3);
    QString current = batchPreviewEdit->toPlainText().trimmed();
    if (!current.isEmpty()) {
        current += '\n';
    }
    batchPreviewEdit->setPlainText(current + entry);
    batchValueEdit->clear();
    protocolStatusLabel->setText(QStringLiteral("已添加到批量列表。"));
}

void MainWindow::onClearProtocolLog()
{
    if (telemetryTable) {
        for (int row = 0; row < floatIDTableSize; ++row)
            telemetryTable->item(row, 2)->setText(QStringLiteral("—"));
    }
}

// ═══════════════════════════════════════════════════════════
// ProtocolWorker 回调 (在主线程执行)
// ═══════════════════════════════════════════════════════════

void MainWindow::onProtocolFrameParsed(uint8_t cmd, const QByteArray &payload,
                                        bool sumOk, bool endOk, uint8_t rxSum, uint8_t calcSum)
{
    if (!sumOk || !endOk) {
        QString reason = !endOk ? QStringLiteral("END错(end=0x%1,期望=0xED)").arg(rxSum, 2, 16, QChar('0'))
                        : QStringLiteral("SUM错(rx=0x%1,calc=0x%2)").arg(rxSum, 2, 16, QChar('0')).arg(calcSum, 2, 16, QChar('0'));
        appendProtocolLog(QStringLiteral("%1 CMD=%2 LEN=%3 %4")
            .arg(reason)
            .arg(cmdName(cmd))
            .arg(static_cast<uint8_t>(payload[0])));
    }
}

void MainWindow::onTelemetryReceived(const QMap<uint8_t, float> &values)
{
    static int telemetryCount = 0;
    telemetryCount++;
    statTelemetry->setText(QString::number(telemetryCount));

    // ── 推入队列 (由定时器批量刷新图表) ────────────
    {
        QMutexLocker lock(&mTelemetryMutex);
        mTelemetryQueue.append(values);
    }
    // ── 更新康复页面标签 (轻量操作) ────────────────
    updateMotorDisplay(values);

    // ── 更新遥测表格 ────────────────────────────────
    if (telemetryTable) {
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            for (int row = 0; row < floatIDTableSize; ++row) {
                if (floatIDTable[row].id == it.key()) {
                    telemetryTable->item(row, 2)->setText(
                        QString::number(it.value(), 'f', 2));
                    break;
                }
            }
        }
    }
}

void MainWindow::onAckReceived(uint8_t ackCmd)
{
    static int ackCount = 0;
    ackCount++;
    statAck->setText(QString::number(ackCount));
    // ACK 不记录日志，仅更新统计
    Q_UNUSED(ackCmd);
}

void MainWindow::onIoReportReceived(uint8_t ioId, uint8_t value)
{
    QString ioName;
    for (int i = 0; i < ioPinTableSize; ++i) {
        if (ioPinTable[i].id == ioId) {
            ioName = QString::fromUtf8(ioPinTable[i].name);
            break;
        }
    }
    if (ioName.isEmpty()) {
        ioName = QStringLiteral("IO_0x%1").arg(ioId, 2, 16, QChar('0'));
    }
    appendProtocolLog(QStringLiteral("← IO_REPORT: %1 = 0x%2")
                          .arg(ioName)
                          .arg(value, 2, 16, QChar('0')));
}

void MainWindow::onProtocolRawReceived(const QByteArray & /*data*/)
{
    // 不记录 RAW 数据，避免刷屏
}

void MainWindow::onProtocolRawSent(const QByteArray & /*data*/)
{
    // 不记录
}

void MainWindow::onProtocolStatsChanged(int totalFrames, int checksumErrors)
{
    statRxFrames->setText(QString::number(totalFrames));
    statChkErr->setText(QString::number(checksumErrors));
    if (checksumErrors > 0) {
        statChkErr->setStyleSheet(QStringLiteral("QLabel { color: #c62828; font-weight: bold; }"));
    }
}

void MainWindow::onProtocolParseError(const QString &msg)
{
    appendProtocolLog(QStringLiteral("⚠ 解析错误: %1").arg(msg));
}

void MainWindow::onProtocolPortError(const QString &msg)
{
    protocolStatusLabel->setText(QStringLiteral("串口错误: %1").arg(msg));
}

void MainWindow::onProtocolPortOpened()
{
    protoOpenBtn->setText(QStringLiteral("关闭串口"));
    protocolStatusLabel->setText(QStringLiteral("串口已打开 (工作线程: 8N1)。"));
    // 重置统计
    statRxFrames->setText(QStringLiteral("0"));
    statChkErr->setText(QStringLiteral("0"));
    statTelemetry->setText(QStringLiteral("0"));
    statAck->setText(QStringLiteral("0"));
    // 重置 motor 曲线
    m1ChartCtx.time = 0; m2ChartCtx.time = 0;
    for (auto &ctx : {&m1ChartCtx, &m2ChartCtx}) {
        for (auto it = ctx->series.begin(); it != ctx->series.end(); ++it) {
            it.value()->clear();
            ctx->buffers[it.key()].clear();
        }
    }
}

void MainWindow::onProtocolPortClosed()
{
    protoOpenBtn->setText(QStringLiteral("打开串口"));
    protocolStatusLabel->setText(QStringLiteral("串口已关闭。"));
}

// ═══════════════════════════════════════════════════════════
// 实时曲线 Tab
// ═══════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════
// Page 3: 电机控制调试
// ═══════════════════════════════════════════════════════════

void MainWindow::createMotorControlPage()
{
    motorPage = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(motorPage);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(4);

    // 顶部
    auto *topLayout = new QHBoxLayout;
    auto *backBtn = new QPushButton(QStringLiteral("返回首页"), motorPage);
    auto *title = new QLabel(QStringLiteral("电机控制调试"), motorPage);
    title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));
    topLayout->addWidget(backBtn);
    topLayout->addWidget(title, 1);

    auto *flashBtn = new QPushButton(QStringLiteral("存入 Flash"), motorPage);
    flashBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #fff3e0; font-weight: bold; font-size: 11px; "
        "border: 1px solid #ff9800; padding: 2px 10px; }"));
    connect(flashBtn, &QPushButton::clicked, this, [this]() {
        if (!protoWorker->isOpen()) return;
        QMetaObject::invokeMethod(protoWorker, "sendFlashSave", Qt::QueuedConnection);
        motorStatusLabel->setText(QStringLiteral("FlashSave 已发送 — M1 + M2 PID 生效+持久化"));
    });
    topLayout->addWidget(flashBtn);

    rootLayout->addLayout(topLayout);
    connect(backBtn, &QPushButton::clicked, this, &MainWindow::showHomePage);

    // ═══════════════════════════════════════════════
    // 双电机面板 (垂直排列)
    // ═══════════════════════════════════════════════
    auto *dualLayout = new QHBoxLayout;
    dualLayout->setSpacing(8);

    // ── 发送 float 指令 ────────────────────────────
    auto sendFloatCmd = [this](const QString &label, uint8_t floatId, QLineEdit *edit) {
        if (!protoWorker->isOpen()) { motorStatusLabel->setText(QStringLiteral("请先打开串口")); return; }
        bool ok; float v = edit->text().toFloat(&ok);
        if (!ok) { motorStatusLabel->setText(QStringLiteral("请输入有效数值")); return; }
        QMetaObject::invokeMethod(protoWorker, "sendFloatWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, floatId), Q_ARG(float, v));
        if (floatId == 0x01) m1LastAngleCmd = v;  // 记录角度指令
        motorStatusLabel->setText(QStringLiteral("已发送: %1 = %2").arg(label).arg(v));
    };

    // ── 创建 float 输入行 (回车即发) ───────────────
    auto makeFloatRow = [&](const QString &label, const QString &placeholder,
                            uint8_t floatId, QWidget *parent) -> QHBoxLayout*
    {
        auto *row = new QHBoxLayout;
        auto *lbl = new QLabel(label, parent);
        lbl->setFixedWidth(52);
        row->addWidget(lbl);
        auto *edit = new QLineEdit(parent);
        edit->setPlaceholderText(placeholder);
        edit->setMaximumHeight(24);
        row->addWidget(edit, 1);
        connect(edit, &QLineEdit::returnPressed, this, [this, edit, floatId, label, sendFloatCmd]() {
            sendFloatCmd(label, floatId, edit);
        });
        return row;
    };

    // ── 遥测标签工厂 ────────────────────────────────
    auto makeTeleLabel = [](QWidget *p) {
        auto *l = new QLabel(QStringLiteral("—"), p);
        l->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px; color: #1565c0;"));
        l->setAlignment(Qt::AlignRight);
        return l;
    };

    // ═══════════════════════════════════════════════
    // 电机1 面板
    // ═══════════════════════════════════════════════
    auto *m1Box = new QGroupBox(QStringLiteral("电机1 (TIM2 控制)"), motorPage);
    auto *m1Layout = new QVBoxLayout(m1Box);
    m1Layout->setSpacing(4);
    m1Layout->setContentsMargins(6, 6, 6, 6);

    // 启停合一
    m1ToggleBtn = new QPushButton(QStringLiteral("▶ 启动"), m1Box);
    m1ToggleBtn->setMaximumHeight(28);
    m1ToggleBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #4caf50; color: white; font-weight: bold; font-size: 12px; }"));
    m1Layout->addWidget(m1ToggleBtn);
    connect(m1ToggleBtn, &QPushButton::clicked, this, [this]() {
        if (!protoWorker->isOpen()) return;
        m1Running = !m1Running;
        if (m1Running) {
            QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
                Q_ARG(uint8_t, IO_M1_START_STOP), Q_ARG(uint8_t, IO_VAL_START));
            m1ToggleBtn->setText(QStringLiteral("■ 停止"));
            m1ToggleBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background-color: #f44336; color: white; font-weight: bold; font-size: 12px; }"));
            motorStatusLabel->setText(QStringLiteral("M1 启动 — 已发送"));
        } else {
            QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
                Q_ARG(uint8_t, IO_M1_START_STOP), Q_ARG(uint8_t, IO_VAL_STOP));
            m1ToggleBtn->setText(QStringLiteral("▶ 启动"));
            m1ToggleBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background-color: #4caf50; color: white; font-weight: bold; font-size: 12px; }"));
            motorStatusLabel->setText(QStringLiteral("M1 停止 — 已发送"));
        }
    });

    // 系统模式 (选择即下发)
    auto *m1SysModeRow = new QHBoxLayout;
    m1SysModeRow->addWidget(new QLabel(QStringLiteral("模式:"), m1Box));
    motorModeCombo = new QComboBox(m1Box);
    motorModeCombo->addItem(QStringLiteral("定点 — PID 角度控制"), MODE_DINGDIAN);
    motorModeCombo->addItem(QStringLiteral("正弦 — 预留"), MODE_ZHENGXIAN);
    motorModeCombo->addItem(QStringLiteral("方波 — 预留"), MODE_FANGBO);
    motorModeCombo->addItem(QStringLiteral("PWM 测试 — 直接输出占空比"), MODE_PWM_TEST);
    motorModeCombo->addItem(QStringLiteral("写 Flash — 保存 PID 并生效"), MODE_WRITE_FLASH);
    motorModeCombo->setCurrentIndex(0);
    m1SysModeRow->addWidget(motorModeCombo, 1);
    m1Layout->addLayout(m1SysModeRow);
    connect(motorModeCombo, &QComboBox::currentIndexChanged, this, [this](int /*idx*/) {
        if (!protoWorker->isOpen()) return;
        uint8_t m = static_cast<uint8_t>(motorModeCombo->currentData().toUInt());
        QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, IO_SYS_MODE), Q_ARG(uint8_t, m));
        motorStatusLabel->setText(QStringLiteral("系统模式 → %1").arg(motorModeCombo->currentText()));
    });

    // 运动参数
    auto *secLabel1 = new QLabel(QStringLiteral("运动参数 (下发即生效)"), m1Box);
    secLabel1->setStyleSheet(QStringLiteral("color: #1565c0; font-weight: bold; font-size: 10px;"));
    m1Layout->addWidget(secLabel1);
    m1Layout->addLayout(makeFloatRow(QStringLiteral("角度目标:"), QStringLiteral("°"), 0x01, m1Box));
    m1Layout->addLayout(makeFloatRow(QStringLiteral("速度目标:"), QStringLiteral(""), 0x02, m1Box));
    m1Layout->addLayout(makeFloatRow(QStringLiteral("电流目标:"), QStringLiteral("mA"), 0x03, m1Box));

    // PID
    auto *secLabel2 = new QLabel(QStringLiteral("PID (缓存→FlashSave)"), m1Box);
    secLabel2->setStyleSheet(QStringLiteral("color: #e65100; font-weight: bold; font-size: 10px;"));
    m1Layout->addWidget(secLabel2);
    m1Layout->addLayout(makeFloatRow(QStringLiteral("Kp:"), QStringLiteral(""), 0x04, m1Box));
    m1Layout->addLayout(makeFloatRow(QStringLiteral("Ki:"), QStringLiteral(""), 0x05, m1Box));

    // PWM
    auto *secLabel3 = new QLabel(QStringLiteral("PWM (PWM_test, 正→CH1 负→CH2)"), m1Box);
    secLabel3->setStyleSheet(QStringLiteral("color: #6a1b9a; font-weight: bold; font-size: 10px;"));
    m1Layout->addWidget(secLabel3);
    m1Layout->addLayout(makeFloatRow(QStringLiteral("PWM:"), QStringLiteral("±1.0"), 0x06, m1Box));
    m1Layout->addLayout(makeFloatRow(QStringLiteral("最大速度:"), QStringLiteral("°/s 默认100"), 0x07, m1Box));


    // M1 遥测曲线
    auto *m1TeleBox = new QGroupBox(QStringLiteral("实时遥测"), m1Box);
    auto *m1TeleLayout = new QVBoxLayout(m1TeleBox);
    m1TeleLayout->setSpacing(2);
    m1TeleLayout->setContentsMargins(2, 2, 2, 2);
    m1Layout->addWidget(m1TeleBox);
    {
        const uint8_t ids[] = {0x01, 0x02, 0x03, 0x04, 0x05};
        const QColor colors[] = {
            QColor(0x1f, 0x77, 0xb4), QColor(0xff, 0x7f, 0x0e),
            QColor(0x2c, 0xa0, 0x2c), QColor(0xd6, 0x27, 0x28),
            QColor(0x94, 0x67, 0xbd),
        };
        buildMotorChart(m1ChartCtx, m1TeleBox, ids, 5, colors);
    }

    dualLayout->addWidget(m1Box, 1);

    // ═══════════════════════════════════════════════
    // 电机2 面板 (RS00 CAN)
    // ═══════════════════════════════════════════════
    auto *m2Box = new QGroupBox(QStringLiteral("电机2 (RS00 CAN)"), motorPage);
    auto *m2Layout = new QVBoxLayout(m2Box);
    m2Layout->setSpacing(4);
    m2Layout->setContentsMargins(6, 6, 6, 6);

    // 使能/停止 合一
    m2ToggleBtn = new QPushButton(QStringLiteral("使能"), m2Box);
    m2ToggleBtn->setMaximumHeight(28);
    m2ToggleBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #4caf50; color: white; font-weight: bold; font-size: 12px; }"));
    m2Layout->addWidget(m2ToggleBtn);
    connect(m2ToggleBtn, &QPushButton::clicked, this, [this]() {
        if (!protoWorker->isOpen()) return;
        m2Running = !m2Running;
        if (m2Running) {
            QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
                Q_ARG(uint8_t, IO_M2_ENABLE), Q_ARG(uint8_t, IO_VAL_ENABLE));
            m2ToggleBtn->setText(QStringLiteral("停止"));
            m2ToggleBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background-color: #f44336; color: white; font-weight: bold; font-size: 12px; }"));
            motorStatusLabel->setText(QStringLiteral("M2 使能 — 已发送"));
        } else {
            QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
                Q_ARG(uint8_t, IO_M2_ENABLE), Q_ARG(uint8_t, IO_VAL_STOP));
            m2ToggleBtn->setText(QStringLiteral("使能"));
            m2ToggleBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background-color: #4caf50; color: white; font-weight: bold; font-size: 12px; }"));
            motorStatusLabel->setText(QStringLiteral("M2 停止 — 已发送"));
        }
    });

    // 运行模式
    auto *m2TopRow = new QHBoxLayout;
    m2TopRow->addWidget(new QLabel(QStringLiteral("模式:"), m2Box));
    m2ModeCombo = new QComboBox(m2Box);
    m2ModeCombo->addItem(QStringLiteral("运控"), M2_MODE_YUNKONG);
    m2ModeCombo->addItem(QStringLiteral("位置"), M2_MODE_POS);
    m2ModeCombo->addItem(QStringLiteral("速度"), M2_MODE_SPEED);
    m2ModeCombo->addItem(QStringLiteral("CSP"), M2_MODE_CSP);
    m2ModeCombo->setCurrentIndex(0);
    m2TopRow->addWidget(m2ModeCombo, 1);
    m2Layout->addLayout(m2TopRow);
    connect(m2ModeCombo, &QComboBox::currentIndexChanged, this, [this](int /*idx*/) {
        if (!protoWorker->isOpen()) return;
        uint8_t m = static_cast<uint8_t>(m2ModeCombo->currentData().toUInt());
        QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, IO_M2_RUN_MODE), Q_ARG(uint8_t, m));
        motorStatusLabel->setText(QStringLiteral("M2 模式 → %1").arg(m2ModeCombo->currentText()));
    });

    // 机械零点 + 清除故障
    auto *m2CmdRow = new QHBoxLayout;
    auto *zeroBtn = new QPushButton(QStringLiteral("设机械零点"), m2Box);
    auto *clrFaultBtn = new QPushButton(QStringLiteral("清除故障"), m2Box);
    zeroBtn->setMaximumHeight(26);
    clrFaultBtn->setMaximumHeight(26);
    zeroBtn->setStyleSheet(QStringLiteral("QPushButton { background-color: #e8eaf6; font-size: 11px; }"));
    clrFaultBtn->setStyleSheet(QStringLiteral("QPushButton { background-color: #ffebee; font-size: 11px; }"));
    m2CmdRow->addWidget(zeroBtn);
    m2CmdRow->addWidget(clrFaultBtn);
    m2Layout->addLayout(m2CmdRow);

    // Float 参数
    auto *m2FloatLbl = new QLabel(QStringLiteral("运控参数 (独立下发, 自动组合)"), m2Box);
    m2FloatLbl->setStyleSheet(QStringLiteral("color: #1565c0; font-weight: bold; font-size: 10px;"));
    m2Layout->addWidget(m2FloatLbl);
    m2Layout->addLayout(makeFloatRow(QStringLiteral("位置:"), QStringLiteral("-12.57~+12.57"), 0x11, m2Box));
    m2Layout->addLayout(makeFloatRow(QStringLiteral("速度:"), QStringLiteral("-33.0~+33.0"), 0x12, m2Box));
    m2Layout->addLayout(makeFloatRow(QStringLiteral("力矩:"), QStringLiteral("-14.0~+14.0"), 0x13, m2Box));
    m2Layout->addLayout(makeFloatRow(QStringLiteral("Kp:"), QStringLiteral("0~500"), 0x14, m2Box));
    m2Layout->addLayout(makeFloatRow(QStringLiteral("Kd:"), QStringLiteral("0~5"), 0x15, m2Box));
    m2Layout->addLayout(makeFloatRow(QStringLiteral("电流限制:"), QStringLiteral("mA"), 0x17, m2Box));
    m2Layout->addLayout(makeFloatRow(QStringLiteral("最大速度:"), QStringLiteral("°/s 默认100"), 0x18, m2Box));

    // M2 遥测曲线
    auto *m2TeleBox = new QGroupBox(QStringLiteral("实时遥测"), m2Box);
    auto *m2TeleLayout = new QVBoxLayout(m2TeleBox);
    m2TeleLayout->setSpacing(2);
    m2TeleLayout->setContentsMargins(2, 2, 2, 2);
    m2Layout->addWidget(m2TeleBox);
    {
        const uint8_t ids[] = {0x11, 0x12, 0x13, 0x14, 0x15};
        const QColor colors[] = {
            QColor(0x1f, 0x77, 0xb4), QColor(0xff, 0x7f, 0x0e),
            QColor(0x2c, 0xa0, 0x2c), QColor(0xd6, 0x27, 0x28),
            QColor(0x94, 0x67, 0xbd),
        };
        buildMotorChart(m2ChartCtx, m2TeleBox, ids, 5, colors);
    }

    dualLayout->addWidget(m2Box, 1);

    rootLayout->addLayout(dualLayout, 1);

    // ── 底部数据标签 ───────────────────────────────
    auto *dataRow = new QHBoxLayout;
    dataRow->setSpacing(16);

    auto makeDataGroup = [](const QString &title, QLabel *labels[], int n, QWidget *parent) -> QGroupBox* {
        auto *box = new QGroupBox(title, parent);
        auto *lay = new QHBoxLayout(box);
        lay->setSpacing(8);
        for (int i = 0; i < n; ++i) {
            auto *lbl = new QLabel(QStringLiteral("—"), box);
            lbl->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px; color: #1565c0;"));
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setMinimumWidth(70);
            labels[i] = lbl;
            lay->addWidget(lbl);
        }
        return box;
    };

    auto *m1DataBox = new QGroupBox(QStringLiteral("Motor1 数据"), motorPage);
    auto *m1DataRow = new QHBoxLayout(m1DataBox);
    m1DataRow->setSpacing(8);
    const char *m1Names[] = {"轨迹输入", "反馈角度", "PID 输出", "Kp", "Ki"};
    for (int i = 0; i < 5; ++i) {
        auto *pair = new QVBoxLayout;
        auto *nameLbl = new QLabel(QString::fromUtf8(m1Names[i]), m1DataBox);
        nameLbl->setStyleSheet(QStringLiteral("font-size: 10px; color: #666;"));
        nameLbl->setAlignment(Qt::AlignCenter);
        pair->addWidget(nameLbl);
        m1DataLabels[i] = new QLabel(QStringLiteral("—"), m1DataBox);
        m1DataLabels[i]->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #1565c0;"));
        m1DataLabels[i]->setAlignment(Qt::AlignCenter);
        pair->addWidget(m1DataLabels[i]);
        m1DataRow->addLayout(pair);
    }
    dataRow->addWidget(m1DataBox, 1);

    auto *m2DataBox = new QGroupBox(QStringLiteral("Motor2 数据"), motorPage);
    auto *m2DataRow = new QHBoxLayout(m2DataBox);
    m2DataRow->setSpacing(8);
    for (int i = 0; i < 5; ++i) {
        auto *pair = new QVBoxLayout;
        const char *m2Names[] = {"位置", "速度", "力矩", "Kp", "Kd"};
        auto *nameLbl = new QLabel(QString::fromUtf8(m2Names[i]), m2DataBox);
        nameLbl->setStyleSheet(QStringLiteral("font-size: 10px; color: #666;"));
        nameLbl->setAlignment(Qt::AlignCenter);
        pair->addWidget(nameLbl);
        m2DataLabels[i] = new QLabel(QStringLiteral("—"), m2DataBox);
        m2DataLabels[i]->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #1565c0;"));
        m2DataLabels[i]->setAlignment(Qt::AlignCenter);
        pair->addWidget(m2DataLabels[i]);
        m2DataRow->addLayout(pair);
    }
    dataRow->addWidget(m2DataBox, 1);
    rootLayout->addLayout(dataRow);

    // ── 状态栏 ─────────────────────────────────────
    motorStatusLabel = new QLabel(QStringLiteral("请先在「协议调试」页面打开串口。"), motorPage);
    motorStatusLabel->setStyleSheet(QStringLiteral("color: #555; padding: 4px; border-top: 1px solid #ddd;"));
    rootLayout->addWidget(motorStatusLabel);

    // ═══════════════════════════════════════════════
    // 信号连接
    // ═══════════════════════════════════════════════

    // M2 机械零点
    connect(zeroBtn, &QPushButton::clicked, this, [this]() {
        if (!protoWorker->isOpen()) return;
        QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, IO_M2_SET_ZERO), Q_ARG(uint8_t, uint8_t(1)));
        motorStatusLabel->setText(QStringLiteral("M2 机械零点 — 已发送"));
    });

    // M2 清除故障
    connect(clrFaultBtn, &QPushButton::clicked, this, [this]() {
        if (!protoWorker->isOpen()) return;
        QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, IO_M2_CLR_FAULT), Q_ARG(uint8_t, uint8_t(1)));
        motorStatusLabel->setText(QStringLiteral("M2 清除故障 — 已发送"));
    });
}

// ═══════════════════════════════════════════════════════════
// Page 4: 康复治疗
// ═══════════════════════════════════════════════════════════

void MainWindow::createRehabPage()
{
    rehabPage = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(rehabPage);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    // 顶部
    auto *topLayout = new QHBoxLayout;
    auto *backBtn = new QPushButton(QStringLiteral("返回首页"), rehabPage);
    backBtn->setMinimumHeight(34);
    auto *title = new QLabel(QStringLiteral("康复治疗"), rehabPage);
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold;"));
    topLayout->addWidget(backBtn);
    topLayout->addWidget(title, 1);
    rootLayout->addLayout(topLayout);
    connect(backBtn, &QPushButton::clicked, this, &MainWindow::showHomePage);

    // 主布局
    auto *mainLayout = new QHBoxLayout;
    mainLayout->setSpacing(16);

    // ── 左侧: 治疗设置 ──────────────────────────────
    auto *leftPanel = new QWidget(rehabPage);
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(12);
    leftPanel->setFixedWidth(350);

    // 治疗模式
    auto *modeBox = new QGroupBox(QStringLiteral("治疗模式"), leftPanel);
    auto *modeLayout = new QVBoxLayout(modeBox);
    treatModeCombo = new QComboBox(modeBox);
    treatModeCombo->addItem(QStringLiteral("被动康复 — 设备带动关节活动"));
    treatModeCombo->addItem(QStringLiteral("主动辅助 — 患者主动+设备辅助"));
    treatModeCombo->addItem(QStringLiteral("抗阻训练 — 施加反向阻力"));
    treatModeCombo->addItem(QStringLiteral("自定义模式"));
    modeLayout->addWidget(treatModeCombo);
    leftLayout->addWidget(modeBox);

    // 参数设置
    auto *paramBox = new QGroupBox(QStringLiteral("治疗参数"), leftPanel);
    auto *paramLayout = new QGridLayout(paramBox);
    paramLayout->setSpacing(8);

    paramLayout->addWidget(new QLabel(QStringLiteral("目标角度 (°):"), paramBox), 0, 0);
    treatAngleEdit = new QLineEdit(paramBox);
    treatAngleEdit->setPlaceholderText(QStringLiteral("90.0"));
    treatAngleEdit->setText(QStringLiteral("90.0"));
    paramLayout->addWidget(treatAngleEdit, 0, 1);

    paramLayout->addWidget(new QLabel(QStringLiteral("运动速度 (°/s):"), paramBox), 1, 0);
    treatSpeedEdit = new QLineEdit(paramBox);
    treatSpeedEdit->setPlaceholderText(QStringLiteral("30.0"));
    treatSpeedEdit->setText(QStringLiteral("30.0"));
    paramLayout->addWidget(treatSpeedEdit, 1, 1);

    paramLayout->addWidget(new QLabel(QStringLiteral("循环次数:"), paramBox), 2, 0);
    treatCyclesSpin = new QSpinBox(paramBox);
    treatCyclesSpin->setRange(1, 9999);
    treatCyclesSpin->setValue(10);
    paramLayout->addWidget(treatCyclesSpin, 2, 1);
    leftLayout->addWidget(paramBox);

    // 控制按钮
    auto *ctrlBox = new QGroupBox(QStringLiteral("治疗控制"), leftPanel);
    auto *ctrlLayout = new QHBoxLayout(ctrlBox);
    treatStartBtn = new QPushButton(QStringLiteral("开始治疗"), ctrlBox);
    treatStartBtn->setStyleSheet(QStringLiteral("QPushButton { background-color: #4caf50; color: white; font-weight: bold; font-size: 14px; }"));
    treatPauseBtn = new QPushButton(QStringLiteral("暂停"), ctrlBox);
    treatPauseBtn->setStyleSheet(QStringLiteral("QPushButton { background-color: #ff9800; color: white; font-weight: bold; }"));
    treatStopBtn = new QPushButton(QStringLiteral("停止"), ctrlBox);
    treatStopBtn->setStyleSheet(QStringLiteral("QPushButton { background-color: #f44336; color: white; font-weight: bold; }"));
    treatStartBtn->setMinimumHeight(40);
    treatPauseBtn->setMinimumHeight(40);
    treatStopBtn->setMinimumHeight(40);
    ctrlLayout->addWidget(treatStartBtn);
    ctrlLayout->addWidget(treatPauseBtn);
    ctrlLayout->addWidget(treatStopBtn);
    leftLayout->addWidget(ctrlBox);

    leftLayout->addStretch();

    // ── 右侧: 实时状态 + 日志 ──────────────────────
    auto *rightPanel = new QWidget(rehabPage);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(12);

    // 实时状态
    auto *statusBox = new QGroupBox(QStringLiteral("实时状态"), rightPanel);
    auto *statusLayout = new QGridLayout(statusBox);
    statusLayout->setSpacing(8);

    auto makeBigLabel = [](QWidget *parent) -> QLabel* {
        auto *lbl = new QLabel(QStringLiteral("—"), parent);
        lbl->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: bold; color: #1565c0;"));
        lbl->setAlignment(Qt::AlignCenter);
        return lbl;
    };

    statusLayout->addWidget(new QLabel(QStringLiteral("运行时间:")), 0, 0);
    treatTimeLabel = new QLabel(QStringLiteral("00:00:00"), statusBox);
    treatTimeLabel->setStyleSheet(QStringLiteral("font-size: 28px; font-weight: bold; color: #2e7d32;"));
    treatTimeLabel->setAlignment(Qt::AlignCenter);
    statusLayout->addWidget(treatTimeLabel, 0, 1);

    statusLayout->addWidget(new QLabel(QStringLiteral("电机1 角度:")), 1, 0);
    treatM1AngleLabel = makeBigLabel(statusBox);
    statusLayout->addWidget(treatM1AngleLabel, 1, 1);

    statusLayout->addWidget(new QLabel(QStringLiteral("电机2 角度:")), 2, 0);
    treatM2AngleLabel = makeBigLabel(statusBox);
    statusLayout->addWidget(treatM2AngleLabel, 2, 1);

    statusLayout->addWidget(new QLabel(QStringLiteral("当前力矩:")), 3, 0);
    treatTorqueLabel = makeBigLabel(statusBox);
    statusLayout->addWidget(treatTorqueLabel, 3, 1);
    rightLayout->addWidget(statusBox);

    // 治疗日志
    auto *logBox = new QGroupBox(QStringLiteral("治疗日志"), rightPanel);
    auto *logLayout = new QVBoxLayout(logBox);
    treatLogEdit = new QTextEdit(rightPanel);
    treatLogEdit->setReadOnly(true);
    treatLogEdit->setPlaceholderText(QStringLiteral("治疗记录..."));
    treatLogEdit->setFont(QFont(QStringLiteral("Consolas"), 9));
    logLayout->addWidget(treatLogEdit, 1);
    auto *clearTreatLogBtn = new QPushButton(QStringLiteral("清空日志"), rightPanel);
    auto *logBtnRow = new QHBoxLayout;
    logBtnRow->addStretch();
    logBtnRow->addWidget(clearTreatLogBtn);
    logLayout->addLayout(logBtnRow);
    connect(clearTreatLogBtn, &QPushButton::clicked, treatLogEdit, &QTextEdit::clear);
    rightLayout->addWidget(logBox, 1);

    // 状态
    treatStatusLabel = new QLabel(QStringLiteral("就绪 — 请配置参数后开始治疗。"), rehabPage);
    treatStatusLabel->setStyleSheet(QStringLiteral("color: #555; padding: 4px; border-top: 1px solid #ddd;"));
    rightLayout->addWidget(treatStatusLabel);

    mainLayout->addWidget(leftPanel);
    mainLayout->addWidget(rightPanel, 1);
    rootLayout->addLayout(mainLayout, 1);

    // ── 治疗控制逻辑 ────────────────────────────────
    connect(treatStartBtn, &QPushButton::clicked, this, [this]() {
        if (!protoWorker->isOpen()) {
            treatStatusLabel->setText(QStringLiteral("请先在协议调试页面打开串口。"));
            return;
        }
        treatRunning = true;
        if (treatElapsedSec == 0) treatTimer->start();
        treatStatusLabel->setText(QStringLiteral("治疗进行中..."));
        appendTreatLog(QStringLiteral("治疗开始 — 模式: %1").arg(treatModeCombo->currentText()));
        // 发送启动命令
        QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, IO_M1_START_STOP), Q_ARG(uint8_t, IO_VAL_START));
        // 设置目标角度
        bool ok; float angle = treatAngleEdit->text().toFloat(&ok);
        if (ok) {
            QMetaObject::invokeMethod(protoWorker, "sendFloatWrite", Qt::QueuedConnection,
                Q_ARG(uint8_t, uint8_t(0x01)), Q_ARG(float, angle));
        }
    });
    connect(treatPauseBtn, &QPushButton::clicked, this, [this]() {
        treatRunning = false;
        treatTimer->stop();
        treatStatusLabel->setText(QStringLiteral("治疗已暂停。"));
        appendTreatLog(QStringLiteral("治疗暂停"));
    });
    connect(treatStopBtn, &QPushButton::clicked, this, [this]() {
        treatRunning = false;
        treatTimer->stop();
        treatElapsedSec = 0;
        treatTimeLabel->setText(QStringLiteral("00:00:00"));
        treatStatusLabel->setText(QStringLiteral("治疗已停止。"));
        appendTreatLog(QStringLiteral("治疗停止"));
        QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, IO_M1_START_STOP), Q_ARG(uint8_t, IO_VAL_STOP));
    });
}

void MainWindow::appendTreatLog(const QString &text)
{
    if (!treatLogEdit) return;
    QString ts = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
    treatLogEdit->append(QStringLiteral("[%1] %2").arg(ts, text));
}

void MainWindow::updateMotorDisplay(const QMap<uint8_t, float> &values)
{
    auto fmtVal = [](float v) { return QString::number(v, 'f', 1); };

    // M1 数据标签
    if (m1DataLabels[0] && values.contains(0x01)) m1DataLabels[0]->setText(fmtVal(values[0x01]));
    if (m1DataLabels[1] && values.contains(0x02)) m1DataLabels[1]->setText(fmtVal(values[0x02]));
    if (m1DataLabels[2] && values.contains(0x03)) m1DataLabels[2]->setText(fmtVal(values[0x03]));
    if (m1DataLabels[3] && values.contains(0x04)) m1DataLabels[3]->setText(fmtVal(values[0x04]));
    if (m1DataLabels[4] && values.contains(0x05)) m1DataLabels[4]->setText(fmtVal(values[0x05]));
    // M2 数据标签
    if (m2DataLabels[0] && values.contains(0x11)) m2DataLabels[0]->setText(fmtVal(values[0x11]));
    if (m2DataLabels[1] && values.contains(0x12)) m2DataLabels[1]->setText(fmtVal(values[0x12]));
    if (m2DataLabels[2] && values.contains(0x13)) m2DataLabels[2]->setText(fmtVal(values[0x13]));
    if (m2DataLabels[3] && values.contains(0x14)) m2DataLabels[3]->setText(fmtVal(values[0x14]));
    if (m2DataLabels[4] && values.contains(0x15)) m2DataLabels[4]->setText(fmtVal(values[0x15]));

    // Rehab page
    auto setLabel = [](QLabel *lbl, float val, const QString &unit) {
        if (!lbl) return;
        lbl->setText(QStringLiteral("%1 %2").arg(val, 0, 'f', 1).arg(unit));
    };
    if (values.contains(0x01)) setLabel(treatM1AngleLabel, values[0x01], QStringLiteral("°"));
    if (values.contains(0x11)) setLabel(treatM2AngleLabel, values[0x11], QStringLiteral("°"));
    if (values.contains(0x13)) setLabel(treatTorqueLabel, values[0x13], QStringLiteral(""));
}

// ═══════════════════════════════════════════════════════════
// 电机图表
// ═══════════════════════════════════════════════════════════

void MainWindow::buildMotorChart(MotorChartCtx &ctx, QWidget *parent,
                                  const uint8_t *ids, int count,
                                  const QColor *colors)
{
    ctx.chart = new QChart();
    ctx.chart->setAnimationOptions(QChart::NoAnimation);
    ctx.chart->legend()->setVisible(false);
    ctx.chart->setMargins(QMargins(0, 0, 0, 0));
    ctx.chart->setBackgroundRoundness(0);

    ctx.axisX = new QValueAxis();
    ctx.axisX->setRange(0, 200);
    ctx.axisX->setLabelsVisible(false);
    ctx.chart->addAxis(ctx.axisX, Qt::AlignBottom);

    ctx.axisY = new QValueAxis();
    ctx.axisY->setRange(-5, 5);
    ctx.axisY->setLabelFormat(QStringLiteral("%.0f"));
    ctx.chart->addAxis(ctx.axisY, Qt::AlignLeft);

    for (int i = 0; i < count; ++i) {
        auto *s = new QLineSeries();
        s->setColor(colors[i]);
        s->setPen(QPen(colors[i], 1.5));
        s->setVisible(false);
        ctx.chart->addSeries(s);
        s->attachAxis(ctx.axisX);
        s->attachAxis(ctx.axisY);
        ctx.series.insert(ids[i], s);
        ctx.buffers.insert(ids[i], {});
    }

    ctx.view = new QChartView(ctx.chart, parent);
    ctx.view->setRenderHint(QPainter::Antialiasing);
    ctx.view->setMinimumHeight(160);
    ctx.view->setMaximumHeight(200);

    // 复选框行
    auto *cbRow = new QHBoxLayout;
    cbRow->setSpacing(2);
    for (int i = 0; i < count; ++i) {
        auto *cb = new QCheckBox(
            QString::fromUtf8(floatIDTable[ids[i] > 0x10 ? ids[i] - 0x0C : ids[i] - 1].name),
            parent);
        cb->setStyleSheet(QStringLiteral("QCheckBox { font-size: 10px; color: %1; }").arg(colors[i].name()));
        cb->setChecked(false);
        uint8_t fid = ids[i];
        connect(cb, &QCheckBox::toggled, parent, [this, &ctx, fid](bool on) {
            if (ctx.series.contains(fid)) ctx.series[fid]->setVisible(on);
        });
        ctx.checkboxes.insert(fid, cb);
        cbRow->addWidget(cb);
    }
    cbRow->addStretch();

    auto *chartArea = qobject_cast<QVBoxLayout*>(parent->layout());
    if (chartArea) {
        chartArea->addWidget(ctx.view);
        chartArea->addLayout(cbRow);
    }
}

void MainWindow::feedMotorChart(MotorChartCtx &ctx, const QMap<uint8_t, float> &values)
{
    bool updated = false;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (!ctx.series.contains(it.key())) continue;
        auto *s = ctx.series[it.key()];
        auto &buf = ctx.buffers[it.key()];
        QPointF pt(ctx.time, static_cast<qreal>(it.value()));
        buf.append(pt);
        if (buf.size() > 200) buf.removeFirst();
        s->clear();
        for (const auto &p : buf) s->append(p);
        updated = true;
    }
    if (!updated) return;

    ctx.time += 1.0;
    qreal xMin = qMax(0.0, ctx.time - 200);
    ctx.axisX->setRange(xMin, xMin + 200);
    // 自适应 Y 轴
    qreal yMin = 1e9, yMax = -1e9;
    bool any = false;
    for (auto it = ctx.series.constBegin(); it != ctx.series.constEnd(); ++it) {
        if (!it.value()->isVisible()) continue;
        for (const auto &p : ctx.buffers[it.key()]) {
            yMin = qMin(yMin, p.y()); yMax = qMax(yMax, p.y()); any = true;
        }
    }
    if (any) {
        qreal m = qMax(1.0, (yMax - yMin) * 0.15);
        ctx.axisY->setRange(yMin - m, yMax + m);
    }
}

// ═══════════════════════════════════════════════════════════
// 辅助函数
// ═══════════════════════════════════════════════════════════

int MainWindow::baudRateFromComboBox() const
{
    return baudComboBox->currentData().toInt();
}

QSerialPort::DataBits MainWindow::dataBitsFromComboBox() const
{
    return static_cast<QSerialPort::DataBits>(dataBitsComboBox->currentData().toInt());
}

QSerialPort::Parity MainWindow::parityFromComboBox() const
{
    return static_cast<QSerialPort::Parity>(parityComboBox->currentData().toInt());
}

QSerialPort::StopBits MainWindow::stopBitsFromComboBox() const
{
    return static_cast<QSerialPort::StopBits>(stopBitsComboBox->currentData().toInt());
}

QSerialPort::FlowControl MainWindow::flowControlFromComboBox() const
{
    return static_cast<QSerialPort::FlowControl>(flowControlComboBox->currentData().toInt());
}

void MainWindow::appendReceiveData(const QByteArray &data)
{
    if (data.isEmpty()) {
        return;
    }

    QString text = hexReceiveCheckBox->isChecked()
        ? QString::fromLatin1(data.toHex(' ').toUpper())
        : QString::fromUtf8(data);
    receiveEdit->append(text);
}

void MainWindow::appendTransmitData(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }
    transmitEdit->append(text);
}

void MainWindow::appendProtocolLog(const QString &text)
{
    if (protocolLogEdit) {
        QString ts = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));
        protocolLogEdit->append(QStringLiteral("[%1] %2").arg(ts, text));
    }
}
