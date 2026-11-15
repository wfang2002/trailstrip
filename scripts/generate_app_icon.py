"""Render the dashboard's code-native front Jeep as an opaque iOS app icon.

Requires clang++ and Pillow. Uses front() directly from vehicle.cpp so the
app icon follows the same artwork as the dashboard, without a screenshot crop.
"""
from pathlib import Path
import json
import subprocess
import tempfile
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'main/vehicle.cpp').read_text()
front = source[source.index('void front('):source.index('void rear(')]
renderer = r'''
#include <iostream>
#include <initializer_list>
#include <cmath>
#include <cstdint>
struct Point {float x,y;};
struct Painter {
 void line(Point a,Point b,uint32_t color=0xffb547,int width=1) const {
  std::cout<<a.x<<" "<<a.y<<" "<<b.x<<" "<<b.y<<" "<<color<<" "<<width<<"\n";
 }
 void path(std::initializer_list<Point> ps,int width=1) const {
  auto p=ps.begin();for(auto q=p+1;q!=ps.end();++q){line(*p,*q,0xffb547,width);p=q;}
 }
 void box(float x,float y,float w,float h,int width=1) const {
  path({{x,y},{x+w,y},{x+w,y+h},{x,y+h},{x,y}},width);
 }
 void circle(float x,float y,float r,int width=1) const {
  Point p{x+r,y};for(int i=1;i<=96;i++){
   float a=i*6.2831853f/96;Point q{x+r*cosf(a),y+r*sinf(a)};
   line(p,q,0xffb547,width);p=q;
  }
 }
};
'''
with tempfile.TemporaryDirectory(prefix='trailstrip-icon-') as tmp:
    cpp, exe = Path(tmp)/'icon.cpp', Path(tmp)/'icon'
    cpp.write_text(renderer + front + '\nint main(){front(Painter{});}\n')
    subprocess.run(['clang++','-std=c++17',str(cpp),'-o',str(exe)],check=True)
    lines = subprocess.check_output([str(exe)],text=True).splitlines()

# Full opaque square; iOS applies its own rounded-corner mask.
aa, scale = 3, 7.0
canvas = Image.new('RGB',(1024*aa,1024*aa),(13,16,19))
draw = ImageDraw.Draw(canvas)
for line in lines:
    x1,y1,x2,y2,color,width = map(float,line.split())
    points = [((512+x*scale)*aa,(512+y*scale)*aa) for x,y in [(x1,y1),(x2,y2)]]
    color = int(color)
    rgb = ((color>>16)&255,(color>>8)&255,color&255)
    thickness = round(width*scale*aa)
    draw.line(points,fill=rgb,width=thickness)
    r = thickness/2
    for x,y in points:
        draw.ellipse((x-r,y-r,x+r,y+r),fill=rgb)
image = canvas.resize((1024,1024),Image.Resampling.LANCZOS)
assets = ROOT/'ios/Trailstrip/Assets.xcassets'
icons = assets/'AppIcon.appiconset'
icons.mkdir(parents=True,exist_ok=True)
image.save(icons/'AppIcon.png')
(assets/'Contents.json').write_text(json.dumps({'info':{'author':'xcode','version':1}},indent=2)+'\n')
(icons/'Contents.json').write_text(json.dumps({'images':[{'filename':'AppIcon.png','idiom':'universal','platform':'ios','size':'1024x1024'}],'info':{'author':'xcode','version':1}},indent=2)+'\n')
print(icons/'AppIcon.png')
