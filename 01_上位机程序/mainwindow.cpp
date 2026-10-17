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
#include <QTextStream>
#include <QScrollArea>
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
    , m_recording(false)
    , m_recordFile(new QFile(this))
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
        // 康复页的两张图已改为「轨迹跟踪视图」（本地参考轨迹 + 时间游标），
        // 不再走这条 50 ms 批量合并（同 ID 取最后一帧）的通道。
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

    // ── 数据记录 — 缓冲 MCU 实际上传的数据 ──────────
    if (m_recording) {
        RecordFrame frame;
        frame.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));
        frame.values = values;
        m_recordBuffer.append(frame);
    }

    // ── 推入队列 (由定时器批量刷新图表) ────────────
    {
        QMutexLocker lock(&mTelemetryMutex);
        mTelemetryQueue.append(values);
    }
    // ── 更新康复页面标签 (轻量操作) ────────────────
    updateMotorDisplay(values);

    // ── 参考轨迹跟踪视图：记录带时间戳的原始样本 ────
    // 必须单独一路，不能复用上面的 50 ms 合并结果（否则点会画在错位的时间上）
    feedTrackerSample(values, QDateTime::currentMSecsSinceEpoch());

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
    // 重置 motor 曲线（康复页的轨迹跟踪视图由实验状态机自管，不在此重置）
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

    // 系统启停 (TIM2 控制循环)
    auto *sysStartBtn = new QPushButton(QStringLiteral("系统启动"), motorPage);
    auto *sysStopBtn  = new QPushButton(QStringLiteral("系统停止"), motorPage);
    sysStartBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #4caf50; color: white; font-weight: bold; font-size: 11px; padding: 2px 8px; }"));
    sysStopBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #f44336; color: white; font-weight: bold; font-size: 11px; padding: 2px 8px; }"));
    connect(sysStartBtn, &QPushButton::clicked, this, [this]() {
        if (!protoWorker->isOpen()) return;
        QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, IO_SYS_START_STOP), Q_ARG(uint8_t, uint8_t(1)));
        motorStatusLabel->setText(QStringLiteral("系统 TIM2 控制循环 → 启动"));
    });
    connect(sysStopBtn, &QPushButton::clicked, this, [this]() {
        if (!protoWorker->isOpen()) return;
        QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, IO_SYS_START_STOP), Q_ARG(uint8_t, uint8_t(2)));
        motorStatusLabel->setText(QStringLiteral("系统 TIM2 控制循环 → 停止"));
    });
    topLayout->addWidget(sysStartBtn);
    topLayout->addWidget(sysStopBtn);

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
    // M1 控制模式 (选择即下发)
    auto *m1ModeRow = new QHBoxLayout;
    m1ModeRow->addWidget(new QLabel(QStringLiteral("模式:"), m1Box));
    motorModeCombo = new QComboBox(m1Box);
    motorModeCombo->addItem(QStringLiteral("位置"), M1_MODE_POS);
    motorModeCombo->addItem(QStringLiteral("速度"), M1_MODE_SPEED);
    motorModeCombo->addItem(QStringLiteral("PWM 测试"), M1_MODE_PWM);
    motorModeCombo->setCurrentIndex(0);
    m1ModeRow->addWidget(motorModeCombo, 1);
    m1Layout->addLayout(m1ModeRow);
    connect(motorModeCombo, &QComboBox::currentIndexChanged, this, [this](int /*idx*/) {
        if (!protoWorker->isOpen()) return;
        uint8_t m = static_cast<uint8_t>(motorModeCombo->currentData().toUInt());
        QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, IO_M1_CTRL_MODE), Q_ARG(uint8_t, m));
        motorStatusLabel->setText(QStringLiteral("M1 模式 → %1").arg(motorModeCombo->currentText()));
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
        motorStatusLabel->setText(QStringLiteral("M2 RS00 → %1").arg(m2ModeCombo->currentText()));
    });

    // M2 轨迹模式 (选择即下发)
    auto *m2TrajRow = new QHBoxLayout;
    m2TrajRow->addWidget(new QLabel(QStringLiteral("轨迹:"), m2Box));
    auto *m2TrajCombo = new QComboBox(m2Box);
    m2TrajCombo->addItem(QStringLiteral("位置"), M2_TRAJ_POS);
    m2TrajCombo->addItem(QStringLiteral("速度"), M2_TRAJ_SPEED);
    m2TrajCombo->setCurrentIndex(0);
    m2TrajRow->addWidget(m2TrajCombo, 1);
    m2Layout->addLayout(m2TrajRow);
    connect(m2TrajCombo, &QComboBox::currentIndexChanged, this, [this, m2TrajCombo]() {
        if (!protoWorker->isOpen()) return;
        uint8_t m = static_cast<uint8_t>(m2TrajCombo->currentData().toUInt());
        QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, IO_M2_TRAJ_MODE), Q_ARG(uint8_t, m));
        motorStatusLabel->setText(QStringLiteral("M2 轨迹 → %1").arg(m2TrajCombo->currentText()));
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
    m2Layout->addLayout(makeFloatRow(QStringLiteral("位置(°):"), QStringLiteral("-720~+720"), 0x11, m2Box));
    m2Layout->addLayout(makeFloatRow(QStringLiteral("速度(°/s):"), QStringLiteral("-1890~+1890"), 0x12, m2Box));
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
        const char *m2Names[] = {"规划位置", "实际位置", "规划速度", "实际速度", "力矩"};
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

    // ── DR304 六维力传感器 ──────────────────────────
    auto *forceBox = new QGroupBox(QStringLiteral("六维力传感器 (DR304)"), motorPage);
    auto *forceRow = new QHBoxLayout(forceBox);
    forceRow->setSpacing(12);
    const uint8_t forceIDs[] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46};
    const char *forceNames[] = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};
    const char *forceUnits[] = {"N", "N", "N", "N·m", "N·m", "N·m"};
    for (int i = 0; i < 6; ++i) {
        auto *pair = new QVBoxLayout;
        auto *nameLbl = new QLabel(QString::fromUtf8(forceNames[i]), forceBox);
        nameLbl->setStyleSheet(QStringLiteral("font-size: 10px; color: #666;"));
        nameLbl->setAlignment(Qt::AlignCenter);
        pair->addWidget(nameLbl);
        forceLabels[i] = new QLabel(QStringLiteral("—"), forceBox);
        forceLabels[i]->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #2e7d32;"));
        forceLabels[i]->setAlignment(Qt::AlignCenter);
        pair->addWidget(forceLabels[i]);
        forceRow->addLayout(pair);
    }
    forceRow->addStretch();
    rootLayout->addWidget(forceBox);

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

    // 康复页内容较多（双栏康复面板 + 两张轨迹跟踪图 + 实时状态 + 六维力 + 治疗日志），
    // 在默认窗口尺寸下总高度会超过窗口。套一层 QScrollArea：
    // 窗口够大时与原来一样铺满，窗口不够时出现滚动条，不会把底部内容切掉。
    auto *pageLayout = new QVBoxLayout(rehabPage);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    auto *scrollArea = new QScrollArea(rehabPage);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto *content = new QWidget();
    scrollArea->setWidget(content);
    pageLayout->addWidget(scrollArea);

    auto *rootLayout = new QVBoxLayout(content);
    rootLayout->setContentsMargins(12, 8, 12, 8);
    rootLayout->setSpacing(6);

    // 顶部
    auto *topLayout = new QHBoxLayout;
    auto *backBtn = new QPushButton(QStringLiteral("返回首页"), rehabPage);
    auto *title = new QLabel(QStringLiteral("康复治疗"), rehabPage);
    title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));
    topLayout->addWidget(backBtn);
    topLayout->addWidget(title, 1);
    rootLayout->addLayout(topLayout);
    connect(backBtn, &QPushButton::clicked, this, &MainWindow::showHomePage);

    // 双栏: M1康复 | M2康复
    auto *dualLayout = new QHBoxLayout;
    dualLayout->setSpacing(12);

    // ── 发送 helper ──────────────────────────────────
    auto sendRehabFloat = [this](uint8_t floatId, float val, const QString &label) {
        if (!protoWorker->isOpen()) return;
        QMetaObject::invokeMethod(protoWorker, "sendFloatWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, floatId), Q_ARG(float, val));
        treatStatusLabel->setText(QStringLiteral("已发送: %1 = %2").arg(label).arg(val, 0, 'f', 1));
        appendTreatLog(QStringLiteral("设置 %1 = %2").arg(label).arg(val, 0, 'f', 1));
    };

    // ── 创建康复面板 ─────────────────────────────────
    auto makeRehabPanel = [&](const QString &title, QWidget *parent,
                               uint8_t minId, uint8_t maxId, uint8_t cyclesId,
                               uint8_t startIO, uint8_t stopIO) -> QGroupBox*
    {
        auto *box = new QGroupBox(title, parent);
        auto *lay = new QVBoxLayout(box);
        lay->setSpacing(6);
        lay->setContentsMargins(8, 8, 8, 8);

        // 参数: 最小角度
        auto *minRow = new QHBoxLayout;
        minRow->addWidget(new QLabel(QStringLiteral("最小(°):"), box));
        auto *minEdit = new QLineEdit(box);
        minEdit->setText(QStringLiteral("0"));
        minEdit->setMaximumHeight(24);
        minRow->addWidget(minEdit, 1);
        lay->addLayout(minRow);
        connect(minEdit, &QLineEdit::returnPressed, this, [=]() {
            bool ok; float v = minEdit->text().toFloat(&ok);
            if (ok) sendRehabFloat(minId, v, title + QStringLiteral(" 最小角度"));
        });

        // 参数: 最大角度
        auto *maxRow = new QHBoxLayout;
        maxRow->addWidget(new QLabel(QStringLiteral("最大(°):"), box));
        auto *maxEdit = new QLineEdit(box);
        maxEdit->setText(QStringLiteral("30"));
        maxEdit->setMaximumHeight(24);
        maxRow->addWidget(maxEdit, 1);
        lay->addLayout(maxRow);
        connect(maxEdit, &QLineEdit::returnPressed, this, [=]() {
            bool ok; float v = maxEdit->text().toFloat(&ok);
            if (ok) sendRehabFloat(maxId, v, title + QStringLiteral(" 最大角度"));
        });

        // 参数: 循环次数
        auto *cyclesRow = new QHBoxLayout;
        cyclesRow->addWidget(new QLabel(QStringLiteral("循环:"), box));
        auto *cyclesSpin = new QSpinBox(box);
        cyclesSpin->setRange(1, 9999);
        cyclesSpin->setValue(5);
        cyclesSpin->setMaximumHeight(24);
        cyclesRow->addWidget(cyclesSpin, 1);
        lay->addLayout(cyclesRow);
        connect(cyclesSpin, &QSpinBox::valueChanged, this, [=](int v) {
            sendRehabFloat(cyclesId, static_cast<float>(v), title + QStringLiteral(" 循环次数"));
        });

        // 控制按钮
        auto *btnRow = new QHBoxLayout;
        auto *startBtn = new QPushButton(QStringLiteral("▶ 启动"), box);
        auto *stopBtn  = new QPushButton(QStringLiteral("■ 停止"), box);
        startBtn->setStyleSheet(QStringLiteral("QPushButton { background-color: #4caf50; color: white; font-weight: bold; }"));
        stopBtn->setStyleSheet(QStringLiteral("QPushButton { background-color: #f44336; color: white; font-weight: bold; }"));
        btnRow->addWidget(startBtn);
        btnRow->addWidget(stopBtn);
        lay->addLayout(btnRow);
        // 末尾加 stretch：窗口变大时让空白落在底部，参数行保持紧凑，
        // 否则每行（含 QLabel）会被拉伸成几十像素的间距，把下方区域挤出去。
        lay->addStretch();

        connect(startBtn, &QPushButton::clicked, this, [=]() {
            if (!protoWorker->isOpen()) return;
            QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
                Q_ARG(uint8_t, startIO), Q_ARG(uint8_t, uint8_t(1)));
            treatStatusLabel->setText(title + QStringLiteral(" 康复启动"));
            appendTreatLog(title + QStringLiteral(" 康复启动"));
        });
        connect(stopBtn, &QPushButton::clicked, this, [=]() {
            if (!protoWorker->isOpen()) return;
            QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
                Q_ARG(uint8_t, stopIO), Q_ARG(uint8_t, uint8_t(2)));
            treatStatusLabel->setText(title + QStringLiteral(" 康复停止"));
            appendTreatLog(title + QStringLiteral(" 康复停止"));
        });

        return box;
    };

    // M1 康复面板 (被动 + 主动)
    auto *m1RehabTabs = new QTabWidget(rehabPage);
    auto *m1PassiveTab = new QWidget();
    auto *m1PassiveLayout = new QVBoxLayout(m1PassiveTab);
    m1PassiveLayout->setContentsMargins(0, 0, 0, 0);
    auto *m1PassiveBox = makeRehabPanel(QStringLiteral("Motor1 被动康复"), m1PassiveTab,
        0x08, 0x09, 0x0A, IO_M1_REHAB, IO_M1_REHAB);
    m1PassiveLayout->addWidget(m1PassiveBox);
    m1RehabTabs->addTab(m1PassiveTab, QStringLiteral("被动"));

    // M1 主动康复 tab
    auto *m1ActiveTab = new QWidget();
    auto *m1ActiveLayout = new QVBoxLayout(m1ActiveTab);
    m1ActiveLayout->setContentsMargins(0, 0, 0, 0);

    auto *m1ActiveBox = new QGroupBox(QStringLiteral("Motor1 主动康复"), m1ActiveTab);
    auto *m1ActLay = new QVBoxLayout(m1ActiveBox);
    m1ActLay->setSpacing(4);
    m1ActLay->setContentsMargins(8, 8, 8, 8);

    // 参数输入行
    struct ActParam { const char *label; uint8_t floatId; const char *placeholder; };
    const ActParam actParams[] = {
        {"T波速度(°/s):", 0x0B, "30"},
        {"极限角1(°):",   0x0C, "-30"},
        {"极限角2(°):",   0x0D, "30"},
        {"停顿时间(s):",  0x0E, "2"},
        {"Tmax(N·m):",    0x0F, "5"},
        {"刚度k:",        0x10, "10"},
    };
    for (int pi = 0; pi < 6; ++pi) {
        const ActParam &p = actParams[pi];
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel(QString::fromUtf8(p.label), m1ActiveBox));
        auto *edit = new QLineEdit(m1ActiveBox);
        edit->setText(QString::fromUtf8(p.placeholder));
        edit->setMaximumHeight(24);
        row->addWidget(edit, 1);
        m1ActLay->addLayout(row);
        // 供轨迹跟踪视图读取参考轨迹参数（只读，实验开始不会把这些值下发）
        m1ActEdits[pi] = edit;
        connect(edit, &QLineEdit::returnPressed, this, [this, edit, fid = p.floatId, label = p.label]() {
            if (!protoWorker->isOpen()) return;
            bool ok; float v = edit->text().toFloat(&ok);
            if (!ok) { treatStatusLabel->setText(QStringLiteral("请输入有效数值")); return; }
            QMetaObject::invokeMethod(protoWorker, "sendFloatWrite", Qt::QueuedConnection,
                Q_ARG(uint8_t, fid), Q_ARG(float, v));
            treatStatusLabel->setText(QStringLiteral("已发送: %1 = %2").arg(QString::fromUtf8(label)).arg(v));
            appendTreatLog(QStringLiteral("M1 主动 %1 = %2").arg(QString::fromUtf8(label)).arg(v));
        });
    }

    // 启停按钮
    auto *actBtnRow = new QHBoxLayout;
    auto *actStartBtn = new QPushButton(QStringLiteral("▶ 启动"), m1ActiveBox);
    auto *actStopBtn  = new QPushButton(QStringLiteral("■ 停止"), m1ActiveBox);
    actStartBtn->setStyleSheet(QStringLiteral("QPushButton { background-color: #4caf50; color: white; font-weight: bold; }"));
    actStopBtn->setStyleSheet(QStringLiteral("QPushButton { background-color: #f44336; color: white; font-weight: bold; }"));
    actBtnRow->addWidget(actStartBtn);
    actBtnRow->addWidget(actStopBtn);
    m1ActLay->addLayout(actBtnRow);
    m1ActLay->addStretch();   // 同上：参数行保持紧凑

    connect(actStartBtn, &QPushButton::clicked, this, [this]() {
        if (!protoWorker->isOpen()) return;
        QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, IO_M1_ACTIVE_REHAB), Q_ARG(uint8_t, uint8_t(1)));
        treatStatusLabel->setText(QStringLiteral("M1 主动康复 → 启动"));
        appendTreatLog(QStringLiteral("M1 主动康复启动"));
    });
    connect(actStopBtn, &QPushButton::clicked, this, [this]() {
        if (!protoWorker->isOpen()) return;
        QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, IO_M1_ACTIVE_REHAB), Q_ARG(uint8_t, uint8_t(2)));
        treatStatusLabel->setText(QStringLiteral("M1 主动康复 → 停止"));
        appendTreatLog(QStringLiteral("M1 主动康复停止"));
    });

    m1ActiveLayout->addWidget(m1ActiveBox);
    m1ActiveLayout->addStretch();
    m1RehabTabs->addTab(m1ActiveTab, QStringLiteral("主动"));
    // 参数面板不需要占满整页：给一个高度上限，
    // 否则窗口放大时它会被拉伸得很高，把下方的轨迹跟踪图挤出可视区。
    m1RehabTabs->setMaximumHeight(300);
    dualLayout->addWidget(m1RehabTabs, 1);

    // M2 康复面板 (被动 + 主动)
    auto *m2RehabTabs = new QTabWidget(rehabPage);
    auto *m2PassiveTab = new QWidget();
    auto *m2PassiveLayout = new QVBoxLayout(m2PassiveTab);
    m2PassiveLayout->setContentsMargins(0, 0, 0, 0);
    auto *m2PassiveBox = makeRehabPanel(QStringLiteral("Motor2 被动康复"), m2PassiveTab,
        0x1A, 0x1B, 0x1C, IO_M2_REHAB, IO_M2_REHAB);
    m2PassiveLayout->addWidget(m2PassiveBox);
    m2RehabTabs->addTab(m2PassiveTab, QStringLiteral("被动"));

    // M2 主动康复 tab
    auto *m2ActiveTab = new QWidget();
    auto *m2ActiveLayout = new QVBoxLayout(m2ActiveTab);
    m2ActiveLayout->setContentsMargins(0, 0, 0, 0);
    auto *m2ActiveBox = new QGroupBox(QStringLiteral("Motor2 主动康复"), m2ActiveTab);
    auto *m2ActLay = new QVBoxLayout(m2ActiveBox);
    m2ActLay->setSpacing(4);
    m2ActLay->setContentsMargins(8, 8, 8, 8);

    // M2 主动模式选择
    auto *m2ActModeRow = new QHBoxLayout;
    m2ActModeRow->addWidget(new QLabel(QStringLiteral("模式:"), m2ActiveBox));
    auto *m2ActModeCombo = new QComboBox(m2ActiveBox);
    m2ActModeCombo->addItem(QStringLiteral("掌屈/背屈 (Fy)"), 1);
    m2ActModeCombo->addItem(QStringLiteral("尺偏/桡偏 (Fz)"), 2);
    m2ActModeRow->addWidget(m2ActModeCombo, 1);
    m2ActLay->addLayout(m2ActModeRow);
    connect(m2ActModeCombo, &QComboBox::currentIndexChanged, this, [this, m2ActModeCombo]() {
        if (!protoWorker->isOpen()) return;
        uint8_t m = static_cast<uint8_t>(m2ActModeCombo->currentData().toUInt());
        QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, IO_M2_ACTIVE_MODE), Q_ARG(uint8_t, m));
        treatStatusLabel->setText(QStringLiteral("M2 主动模式: %1").arg(m2ActModeCombo->currentText()));
    });

    const struct { const char *label; uint8_t fid; const char *defVal; } m2ActParams[] = {
        {"T波速度:",  0x1D, "30"},
        {"极限角1:",  0x1E, "-30"},
        {"极限角2:",  0x1F, "30"},
        {"停顿时间:", 0x20, "2"},
        {"Fmax(N):",  0x23, "5"},
        {"刚度k:",    0x24, "10"},
    };
    for (int pi = 0; pi < 6; ++pi) {
        const auto &p = m2ActParams[pi];
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel(QString::fromUtf8(p.label), m2ActiveBox));
        auto *edit = new QLineEdit(m2ActiveBox);
        edit->setText(QString::fromUtf8(p.defVal));
        edit->setMaximumHeight(24);
        row->addWidget(edit, 1);
        m2ActLay->addLayout(row);
        // 供轨迹跟踪视图读取参考轨迹参数（只读，实验开始不会把这些值下发）
        m2ActEdits[pi] = edit;
        connect(edit, &QLineEdit::returnPressed, this, [this, edit, fid = p.fid, label = p.label]() {
            if (!protoWorker->isOpen()) return;
            bool ok; float v = edit->text().toFloat(&ok);
            if (!ok) return;
            QMetaObject::invokeMethod(protoWorker, "sendFloatWrite", Qt::QueuedConnection,
                Q_ARG(uint8_t, fid), Q_ARG(float, v));
            treatStatusLabel->setText(QStringLiteral("已发送: M2 %1 = %2").arg(QString::fromUtf8(label)).arg(v));
            appendTreatLog(QStringLiteral("M2 主动 %1 = %2").arg(QString::fromUtf8(label)).arg(v));
        });
    }

    auto *m2ActBtnRow = new QHBoxLayout;
    auto *m2ActStart = new QPushButton(QStringLiteral("▶ 启动"), m2ActiveBox);
    auto *m2ActStop  = new QPushButton(QStringLiteral("■ 停止"), m2ActiveBox);
    m2ActStart->setStyleSheet(QStringLiteral("QPushButton { background-color: #4caf50; color: white; font-weight: bold; }"));
    m2ActStop->setStyleSheet(QStringLiteral("QPushButton { background-color: #f44336; color: white; font-weight: bold; }"));
    m2ActBtnRow->addWidget(m2ActStart);
    m2ActBtnRow->addWidget(m2ActStop);
    m2ActLay->addLayout(m2ActBtnRow);
    m2ActLay->addStretch();   // 同上：参数行保持紧凑
    connect(m2ActStart, &QPushButton::clicked, this, [this]() {
        if (!protoWorker->isOpen()) return;
        QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, IO_M2_ACTIVE_REHAB), Q_ARG(uint8_t, uint8_t(1)));
        treatStatusLabel->setText(QStringLiteral("M2 主动康复 → 启动"));
        appendTreatLog(QStringLiteral("M2 主动康复启动"));
    });
    connect(m2ActStop, &QPushButton::clicked, this, [this]() {
        if (!protoWorker->isOpen()) return;
        QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, IO_M2_ACTIVE_REHAB), Q_ARG(uint8_t, uint8_t(2)));
        treatStatusLabel->setText(QStringLiteral("M2 主动康复 → 停止"));
        appendTreatLog(QStringLiteral("M2 主动康复停止"));
    });

    m2ActiveLayout->addWidget(m2ActiveBox);
    m2ActiveLayout->addStretch();
    m2RehabTabs->addTab(m2ActiveTab, QStringLiteral("主动"));
    m2RehabTabs->setMaximumHeight(300);   // 同上：参数面板封顶
    dualLayout->addWidget(m2RehabTabs, 1);

    // 双栅参数面板不需要弹性空间（它自己有 addStretch），多余的纵向空间全部给下面的轨迹图。
    rootLayout->addLayout(dualLayout, 0);

    // ── 轨迹跟踪实验（Motor1 + Motor2 共用一套机制）──────
    // 参考轨迹由上位机本地生成；本区域不下发任何控制指令。
    auto *trackerBox = new QGroupBox(
        QStringLiteral("轨迹跟踪实验（参考轨迹由上位机本地生成，不下发任何指令）"), rehabPage);
    auto *trackerLay = new QVBoxLayout(trackerBox);
    trackerLay->setContentsMargins(6, 4, 6, 4);
    trackerLay->setSpacing(4);

    auto *trackerCtrlRow = new QHBoxLayout;
    auto *trackerHint = new QLabel(
        QStringLiteral("参考轨迹参数取自各电机「主动」选项卡：T波速度 / 极限角1 / 极限角2 / 停顿时间"),
        trackerBox);
    trackerHint->setStyleSheet(QStringLiteral("QLabel { font-size: 10px; color: #777; }"));
    trackerCtrlRow->addWidget(trackerHint, 1);
    trackerCtrlRow->addWidget(new QLabel(QStringLiteral("周期数:"), trackerBox));
    trackerCyclesSpin = new QSpinBox(trackerBox);
    trackerCyclesSpin->setRange(1, 20);
    trackerCyclesSpin->setValue(5);
    trackerCyclesSpin->setMaximumHeight(24);
    trackerCyclesSpin->setFixedWidth(64);
    trackerCtrlRow->addWidget(trackerCyclesSpin);
    trackerStartBtn = new QPushButton(QStringLiteral("▶ 开始实验"), trackerBox);
    trackerStartBtn->setMinimumHeight(26);
    trackerStartBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #4caf50; color: white; font-weight: bold; padding: 0px 14px; }"));
    trackerCtrlRow->addWidget(trackerStartBtn);
    auto *trackerPreviewBtn = new QPushButton(QStringLiteral("刷新预览"), trackerBox);
    trackerPreviewBtn->setMinimumHeight(26);
    trackerCtrlRow->addWidget(trackerPreviewBtn);
    trackerStateLabel = new QLabel(QStringLiteral("未开始"), trackerBox);
    trackerStateLabel->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; color: #1565c0; }"));
    trackerCtrlRow->addWidget(trackerStateLabel);
    trackerLay->addLayout(trackerCtrlRow);

    auto *trackerChartRow = new QHBoxLayout;
    trackerChartRow->setSpacing(8);

    // 单张轨迹跟踪图（Motor1 / Motor2 各一张，同一套机制）
    auto makeTrackerColumn = [&](TrackerCtx &ctx, const QString &title,
                                 uint8_t tId, uint8_t aId, uint8_t rId,
                                 const QString &rawLabel) -> QGroupBox* {
        auto *box = new QGroupBox(title, trackerBox);
        auto *lay = new QVBoxLayout(box);
        lay->setContentsMargins(2, 2, 2, 2);
        lay->setSpacing(2);

        ctx.name = title;
        ctx.targetId = tId;
        ctx.actualId = aId;
        ctx.rawId = rId;
        buildTrackerChart(ctx, box);
        lay->addWidget(ctx.view);

        auto *cbRow = new QHBoxLayout;
        cbRow->setSpacing(8);
        ctx.refCheck = new QCheckBox(QStringLiteral("显示参考轨迹"), box);
        ctx.refCheck->setChecked(true);
        ctx.refCheck->setStyleSheet(QStringLiteral("QCheckBox { font-size: 10px; color: #2ca02c; }"));
        cbRow->addWidget(ctx.refCheck);
        ctx.histCheck = new QCheckBox(QStringLiteral("显示历史轨迹"), box);
        ctx.histCheck->setChecked(true);   // 扫描式实时绘图：默认开启，随游标从左向右生长
        ctx.histCheck->setToolTip(QStringLiteral("已收到的历史数据按时间顺序连成曲线；取消勾选可只看当前点与参考轨迹"));
        ctx.histCheck->setStyleSheet(QStringLiteral("QCheckBox { font-size: 10px; color: #555; }"));
        cbRow->addWidget(ctx.histCheck);
        ctx.rawCheck = new QCheckBox(rawLabel, box);
        ctx.rawCheck->setChecked(false);
        ctx.rawCheck->setStyleSheet(QStringLiteral("QCheckBox { font-size: 10px; color: #884da0; }"));
        cbRow->addWidget(ctx.rawCheck);
        cbRow->addStretch();
        lay->addLayout(cbRow);

        ctx.readoutLabel = new QLabel(QStringLiteral("—"), box);
        ctx.readoutLabel->setStyleSheet(QStringLiteral("QLabel { font-size: 11px; color: #333; }"));
        ctx.readoutLabel->setWordWrap(true);
        ctx.readoutLabel->setMinimumHeight(32);
        ctx.readoutLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        lay->addWidget(ctx.readoutLabel);

        TrackerCtx *cp = &ctx;
        connect(ctx.refCheck, &QCheckBox::toggled, this, [this, cp](bool on) {
            if (cp->refSeries) cp->refSeries->setVisible(on);
            refreshTrackerLegend(*cp);
        });
        connect(ctx.rawCheck, &QCheckBox::toggled, this, [this, cp](bool on) {
            if (cp->rawSeries) cp->rawSeries->setVisible(on);
            refreshTrackerLegend(*cp);
        });
        connect(ctx.histCheck, &QCheckBox::toggled, this, [this, cp](bool on) {
            // 历史数据始终在累积，本复选框只控制是否显示（取消勾选不会丢数据）
            if (cp->histTargetSeries) cp->histTargetSeries->setVisible(on);
            if (cp->histActualSeries) cp->histActualSeries->setVisible(on);
            refreshTrackerLegend(*cp);
        });
        return box;
    };

    trackerChartRow->addWidget(makeTrackerColumn(
        trackerM1, QStringLiteral("Motor1 轨迹跟踪"), 0x01, 0x02, 0x0B,
        QStringLiteral("显示下位机 0x0B 原始")), 1);
    trackerChartRow->addWidget(makeTrackerColumn(
        trackerM2, QStringLiteral("Motor2 轨迹跟踪"), 0x11, 0x12, 0x1D,
        QStringLiteral("显示下位机 0x1D 原始")), 1);
    trackerLay->addLayout(trackerChartRow);
    rootLayout->addWidget(trackerBox, 1);   // 轨迹跟踪区吃掉多余的纵向空间

    // ── 轨迹跟踪：信号连接与刷新定时器 ──────────────
    connect(trackerStartBtn, &QPushButton::clicked, this, &MainWindow::startExperiment);
    connect(trackerPreviewBtn, &QPushButton::clicked, this, [this]() {
        applyReference(false);
        appendTreatLog(QStringLiteral("参考轨迹预览已刷新（未下发指令）"));
    });
    connect(trackerCyclesSpin, &QSpinBox::valueChanged, this, [this](int) {
        // 未运行时改周期数会自动刷新预览；运行中不允许改变本次实验的参考轨迹
        if (!trackerM1.running && !trackerM2.running) applyReference(false);
    });

    mTrackerTimer = new QTimer(this);
    mTrackerTimer->setInterval(33);   // 约 30 Hz，仅用于游标与当前点
    connect(mTrackerTimer, &QTimer::timeout, this, &MainWindow::updateTrackerViews);
    mTrackerTimer->start();

    // 打开页面即可看到完整参考轨迹（离线预览，不需要设备在线）
    applyReference(false);

    // ── 实时状态行 ──────────────────────────────────
    auto *statusRow = new QHBoxLayout;
    statusRow->setSpacing(16);

    auto makeStatusPair = [](const QString &label, QWidget *parent) -> QLabel* {
        auto *box = new QHBoxLayout;
        box->addWidget(new QLabel(label, parent));
        auto *val = new QLabel(QStringLiteral("—"), parent);
        val->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 16px; color: #1565c0;"));
        box->addWidget(val);
        ((QVBoxLayout*)parent->layout())->addLayout(box);
        return val;
    };

    auto *statusBox = new QGroupBox(QStringLiteral("实时状态"), rehabPage);
    auto *statusBoxLayout = new QVBoxLayout(statusBox);
    treatTimeLabel = new QLabel(QStringLiteral("运行: 00:00:00"), statusBox);
    treatTimeLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; color: #2e7d32;"));
    statusBoxLayout->addWidget(treatTimeLabel);
    treatM1AngleLabel = makeStatusPair(QStringLiteral("M1 角度:"), statusBox);
    treatM2AngleLabel = makeStatusPair(QStringLiteral("M2 角度:"), statusBox);
    treatTorqueLabel   = makeStatusPair(QStringLiteral("力矩:"), statusBox);
    statusRow->addWidget(statusBox);

    // ── 力传感器 ───────────────────────────────────
    auto *forceBox = new QGroupBox(QStringLiteral("六维力传感器"), rehabPage);
    auto *forceOuter = new QVBoxLayout(forceBox);
    auto *forceLayout = new QHBoxLayout;
    forceLayout->setSpacing(10);
    const char *fNames[] = {"Fx","Fy","Fz","Mx","My","Mz"};
    for (int i = 0; i < 6; ++i) {
        auto *p = new QVBoxLayout;
        auto *n = new QLabel(QString::fromUtf8(fNames[i]), forceBox);
        n->setStyleSheet(QStringLiteral("font-size: 10px; color: #666;"));
        n->setAlignment(Qt::AlignCenter);
        p->addWidget(n);
        auto *v = new QLabel(QStringLiteral("—"), forceBox);
        v->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 13px; color: #2e7d32;"));
        v->setAlignment(Qt::AlignCenter);
        p->addWidget(v);
        forceLayout->addLayout(p);
    }
    // 力传感器归零按钮
    auto *forceZeroBtn = new QPushButton(QStringLiteral("归零"), forceBox);
    forceZeroBtn->setMaximumHeight(22);
    forceZeroBtn->setStyleSheet(QStringLiteral("QPushButton { font-size: 10px; padding: 0px 8px; }"));
    connect(forceZeroBtn, &QPushButton::clicked, this, [this]() {
        if (!protoWorker->isOpen()) return;
        QMetaObject::invokeMethod(protoWorker, "sendIOWrite", Qt::QueuedConnection,
            Q_ARG(uint8_t, IO_FORCE_ZERO), Q_ARG(uint8_t, uint8_t(1)));
        treatStatusLabel->setText(QStringLiteral("力传感器归零 → 已发送"));
        appendTreatLog(QStringLiteral("力传感器归零"));
    });
    forceOuter->addLayout(forceLayout);
    forceOuter->addWidget(forceZeroBtn, 0, Qt::AlignCenter);
    statusRow->addWidget(forceBox, 1);

    // 将力标签存入 rehabForceLabels
    {
        auto kids = forceBox->findChildren<QLabel*>();
        int idx = 0;
        for (auto *k : kids) {
            if (k->styleSheet().contains(QStringLiteral("2e7d32")) && idx < 6)
                rehabForceLabels[idx++] = k;
        }
    }

    rootLayout->addLayout(statusRow);

    // ── 治疗日志 ────────────────────────────────────
    auto *logBox = new QGroupBox(QStringLiteral("治疗日志"), rehabPage);
    auto *logLayout = new QVBoxLayout(logBox);
    treatLogEdit = new QTextEdit(rehabPage);
    treatLogEdit->setReadOnly(true);
    treatLogEdit->setMaximumHeight(60);
    treatLogEdit->setFont(QFont(QStringLiteral("Consolas"), 9));
    logLayout->addWidget(treatLogEdit);
    auto *clearLog = new QPushButton(QStringLiteral("清空"), rehabPage);
    auto *logBtn = new QHBoxLayout;
    // 数据记录按钮
    auto *recordBtn = new QPushButton(QStringLiteral("● 开始记录"), rehabPage);
    recordBtn->setMinimumHeight(30);
    recordBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #e8f5e9; font-weight: bold; font-size: 12px; "
        "border: 2px solid #4caf50; padding: 0px 12px; }"));
    connect(recordBtn, &QPushButton::clicked, this, [this, recordBtn]() {
        if (m_recording) {
            // ── 停止记录：刷出缓冲区到 CSV ──────────────
            m_recording = false;
            if (!m_recordBuffer.isEmpty()) {
                // 1) 收集所有出现过的 ID（按首次出现顺序）
                QList<uint8_t> colIds;
                QSet<uint8_t> seen;
                for (const auto &frame : m_recordBuffer) {
                    for (auto it = frame.values.constBegin(); it != frame.values.constEnd(); ++it) {
                        if (!seen.contains(it.key())) {
                            seen.insert(it.key());
                            colIds.append(it.key());
                        }
                    }
                }
                // 2) 写 CSV
                QTextStream out(m_recordFile);
                // 表头
                out << QStringLiteral("时间");
                for (uint8_t id : colIds) {
                    QString name;
                    for (int i = 0; i < floatIDTableSize; ++i) {
                        if (floatIDTable[i].id == id) {
                            name = QString::fromUtf8(floatIDTable[i].name);
                            break;
                        }
                    }
                    if (name.isEmpty()) name = QStringLiteral("0x%1").arg(id, 2, 16, QChar('0'));
                    out << "," << name;
                }
                out << "\n";
                // 数据行
                for (const auto &frame : m_recordBuffer) {
                    out << frame.timestamp;
                    for (uint8_t id : colIds) {
                        out << ",";
                        if (frame.values.contains(id))
                            out << QString::number(frame.values[id], 'f', 3);
                    }
                    out << "\n";
                }
                m_recordFile->close();
                int frames = m_recordBuffer.size();
                m_recordBuffer.clear();
                treatStatusLabel->setText(QStringLiteral("数据记录已停止 (共 %1 帧, %2 列)")
                    .arg(frames).arg(colIds.size()));
                appendTreatLog(QStringLiteral("数据记录已停止: %1 帧, %2 列").arg(frames).arg(colIds.size()));
            } else {
                m_recordFile->close();
                treatStatusLabel->setText(QStringLiteral("数据记录已停止 (无数据)"));
                appendTreatLog(QStringLiteral("数据记录已停止 (无数据)"));
            }
            recordBtn->setText(QStringLiteral("● 开始记录"));
            recordBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background-color: #e8f5e9; font-weight: bold; font-size: 12px; "
                "border: 2px solid #4caf50; padding: 0px 12px; }"));
        } else {
            // ── 开始记录 ────────────────────────────────
            QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
            QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/telemetry_%1.csv").arg(ts);
            m_recordFile->setFileName(path);
            if (m_recordFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
                m_recordBuffer.clear();
                m_recording = true;
                recordBtn->setText(QStringLiteral("■ 停止记录"));
                recordBtn->setStyleSheet(QStringLiteral(
                    "QPushButton { background-color: #ffebee; font-weight: bold; font-size: 12px; "
                    "border: 2px solid #f44336; padding: 0px 12px; }"));
                treatStatusLabel->setText(QStringLiteral("正在记录数据 → %1").arg(ts));
                appendTreatLog(QStringLiteral("开始记录数据 → %1").arg(ts));
            } else {
                treatStatusLabel->setText(QStringLiteral("无法创建文件"));
            }
        }
    });
    logBtn->addWidget(recordBtn);
    logBtn->addStretch();
    logBtn->addWidget(clearLog);
    connect(clearLog, &QPushButton::clicked, treatLogEdit, &QTextEdit::clear);
    logLayout->addLayout(logBtn);
    rootLayout->addWidget(logBox);

    // 状态
    treatStatusLabel = new QLabel(QStringLiteral("就绪 — 设置参数后点击启动。"), rehabPage);
    treatStatusLabel->setStyleSheet(QStringLiteral("color: #555; padding: 4px; border-top: 1px solid #ddd;"));
    rootLayout->addWidget(treatStatusLabel);
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

    // DR304 力传感器 (电机页 + 康复页)
    const uint8_t fids[] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46};
    for (int i = 0; i < 6; ++i) {
        if (values.contains(fids[i])) {
            float v = values[fids[i]];
            if (forceLabels[i]) forceLabels[i]->setText(fmtVal(v));
            if (rehabForceLabels[i]) rehabForceLabels[i]->setText(fmtVal(v));
        }
    }

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
        // 查表获取正确名称
        QString cbName;
        for (int j = 0; j < floatIDTableSize; ++j) {
            if (floatIDTable[j].id == ids[i]) {
                cbName = QString::fromUtf8(floatIDTable[j].name);
                break;
            }
        }
        auto *cb = new QCheckBox(cbName, parent);
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
// 轨迹跟踪视图（康复治疗页）
//   绿线 = 上位机本地生成的固定 N 周期参考轨迹（不依赖下位机 0x0B）
//   游标 = 当前实验时间竖直线
//   点   = 游标上的 🔵 目标角度 / 🟠 实测角度
//   ⚠️ 本区域的函数只做本地计时与绘图，不下发任何控制指令。
// ═══════════════════════════════════════════════════════════

