// NFGameThread.h - 游戏主线程任务派发
// 游戏 CALL 必须在游戏窗口线程调用。工作线程通过此模块投递任务；
// 派发触发采用 EquipmentSwap 同款方案：WH_CALLWNDPROC + WH_GETMESSAGE
// 消息挂钩 + PostThreadMessageW，无需 D3D / 内联 Hook。
#pragma once

#include <functional>

namespace gthread {

// 安装游戏线程挂钩（传入 NF.dll 自身 HMODULE）。幂等。
bool Install(void* module);

// 挂钩是否已成功安装。
bool Installed();

// 卸载挂钩并唤醒所有等待者。
void Shutdown();

// 投递任务到游戏线程，立即返回（异步）。
void Post(std::function<void()> task);

// 投递任务并等待其在游戏线程执行完成。
// timeout_ms 为等待上限，超时返回 false。
// 在游戏线程内部调用时直接同步执行，避免死锁。
bool RunSync(std::function<void()> task, unsigned int timeout_ms = 5000);

// 标记当前线程为游戏主线程。
void MarkCurrentAsGameThread();

// 当前线程是否为游戏主线程。
bool IsGameThread();

}  // namespace gthread
