#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSerialPort>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QLegendMarker>
#include <QLineSeries>
#include <QScatterSeries>
#include <QChart>
#include <QChartView>
#include <QElapsedTimer>
#include <QValueAxis>

class ProtocolWorker;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // ── 串口调试页 (Page 1) ─────────────────────────
    void onRefreshPorts();
    void onOpenClosePort();
    void onSendData();
    void onClearReceive();
    void onClearTransmit();
    void onSerialReadyRead();
    void onAutoSendTimeout();
    void showHomePage();
    void showSerialPage();
    void showProtocolPage();
    void showMotorPage();
    void showRehabPage();

    // ── 协议调试页 (Page 2) ─────────────────────────
    void onProtocolOpenClose();
    void onProtocolRefreshPorts();
    void onSendPing();
    void onSendStart();
    void onSendStop();
    void onSendFloatWrite();
    void onSendBatchWrite();
    void onClearProtocolLog();

    // ── ProtocolWorker 回调 ──────────────────────────
    void onProtocolFrameParsed(uint8_t cmd, const QByteArray &payload,
                                bool sumOk, bool endOk, uint8_t rxSum, uint8_t calcSum);
    void onTelemetryReceived(const QMap<uint8_t, float> &values);
    void onAckReceived(uint8_t ackCmd);
    void onIoReportReceived(uint8_t ioId, uint8_t value);
    void onProtocolRawReceived(const QByteArray &data);
    void onProtocolRawSent(const QByteArray &data);
    void onProtocolStatsChanged(int totalFrames, int checksumErrors);
    void onProtocolParseError(const QString &msg);
    void onProtocolPortError(const QString &msg);
    void onProtocolPortOpened();
    void onProtocolPortClosed();

