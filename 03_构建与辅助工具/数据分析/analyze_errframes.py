#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""分析 error_frames.txt（校验失败帧），检验是否存在系统性帧/校验定义问题。"""
import re, sys, struct
from collections import Counter

PATH = r"/mnt/c/Users/Lenovo/Desktop/腕部驱动设备/build/Desktop_Qt_6_8_2_MinGW_64_bit_Release/error_frames.txt"

pat = re.compile(r"\[chk_fail\] rxSum=0x([0-9a-fA-F]{1,2}) calcSum=0x([0-9a-fA-F]{1,2}) len=(\d+) cmd=0x([0-9a-fA-F]{1,2}) raw=(.*)$")

frames = []
with open(PATH, "r", encoding="utf-8", errors="replace") as f:
    for line in f:
        m = pat.search(line.strip())
        if not m:
            continue
        rx, calc, ln, cmd, raw = m.groups()
        b = [int(x, 16) for x in raw.split()]
        frames.append(dict(rx=int(rx,16), calc=int(calc,16), len=int(ln), cmd=int(cmd,16), raw=b))

print("解析帧数:", len(frames))
print("raw 长度分布:", Counter(len(f['raw']) for f in frames))
print("len 分布:", Counter(f['len'] for f in frames))
print("cmd 分布:", Counter(hex(f['cmd']) for f in frames))
print("rxSum==calcSum 的帧数:", sum(1 for f in frames if f['rx']==f['calc']))

# 校验：应用自己算的 calcSum 是否等于文档公式
def doc_sum(raw):
    LEN, CMD = raw[2], raw[3]
    payload = raw[4:4+LEN]
    return (LEN + CMD + sum(payload)) & 0xFF

same = sum(1 for f in frames if doc_sum(f['raw']) == f['calc'])
print("calcSum 与文档公式一致:", same, "/", len(frames))

# 检查 raw 长度与 LEN+6
bad_len = [f for f in frames if len(f['raw']) != f['len']+6]
print("raw 长度 != LEN+6 的帧数:", len(bad_len))

# 各种备选校验公式，看哪种能得到 rxSum
def variants(raw):
    LEN, CMD = raw[2], raw[3]
    payload = raw[4:4+LEN]
    end = raw[-1]
    yield "len+cmd+payload", (LEN + CMD + sum(payload)) & 0xFF
    yield "cmd+payload", (CMD + sum(payload)) & 0xFF
    yield "payload", sum(payload) & 0xFF
    yield "len+cmd+payload+end", (LEN + CMD + sum(payload) + end) & 0xFF
    yield "xor(payload)", __import__("functools").reduce(lambda a,b:a^b, payload, 0)
    yield "xor(len,cmd,payload)", __import__("functools").reduce(lambda a,b:a^b, [LEN,CMD]+payload, 0)
    yield "~(payload)&0xff", (~sum(payload)) & 0xFF
    yield "sum(payload[1:])", sum(payload[1:]) & 0xFF
    yield "len+cmd+payload[:-1]", (LEN + CMD + sum(payload[:-1])) & 0xFF

cnt = Counter()
for f in frames:
    for name, v in variants(f['raw']):
        if v == f['rx']:
            cnt[name] += 1
print("各备选校验公式命中 rxSum 的次数 (总帧数 %d):" % len(frames))
for k,v in cnt.most_common():
    print("   %-22s %6d  (%.2f%%)" % (k, v, 100.0*v/len(frames)))

# 逐对解码（假设 count + (id,f32)*）
ids = {0x01:"M1轨迹输入",0x02:"M1反馈角度",0x03:"M1 PID输出",0x04:"M1 Kp",0x05:"M1 Ki",
       0x06:"M1 PWM",0x07:"M1最大速度",0x08:"M1康复最小",0x09:"M1康复最大",0x0A:"M1循环次数",
       0x0B:"T波虚拟轨迹",0x11:"M2规划位置",0x12:"M2实际位置",0x13:"M2规划速度",
       0x14:"M2实际速度",0x15:"M2力矩",0x31:"IMU Roll",0x32:"IMU Pitch",0x33:"IMU Yaw",
       0x41:"Fx",0x42:"Fy",0x43:"Fz",0x44:"Mx",0x45:"My",0x46:"Mz",0x21:"传感器角度",0x22:"传感器电流"}

print("\n=== 前 3 帧按 count+(id,f32) 解码 ===")
for f in frames[:3]:
    raw = f['raw']; LEN = raw[2]; payload = raw[4:4+LEN]
    cntb = payload[0]
    print("帧: count=%d, 期望对数=%d, 实际字节=%d" % (cntb, (LEN-1)//5, LEN-1))
    body = payload[1:]
    for i in range(0, len(body)-4, 5):
        fid = body[i]
        v = struct.unpack("<f", bytes(body[i+1:i+5]))[0]
        print("   id=0x%02X %-12s = %g" % (fid, ids.get(fid, "?"), v))
    print("   --")