// 在参考轨迹折线上按时间线性插值取值（与画出来的绿线严格一致）
static qreal refValueAt(const QList<QPointF> &traj, qreal t)
{
    if (traj.isEmpty()) return 0.0;
    if (t <= traj.first().x()) return traj.first().y();
    for (int i = 1; i < traj.size(); ++i) {
        if (t <= traj[i].x()) {
            const QPointF &a = traj[i - 1];
            const QPointF &b = traj[i];
            const qreal dt = b.x() - a.x();
            if (dt <= 0.0) return b.y();
            return a.y() + (b.y() - a.y()) * ((t - a.x()) / dt);
        }
    }
    return traj.last().y();
}

// 生成 N 周期分段线性三角波：
//   保持(极限角1, 停顿) → 斜坡到极限角2 → 保持(极限角2, 停顿) → 斜坡回极限角1
// 总时长 T = N × (2×停顿 + 2×(极限角2−极限角1)/T波速度)
static QList<QPointF> makeRefTrajectory(float l1, float l2, float speed, float pause,
                                        int cycles, qreal *totalT)
{
    if (!qIsFinite(l1)) l1 = -30.0f;
    if (!qIsFinite(l2)) l2 = 30.0f;
    if (!qIsFinite(speed) || speed <= 0.01f) speed = 30.0f;
    if (!qIsFinite(pause) || pause < 0.0f) pause = 0.0f;
    cycles = qBound(1, cycles, 20);

    const qreal hold = static_cast<qreal>(pause);
    const qreal ramp = qAbs(static_cast<qreal>(l2) - static_cast<qreal>(l1)) / static_cast<qreal>(speed);
    qreal t = 0.0;

    QList<QPointF> pts;
    pts.append(QPointF(0.0, static_cast<qreal>(l1)));   // 起点：极限角1 保持段开始

    for (int c = 0; c < cycles; ++c) {
        t += hold; pts.append(QPointF(t, static_cast<qreal>(l1)));  // 在极限角1 保持
        t += ramp; pts.append(QPointF(t, static_cast<qreal>(l2)));  // 斜坡 → 极限角2
        t += hold; pts.append(QPointF(t, static_cast<qreal>(l2)));  // 在极限角2 保持
        t += ramp; pts.append(QPointF(t, static_cast<qreal>(l1)));  // 斜坡 → 极限角1
    }
    if (totalT) *totalT = t;
    return pts;
}

