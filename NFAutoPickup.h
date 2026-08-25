// NFAutoPickup.h - 自动拾取
#pragma once

#include "NFConfig.h"

namespace pickup {

// 启动拾取线程。
void Start();

// 停止拾取线程。
void Stop();

// 切换开关。
void Toggle();

// 当前是否运行中。
bool Running();

// 配置变更时刷新参数。
void OnConfigChanged(const config::Settings& s);

}  // namespace pickup