private:
    void createHomePage();
    void createSerialPage();
    void createProtocolDebugPage();
    void createMotorControlPage();
    void createRehabPage();

    int baudRateFromComboBox() const;
    QSerialPort::DataBits dataBitsFromComboBox() const;
    QSerialPort::Parity parityFromComboBox() const;
    QSerialPort::StopBits stopBitsFromComboBox() const;
    QSerialPort::FlowControl flowControlFromComboBox() const;

    void appendReceiveData(const QByteArray &data);
    void appendTransmitData(const QString &text);
    void appendProtocolLog(const QString &text);

    // ── Page 1: 串口调试页成员 ───────────────────────
    Ui::MainWindow *ui;
    QStackedWidget *stackedWidget;
    QWidget *homePage;
    QWidget *serialPage;
    QWidget *protocolPage;
    QWidget *motorPage;
    QWidget *rehabPage;

    QComboBox *portComboBox;
    QComboBox *baudComboBox;
    QComboBox *dataBitsComboBox;
    QComboBox *parityComboBox;
    QComboBox *stopBitsComboBox;
    QComboBox *flowControlComboBox;
    QPushButton *refreshPortsButton;
    QPushButton *openCloseButton;
    QPushButton *sendButton;
    QPushButton *backButton;
    QTextEdit *receiveEdit;
    QTextEdit *transmitEdit;
    QLineEdit *sendLineEdit;
    QCheckBox *hexSendCheckBox;
    QCheckBox *hexReceiveCheckBox;
    QCheckBox *autoSendCheckBox;
    QSpinBox *autoIntervalSpinBox;
    QLabel *statusLabel;

    QSerialPort *serialPort;
    QTimer *autoSendTimer;

    // ── Page 2: 协议调试页成员 ───────────────────────
    // 串口配置行
    QComboBox *protoPortCombo;
    QComboBox *protoBaudCombo;
    QPushButton *protoOpenBtn;
    QPushButton *protoRefreshBtn;

    // 命令面板
    QPushButton *pingBtn;
    QPushButton *startBtn;
    QPushButton *stopBtn;
    QComboBox *floatIdCombo;
    QLineEdit *floatValueEdit;
    QPushButton *floatWriteBtn;
    QComboBox *batchIdCombo;
    QLineEdit *batchValueEdit;
    QPushButton *batchAddBtn;
    QTextEdit *batchPreviewEdit;
    QPushButton *batchSendBtn;

    // 遥测数据表格
    QTableWidget *telemetryTable;

    // 诊断日志
    QTextEdit *protocolLogEdit;

    // 统计标签
    QLabel *statRxFrames;
    QLabel *statChkErr;
    QLabel *statTelemetry;
    QLabel *statAck;

    // 状态
    // 数据记录
    bool m_recording;
    QFile *m_recordFile;
    struct RecordFrame {
        QString timestamp;
        QMap<uint8_t, float> values;
    };
    QList<RecordFrame> m_recordBuffer;

    QLabel *protocolStatusLabel;

    // ── 电机图表数据 ──────────────────────────────────
    struct MotorChartCtx {
        QChartView *view = nullptr;
        QChart *chart = nullptr;
        QValueAxis *axisX = nullptr;
        QValueAxis *axisY = nullptr;
        QMap<uint8_t, QLineSeries*> series;
        QMap<uint8_t, QList<QPointF>> buffers;
        QMap<uint8_t, QCheckBox*> checkboxes;
        qreal time = 0;
    };
    void buildMotorChart(MotorChartCtx &ctx, QWidget *parent,
                         const uint8_t *ids, int count,
                         const QColor *colors);
    void feedMotorChart(MotorChartCtx &ctx, const QMap<uint8_t, float> &values);

    // ── 参考轨迹跟踪视图（康复页；参考轨迹由上位机本地生成）────
    // 注意：本视图只做“本地计时 + 绘图”，不下发任何控制指令。
    struct TrackerCtx {
        // 图对象（绘制顺序即叠加顺序：先加的在下层）
        QChartView *view = nullptr;
        QChart *chart = nullptr;
        QValueAxis *axisX = nullptr;
        QValueAxis *axisY = nullptr;
        QLineSeries *rawSeries = nullptr;         // 下位机 0x0B/0x1D 原始轨迹（不复用为参考）
        QLineSeries *histTargetSeries = nullptr;  // 历史目标（可选显示）
        QLineSeries *histActualSeries = nullptr;  // 历史实测（可选显示）
        QLineSeries *refSeries = nullptr;         // 🟢 本地生成的参考轨迹
        QLineSeries *cursorSeries = nullptr;      // 时间游标竖直线
        QScatterSeries *ptTarget = nullptr;       // 🔵 游标上的目标点
        QScatterSeries *ptActual = nullptr;       // 🟠 游标上的实测点
        // 控件
        QCheckBox *refCheck = nullptr;
        QCheckBox *histCheck = nullptr;
        QCheckBox *rawCheck = nullptr;
        QLabel *readoutLabel = nullptr;

        // 标识与数据 ID
        QString name;
        uint8_t targetId = 0x00;
        uint8_t actualId = 0x00;
        uint8_t rawId = 0x00;

        // 参考轨迹参数（从现有「主动」选项卡输入框读取，只读不进下发）
        float limit1 = -30.0f;
        float limit2 = 30.0f;
        float speed = 30.0f;    // °/s
        float pause = 2.0f;     // s
        int cycles = 5;

        // 本地生成的参考轨迹与总时长
        QList<QPointF> refTraj;
        qreal totalT = 0.0;

        // 实验状态机
        bool running = false;
        bool finished = false;
        QElapsedTimer clock;
        qreal nowT = 0.0;

        // 实时样本
        // 历史轨迹不再单独缓存：直接按接收时间追加到 histTarget/histActual 两条 series
        // （扫描式绘图，见 feedTrackerSample）。这里只保存“最新值”用于游标上的当前点。
        bool hasTarget = false;
        bool hasActual = false;
        bool hasRaw = false;
        float lastTarget = 0.0f;
        float lastActual = 0.0f;
        float lastRaw = 0.0f;
        qint64 lastTargetMs = 0;
        qint64 lastActualMs = 0;
        qint64 lastRawMs = 0;
        int dropped = 0;        // 被合理性过滤丢弃的样本数
    };
    TrackerCtx trackerM1;
    TrackerCtx trackerM2;
    QSpinBox *trackerCyclesSpin = nullptr;
    QPushButton *trackerStartBtn = nullptr;
    QLabel *trackerStateLabel = nullptr;
    QTimer *mTrackerTimer = nullptr;
    QLineEdit *m1ActEdits[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    QLineEdit *m2ActEdits[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

    void buildTrackerChart(TrackerCtx &ctx, QWidget *parent);
    void loadTrackerParams(TrackerCtx &ctx);
    void applyReference(bool resetState);
    void startExperiment();
    void updateTrackerViews();
    void refreshTrackerLegend(TrackerCtx &ctx);   // 图例只保留三条数据曲线
    void feedTrackerSample(const QMap<uint8_t, float> &values, qint64 ms);

    // ── Page 3: 电机控制调试 ──────────────────────────
    // Motor1 遥测标签
    QLabel *m1AngleLabel;
    QLabel *m1SpeedLabel;
    QLabel *m1CurrentLabel;
    QLabel *m1KpLabel;
    QLabel *m1KiLabel;
    // Motor2 遥测标签
    QLabel *m2PosLabel;
    QLabel *m2SpdLabel;
    QLabel *m2TrqLabel;
    // 电机控制按钮 (启停合一, 切换态)
    QPushButton *m1ToggleBtn;
    QPushButton *m2ToggleBtn;
    bool m1Running;
    bool m2Running;
    QComboBox *motorModeCombo;      // 系统模式
    QComboBox *m2ModeCombo;         // M2 运行模式
    MotorChartCtx m1ChartCtx;       // M1 曲线图 (电机页)
    MotorChartCtx m2ChartCtx;       // M2 曲线图 (电机页)
    // 遥测队列 + 定时刷新
    QList<QMap<uint8_t, float>> mTelemetryQueue;
    QMutex mTelemetryMutex;
    QTimer *mChartTimer;

    // 底部数据标签
    QLabel *m1DataLabels[5];  // M1: 角度指令/角度响应/控制器输出/数据4/数据5
    QLabel *m2DataLabels[5];  // M2: 数据1~5
    QLabel *forceLabels[6];      // DR304: Fx/Fy/Fz/Mx/My/Mz (电机页)
    QLabel *rehabForceLabels[6]; // DR304: Fx/Fy/Fz/Mx/My/Mz (康复页)
    float m1LastAngleCmd;     // 记录最后下发的角度指令

    QLabel *motorStatusLabel;

    // ── Page 4: 康复治疗 ──────────────────────────────
    QComboBox *treatModeCombo;
    QLineEdit *treatAngleEdit;
    QLineEdit *treatSpeedEdit;
    QSpinBox *treatCyclesSpin;
    QPushButton *treatStartBtn, *treatPauseBtn, *treatStopBtn;
    QLabel *treatTimeLabel;
    QLabel *treatM1AngleLabel;
    QLabel *treatM2AngleLabel;
    QLabel *treatTorqueLabel;
    QTextEdit *treatLogEdit;
    QLabel *treatStatusLabel;
    QTimer *treatTimer;
    int treatElapsedSec;
    bool treatRunning;

    void appendTreatLog(const QString &text);
    void updateMotorDisplay(const QMap<uint8_t, float> &values);

    // ── 多线程 ───────────────────────────────────────
    ProtocolWorker *protoWorker;
    QThread *protoThread;
};

#endif // MAINWINDOW_H