// 图例条目固定为三条数据曲线：参考轨迹 / 轨迹输入 / 反馈角度。
// Qt Charts 的图例 marker 会跟随 series 可见性变化（勾选框、清空数据等都可能触发），
// 因此每次可见性变化后都调一次本函数校正，保证图例在
// 「开始前 / 运行中 / 结束后 / 重新开始后」四种状态下始终一致且完整。
void MainWindow::refreshTrackerLegend(TrackerCtx &ctx)
{
    if (!ctx.chart || !ctx.chart->legend()) return;
    ctx.chart->legend()->setVisible(true);
    ctx.chart->legend()->setAlignment(Qt::AlignTop);
    const QList<QLegendMarker *> markers = ctx.chart->legend()->markers();
    for (QLegendMarker *m : markers) {
        if (!m) continue;
        QAbstractSeries *s = m->series();
        // 只保留三种数据；时间游标、下位机原始轨迹、两个当前点不进图例
        const bool keep = (s == ctx.refSeries) || (s == ctx.histTargetSeries)
                          || (s == ctx.histActualSeries);
        m->setVisible(keep);
    }
}

void MainWindow::loadTrackerParams(TrackerCtx &ctx)
{
    QLineEdit **e = (ctx.targetId == 0x01) ? m1ActEdits : m2ActEdits;
    auto rd = [](QLineEdit *w, float def) -> float {
        if (!w) return def;
        bool ok = false;
        const float v = w->text().trimmed().toFloat(&ok);
        return ok ? v : def;
    };
    // 顺序与「主动」选项卡一致：T波速度 / 极限角1 / 极限角2 / 停顿时间
    ctx.speed  = rd(e[0], 30.0f);
    ctx.limit1 = rd(e[1], -30.0f);
    ctx.limit2 = rd(e[2], 30.0f);
    ctx.pause  = rd(e[3], 2.0f);
    if (!qIsFinite(ctx.speed) || ctx.speed <= 0.01f) ctx.speed = 30.0f;
    if (!qIsFinite(ctx.pause) || ctx.pause < 0.0f) ctx.pause = 0.0f;
    if (!qIsFinite(ctx.limit1)) ctx.limit1 = -30.0f;
    if (!qIsFinite(ctx.limit2)) ctx.limit2 = 30.0f;
}

