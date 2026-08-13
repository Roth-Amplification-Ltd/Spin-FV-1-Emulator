#!/usr/bin/env python3
import math, struct, sys, wave
path = sys.argv[1]
rate = 32768
frames = rate * 2
with wave.open(path, 'wb') as w:
    w.setnchannels(2)
    w.setsampwidth(2)
    w.setframerate(rate)
    for i in range(frames):
        # A repeatable guitar-ish two-tone stimulus, intentionally well below full scale.
        x = 0.22*math.sin(2*math.pi*220*i/rate) + 0.10*math.sin(2*math.pi*660*i/rate)
        y = 0.20*math.sin(2*math.pi*220*i/rate + 0.3)
        w.writeframesraw(struct.pack('<hh', int(x*32767), int(y*32767)))
