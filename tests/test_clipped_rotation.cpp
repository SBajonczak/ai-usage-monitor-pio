#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <cmath>
#include <algorithm>
#include "clipped_rotation.h"
int main() {
  const int w=466,h=466;
  std::vector<uint16_t> src(w*h), original(w*h), latest(w*h);
  for(int y=0;y<h;++y) for(int x=0;x<w;++x)
    src[y*w+x] = (uint16_t)(((x*31/(w-1))<<11) | ((y*63/(h-1))<<5) | ((x*y)%32));
  for(float a: {0.f,0.1f,0.5f,13.25f,30.f,44.9f,45.f,89.5f,90.f,
                135.5f,179.5f,180.f,230.1f,270.f,315.1f,359.8f}) {
    fast_rotation::rotate_rgb565(src.data(),original.data(),w,h,a,false);
    clipped_rotation::render_rgb565(src.data(),latest.data(),w,h,a,false);
    int different=0;
    for(int i=0;i<w*h;++i) if(original[i] != latest[i]) ++different;
    printf("a=%6.2f diff=%d / %d\n",a,different,w*h);
    // Should be identical because exact same coordinates as v1.8 (except
    // rare fixed-rounding at the last pixel on mathematical edges).
    assert(different < 120);
    clipped_rotation::render_rgb565(src.data(),latest.data(),w,h,a,true);
    fast_rotation::rotate_rgb565(src.data(),original.data(),w,h,a,true);
    assert(original == latest);
  }
  puts("clipped rotation reference tests OK");
}