void MainWindow::buildTrackerChart(TrackerCtx &ctx, QWidget *parent)
{
    ctx.chart = new QChart();
    ctx.chart->setAnimationOptions(QChart::NoAnimation);
    // 图例：必须一直可见，用于标识三种数据（参考轨迹 / 轨迹输入 / 反馈角度）。
    // 放在顶部，不会与曲线重叠；具体条目由 refreshTrackerLegend() 控制。
    ctx.chart->legend()->setVisible(true);
    ctx.chart->legend()->setAlignment(Qt::AlignTop);
    ctx.chart->legend()->setLabelColor(QColor(0x33, 0x33, 0x33));
    ctx.chart->setMargins(QMargins(4, 2, 4, 2));
    ctx.chart->setBackgroundRoundness(0);

    ctx.axisX = new QValueAxis();
    ctx.axisX->setRange(0.0, 40.0);
    ctx.axisX->setLabelFormat(QStringLiteral("%.0f"));
    // 不加轴标题：康复页纵向空间紧张，轴标题会把刻度文字挤成 "..."
    ctx.chart->addAxis(ctx.axisX, Qt::AlignBottom);

    ctx.axisY = new QValueAxis();
    ctx.axisY->setRange(-35.0, 35.0);
    ctx.axisY->setLabelFormat(QStringLiteral("%.0f"));
    ctx.chart->addAxis(ctx.axisY, Qt::AlignLeft);

    // 康复页纵向空间紧张、且本机为高 DPI，默认字号会让刻度被省略成 "..."，
    // 因此把两个轴的刻度字号略调小，保证数字能完整显示。
    QFont axisFont = ctx.axisX->labelsFont();
    if (axisFont.pointSizeF() > 0.0)
        axisFont.setPointSizeF(qMax(6.0, axisFont.pointSizeF() - 1.5));
    else
        axisFont.setPixelSize(qMax(8, axisFont.pixelSize() - 3));
    ctx.axisX->setLabelsFont(axisFont);
    ctx.axisY->setLabelsFont(axisFont);
    ctx.chart->legend()->setFont(axisFont);

    // 层叠顺序 = addSeries 的先后（后加的在上面）：
    //   原始曲线 → 参考轨迹 → 历史实际轨迹 → 游标 → 当前点
    // 参考轨迹用「较粗的半透明绿带」并画在数据曲线之下：
    //   - 游标右侧尚未扫到的部分：仍是一条完整的绿色参考线；
    //   - 数据与参考重合时：蓝/橙细线仍清晰可见，绿带从两侧透出；
    //   - 数据偏离参考时：彩色曲线直接跑出绿带，一目了然。
    auto addLine = [&ctx](QLineSeries *s, const QColor &c, qreal w, Qt::PenStyle st, bool visible) {
        QPen pen(c, w);
        pen.setStyle(st);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        s->setPen(pen);
        s->setColor(c);
        ctx.chart->addSeries(s);
        s->attachAxis(ctx.axisX);
        s->attachAxis(ctx.axisY);
        s->setVisible(visible);
    };

    ctx.rawSeries = new QLineSeries();
    addLine(ctx.rawSeries, QColor(0x88, 0x4d, 0xa0), 1.0, Qt::SolidLine, false);
    ctx.rawSeries->setName(QStringLiteral("下位机原始轨迹"));
    ctx.refSeries = new QLineSeries();
    addLine(ctx.refSeries, QColor(0x2c, 0xa0, 0x2c, 150), 3.5, Qt::SolidLine, true);
    ctx.refSeries->setName(QStringLiteral("参考轨迹"));
    ctx.histTargetSeries = new QLineSeries();
    addLine(ctx.histTargetSeries, QColor(0x1f, 0x77, 0xb4), 1.8, Qt::SolidLine, true);
    ctx.histTargetSeries->setName(QStringLiteral("轨迹输入"));
    ctx.histActualSeries = new QLineSeries();
    addLine(ctx.histActualSeries, QColor(0xff, 0x7f, 0x0e), 1.8, Qt::SolidLine, true);
    ctx.histActualSeries->setName(QStringLiteral("反馈角度"));
    ctx.cursorSeries = new QLineSeries();
    addLine(ctx.cursorSeries, QColor(0x55, 0x55, 0x55), 1.0, Qt::DashLine, true);
    ctx.cursorSeries->setName(QStringLiteral("时间游标"));

    // 两个当前点用「不同形状 + 不同尺寸」，这样即使目标与实测重合也能分辨：
    // 方形的 🔵 目标会被圆形/略小的 🟠 实测盖住中心，但四角仍会露出来。
    auto addPoint = [&ctx](QScatterSeries *s, const QColor &c, qreal size,
                           QScatterSeries::MarkerShape shape) {
        s->setColor(c);
        s->setBorderColor(c.darker(150));
        s->setMarkerSize(size);
        s->setMarkerShape(shape);
        ctx.chart->addSeries(s);
        s->attachAxis(ctx.axisX);
        s->attachAxis(ctx.axisY);
        s->setVisible(true);
    };
    ctx.ptTarget = new QScatterSeries();
    addPoint(ctx.ptTarget, QColor(0x1f, 0x77, 0xb4), 12.0, QScatterSeries::MarkerShapeRectangle);
    ctx.ptTarget->setName(QStringLiteral("轨迹输入(当前点)"));
    ctx.ptActual = new QScatterSeries();
    addPoint(ctx.ptActual, QColor(0xff, 0x7f, 0x0e), 10.0, QScatterSeries::MarkerShapeCircle);
    ctx.ptActual->setName(QStringLiteral("反馈角度(当前点)"));

    // 图例只保留三条数据曲线（游标、原始轨迹、当前点都从图例里排除）
    refreshTrackerLegend(ctx);

    ctx.view = new QChartView(ctx.chart, parent);
    ctx.view->setRenderHint(QPainter::Antialiasing);
    ctx.view->setMinimumHeight(160);
    ctx.view->setMaximumHeight(195);
}

