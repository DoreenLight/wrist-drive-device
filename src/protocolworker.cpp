#include "protocolworker.h"
#include "floatiddefs.h"

#include <QCoreApplication>
#include <QDir>
#include <QTextStream>
#include <cstring>

// ═══════════════════════════════════════════════════════════
// 构造 / 析构
// ═══════════════════════════════════════════════════════════

ProtocolWorker::ProtocolWorker(QObject *parent)
    : QObject(parent)
    , m_serial(new QSerialPort(this))
    , m_len(0)
    , m_cmd(0)
    , m_payloadIdx(0)
    , m_totalFrames(0)
    , m_checksumErrors(0)
{
    connect(m_serial, &QSerialPort::readyRead,
            this, &ProtocolWorker::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred,
            this, [this](QSerialPort::SerialPortError err) {
        if (err != QSerialPort::NoError) {
            emit portError(m_serial->errorString());
        }
    });
}

ProtocolWorker::~ProtocolWorker()
{
    flushErrorFrames();
    if (m_serial->isOpen()) {
        m_serial->close();
    }
}

bool ProtocolWorker::isOpen() const
{
    return m_serial->isOpen();
}

// ═══════════════════════════════════════════════════════════
// 串口操作 (在 worker 线程中执行)
// ═══════════════════════════════════════════════════════════

void ProtocolWorker::openPort(const QString &portName, int baudRate,
                              int dataBits, int parity,
                              int stopBits, int flowControl)
{
    if (m_serial->isOpen()) {
        m_serial->close();
    }

    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(static_cast<QSerialPort::DataBits>(dataBits));
    m_serial->setParity(static_cast<QSerialPort::Parity>(parity));
    m_serial->setStopBits(static_cast<QSerialPort::StopBits>(stopBits));
    m_serial->setFlowControl(static_cast<QSerialPort::FlowControl>(flowControl));

    if (m_serial->open(QIODevice::ReadWrite)) {
        resetParser();
        emit portOpened();
    } else {
        emit portError(QStringLiteral("无法打开串口: %1").arg(m_serial->errorString()));
    }
}

void ProtocolWorker::closePort()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        emit portClosed();
    }
}

// ═══════════════════════════════════════════════════════════
// 协议命令发送
// ═══════════════════════════════════════════════════════════

void ProtocolWorker::sendPing()
{
    // PING: LEN=0, 无载荷
    QByteArray frame = buildFrame(CMD_PING, QByteArray());
    m_serial->write(frame);
    emit rawDataSent(frame);
}

void ProtocolWorker::sendIOWrite(uint8_t ioId, uint8_t value)
{
    QByteArray payload;
    payload.append(static_cast<char>(ioId));
    payload.append(static_cast<char>(value));
    QByteArray frame = buildFrame(CMD_IO_WRITE, payload);
    m_serial->write(frame);
    emit rawDataSent(frame);
}

void ProtocolWorker::sendFloatWrite(uint8_t floatId, float value)
{
    QByteArray payload;
    payload.append(static_cast<char>(floatId));

    // IEEE 754 小端序
    uint32_t raw;
    static_assert(sizeof(raw) == sizeof(value), "float/uint32_t size mismatch");
    std::memcpy(&raw, &value, 4);
    payload.append(static_cast<char>(raw & 0xFF));
    payload.append(static_cast<char>((raw >> 8) & 0xFF));
    payload.append(static_cast<char>((raw >> 16) & 0xFF));
    payload.append(static_cast<char>((raw >> 24) & 0xFF));

    QByteArray frame = buildFrame(CMD_FLOAT_WRITE, payload);
    m_serial->write(frame);
    emit rawDataSent(frame);
}

void ProtocolWorker::sendBatchWrite(const BatchWriteItems &items)
{
    if (items.isEmpty()) return;

    QByteArray payload;
    uint8_t cnt = static_cast<uint8_t>(items.size());
    payload.append(static_cast<char>(cnt));

    for (const auto &item : items) {
        payload.append(static_cast<char>(item.first));
        uint32_t raw;
        std::memcpy(&raw, &item.second, 4);
        payload.append(static_cast<char>(raw & 0xFF));
        payload.append(static_cast<char>((raw >> 8) & 0xFF));
        payload.append(static_cast<char>((raw >> 16) & 0xFF));
        payload.append(static_cast<char>((raw >> 24) & 0xFF));
    }

    QByteArray frame = buildFrame(CMD_BATCH_WRITE, payload);
    m_serial->write(frame);
    emit rawDataSent(frame);
}

