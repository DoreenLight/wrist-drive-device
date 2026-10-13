#ifndef FLOATIDDEFS_H
#define FLOATIDDEFS_H

#include <cstdint>
#include <QMap>
#include <QString>

// ── 命令码 (CMD) ──────────────────────────────────────────
// 上位机 → MCU
constexpr uint8_t CMD_IO_WRITE    = 0x01;
constexpr uint8_t CMD_FLOAT_WRITE = 0x02;
constexpr uint8_t CMD_BATCH_WRITE = 0x03;
constexpr uint8_t CMD_PING        = 0x84;
constexpr uint8_t CMD_FLASH_SAVE  = 0x85;

// MCU → 上位机
constexpr uint8_t CMD_TELEMETRY   = 0x81;
constexpr uint8_t CMD_IO_REPORT   = 0x82;
constexpr uint8_t CMD_ACK         = 0x83;

// ── 帧格式常量 ────────────────────────────────────────────
constexpr uint8_t  FRAME_SYNC_H   = 0x6C;
constexpr uint8_t  FRAME_SYNC_L   = 0x67;
constexpr uint8_t  FRAME_END      = 0xED;
constexpr uint8_t  FRAME_MAX_LEN  = 250;
constexpr int      FRAME_MIN_SIZE = 6;   // SYNC_H + SYNC_L + LEN + CMD + SUM + END

// ── IO ID 定义 ────────────────────────────────────────────
constexpr uint8_t IO_M1_START_STOP = 0x01;  // M1 启停
constexpr uint8_t IO_M1_CTRL_MODE  = 0x02;  // M1 控制模式
constexpr uint8_t IO_M2_ENABLE     = 0x03;  // M2 启停 (使能/停止)
constexpr uint8_t IO_M2_RUN_MODE   = 0x04;  // M2 RS00 运行模式
constexpr uint8_t IO_M2_SET_ZERO   = 0x05;  // M2 机械零点
constexpr uint8_t IO_M2_CLR_FAULT  = 0x06;  // M2 清除故障
constexpr uint8_t IO_M2_TRAJ_MODE  = 0x07;  // M2 轨迹模式
constexpr uint8_t IO_M1_REHAB      = 0x08;  // M1 康复启停
constexpr uint8_t IO_M2_REHAB      = 0x09;  // M2 康复启停
constexpr uint8_t IO_SYS_START_STOP   = 0x0A;  // 系统启停 (TIM2 控制循环)
constexpr uint8_t IO_M1_ACTIVE_REHAB  = 0x0B;  // M1 主动康复启停
constexpr uint8_t IO_FORCE_ZERO       = 0x0C;  // 力传感器清零
constexpr uint8_t IO_M2_ACTIVE_REHAB  = 0x0D;  // M2 主动康复启停
constexpr uint8_t IO_M2_ACTIVE_MODE   = 0x0E;  // M2 主动康复模式 (1=Fy, 2=Fz)

// 通用 IO 值
constexpr uint8_t IO_VAL_START  = 1;
constexpr uint8_t IO_VAL_STOP   = 2;
constexpr uint8_t IO_VAL_ENABLE = 1;

// ── M1 控制模式 (IO_ID=0x02) ──────────────────────────────
constexpr uint8_t M1_MODE_POS     = 0;  // 位置
constexpr uint8_t M1_MODE_SPEED   = 1;  // 速度
constexpr uint8_t M1_MODE_PWM     = 3;  // PWM测试

// ── M2 RS00 模式 (IO_ID=0x04) ─────────────────────────────
constexpr uint8_t M2_MODE_YUNKONG = 1;  // 运控
constexpr uint8_t M2_MODE_POS     = 2;  // 位置
constexpr uint8_t M2_MODE_SPEED   = 3;  // 速度
constexpr uint8_t M2_MODE_CSP     = 5;  // CSP

// ── M2 轨迹模式 (IO_ID=0x07) ──────────────────────────────
constexpr uint8_t M2_TRAJ_POS   = 0;  // 位置
constexpr uint8_t M2_TRAJ_SPEED = 1;  // 速度

// ── 浮点 ID → 名称 / 单位 ─────────────────────────────────
struct FloatIDInfo {
    uint8_t id;
    const char *name;
    const char *unit;
    bool writable;  // true = 双向, false = 只读(上行)
};