// 重算参考轨迹与坐标轴范围；resetState=true 时同时复位实验状态（用于「开始实验/重新开始」）
void MainWindow::applyReference(bool resetState)
{
    const int cycles = trackerCyclesSpin ? trackerCyclesSpin->value() : 5;
    for (TrackerCtx *c : {&trackerM1, &trackerM2}) {
        loadTrackerParams(*c);
        c->cycles = cycles;
        qreal T = 0.0;
        c->refTraj = makeRefTrajectory(c->limit1, c->limit2, c->speed, c->pause, cycles, &T);
        c->totalT = T;

        if (c->refSeries) {
            c->refSeries->clear();
            for (const QPointF &p : c->refTraj) c->refSeries->append(p);
        }
        if (c->axisX) c->axisX->setRange(0.0, qMax(1.0, T));
        if (c->axisY) {
            const qreal lo = qMin(static_cast<qreal>(c->limit1), static_cast<qreal>(c->limit2)) - 5.0;
            const qreal hi = qMax(static_cast<qreal>(c->limit1), static_cast<qreal>(c->limit2)) + 5.0;
            c->axisY->setRange(lo, hi);
        }

        if (resetState) {
            c->hasTarget = c->hasActual = c->hasRaw = false;
            c->lastTarget = c->lastActual = c->lastRaw = 0.0f;
            c->lastTargetMs = c->lastActualMs = c->lastRawMs = 0;
            c->dropped = 0;
            c->nowT = 0.0;
            c->running = false;
            c->finished = false;
            // 清空历史痕迹，让新一轮实验从空白开始“扫”一遍
            const bool showHist = c->histCheck ? c->histCheck->isChecked() : true;
            if (c->histTargetSeries) { c->histTargetSeries->clear(); c->histTargetSeries->setVisible(showHist); }
            if (c->histActualSeries) { c->histActualSeries->clear(); c->histActualSeries->setVisible(showHist); }
            if (c->rawSeries) c->rawSeries->clear();
            // 重新开始后图例保持完整
            refreshTrackerLegend(*c);
        } else {
            c->nowT = qBound(0.0, c->nowT, qMax(0.001, c->totalT));
        }
    }
    updateTrackerViews();
}