void ProtocolWorker::sendFlashSave()
{
    // FLASH_SAVE: LEN=0, 无载荷
    QByteArray frame = buildFrame(CMD_FLASH_SAVE, QByteArray());
    m_serial->write(frame);
    emit rawDataSent(frame);
}

// ═══════════════════════════════════════════════════════════
// 数据接收 — 缓冲式解析
// ═══════════════════════════════════════════════════════════

void ProtocolWorker::onReadyRead()
{
    QByteArray data = m_serial->readAll();
    m_rxBuf.append(data);
    emit rawDataReceived(data);

    while (m_rxBuf.size() >= FRAME_MIN_SIZE) {
        int consumed = parseFrame(
            reinterpret_cast<const uint8_t*>(m_rxBuf.constData()),
            m_rxBuf.size());
        if (consumed <= 0) break;
        m_rxBuf.remove(0, consumed);
    }
}

// 从 data 中尝试解析一帧。返回消耗的字节数（>=FRAME_MIN_SIZE），失败也消耗。
int ProtocolWorker::parseFrame(const uint8_t *data, int maxLen)
{
    // 1. 搜索 SYNC_H
    int pos = 0;
    while (pos < maxLen && data[pos] != FRAME_SYNC_H) pos++;
    if (pos + FRAME_MIN_SIZE > maxLen) return pos > 0 ? pos : 0;  // 数据不够或只跳过了垃圾

    // 2. 验证 SYNC_L
    if (data[pos + 1] != FRAME_SYNC_L) {
        return pos + 1;  // 跳过这个假的 6C
    }

    // 3. 读取 LEN
    uint8_t len = data[pos + 2];
    if (len > FRAME_MAX_LEN) {
        return pos + 1;  // 跳过假的 6C
    }

    // 4. 检查完整帧是否在缓冲区内: SYNC_H+SYNC_L+LEN+CMD+PAYLOAD+SUM+END
    int frameLen = 2 + 1 + 1 + len + 1 + 1;  // = len + 6
    if (pos + frameLen > maxLen) return pos;  // 等待更多数据

    // 5. 读取各字段
    uint8_t cmd = data[pos + 3];
    const uint8_t *payload = data + pos + 4;
    uint8_t rxSum = data[pos + 4 + len];
    uint8_t endByte = data[pos + 4 + len + 1];

    // 6. 验证 END
    if (endByte != FRAME_END) {
        // 假同步 — 跳过这个 6C, 从下一个字节继续搜索
        return pos + 1;
    }

    // 7. 验证 SUM
    uint32_t sum = len + cmd;
    for (int i = 0; i < len; i++) sum += payload[i];
    uint8_t calcSum = static_cast<uint8_t>(sum & 0xFF);
    bool sumOk = (rxSum == calcSum);

    m_totalFrames++;
    if (!sumOk) {
        m_checksumErrors++;
        // 记录错误帧
        QByteArray raw(reinterpret_cast<const char*>(data + pos), frameLen);
        m_errorFramesBuffer.append(
            QStringLiteral("[chk_fail] rxSum=0x%1 calcSum=0x%2 len=%3 cmd=0x%4 raw=%5\n")
                .arg(rxSum, 2, 16, QChar('0')).arg(calcSum, 2, 16, QChar('0'))
                .arg(len).arg(cmd, 2, 16, QChar('0'))
                .arg(QString::fromLatin1(raw.toHex(' ').toUpper())).toUtf8());
        if (m_errorFramesBuffer.count('\n') >= 50) flushErrorFrames();
    }

    emit statsChanged(m_totalFrames, m_checksumErrors);

    // 构建 fullPayload 用于 UI 诊断
    QByteArray fullPayload;
    fullPayload.append(static_cast<char>(len));
    fullPayload.append(static_cast<char>(cmd));
    fullPayload.append(reinterpret_cast<const char*>(payload), len);
    fullPayload.append(static_cast<char>(rxSum));
    fullPayload.append(static_cast<char>(endByte));
    emit frameParsed(cmd, fullPayload, sumOk, true, rxSum, calcSum);

    if (sumOk) {
        // 更新内部状态用于 processFrame
        m_len = len; m_cmd = cmd;
        m_payload = QByteArray(reinterpret_cast<const char*>(payload), len);
        processFrame();
    }

    return pos + frameLen;  // 消耗完整帧
}