inline const FloatIDInfo floatIDTable[] = {
    // ── 电机1 遥测 (MCU 上传) ───────────────────────
    { 0x01, "M1 轨迹输入",      "",      true  },   // PID 设定值
    { 0x02, "M1 反馈角度",      "°",     true  },   // roll IMU 实测
    { 0x03, "M1 PID 输出",      "",      true  },   // Control_P_PID.u_out
    { 0x04, "M1 Kp",            "",      true  },
    { 0x05, "M1 Ki",            "",      true  },
    { 0x06, "M1 PWM 占空比",    "±1.0",  true  },
    { 0x07, "M1 最大速度",      "°/s",   true  },
    { 0x08, "M1 康复最小角度",  "°",     true  },
    { 0x09, "M1 康复最大角度",  "°",     true  },
    { 0x0A, "M1 康复循环次数",  "",      true  },
    { 0x0B, "T波虚拟轨迹",      "°",     true  },
    { 0x0C, "M1 主动极限角1",   "°",     true  },
    { 0x0D, "M1 主动极限角2",   "°",     true  },
    { 0x0E, "M1 主动停顿时间",  "s",     true  },
    { 0x0F, "M1 主动Tmax",      "N·m",  true  },
    { 0x10, "M1 主动刚度k",     "",      true  },
    // ── 电机2 遥测 ──────────────────────────────────
    { 0x11, "M2 规划位置",      "°",     true  },
    { 0x12, "M2 实际位置",      "°",     true  },
    { 0x13, "M2 规划速度",      "°/s",   true  },
    { 0x14, "M2 实际速度",      "°/s",   true  },
    { 0x15, "M2 力矩",          "N·m",   true  },
    { 0x17, "M2 电流限制",      "A",     true  },
    { 0x18, "M2 最大速度",      "°/s",   true  },
    { 0x1A, "M2 康复最小角度",  "°",     true  },
    { 0x1B, "M2 康复最大角度",  "°",     true  },
    { 0x1C, "M2 康复循环次数",  "",      true  },
    { 0x1D, "M2 T波轨迹",       "°",     true  },
    { 0x1E, "M2 主动极限角1",   "°",     true  },
    { 0x1F, "M2 主动极限角2",   "°",     true  },
    { 0x20, "M2 主动停顿时间",  "s",     true  },
    { 0x23, "M2 主动Fmax",      "N",     true  },
    { 0x24, "M2 主动刚度k",     "",      true  },
    // ── DR304 六维力传感器 ──────────────────────────
    { 0x41, "Fx",               "N",     false },
    { 0x42, "Fy",               "N",     false },
    { 0x43, "Fz",               "N",     false },
    { 0x44, "Mx",               "N·m",   false },
    { 0x45, "My",               "N·m",   false },
    { 0x46, "Mz",               "N·m",   false },
    // ── 遥测 (只读) ─────────────────────────────────
    { 0x21, "传感器角度",       "°",     false },
    { 0x22, "传感器电流",       "mA",    false },
    // ── IMU (只读) ──────────────────────────────────
    { 0x31, "IMU Roll",         "°",     false },
    { 0x32, "IMU Pitch",        "°",     false },
    { 0x33, "IMU Yaw",          "°",     false },
};

inline const int floatIDTableSize = sizeof(floatIDTable) / sizeof(floatIDTable[0]);

// 构建查找用 QMap
inline QMap<uint8_t, FloatIDInfo> buildFloatIDMap()
{
    QMap<uint8_t, FloatIDInfo> map;
    for (int i = 0; i < floatIDTableSize; ++i) {
        map.insert(floatIDTable[i].id, floatIDTable[i]);
    }
    return map;
}

// ── IO ID → 名称 ──────────────────────────────────────────
struct IOPinInfo {
    uint8_t id;
    const char *name;
};

inline const IOPinInfo ioPinTable[] = {
    { IO_M1_START_STOP, "M1 启停"       },
    { IO_M1_CTRL_MODE,  "M1 控制模式"    },
    { IO_M2_ENABLE,     "M2 启停"        },
    { IO_M2_RUN_MODE,   "M2 RS00 模式"   },
    { IO_M2_SET_ZERO,   "M2 机械零点"    },
    { IO_M2_CLR_FAULT,  "M2 清除故障"    },
    { IO_M2_TRAJ_MODE,  "M2 轨迹模式"    },
    { IO_M1_REHAB,      "M1 康复启停"    },
    { IO_M2_REHAB,      "M2 康复启停"    },
    { IO_SYS_START_STOP,"系统启停"       },
    { IO_M1_ACTIVE_REHAB,"M1 主动康复"   },
    { IO_FORCE_ZERO,     "力传感器清零"   },
    { IO_M2_ACTIVE_REHAB,"M2 主动康复"   },
    { IO_M2_ACTIVE_MODE, "M2 主动模式"    },
};

inline const int ioPinTableSize = sizeof(ioPinTable) / sizeof(ioPinTable[0]);

// ── 命令码 → 名称 ──────────────────────────────────────────
inline QString cmdName(uint8_t cmd)
{
    switch (cmd) {
    case CMD_IO_WRITE:    return QStringLiteral("IO_WRITE");
    case CMD_FLOAT_WRITE: return QStringLiteral("FLOAT_WRITE");
    case CMD_BATCH_WRITE: return QStringLiteral("BATCH_WRITE");
    case CMD_PING:        return QStringLiteral("PING");
    case CMD_FLASH_SAVE:  return QStringLiteral("FLASH_SAVE");
    case CMD_TELEMETRY:   return QStringLiteral("TELEMETRY");
    case CMD_IO_REPORT:   return QStringLiteral("IO_REPORT");
    case CMD_ACK:         return QStringLiteral("ACK");
    default:              return QStringLiteral("UNKNOWN(0x%1)").arg(cmd, 2, 16, QChar('0'));
    }
}

#endif // FLOATIDDEFS_H
