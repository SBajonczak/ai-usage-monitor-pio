#include <cassert>
#include <cstdint>
#include <vector>
#include "display_transform.h"
using namespace display_transform;

int main() {
  assert(valid_rotation(0) && valid_rotation(90) && valid_rotation(180) && valid_rotation(270));
  assert(!valid_rotation(45));
  // Non-square tile at a non-centered position; physical display is 466x466.
  Rect region = {24, 102, 29, 105}; // tile width 6, height 4
  for(int deg : {0,90,180,270}) {
    Rect r = rotate_area(region, 466, 466, deg);
    int w=6,h=4, dw = (deg==90 || deg==270)?h:w, dh=(deg==90 || deg==270)?w:h;
    assert(r.x2-r.x1+1 == dw);
    assert(r.y2-r.y1+1 == dh);
    std::vector<uint16_t> src(w*h), dst(w*h,0);
    for(int i=0; i<w*h; ++i) src[i] = (uint16_t)(0x1100 + i);
    rotate_tile_rgb565(src.data(),dst.data(),w,h,deg);
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
      int gx=region.x1+x, gy=region.y1+y;
      Point physical;
      switch(deg) {
        case 90: physical = {(int16_t)(465-gy),(int16_t)gx};break;
        case 180: physical = {(int16_t)(465-gx),(int16_t)(465-gy)};break;
        case 270:physical = {(int16_t)gy,(int16_t)(465-gx)};break;
        default: physical = {(int16_t)gx,(int16_t)gy};break;
      }
      assert(physical.x >= r.x1 && physical.x <= r.x2);
      assert(physical.y >= r.y1 && physical.y <= r.y2);
      int dx=physical.x-r.x1, dy=physical.y-r.y1;
      assert(dst[dy*dw+dx] == panel_rgb565(src[y*w+x]));
      Point logical=touch_to_logical(physical.x,physical.y,466,466,deg);
      assert(logical.x==gx && logical.y==gy);
    }
  }
  assert(panel_rgb565(0x1234) == 0x3412);
  assert(touch_to_logical(466,466,466,466,0).x==465);
}
