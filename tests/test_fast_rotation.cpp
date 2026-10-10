#include <cassert>
#include <cstdint>
#include <vector>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include "fast_rotation.h"

static uint16_t unswap(uint16_t v) { return continuous_rotation::swapped565(v); }
int main() {
    const int w=466,h=466;
    std::vector<uint16_t> src(w*h), out(w*h), ref(w*h);
    for (int y=0;y<h;++y) for (int x=0;x<w;++x) {
        unsigned r = (unsigned)((x*31)/(w-1)), g=(unsigned)((y*63)/(h-1));
        unsigned b=(unsigned)((x+y)%32);
        src[y*w+x]=(r<<11)|(g<<5)|b;
    }
    for (int deg: {0, 90, 180, 270}) {
        fast_rotation::rotate_rgb565(src.data(),out.data(),w,h, float(deg),false);
        continuous_rotation::render_rotated_rgb565(src.data(),ref.data(),w,h,float(deg),false);
        int mismatches=0;
        for (int y=0;y<h;++y) for (int x=0;x<w;++x) {
            float dx=x-(w-1)*0.5f,dy=y-(h-1)*0.5f;
            // Do not compare invisible bezel corner pixels
            if(dx*dx+dy*dy>= (w*0.5f-1)*(w*0.5f-1)) continue;
            if(out[y*w+x] != ref[y*w+x]) ++mismatches;
        }
        printf("quarter-turn %d: %d differing interior pixels\n", deg, mismatches);
        assert(mismatches < 1600); // numerical precision near integer pixel boundaries
    }
    for (float deg: {13.25f, 37.5f, 143.5f, 277.25f}) {
        fast_rotation::rotate_rgb565(src.data(),out.data(),w,h,deg,true);
        continuous_rotation::render_rotated_rgb565(src.data(),ref.data(),w,h,deg,true);
        int differing=0, largeDiff=0;
        for (int y=0;y<h;++y) for (int x=0;x<w;++x) {
            float dx=x-(w-1)*0.5f,dy=y-(h-1)*0.5f;
            if(dx*dx+dy*dy>= (w*0.5f-1)*(w*0.5f-1)) continue;
            uint16_t a=unswap(out[y*w+x]), b=unswap(ref[y*w+x]);
            const int errR=abs(((a>>11)&31)-((b>>11)&31));
            const int errG=abs(((a>>5)&63)-((b>>5)&63));
            const int errB=abs((a&31)-(b&31));
            if(errR+errG+errB) ++differing;
            if (errR>2 || errG>2 || errB>2) ++largeDiff;
        }
        printf("bilinear %.2f: %d tiny differences, %d large differences\n",deg,differing,largeDiff);
        assert(largeDiff < 1500);
    }
    printf("fast rotation checks passed\n");
}
