#include "mascot_frames.h"
#include <cassert>
#include <cstdint>
int main() {
  using namespace mascot_frames;
  static_assert(SIDE == 120, "expected small sprite");
  static_assert(COUNT == 3, "idle bob and blink");
  for (int f=0;f<COUNT;f++) {
    assert(claude[f] && jadebyte[f]);
    bool visibleC=false, visibleJ=false;
    for (int i=0;i<SIDE*SIDE;i++) {
      visibleC |= (claude[f][i] >> 24) != 0;
      visibleJ |= (jadebyte[f][i] >> 24) != 0;
    }
    assert(visibleC && visibleJ);
  }
  return 0;
}