void ProtocolWorker::processFrame()
{
    switch (m_cmd) {

    case CMD_TELEMETRY: {
        // 载荷: [cnt:1B] [(id:1B)(val:4B_LE)]*
        if (m_payload.size() < 1) return;

        uint8_t cnt = static_cast<uint8_t>(m_payload[0]);
        QMap<uint8_t, float> values;

        int offset = 1;
        for (int i = 0; i < cnt; ++i) {
            if (offset + 5 > m_payload.size()) break;

            uint8_t fid = static_cast<uint8_t>(m_payload[offset]);
            const uint8_t *valPtr = reinterpret_cast<const uint8_t *>(m_payload.constData() + offset + 1);

            uint32_t raw = valPtr[0] | (valPtr[1] << 8) | (valPtr[2] << 16) | (valPtr[3] << 24);
            float fval;
            std::memcpy(&fval, &raw, 4);

            values.insert(fid, fval);
            offset += 5;
        }
        emit telemetryReceived(values);
        break;
    }

    case CMD_ACK: {
        // 载荷: [ack_cmd:1B]
        uint8_t ackCmd = 0;
        if (m_payload.size() >= 1) {
            ackCmd = static_cast<uint8_t>(m_payload[0]);
        }
        emit ackReceived(ackCmd);
        break;
    }

    case CMD_IO_REPORT: {
        // 载荷: [id:1B] [val:1B]
        if (m_payload.size() >= 2) {
            uint8_t ioId = static_cast<uint8_t>(m_payload[0]);
            uint8_t ioVal = static_cast<uint8_t>(m_payload[1]);
            emit ioReportReceived(ioId, ioVal);
        }
        break;
    }

    default:
        // 对于 IO_WRITE / FLOAT_WRITE / BATCH_WRITE 等，仅记录日志
        break;
    }
}

void ProtocolWorker::resetParser()
{
    m_rxBuf.clear();
    m_len = 0;
    m_cmd = 0;
    m_payload.clear();
    m_payloadIdx = 0;
    m_totalFrames = 0;
    m_checksumErrors = 0;
    emit statsChanged(0, 0);
}

// ═══════════════════════════════════════════════════════════
// 帧构建
// ═══════════════════════════════════════════════════════════

QByteArray ProtocolWorker::buildFrame(uint8_t cmd, const QByteArray &payload) const
{
    QByteArray frame;
    uint8_t len = static_cast<uint8_t>(payload.size());

    frame.append(static_cast<char>(FRAME_SYNC_H));   // 0x6C
    frame.append(static_cast<char>(FRAME_SYNC_L));   // 0x67
    frame.append(static_cast<char>(len));
    frame.append(static_cast<char>(cmd));
    frame.append(payload);
    frame.append(static_cast<char>(calcChecksum(len, cmd, payload)));
    frame.append(static_cast<char>(FRAME_END));

    return frame;
}

uint8_t ProtocolWorker::calcChecksum(uint8_t len, uint8_t cmd, const QByteArray &payload)
{
    uint32_t sum = len + cmd;
    for (int i = 0; i < payload.size(); ++i) {
        sum += static_cast<uint8_t>(payload[i]);
    }
    return static_cast<uint8_t>(sum & 0xFF);
}

void ProtocolWorker::flushErrorFrames()
{
    if (m_errorFramesBuffer.isEmpty()) return;

    QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/error_frames.txt");
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        f.write(m_errorFramesBuffer);
        f.close();
    }
    m_errorFramesBuffer.clear();
}
