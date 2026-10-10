#define RAD_TO_DEG 57.2957795130823208768f
#define degrees(rad) ((rad) * RAD_TO_DEG)
#include <cassert>
#include <cmath>
#include <vector>
#include "continuous_rotation.h"
using namespace continuous_rotation;

int main() {
    assert(std::fabs(shortest_difference(2.f, 358.f)-4.f)<0.01f);
    assert(std::fabs(shortest_difference(358.f, 2.f)+4.f)<0.01f);
    assert(wrap360(-90.f)==270.f);
    assert(swapped565(0x1234)==0x3412);
    // Every integer quarter-turn matches the old discrete rotation.
    const int w=7, h=7;
    std::vector<uint16_t> input(w*h), output(w*h);
    for (int i=0;i<w*h;i++) input[i] = i+17;
    for (int deg: {0,90,180,270}) {
        render_rotated_rgb565(input.data(), output.data(), w,h, float(deg),false);
        for (int y=0; y<h; ++y) for (int x=0; x<w; ++x) {
            const auto p = touch_to_logical(x,y,w,h, float(deg));
            assert(output[y*w+x] == swapped565(input[p.y*w+p.x]));
        }
    }
    // Bilinear is stable on uniform image and produces interpolated colors.
    std::fill(input.begin(),input.end(),0x07E0); // green
    render_rotated_rgb565(input.data(),output.data(),w,h,32.f,true);
    assert(output[3*w+3] == swapped565(0x07E0));
    assert(output[0] == 0 || output[0] == swapped565(0x07E0));
    // gyro immediately moves image rather than waiting until 90-degree threshold.
    ComplementaryRoll filter;
    filter.update(0,9.807,0,0,0.02,.03,2.5);
    filter.update(0,9.807,0,-1.5707963f,.02,.03,2.5);
    assert(filter.angleDegrees() < -1.0f && filter.angleDegrees() > -5.f);
    // Continuous rotation can track 30 degrees (not snapped to quadrant).
    for (int i=0;i<40;i++) filter.update(0,0,9.807,-0.5,0.02,.03,2.5);
    assert(filter.angleDegrees() < -15.f);
    return 0;
}