void MainWindow::startExperiment()
{
    // ⚠️ 需求 D5：本函数只做本地计时与绘图，不允许调用任何 send* 下发函数。
    applyReference(true);
    trackerM1.clock.start();
    trackerM2.clock.start();
    trackerM1.running = true;
    trackerM2.running = true;
    if (trackerStartBtn) trackerStartBtn->setText(QStringLiteral("重新开始"));
    if (treatStatusLabel)
        treatStatusLabel->setText(QStringLiteral("轨迹跟踪实验运行中（仅本地计时与绘图，未下发控制指令）"));
    appendTreatLog(QStringLiteral("轨迹跟踪实验开始：%1 周期 / M1 总时长 %2 s / M2 总时长 %3 s")
        .arg(trackerM1.cycles)
        .arg(static_cast<double>(trackerM1.totalT), 0, 'f', 1)
        .arg(static_cast<double>(trackerM2.totalT), 0, 'f', 1));
    updateTrackerViews();
}

void MainWindow::updateTrackerViews()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    qreal maxT = 0.0;
    bool anyRunning = false;

    for (TrackerCtx *c : {&trackerM1, &trackerM2}) {
        if (c->running) {
            c->nowT = static_cast<qreal>(c->clock.elapsed()) / 1000.0;
            if (c->nowT >= c->totalT) {
                c->nowT = c->totalT;
                c->running = false;
                c->finished = true;
            }
        }
        maxT = qMax(maxT, c->totalT);
        anyRunning = anyRunning || c->running;

        const qreal t = c->nowT;
        const qreal ylo = c->axisY ? c->axisY->min() : -35.0;
        const qreal yhi = c->axisY ? c->axisY->max() : 35.0;

        if (c->cursorSeries) {
            c->cursorSeries->clear();
            c->cursorSeries->append(t, ylo);
            c->cursorSeries->append(t, yhi);
        }

        const bool tStale = c->running && c->hasTarget && (nowMs - c->lastTargetMs > 300);
        const bool aStale = c->running && c->hasActual && (nowMs - c->lastActualMs > 300);

        if (c->ptTarget) {
            c->ptTarget->clear();
            if (c->hasTarget) c->ptTarget->append(t, static_cast<qreal>(c->lastTarget));
            c->ptTarget->setColor(tStale ? QColor(0xb0, 0xbe, 0xc5) : QColor(0x1f, 0x77, 0xb4));
        }
        if (c->ptActual) {
            c->ptActual->clear();
            if (c->hasActual) c->ptActual->append(t, static_cast<qreal>(c->lastActual));
            c->ptActual->setColor(aStale ? QColor(0xb0, 0xbe, 0xc5) : QColor(0xff, 0x7f, 0x0e));
        }

        if (c->readoutLabel) {
            const qreal ref = refValueAt(c->refTraj, t);
            QString s;
            if (!c->hasTarget && !c->hasActual) {
                s = QStringLiteral("t = %1 / %2 s　[等待下位机数据]　丢弃异常 %3")
                        .arg(static_cast<double>(t), 0, 'f', 2)
                        .arg(static_cast<double>(c->totalT), 0, 'f', 1)
                        .arg(c->dropped);
            } else {
                const qreal err = ref - static_cast<qreal>(c->lastActual);
                s = QStringLiteral("t = %1 / %2 s　参考 %3°　目标 %4°　实测 %5°　误差 %6°")
                        .arg(static_cast<double>(t), 0, 'f', 2)
                        .arg(static_cast<double>(c->totalT), 0, 'f', 1)
                        .arg(ref, 0, 'f', 2)
                        .arg(static_cast<double>(c->lastTarget), 0, 'f', 2)
                        .arg(static_cast<double>(c->lastActual), 0, 'f', 2)
                        .arg(err, 0, 'f', 2);
                if (c->dropped > 0) s += QStringLiteral("　丢弃异常 %1").arg(c->dropped);
            }
            if (c->running && aStale)
                s += QStringLiteral("　⚠ 实测断流 %1 ms").arg(nowMs - c->lastActualMs);
            if (c->finished) s += QStringLiteral("　[已结束]");
            c->readoutLabel->setText(s);
        }
    }

    if (trackerStateLabel) {
        if (anyRunning) {
            trackerStateLabel->setText(QStringLiteral("运行中　t = %1 / %2 s")
                .arg(static_cast<double>(trackerM1.nowT), 0, 'f', 2)
                .arg(static_cast<double>(maxT), 0, 'f', 1));
            trackerStateLabel->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; color: #2e7d32; }"));
        } else if (trackerM1.finished || trackerM2.finished) {
            trackerStateLabel->setText(QStringLiteral("已结束　T = %1 s（点『重新开始』进入下一轮）")
                .arg(static_cast<double>(maxT), 0, 'f', 1));
            trackerStateLabel->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; color: #c62828; }"));
        } else {
            trackerStateLabel->setText(QStringLiteral("未开始　参考轨迹 %1 周期 / %2 s")
                .arg(trackerM1.cycles)
                .arg(static_cast<double>(trackerM1.totalT), 0, 'f', 1));
            trackerStateLabel->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; color: #1565c0; }"));
        }
    }
}

