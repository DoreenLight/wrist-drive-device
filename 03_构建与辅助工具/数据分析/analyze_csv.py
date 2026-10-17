#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""分析已采集的遥测 CSV：三条曲线的真实数值/周期特性，为需求整理提供数据依据。"""
import csv, glob, os
from collections import defaultdict

DIR = r"/mnt/c/Users/Lenovo/Desktop/腕部驱动设备/02_采集数据/telemetry"

for path in sorted(glob.glob(os.path.join(DIR, "*.csv"))):
    print("=" * 70)
    print(os.path.basename(path))
    with open(path, encoding="utf-8-sig", newline="") as f:
        rd = csv.DictReader(f)
        cols = rd.fieldnames
        rows = list(rd)
    print("列 (%d): %s" % (len(cols), cols))
    print("行数:", len(rows))
    # 每列有效样本数与数值范围
    print("%-16s %6s %12s %12s %10s %10s" % ("列", "样本", "min", "max", "均值", "标准差"))
    for c in cols[1:]:
        vals = []
        for r in rows:
            v = (r.get(c) or "").strip()
            if v == "":
                continue
            try:
                vals.append(float(v))
            except ValueError:
                pass
        if not vals:
            print("%-16s %6d %12s %12s %10s %10s" % (c, 0, "-", "-", "-", "-"))
            continue
        mean = sum(vals)/len(vals)
        var = sum((x-mean)**2 for x in vals)/len(vals)
        print("%-16s %6d %12.4f %12.4f %10.4f %10.4f" % (c, len(vals), min(vals), max(vals), mean, var**0.5))

    # 采样间隔统计
    ts = []
    for r in rows:
        t = (r.get("时间") or "").strip()
        if not t:
            continue
        try:
            hh, mm, ss = t.split(":")
            ts.append(int(hh)*3600 + int(mm)*60 + float(ss))
        except Exception:
            pass
    if len(ts) > 2:
        gaps = [round((ts[i+1]-ts[i])*1000) for i in range(len(ts)-1)]
        gaps = [g for g in gaps if g >= 0]
        print("时长: %.2f s, 平均间隔: %.1f ms, 间隔中位数: %d ms" % (
            ts[-1]-ts[0], sum(gaps)/len(gaps) if gaps else 0,
            sorted(gaps)[len(gaps)//2] if gaps else 0))

    # 关键三列：M1 轨迹输入 / M1 反馈角度 / T波虚拟轨迹
    keys = ["M1 轨迹输入", "M1 反馈角度", "T波虚拟轨迹", "M1 PID 输出", "M2 T波轨迹"]
    shown = [k for k in keys if k in cols]
    if shown:
        print("\n--- 前 30 个非空样本 (%s) ---" % ", ".join(shown))
        print("      t(s)  " + "".join("%18s" % k for k in shown))
        cnt = 0
        for i, r in enumerate(rows):
            vals = [(r.get(k) or "").strip() for k in shown]
            if all(v == "" for v in vals):
                continue
            t = (rows[i].get("时间") or "").strip()
            try:
                hh, mm, ss = t.split(":")
                tt = int(hh)*3600+int(mm)*60+float(ss)
            except Exception:
                tt = 0
            print("%10.3f  " % tt + "".join("%18s" % (v if v else "-") for v in vals))
            cnt += 1
            if cnt >= 30:
                break
    print()
