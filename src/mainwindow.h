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
#include <QLineSeries>
#include <QChart>
#include <QChartView>
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
    MotorChartCtx rehabM1Ctx;       // M1 曲线图 (康复页)
    MotorChartCtx rehabM2Ctx;       // M2 曲线图 (康复页)
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
