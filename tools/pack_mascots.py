"""Regenerate include/mascot_frames.h from assets/claude_{0..2}.png and
assets/jadebyte_{0..2}.png. Requires Pillow: pip install Pillow.

Images must be 120x120 RGBA. Output is uint32_t ARGB8888 (little-endian BGRA).
"""
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]
side=120
output=root/'include'/'mascot_frames.h'
with output.open('w',encoding='utf8') as f:
    f.write('#pragma once\n#include <stdint.h>\nnamespace mascot_frames {\nconstexpr int SIDE = 120;\nconstexpr int COUNT = 3;\n')
    for name in ('claude','jadebyte'):
        for i in range(3):
            im=Image.open(root/'assets'/f'{name}_{i}.png').convert('RGBA')
            if im.size!=(side,side): raise ValueError(f'{name}_{i} must be {side}x{side}')
            f.write(f'alignas(4) static const uint32_t {name}_{i}[SIDE*SIDE] = {{\n')
            pixels=list(im.getdata())
            for k in range(0,len(pixels),16):
                f.write('  '+', '.join('0x%08X'%((a<<24)|(r<<16)|(g<<8)|b) for (r,g,b,a) in pixels[k:k+16])+',\n')
            f.write('};\n')
    for name in ('claude','jadebyte'):
        f.write(f'static const uint32_t *const {name}[COUNT] = {{{", ".join(name+"_"+str(i) for i in range(3))}}};\n')
    f.write('} // namespace mascot_frames\n')
print('Wrote',output)
