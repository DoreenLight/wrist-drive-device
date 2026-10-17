#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从遥测 CSV 反推 T 波（参考轨迹）的波形、幅值、周期、停顿，回答“5 个周期多长/是否连续”。"""
import csv, glob, os, math

DIR = r"/mnt/c/Users/Lenovo/Desktop/腕部驱动设备/02_采集数据/telemetry"

def tsec(s):
    hh, mm, ss = s.split(":")
    return int(hh)*3600 + int(mm)*60 + float(ss)

KEYS = ["M1 轨迹输入", "M1 反馈角度", "T波虚拟轨迹", "M2 T波轨迹", "M2 规划位置", "M2 实际位置"]

for path in sorted(glob.glob(os.path.join(DIR, "*.csv"))):
    with open(path, encoding="utf-8-sig", newline="") as f:
        rd = csv.DictReader(f)
        cols = rd.fieldnames or []
        rows = list(rd)
    if "M1 轨迹输入" not in cols:
        print("跳过(非宽表): %s  cols=%s" % (os.path.basename(path), cols[:8]))
        continue
    key = "M1 轨迹输入"
    ts, vs = [], []
    for r in rows:
        v = (r.get(key) or "").strip()
        if v == "":
            continue
        ts.append(tsec(r["时间"])); vs.append(float(v))
    if len(ts) < 10:
        continue
    print("=" * 72)
    print("%s  N=%d  时长=%.2fs  采样率=%.1f Hz" % (os.path.basename(path), len(ts), ts[-1]-ts[0], len(ts)/(ts[-1]-ts[0])))
    # 单调段(升降沿)划分 + 转折点
    n = len(vs)
    # 以 0.5° 阈值找局部极值
    turns = []   # (t, v, type)
    thr = 0.3
    i = 0
    direction = 0
    last_ext = vs[0]
    last_ext_t = ts[0]
    for i in range(1, n):
        dv = vs[i] - last_ext
        if direction >= 0 and dv > thr:
            if direction > 0:
                pass
            direction = 1
        elif direction <= 0 and dv < -thr:
            direction = -1
        else:
            continue
        if direction == 1 and dv < -thr:
            pass
    # 简化：直接用斜率符号变化 + 阈值去抖
    ext = []
    for i in range(1, n-1):
        if vs[i] >= vs[i-1] and vs[i] > vs[i+1]:
            ext.append((ts[i], vs[i], "max"))
        elif vs[i] <= vs[i-1] and vs[i] < vs[i+1]:
            ext.append((ts[i], vs[i], "min"))
    # 去掉时间上过近的重复极值
    ext2 = []
    for e in ext:
        if ext2 and e[0] - ext2[-1][0] < 0.15:
            if e[2] == ext2[-1][2]:
                # 保留更极端的
                if (e[2] == "max" and e[1] > ext2[-1][1]) or (e[2] == "min" and e[1] < ext2[-1][1]):
                    ext2[-1] = e
            continue
        ext2.append(e)
    print("极值点数(去抖后): %d" % len(ext2))
    print("前 24 个极值点  时间(s) 值(°) 类型")
    for e in ext2[:24]:
        print("   %8.3f  %8.2f  %s" % e)
    # 峰谷幅值
    mx = [v for _, v, ty in ext2 if ty == "max"]
    mn = [v for _, v, ty in ext2 if ty == "min"]
    if mx: print("波峰: n=%d 均值=%.2f° 范围 %.2f..%.2f" % (len(mx), sum(mx)/len(mx), min(mx), max(mx)))
    if mn: print("波谷: n=%d 均值=%.2f° 范围 %.2f..%.2f" % (len(mn), sum(mn)/len(mn), min(mn), max(mn)))
    # 相邻同类型极值间隔 = 周期
    per = [round(ext2[i+2][0]-ext2[i][0], 3) for i in range(len(ext2)-2) if ext2[i][2] == ext2[i+2][2]]
    if per:
        print("相邻同向极值间隔(即周期) n=%d: %s" % (len(per), per[:20]))
        print("   周期 中位数=%.3f s 均值=%.3f s" % (sorted(per)[len(per)//2], sum(per)/len(per)))
    # 上升沿/下降沿耗时
    ups, dns, pauses = [], [], []
    for a, b in zip(ext2, ext2[1:]):
        dt = b[0]-a[0]
        if a[2] == "min" and b[2] == "max":
            ups.append(dt)
        elif a[2] == "max" and b[2] == "min":
            dns.append(dt)
    if ups: print("上升沿(-→+)耗时 均值=%.3f s  n=%d" % (sum(ups)/len(ups), len(ups)))
    if dns: print("下降沿(+→-)耗时 均值=%.3f s  n=%d" % (sum(dns)/len(dns), len(dns)))
    # 中段停顿检测：在极值后保持不动的时间
    print("数值停留检测(相邻样本 |Δ|<0.05 且持续>0.3s 的片段):")
    segs = []
    st = None
    for i in range(1, n):
        if abs(vs[i]-vs[i-1]) < 0.05:
            if st is None: st = i-1
        else:
            if st is not None and ts[i-1]-ts[st] > 0.3:
                segs.append((ts[st], ts[i-1], vs[st]))
            st = None
    if st is not None and ts[-1]-ts[st] > 0.3:
        segs.append((ts[st], ts[-1], vs[st]))
    print("   共 %d 段" % len(segs))
    for s in segs[:20]:
        print("   %8.3f → %8.3f  (%.2fs) 值=%.2f°" % (s[0], s[1], s[1]-s[0], s[2]))
    print()
