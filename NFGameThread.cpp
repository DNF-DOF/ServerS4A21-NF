// NFGameThread.cpp - 游戏主线程任务派发
// 队列机制移植自 参考/nf/GameThread.cpp；
// 泵送触发改为 EquipmentSwap 同款消息挂钩（WH_CALLWNDPROC + WH_GETMESSAGE）。
#include "NFGameThread.h"

#include <windows.h>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace gthread {
namespace {

struct Item {
  std::function<void()> fn;
  bool sync = false;
  bool done = false;
  std::shared_ptr<std::mutex> m;
  std::shared_ptr<std::condition_variable> cv;
};

std::mutex g_mutex;
std::deque<std::shared_ptr<Item>> g_queue;
DWORD g_game_tid = 0;
bool g_shutdown = false;

HMODULE g_module = nullptr;
HHOOK g_call_hook = nullptr;
HHOOK g_message_hook = nullptr;
DWORD g_hook_tid = 0;
UINT g_dispatch_message = 0;

// 独立函数：SEH 不能与需要对象析构的作用域共存。
__declspec(noinline) void SafeInvoke(const std::function<void()>* fn) {
  __try {
    (*fn)();
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    // 游戏 CALL 异常时保护游戏线程不崩溃。
  }
}

// 在游戏线程中执行队列中的全部任务。
void PumpLocked() {
  for (;;) {
    std::shared_ptr<Item> item;
    {
      std::lock_guard<std::mutex> lock(g_mutex);
      if (g_queue.empty()) return;
      item = g_queue.front();
      g_queue.pop_front();
    }

    if (item->fn) {
      SafeInvoke(&item->fn);
    }

    if (item->sync) {
      {
        std::lock_guard<std::mutex> lk(*item->m);
        item->done = true;
      }
      item->cv->notify_all();
    }
  }
}

void TriggerPump() {
  if (g_shutdown || g_hook_tid == 0 || g_dispatch_message == 0) return;
  PostThreadMessageW(g_hook_tid, g_dispatch_message, 0, 0);
}

BOOL CALLBACK FindGameWindow(HWND window, LPARAM parameter) {
  DWORD process_id = 0;
  const DWORD thread_id = GetWindowThreadProcessId(window, &process_id);
  if (process_id != GetCurrentProcessId() || !IsWindowVisible(window) ||
      GetParent(window))
    return TRUE;
  RECT rect = {};
  if (!GetWindowRect(window, &rect)) return TRUE;
  const long area =
      (rect.right - rect.left) * (rect.bottom - rect.top);
  auto* best = reinterpret_cast<std::pair<HWND, long>*>(parameter);
  if (area > best->second) *best = {window, area};
  return TRUE;
}

LRESULT CALLBACK GameCallWindowHook(int code, WPARAM wParam, LPARAM lParam) {
  if (code >= 0) {
    const auto* call = reinterpret_cast<const CWPSTRUCT*>(lParam);
    if (call && call->message == g_dispatch_message) {
      MarkCurrentAsGameThread();
      PumpLocked();
    }
  }
  return CallNextHookEx(g_call_hook, code, wParam, lParam);
}

LRESULT CALLBACK GameGetMessageHook(int code, WPARAM wParam, LPARAM lParam) {
  if (code >= 0 && wParam == PM_REMOVE && lParam) {
    auto* message = reinterpret_cast<MSG*>(lParam);
    if (message->message == g_dispatch_message) {
      MarkCurrentAsGameThread();
      PumpLocked();
    }
  }
  return CallNextHookEx(g_message_hook, code, wParam, lParam);
}

}  // namespace

void MarkCurrentAsGameThread() { g_game_tid = GetCurrentThreadId(); }

bool Installed() {
  return g_call_hook != nullptr && g_message_hook != nullptr;
}

bool IsGameThread() {
  return g_game_tid != 0 && GetCurrentThreadId() == g_game_tid;
}

bool Install(void* module) {
  if (g_call_hook && g_message_hook) return true;
  if (!module) return false;

  if (g_dispatch_message == 0)
    g_dispatch_message = RegisterWindowMessageW(L"NF.GameDispatch");

  std::pair<HWND, long> best = {nullptr, 0};
  EnumWindows(FindGameWindow, reinterpret_cast<LPARAM>(&best));
  if (!best.first) return false;

  DWORD process_id = 0;
  const DWORD thread_id = GetWindowThreadProcessId(best.first, &process_id);
  if (!thread_id || process_id != GetCurrentProcessId()) return false;

  HMODULE mod = reinterpret_cast<HMODULE>(module);
  HHOOK call_hook =
      SetWindowsHookExW(WH_CALLWNDPROC, GameCallWindowHook, mod, thread_id);
  if (!call_hook) return false;
  HHOOK message_hook =
      SetWindowsHookExW(WH_GETMESSAGE, GameGetMessageHook, mod, thread_id);
  if (!message_hook) {
    UnhookWindowsHookEx(call_hook);
    return false;
  }

  g_module = mod;
  g_hook_tid = thread_id;
  g_call_hook = call_hook;
  g_message_hook = message_hook;
  return true;
}

void Post(std::function<void()> task) {
  if (!task) return;
  if (g_shutdown) return;
  auto item = std::make_shared<Item>();
  item->fn = std::move(task);
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_queue.push_back(item);
  }
  TriggerPump();
}

bool RunSync(std::function<void()> task, unsigned int timeout_ms) {
  if (!task) return false;
  if (g_shutdown) return false;

  // 已经在游戏线程内，直接执行避免死锁。
  if (g_game_tid != 0 && GetCurrentThreadId() == g_game_tid) {
    task();
    return true;
  }

  auto item = std::make_shared<Item>();
  item->fn = std::move(task);
  item->sync = true;
  item->m = std::make_shared<std::mutex>();
  item->cv = std::make_shared<std::condition_variable>();

  {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_queue.push_back(item);
  }
  TriggerPump();

  std::unique_lock<std::mutex> lock(*item->m);
  return item->cv->wait_for(lock, std::chrono::milliseconds(timeout_ms),
                            [&item] { return item->done; });
}

void Shutdown() {
  g_shutdown = true;

  std::deque<std::shared_ptr<Item>> pending;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    pending.swap(g_queue);
  }
  for (auto& item : pending) {
    if (!item->sync) continue;
    {
      std::lock_guard<std::mutex> lk(*item->m);
      item->done = true;
    }
    item->cv->notify_all();
  }

  if (g_call_hook) {
    UnhookWindowsHookEx(g_call_hook);
    g_call_hook = nullptr;
  }
  if (g_message_hook) {
    UnhookWindowsHookEx(g_message_hook);
    g_message_hook = nullptr;
  }
  g_hook_tid = 0;
}

}  // namespace gthread
