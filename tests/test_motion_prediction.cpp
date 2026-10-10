#include <cassert>
#include <cmath>
#include <iostream>
#include "motion_prediction.h"
static bool near(float a, float b) { return std::fabs(a-b)<0.001f; }
int main() {
 assert(near(motion_prediction::predict(90, 0, 0, 1, 15), 90));
 assert(near(motion_prediction::predict(90, 0, 100, 10, 15), 92.5));
 assert(near(motion_prediction::predict(90, 270, 0, 0, 0), 0));
 assert(near(motion_prediction::predict(90, 0, -100, 20, 15), 86.5));
 assert(near(motion_prediction::predict(90, 0, 100, 120, 30), 95)); // bounded 50ms
 std::cout << "motion prediction tests passed\n";
}
