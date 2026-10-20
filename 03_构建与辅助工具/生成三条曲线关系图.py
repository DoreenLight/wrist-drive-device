#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成「三条曲线关系图」SVG（文字精确排版）。

用法（在项目根目录或任意位置执行）：
    python3 03_构建与辅助工具/生成三条曲线关系图.py
输出：
    00_项目说明/设备与数据说明/配图/三条曲线关系图.svg

如需再导出 PNG（本项目实测可用）：
    chrome --headless=new --hide-scrollbars --force-device-scale-factor=1 \
           --window-size=2100,1580 --screenshot=输出.png 上述.svg
"""
import os

W, H = 2100, 1580
GREEN = "#2CA02C"; GREEN_BG = "#E8F5E9"
BLUE  = "#1F77B4"; BLUE_BG  = "#E3F2FD"
ORANGE= "#FF7F0E"; ORANGE_BG= "#FFF3E0"
INK   = "#1F2328"; SUB = "#5A6672"; LINE = "#D0D7DE"; BG = "#FFFFFF"
FONT  = "Microsoft YaHei, PingFang SC, Noto Sans CJK SC, sans-serif"
MONO  = "Consolas, Courier New, monospace"

out = []
def add(s): out.append(s)

def esc(t):
    return (t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))

def char_w(ch, size):
    o = ord(ch)
    if o > 0x2E80: return size * 1.0
    if ch in "，。、；：「」（）【】～·": return size * 1.0
    if ch == " ": return size * 0.30
    return size * 0.56

def wrap(text, size, max_w):
    lines, cur, w = [], "", 0.0
    for ch in text:
        cw = char_w(ch, size)
        if w + cw > max_w and cur:
            lines.append(cur); cur, w = ch, cw
        else:
            cur += ch; w += cw
    if cur: lines.append(cur)
    return lines

def text(x, y, s, size=17, color=INK, bold=False, anchor="start", mono=False, italic=False):
    fam = MONO if mono else FONT
    wt = "700" if bold else "400"
    st = ' font-style="italic"' if italic else ""
    add(f'<text x="{x}" y="{y}" font-family="{fam}" font-size="{size}" fill="{color}" '
        f'font-weight="{wt}" text-anchor="{anchor}"{st}>{esc(s)}</text>')

def block(x, y, s, size=17, color=INK, max_w=600, lh=None, bold=False, mono=False, anchor="start"):
    lh = lh or int(size * 1.55)
    lines = wrap(s, size, max_w)
    for i, ln in enumerate(lines):
        text(x, y + i * lh, ln, size, color, bold, anchor, mono)
    return y + len(lines) * lh

def rect(x, y, w, h, fill=BG, stroke=LINE, sw=2, rx=12, dash=None):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" '
        f'stroke="{stroke}" stroke-width="{sw}"{d}/>')

def section(y, no, title, note=""):
    add(f'<rect x="40" y="{y}" width="8" height="34" rx="4" fill="{BLUE}"/>')
    text(62, y + 26, f"{no}  {title}", 26, INK, bold=True)
    if note: text(62 + 30 + len(title) * 26, y + 25, note, 16, SUB)

add(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">')
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')

# ── 标题 ────────────────────────────────────────────────────────────
text(40, 74, "腕部康复上位机 · 三条曲线关系图", 40, INK, bold=True)
text(40, 110, "参考轨迹（绿） · 轨迹输入（蓝） · 反馈角度（橙）　　依据：Qt 上位机源码 ＋ STM32F407 下位机固件 main.c 交叉确认",
     19, SUB)
add(f'<line x1="40" y1="126" x2="{W-40}" y2="126" stroke="{LINE}" stroke-width="2"/>')

# ── ① 控制链路 ──────────────────────────────────────────────────────
section(150, "①", "三条曲线分别在哪里产生（控制链路）")

boxes = [
    ("上位机 Qt", "Uart_controller.exe", [
        ("● 本地计算「参考轨迹」", GREEN),
        ("    纯标尺，**不下发**设备", SUB),
        ("· 下发：参数 + 启停开关", SUB),
        ("· 接收遥测，画三条曲线", SUB),
    ]),
    ("串口链路", "CH342 · COM13 · 230400", [
        ("下行：FLOAT_WRITE / IO_WRITE", SUB),
        ("上行：TELEMETRY（20 Hz）", SUB),
        ("⚠ 链路存在偶发误码", "#C62828"),
    ]),
    ("下位机 MCU", "STM32F407 · TIM2 100 Hz", [
        ("● Order_Creat_process()", BLUE),
        ("    生成「轨迹输入」In_val", BLUE),
        ("⚙ PID：(轨迹输入 − 反馈角度)", SUB),
        ("    → 输出 0x03 → PWM", SUB),
    ]),
    ("执行机构", "电机 + 万向节", [
        ("M1：TIM2 位置/速度环", SUB),
        ("M2：RS00 伺服（CAN）", SUB),
        ("实际带动腕关节运动", SUB),
    ]),
    ("传感器", "MPU6050 · DR304", [
        ("● IMU roll → 「反馈角度」", ORANGE),
        ("    0x02，PID 的反馈量", ORANGE),
        ("力/力矩 Fx..Mz → 主动康复", SUB),
    ]),
]
bx, bw, gap, by, bh = 40, 384, 24, 200, 250
for i, (title_t, sub_t, lines) in enumerate(boxes):
    x = bx + i * (bw + gap)
    rect(x, by, bw, bh, "#FBFCFD")
    add(f'<rect x="{x}" y="{by}" width="{bw}" height="6" rx="3" fill="{BLUE}"/>')
    text(x + 18, by + 44, title_t, 22, INK, bold=True)
    text(x + 18, by + 72, sub_t, 15, SUB)
    yy = by + 104
    for s, col in lines:
        b = s.startswith("    ")
        s2 = s.replace("**", "")
        yy = block(x + 18, yy, s2, 16, col, bw - 36, 25, bold=False) + 4
    if i < len(boxes) - 1:
        ax = x + bw + 4
        add(f'<path d="M {ax} {by+bh/2} L {ax+16} {by+bh/2}" stroke="{SUB}" stroke-width="2.5"/>')
        add(f'<path d="M {ax+12} {by+bh/2-6} L {ax+19} {by+bh/2} L {ax+12} {by+bh/2+6}" fill="{SUB}"/>')

text(40, 492, "🟢 参考轨迹只存在于上位机　　🔵 轨迹输入与 PID 只存在于下位机　　🟠 反馈角度来自 IMU，经遥测回到上位机", 17, SUB)

# ── ② 曲线示意 + 速查表 ─────────────────────────────────────────────
section(524, "②", "三条线长什么样（示意）", "＋ 速查表")

cx, cy, cw, ch = 40, 560, 900, 380
rect(cx, cy, cw, ch, "#FCFDFE")
# 坐标网格
for gy in range(int(cy + 60), int(cy + ch - 40), 60):
    add(f'<line x1="{cx+70}" y1="{gy}" x2="{cx+cw-30}" y2="{gy}" stroke="#EEF2F6" stroke-width="1"/>')
add(f'<line x1="{cx+70}" y1="{cy+ch/2}" x2="{cx+cw-30}" y2="{cy+ch/2}" stroke="#DCE3EA" stroke-width="1.5" stroke-dasharray="6 4"/>')
text(cx + 24, cy + ch/2 + 5, "0°", 14, SUB)
text(cx + 24, cy + 70, "+", 14, SUB)
text(cx + 24, cy + ch - 40, "−", 14, SUB)

p0, p1 = cx + 90, cx + cw - 40
mid, amp = cy + ch / 2, 118
def trap(shift, a):
    """梯形往复（含停顿）：停低 → 升 → 停高 → 降"""
    pts = []
    x = p0 + shift
    seq = [(50, -1), (70, +1), (80, +1), (60, -1)]
    for seg_len, dirn in seq:
        pass
    pts.append((x, mid + a)); x += 55
    pts.append((x, mid + a)); x += 78; pts.append((x, mid - a))
    x += 95;  pts.append((x, mid - a)); x += 78; pts.append((x, mid + a))
    x += 55;  pts.append((x, mid + a)); x += 78; pts.append((x, mid - a))
    x += 95;  pts.append((x, mid - a)); x += 78; pts.append((x, mid + a))
    x += 55;  pts.append((x, mid + a))
    return "M " + " L ".join(f"{px:.1f} {py:.1f}" for px, py in pts)

# 参考轨迹（绿，粗半透明带）
add(f'<path d="{trap(0, amp)}" fill="none" stroke="{GREEN}" stroke-width="11" '
    f'stroke-opacity="0.28" stroke-linejoin="round" stroke-linecap="round"/>')
add(f'<path d="{trap(0, amp)}" fill="none" stroke="{GREEN}" stroke-width="2.5" stroke-dasharray="8 6"/>')
# 轨迹输入（蓝）
add(f'<path d="{trap(22, amp*0.86)}" fill="none" stroke="{BLUE}" stroke-width="3.4" '
    f'stroke-linejoin="round" stroke-linecap="round"/>')
# 反馈角度（橙，滞后 + 圆角）
add(f'<path d="{trap(46, amp*0.82)}" fill="none" stroke="{ORANGE}" stroke-width="3.4" '
    f'stroke-linejoin="round" stroke-linecap="round" opacity="0.95"/>')

# 图例
lx, ly = cx + cw - 250, cy + 22
for i, (nm, col) in enumerate([("参考轨迹（上限/下限+停顿）", GREEN),
                               ("轨迹输入（被限速平滑）", BLUE),
                               ("反馈角度（滞后跟随）", ORANGE)]):
    add(f'<rect x="{lx}" y="{ly + i*26}" width="26" height="6" rx="3" fill="{col}"/>')
    text(lx + 36, ly + i * 26 + 8, nm, 14, SUB)
text(cx + 24, cy + ch - 12, "示意：非等比真实数据；绿色虚线为参考轨迹本身，实心部分是它的可视化强调", 13, SUB)

# 速查表（每格显式换行，避免自动断词）
tx, ty, tw = 976, 550, 1084
cols = [("颜色 / 名称", 160), ("谁生成", 240), ("是否下发", 124), ("关键作用", 300), ("依赖什么", 260)]
rh, hh = 108, 46
rect(tx, ty, tw, hh + rh * 3, "#FFFFFF")
add(f'<rect x="{tx}" y="{ty}" width="{tw}" height="{hh}" rx="10" fill="#F2F5F8"/>')
add(f'<rect x="{tx}" y="{ty+hh-12}" width="{tw}" height="12" fill="#F2F5F8"/>')
xx = tx + 16
for name, cwid in cols:
    text(xx, ty + 30, name, 16, SUB, bold=True)
    xx += cwid

def mlines(x, y, s, size, color, lh=21, bold=False, mono=False):
    for i, ln in enumerate(s.split("\n")):
        text(x, y + i * lh, ln, size, color, bold, "start", mono)

rows = [
    (GREEN, "🟢 参考轨迹",
     "上位机本地计算\nmakeRefTrajectory()\n（主动页参数+周期数）",
     "❌ 不下发",
     "评价标尺：「我以为应该\n这样动」。可把误差拆成\n①指令偏差 ②跟随偏差",
     "T波速度 / 极限角1·2\n停顿时间 / 周期数"),
    (BLUE, "🔵 轨迹输入\n(0x01)",
     "下位机 MCU\nOrder_Creat_process()\n100 Hz 更新",
     "✅ 是设定值",
     "M1 位置环唯一设定值\n（PID 的 In_val）；遥测\n0x01 上报；记录你的用力",
     "主动：Mx + Tmax + 刚度k\n被动：目标角 + 最大速度"),
    (ORANGE, "🟠 反馈角度\n(0x02)",
     "MPU6050 的 roll 值\nmain.c 直接赋给\nFeedback_val",
     "遥测上报",
     "PID 的反馈量，代表\n「实际在哪」；与轨迹\n输入平均差 1.51°",
     "腕关节真实机械角度"),
]
ry = ty + hh
for col, name, who, sent, use, dep in rows:
    add(f'<rect x="{tx}" y="{ry}" width="{tw}" height="{rh}" fill="#FFFFFF"/>')
    add(f'<rect x="{tx}" y="{ry}" width="6" height="{rh}" fill="{col}"/>')
    add(f'<line x1="{tx}" y1="{ry+rh}" x2="{tx+tw}" y2="{ry+rh}" stroke="#EDF1F5" stroke-width="1.5"/>')
    xx = tx + 18
    mlines(xx, ry + 44, name, 18, col, 22, bold=True)
    xx += cols[0][1]
    mlines(xx, ry + 36, who, 14, INK, 21)
    xx += cols[1][1]
    mlines(xx, ry + 36, sent, 14, "#C62828" if "❌" in sent else "#2E7D32", 21)
    xx += cols[2][1]
    mlines(xx, ry + 36, use, 14, INK, 21)
    xx += cols[3][1]
    mlines(xx, ry + 36, dep, 13.5, SUB, 20)
    ry += rh

# ── ③ 轨迹输入重点 ─────────────────────────────────────────────────
section(980, "③", "重点：「轨迹输入」在哪起作用、怎么算出来、从哪来")

# 左卡：作用
lx0, ly0, lw0, lh0 = 40, 1020, 660, 434
rect(lx0, ly0, lw0, lh0, BLUE_BG, BLUE, 2.5)
text(lx0 + 22, ly0 + 40, "🔵 它在哪起作用", 23, BLUE, bold=True)
y = ly0 + 78
items = [
    "**唯一设定值**：M1 位置环的 PID 输入量 In_val，100 Hz 刷新",
    "**控制公式**：偏差 = 轨迹输入 − 反馈角度 → PID → 输出(0x03) → PWM",
    "**遥测上报**：作为 0x01 发出（20 Hz），所以它就是设备「此刻准备去的位置」",
    "**人机交互产物**：主动康复时它记录的是「你的用力推动出来的位置」",
]
for it in items:
    add(f'<circle cx="{lx0+28}" cy="{y-6}" r="4" fill="{BLUE}"/>')
    y = block(lx0 + 42, y, it.replace("**", ""), 16, INK, lw0 - 70, 25) + 8
add(f'<rect x="{lx0+22}" y="{ly0+296}" width="{lw0-44}" height="112" rx="10" fill="#FFFFFF" stroke="{ORANGE}" stroke-width="2"/>')
text(lx0 + 40, ly0 + 326, "⚠ 关键澄清", 17, ORANGE, bold=True)
block(lx0 + 40, ly0 + 354, "T 波（0x0B）在任何模式下都不参与轨迹输入的生成，它只是遥测里那条绿色参照线。",
      16, INK, lw0 - 80, 24)

# 中卡：A 主动康复
mx0, my0, mw, mhh = 720, 1020, 680, 352
rect(mx0, my0, mw, mhh, "#FFFDF5", ORANGE, 2.5)
text(mx0 + 22, my0 + 38, "A  主动康复 —— 力顺应（导纳型）", 22, ORANGE, bold=True)
text(mx0 + 22, my0 + 66, "轨迹输入由【力矩 + Tmax + 刚度 k】积分而来", 16, SUB)
code_a = [
    "mx   = DR304[3];            // Mx 力矩 (N·m)",
    "tmax = actv_tmax;  k = actv_stiffness;",
    "if (fabs(mx) > tmax)",
    "    v = -sign(mx)*(fabs(mx)-tmax)/k;   // 超限÷k",
    "else  v = 0.0f;                       // 冻结",
    "traj += v * 0.01f;                    // 积分",
    "Control_P_PID.In_val = traj;          // ← 0x01",
]
add(f'<rect x="{mx0+20}" y="{my0+84}" width="{mw-40}" height="{len(code_a)*21+20}" rx="8" fill="#2B2F36"/>')
for i, ln in enumerate(code_a):
    text(mx0 + 34, my0 + 108 + i * 21, ln, 15, "#E6EDF3", mono=True)
y2 = my0 + 84 + len(code_a) * 21 + 42
for s in ["|Mx| ≤ Tmax → 速度 0 → 轨迹冻结（轻碰不动的门槛）",
          "用力越大走得越快；k 越大同样力矩越慢 → 越「硬」"]:
    add(f'<circle cx="{mx0+28}" cy="{y2-6}" r="4" fill="{ORANGE}"/>')
    y2 = block(mx0 + 42, y2, s, 15.5, INK, mw - 70, 24) + 6

# 右卡：B 被动 / C 手动
rx0, ry0, rw, rhh = 1420, 1020, 640, 352
rect(rx0, ry0, rw, rhh, "#F7FCF8", GREEN, 2.5)
text(rx0 + 22, ry0 + 38, "B  被动康复 / C  手动调试", 22, GREEN, bold=True)
text(rx0 + 22, ry0 + 66, "轨迹输入由【目标角度 + 最大速度 + 固定加速度】规划而来", 16, SUB)
code_b = [
    "target = 康复最大/最小角度（状态机）",
    "       / 上位机「角度目标」（手动）",
    "error  = target - traj;",
    "v_max  = min(最大速度, sqrt(2*0.1*dist));",
    "加速度恒为 0.1 °/周期²，速度限幅后累加",
    "到 |error|<0.05 → 切换目标",
]
add(f'<rect x="{rx0+20}" y="{ry0+84}" width="{rw-40}" height="{len(code_b)*21+20}" rx="8" fill="#2B2F36"/>')
for i, ln in enumerate(code_b):
    text(rx0 + 34, ry0 + 108 + i * 21, ln, 15, "#E6EDF3", mono=True)
text(rx0 + 22, ry0 + 262, "→ 不依赖力反馈，也不依赖角度反馈", 16, GREEN, bold=True)

# 结论条
kx, ky, kw, kh = 720, 1388, 1340, 140
rect(kx, ky, kw, kh, "#F2F5F8", SUB, 2)
text(kx + 22, ky + 34, "一句话总结", 19, INK, bold=True)
y3 = ky + 62
y3 = block(kx + 22, y3, "轨迹输入 = Control_P_PID.In_val = m1_traj_current（100 Hz）：主动康复 = 由 Mx + Tmax + 刚度 k 积分而来；被动/手动 = 由目标角度 + 最大速度 + 0.1°/周期² 规划而来。",
           15.5, INK, kw - 44, 24)
block(kx + 22, y3 + 2, "两个「误差」别混：控制内环偏差 = 轨迹输入 − 反馈角度（固件内部）；界面显示误差 = 参考轨迹 − 反馈角度（上位机读数行）。",
      15.5, "#C62828", kw - 44, 24)

text(40, 1556, "生成依据：03-HostApp/01_SerialConsole（Qt 上位机）＋ 02-Firmware/01_MotorBoard/Core/Src/main.c（Order_Creat_process / Motor1_dingdian_control_process）",
     14, SUB)

add("</svg>")

dst = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "..", "00_项目说明", "设备与数据说明", "配图", "三条曲线关系图.svg")
dst = os.path.normpath(dst)
with open(dst, "w", encoding="utf-8") as f:
    f.write("\n".join(out))
print("written:", dst, len("\n".join(out)), "bytes")
