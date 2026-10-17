#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""逐字节位置分析：找出 error_frames 中稳定/可变的位置，判断损坏模式。"""
import re, struct
from collections import Counter

PATH = r"/mnt/c/Users/Lenovo/Desktop/腕部驱动设备/build/Desktop_Qt_6_8_2_MinGW_64_bit_Release/error_frames.txt"
pat = re.compile(r"raw=(.*)$")

raws = []
with open(PATH, "r", encoding="utf-8", errors="replace") as f:
    for line in f:
        m = pat.search(line.strip())
        if m:
            b = [int(x, 16) for x in m.group(1).split()]
            if len(b) == 97:
                raws.append(b)

n = len(raws)
print("帧数(97B):", n)

# 每字节位置的取值分布
print("\n=== 每个字节位置的取值多样性 (前 20 个最常见取值) ===")
print("off  uniq  常见取值")
for off in range(97):
    c = Counter(r[off] for r in raws)
    uniq = len(c)
    top = " ".join("%02X:%d" % (v, k) for v, k in c.most_common(8))
    print("%3d  %4d  %s" % (off, uniq, top))

# 校验和差值分布
print("\n=== (rxSum - calcSum) mod 256 分布 top10 ===")
d = Counter((r[-2] - ((r[2] + r[3] + sum(r[4:4+r[2]])) & 0xFF)) & 0xFF for r in raws)
for v, k in d.most_common(10):
    print("   %02X  %d" % (v, k))

# 是否所有帧在某个偏移上完全恒定
const = [off for off in range(97) if len(set(r[off] for r in raws)) == 1]
print("\n完全恒定的字节位置:", const, [("%02X" % raws[0][o]) for o in const])
