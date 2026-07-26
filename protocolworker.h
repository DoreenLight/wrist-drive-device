#ifndef PROTOCOLWORKER_H
#define PROTOCOLWORKER_H

#include <QObject>
#include <QSerialPort>
#include <QByteArray>
#include <QFile>
#include <QMap>
#include <QMetaType>
#include <cstdint>

// 批量写入类型别名 (避免 Q_ARG 宏中模板逗号问题)
using BatchWriteItems = QList<QPair<uint8_t, float>>;

class ProtocolWorker : public QObject
{
    Q_OBJECT

public:
    explicit ProtocolWorker(QObject *parent = nullptr);
    ~ProtocolWorker() override;

    bool isOpen() const;

public slots:
    void openPort(const QString &portName, int baudRate,
                  int dataBits, int parity,
                  int stopBits, int flowControl);
    void closePort();

    // ── 协议命令发送 ───────────────────────────────────
    void sendPing();
    void sendIOWrite(uint8_t ioId, uint8_t value);
    void sendFloatWrite(uint8_t floatId, float value);
    void sendBatchWrite(const BatchWriteItems &items);
    void sendFlashSave();

signals:
    // ── 解析结果 ───────────────────────────────────────
    void frameParsed(uint8_t cmd, const QByteArray &payload,
                     bool sumOk, bool endOk, uint8_t rxSum, uint8_t calcSum);

    // 遥测数据 (TELEMETRY)
    void telemetryReceived(const QMap<uint8_t, float> &values);

    // ACK 应答
    void ackReceived(uint8_t ackCmd);

    // IO 状态上报
    void ioReportReceived(uint8_t ioId, uint8_t value);

    // ── 原始数据 ───────────────────────────────────────
    void rawDataReceived(const QByteArray &data);
    void rawDataSent(const QByteArray &data);

    // ── 统计信息 ───────────────────────────────────────
    void statsChanged(int totalFrames, int checksumErrors);

    // ── 错误 / 状态 ─────────────────────────────────────
    void parseError(const QString &message);
    void portError(const QString &message);
    void portOpened();
    void portClosed();

private slots:
    void onReadyRead();

private:
    int parseFrame(const uint8_t *data, int maxLen);  // 返回消耗的字节数（含失败帧）
    void processFrame();
    void resetParser();

    // 帧构建
    QByteArray buildFrame(uint8_t cmd, const QByteArray &payload) const;
    static uint8_t calcChecksum(uint8_t len, uint8_t cmd, const QByteArray &payload);

    QSerialPort *m_serial;

    // ── 解析器状态 ─────────────────────────────────────
    uint8_t    m_len;
    uint8_t    m_cmd;
    QByteArray m_payload;
    QByteArray m_rxBuf;      // 接收缓冲区
    int        m_payloadIdx;

    // ── 错误帧记录 ─────────────────────────────────────
    QByteArray m_errorFramesBuffer;
    void flushErrorFrames();

    // ── 统计 ───────────────────────────────────────────
    int m_totalFrames;
    int m_checksumErrors;
};

Q_DECLARE_METATYPE(BatchWriteItems)

#endif // PROTOCOLWORKER_H
