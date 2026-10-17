#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""三条曲线的定量关系：谁领先谁、相位差、幅值、单位是否一致。"""
import csv, os, math

DIR = r"/mnt/c/Users/Lenovo/Desktop/腕部驱动设备/02_采集数据/telemetry"

def tsec(s):
    hh, mm, ss = s.split(":")
    return int(hh)*3600 + int(mm)*60 + float(ss)

for name in ["telemetry_20260727_163615.csv", "telemetry_20260727_164609.csv",
             "telemetry_20260727_164733.csv"]:
    path = os.path.join(DIR, name)
    if not os.path.exists(path):
        continue
    with open(path, encoding="utf-8-sig", newline="") as f:
        rows = list(csv.DictReader(f))
    cols = list(rows[0].keys())
    tgt = [c for c in cols if c in ("M1 轨迹输入", "M1 反馈角度", "T波虚拟轨迹", "M2 规划位置", "M2 实际位置", "M2 T波轨迹")]
    if len(tgt) < 2:
        continue
    print("=" * 74)
    print(name, " 可用列:", tgt)
    # 对齐成统一时间栅格
    ts = [tsec(r["时间"]) for r in rows]
    t0 = ts[0]
    grid_t = [t - t0 for t in ts]
    data = {}
    for c in tgt:
        vals = []
        for r in rows:
            v = (r.get(c) or "").strip()
            vals.append(float(v) if v else float("nan"))
        data[c] = vals

    def corr(a, b):
        n = 0; sa = 0.0; sb = 0.0; saa = 0.0; sbb = 0.0; sab = 0.0
        for x, y in zip(a, b):
            if math.isnan(x) or math.isnan(y): continue
            n += 1; sa += x; sb += y; saa += x*x; sbb += y*y; sab += x*y
        if n < 5: return float("nan"), n
        cov = sab/n - (sa/n)*(sb/n)
        va = saa/n - (sa/n)**2; vb = sbb/n - (sb/n)**2
        if va <= 0 or vb <= 0: return float("nan"), n
        return cov/math.sqrt(va*vb), n

    print("\n单位/量级对比 (判断是否同一物理量)：")
    for c in tgt:
        v = [x for x in data[c] if not math.isnan(x)]
        if not v: continue
        print("   %-12s n=%4d  min=%9.3f  max=%9.3f  span=%8.3f" % (c, len(v), min(v), max(v), max(v)-min(v)))

    # 互相关：求最佳时移
    print("\n两两最佳时移 (正=第二个滞后于第一个)：")
    for i, a in enumerate(tgt):
        for b in tgt[i+1:]:
            best = (None, -2.0)
            for lag in range(-30, 31):
                aa = []; bb = []
                for k in range(len(ts)):
                    j = k + lag
                    if 0 <= j < len(ts):
                        aa.append(data[a][k]); bb.append(data[b][j])
                c, n = corr(aa, bb)
                if not math.isnan(c) and c > best[1]:
                    best = (lag, c)
            # 采样间隔中位数
            gaps = sorted(round((ts[i+1]-ts[i])*1000) for i in range(len(ts)-1))
            dt = gaps[len(gaps)//2]/1000.0 if gaps else 0
            print("   %-12s vs %-12s  最佳lag=%s 样本(%.3fs)  r=%.4f" % (a, b, best[0], dt*(best[0] or 0), best[1]))

    # 误差统计
    pairs = [("T波虚拟轨迹", "M1 轨迹输入"), ("T波虚拟轨迹", "M1 反馈角度"),
             ("M1 轨迹输入", "M1 反馈角度"), ("M2 规划位置", "M2 实际位置"),
             ("M2 T波轨迹", "M2 规划位置")]
    print("\n差值统计 (a-b)：")
    for a, b in pairs:
        if a not in data or b not in data: continue
        d = [x-y for x, y in zip(data[a], data[b]) if not (math.isnan(x) or math.isnan(y))]
        if not d: continue
        mean = sum(d)/len(d)
        sd = (sum((x-mean)**2 for x in d)/len(d))**0.5
        print("   %-12s - %-12s  n=%4d 均值=%8.3f 标准差=%7.3f 最大|差|=%8.3f" % (
            a, b, len(d), mean, sd, max(abs(x) for x in d)))
    print()
