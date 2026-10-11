"""Offline visualization of the dimensions and sprites used in v2.0.
This is a layout preview, not an ESP32 LVGL hardware screenshot.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import math
base=Path(__file__).resolve().parents[1]
assets=base/'assets'
try:
    font='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
    bold='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
except Exception: font=bold=None

def F(sz,heavy=False):
    try: return ImageFont.truetype(bold if heavy else font,sz)
    except Exception: return ImageFont.load_default()

def text_center(draw,at,text,size,color,heavy=False):
    bb=draw.textbbox((0,0),text,font=F(size,heavy))
    draw.text((at[0]-(bb[2]-bb[0])/2,at[1]-(bb[3]-bb[1])/2),text,font=F(size,heavy),fill=color)

def make(name,s,w):
    W=466
    scale=2
    bg=(8,13,21)
    canvas=Image.new('RGB',(W*scale,W*scale),bg)
    d=ImageDraw.Draw(canvas)
    c=W*scale//2
    color=(23,218,232) if name=='jadebyte' else (246,169,111)
    outer=(218,62,229) if name=='jadebyte' else (191,125,244)
    # match source ring radii 214,186, widths 15,14
    for radius,width,percent,col,start in [(214,15,w,outer,5),(186,14,s,color,224)]:
        r=radius*scale; wd=width*scale
        box=(c-r,c-r,c+r,c+r)
        d.arc(box,0,359,fill=(42,60,80),width=wd)
        # source 0=top clockwise
        d.arc(box,start-90,start-90+360*percent/100,fill=col,width=wd)
    text_center(d,(c,(233-142)*scale),'CODEX' if name=='jadebyte' else 'CLAUDE',24*scale,(245,247,255),True)
    im=Image.open(assets/f'{name}_0.png').convert('RGBA').resize((120*scale,120*scale),Image.Resampling.LANCZOS)
    canvas.paste(im,((233-60)*scale,(233-53-60)*scale),im)
    text_center(d,(c,(233+20)*scale),f'5H  {s}% FREI',28*scale,(246,247,252),True)
    card=(102*scale,(233+67-22)*scale,364*scale,(233+67+22)*scale)
    d.rounded_rectangle(card,radius=22*scale,fill=(21,30,45),outline=(52,69,94),width=2*scale)
    text_center(d,(167*scale,(233+67)*scale),'WOCHE',16*scale,(198,211,227))
    text_center(d,(300*scale,(233+67)*scale),f'{w}% FREI',18*scale,color,True)
    text_center(d,(c,(233+112)*scale),'RESET 5H 21:30  |  W MO 11:00',14*scale,(183,201,219))
    text_center(d,(c,(233+135)*scale),'DEMO  /  TIPPEN',14*scale,(130,149,174))
    text_center(d,(c,(233+158)*scale),'ONLINE   •   MQTT OK',14*scale,(142,164,186))
    return canvas.resize((466,466),Image.Resampling.LANCZOS)

claude=make('claude',72,57)
jade=make('jadebyte',46,35)
claude.save(assets/'claude-screen-preview.png')
jade.save(assets/'jadebyte-screen-preview.png')
combined=Image.new('RGB',(932,466),(8,13,21))
combined.paste(claude,(0,0)); combined.paste(jade,(466,0))
combined.save(assets/'v2-layout-preview.png')
