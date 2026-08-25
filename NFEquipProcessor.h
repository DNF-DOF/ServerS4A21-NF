// NFEquipProcessor.h - 一键装备处理
#pragma once

#include <cstdint>

#include "NFConfig.h"

namespace equip {

// 处理结果统计
struct Result {
  int32_t sold[config::kQualityCount] = {0, 0, 0, 0, 0, 0};
  int32_t disassembled[config::kQualityCount] = {0, 0, 0, 0, 0, 0};
  int32_t scanned = 0;
  bool busy = false;
};

// 异步执行一键处理（内部投递到游戏主线程）。
void ProcessAsync();

// 是否处理中。
bool Busy();

// 最近一次处理结果。
Result LastResult();

}  // namespace equip
