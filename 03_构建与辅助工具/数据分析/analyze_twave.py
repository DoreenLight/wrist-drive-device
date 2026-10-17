#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""聚焦分析 telemetry_20260727_164733.csv：绿色参考轨迹(0x0B T波虚拟轨迹)的真实波形/周期/停顿。"""
import csv, os

P = r"/mnt/c/Users/Lenovo/Desktop/腕部驱动设备/02_采集数据/telemetry/telemetry_20260727_164733.csv"

def tsec(s):
    hh, mm, ss = s.split(":")
    return int(hh)*3600+int(mm)*60+float(ss)

with open(P, encoding="utf-8-sig", newline="") as f:
    rows = list(csv.DictReader(f))

A = "T波虚拟轨迹"     # 绿 0x0B
B = "M1 轨迹输入"     # 蓝 0x01
C = "M1 反馈角度"     # 橙 0x02

data = []
for r in rows:
    try:
        t = tsec(r["时间"])
    except Exception:
        continue
    def g(k):
        v = (r.get(k) or "").strip()
        return float(v) if v else None
    data.append((t, g(A), g(B), g(C)))

t0 = data[0][0]
data = [(t-t0, a, b, c) for t, a, b, c in data]

print("总时长 %.2f s, 样本 %d" % (data[-1][0], len(data)))
print("\n=== 每 0.25 s 抽样：时间 | 绿(参考) | 蓝(轨迹输入) | 橙(反馈) | 蓝-绿 | 橙-绿 ===")
print("%8s %10s %10s %10s %9s %9s" % ("t(s)", "绿(°)", "蓝(°)", "橙(°)", "蓝-绿", "橙-绿"))
next_t = 0.0
for t, a, b, c in data:
    if t + 1e-9 < next_t:
        continue
    def f(x):
        return "%10.2f" % x if x is not None else "%10s" % "-"
    d1 = "%9.2f" % (b-a) if (a is not None and b is not None) else "%9s" % "-"
    d2 = "%9.2f" % (c-a) if (a is not None and c is not None) else "%9s" % "-"
    print("%8.2f %s %s %s %s %s" % (t, f(a), f(b), f(c), d1, d2))
    next_t = t + 0.25

# 绿色参考轨迹的转折点/停顿
print("\n=== 绿色参考轨迹 0x0B 的跳变/转折点 ===")
prevt, prevv = data[0][0], data[0][1]
turns = []
for t, a, b, c in data[1:]:
    if a is None or prevv is None:
        prevt, prevv = t, a; continue
    pass
# 分段线性近似：找斜率变化点
segs = []
cur_start = data[0][0]; cur_val = data[0][1]
last_slope = None
pts = [(t, a) for t, a, b, c in data if a is not None]
print("绿色轨迹采样点数:", len(pts))
for i in range(1, len(pts)):
    dt = pts[i][0]-pts[i-1][0]
    if dt <= 0:
        continue
    s = (pts[i][1]-pts[i-1][1])/dt
    if last_slope is None or abs(s-last_slope) > 0.5:
        segs.append((pts[i-1][0], pts[i-1][1], round(s, 2)))
        last_slope = s
print("斜率变化点 (t, 值, 新斜率 °/s):")
for s in segs:
    print("   %8.3f  %8.2f  %8.2f" % s)
# 停留段
print("\n绿色轨迹停留段 (|Δ|<0.01 持续 >0.2s):")
st = None
for i in range(1, len(pts)):
    if abs(pts[i][1]-pts[i-1][1]) < 0.01:
        if st is None: st = i-1
    else:
        if st is not None and pts[i-1][0]-pts[st][0] > 0.2:
            print("   %8.3f → %8.3f  (%.2fs)  值=%.2f°" % (pts[st][0], pts[i-1][0], pts[i-1][0]-pts[st][0], pts[st][1]))
        st = None
if st is not None and pts[-1][0]-pts[st][0] > 0.2:
    print("   %8.3f → %8.3f  (%.2fs)  值=%.2f°" % (pts[st][0], pts[-1][0], pts[-1][0]-pts[st][0], pts[st][1]))
