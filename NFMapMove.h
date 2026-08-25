// NFMapMove.h - 顺图
#pragma once

namespace mapmove {

enum Direction : int {
  kLeft = 0,
  kRight = 1,
  kUp = 2,
  kDown = 3
};

// 按配置的顺图模式执行对应方向的顺图。
void Move(Direction dir);

// 坐标顺图是否可用（走对象虚表调用，不依赖外部地址，始终可用）。
bool CoordModeAvailable();

}  // namespace mapmove