// 向曲线追加一个点，超过上限时成批丢弃最旧的点（防止高采样率下无限增长）。
// 只删到上限为止、不逐点删，保证摊销代价 O(1) 量级。
static void appendBounded(QLineSeries *s, qreal x, qreal y, int limit = 30000)
{
    if (!s) return;
    s->append(x, y);
    const int n = s->count();
    if (n > limit) s->removePoints(0, n - limit);
}

// 每个遥测帧调用一次：记录带时间戳的原始样本（不经过 50 ms 合并的降采样）
void MainWindow::feedTrackerSample(const QMap<uint8_t, float> &values, qint64 ms)
{
    for (TrackerCtx *c : {&trackerM1, &trackerM2}) {
        // 数值合理性过滤：非有限值，或 |角度| > |极限角| × 1.5 一律丢弃并计数。
        // 原因：设备当前上传的遥测帧存在损坏，0x12（M2 实际位置）会偶尔出现 3e-34 量级垃圾值。
        const float maxAbs = qMax(1.0f, qMax(qAbs(c->limit1), qAbs(c->limit2)) * 1.5f);

        bool gotTarget = false;
        bool gotActual = false;
        if (c->targetId != 0x00 && values.contains(c->targetId)) {
            const float v = values.value(c->targetId);
            if (qIsFinite(v) && qAbs(v) <= maxAbs) {
                c->lastTarget = v;
                c->hasTarget = true;
                c->lastTargetMs = ms;
                gotTarget = true;
            } else {
                ++c->dropped;
            }
        }
        if (c->actualId != 0x00 && values.contains(c->actualId)) {
            const float v = values.value(c->actualId);
            if (qIsFinite(v) && qAbs(v) <= maxAbs) {
                c->lastActual = v;
                c->hasActual = true;
                c->lastActualMs = ms;
                gotActual = true;
            } else {
                ++c->dropped;
            }
        }
        if (c->rawId != 0x00 && values.contains(c->rawId)) {
            const float v = values.value(c->rawId);
            if (qIsFinite(v) && qAbs(v) <= maxAbs) {
                c->hasRaw = true;
                c->lastRaw = v;
                c->lastRawMs = ms;
                if (c->rawSeries && c->rawSeries->isVisible() && c->running)
                    appendBounded(c->rawSeries, c->nowT, static_cast<qreal>(v));
            }
        }

        // 扫描式实时绘图：历史数据按“接收时刻的实验时间”持续追加，
        // 曲线随游标从左向右生长；已画出的历史不会因游标前进而消失。
        // 只在本帧真的收到该 ID 时才追加，不拿上一帧的值凑点，避免伪造采样。
        if (c->running) {
            if (gotTarget) appendBounded(c->histTargetSeries, c->nowT, static_cast<qreal>(c->lastTarget));
            if (gotActual) appendBounded(c->histActualSeries, c->nowT, static_cast<qreal>(c->lastActual));
        }
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
