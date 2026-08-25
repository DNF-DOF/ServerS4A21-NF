// NFAutoPickup.cpp - 自动拾取
// 移植自 参考/nf/AutoPickup.cpp，地址改为 ClientAddress() 路由。
#include "NFAutoPickup.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "NFAddresses.h"
#include "NFGameThread.h"
#include "NFMemory.h"
#include "NFPacket.h"
#include "NFRuntime.h"

namespace pickup {
namespace {

std::atomic<bool> g_running(false);
std::thread g_thread;
std::mutex g_cfg_mutex;
config::Settings g_cfg;
HANDLE g_stop_event = nullptr;

config::Settings CurrentConfig() {
  std::lock_guard<std::mutex> lock(g_cfg_mutex);
  return g_cfg;
}

// 判断物品ID是否允许拾取。
bool Allowed(int32_t item_id, const std::vector<int32_t>& ids, int filter_mode) {
  const bool listed = std::find(ids.begin(), ids.end(), item_id) != ids.end();
  if (filter_mode == 1) {
    // 拾取指定：只捡列表里的；列表为空时什么都不捡。
    // item_id == -1 表示解密失败，无法确认身份，一律不捡。
    if (ids.empty() || item_id == -1) return false;
    return listed;
  }
  // 过滤指定：列表里的不捡，其余都捡；解密失败时宁可多捡。
  if (item_id == -1) return true;
  return !listed;
}

// 单轮拾取，必须在游戏主线程执行。
void PickupOnce(const config::Settings& cfg) {
  const uint32_t map = runtime::MapPtr();
  if (map == 0) return;

  const uint32_t head = mem::ReadDword(map + addr::kOffListHead, 0);
  const uint32_t tail = mem::ReadDword(map + addr::kOffListTail, 0);
  if (head == 0 || tail <= head) return;

  const std::vector<int32_t> ids = cfg.ParsedFilterIds();
  // runtime::PosX/Y 直接把 float 位模式当 DWORD 强转 int，这里做位拷贝转 float 取整
  union { int32_t i; float f; } cvt;
  cvt.i = runtime::PosX();
  const int32_t px = static_cast<int32_t>(cvt.f);
  cvt.i = runtime::PosY();
  const int32_t py = static_cast<int32_t>(cvt.f);

  for (uint32_t it = head; it + 4 <= tail; it += 4) {
    const uint32_t obj = mem::ReadDword(it, 0);
    if (obj == 0) continue;
    if (static_cast<int32_t>(mem::ReadDword(obj + addr::kOffEntityType, 0)) !=
        addr::kEntityTypeDrop) {
      continue;
    }

    // 掉落物原始坐标（DWORD里是float位模式，位拷贝转float）
    const uint32_t coord = mem::ReadDword(obj + addr::kOffDir, 0);
    int32_t ix = 0, iy = 0;
    if (coord != 0) {
      union { uint32_t u; float f; } icvt;
      icvt.u = mem::ReadDword(coord + 16, 0);
      ix = static_cast<int32_t>(icvt.f);
      icvt.u = mem::ReadDword(coord + 20, 0);
      iy = static_cast<int32_t>(icvt.f);
    }

    // 过滤依赖解密出的物品ID；-1 表示解密失败，交由 Allowed 按模式决定。
    const int32_t item_id = mem::SuperDecrypt(obj + addr::kOffEntityId);
    if (!Allowed(item_id, ids, cfg.filter_mode)) continue;

    if (cfg.pickup_mode == 0) {
      // 吸物：把掉落物坐标改到人物脚下（coord+16/+20 内存里是float，
      // 先转 float 再位拷贝成 DWORD 写回，否则 int 值被当 float ≈ 0）
      if (coord == 0) continue;
      union { float f; uint32_t u; } wcvt;
      wcvt.f = static_cast<float>(px);
      mem::WriteDword(coord + 16, wcvt.u);
      wcvt.f = static_cast<float>(py);
      mem::WriteDword(coord + 20, wcvt.u);
      mem::WriteDword(mem::ClientAddress(addr::kAutoPickFlag),
                      static_cast<uint32_t>(addr::kPickFlagSuck));
    } else {
      // 发包拾取：物品地址=SuperDecrypt(obj + 组包拾取偏移)
      const int32_t pickup_addr = mem::SuperDecrypt(
          obj + addr::kOffPickupAddress);
      mem::WriteDword(mem::ClientAddress(addr::kAutoPickFlag),
                      static_cast<uint32_t>(addr::kPickFlagPacket));
      packet::Pickup(pickup_addr, px, py, ix, iy);
    }
  }
}

void WorkerProc() {
  while (g_running.load()) {
    const config::Settings cfg = CurrentConfig();

    // 城镇（地图基址整数为 0）不扫描，避免空转遍历 NPC/玩家实体。
    if (state::PlayerReady() && runtime::MapPtr() != 0) {
      gthread::RunSync([cfg] { PickupOnce(cfg); }, 3000);
    }

    int interval = cfg.pickup_interval;
    if (interval < 50) interval = 50;
    if (WaitForSingleObject(g_stop_event, static_cast<DWORD>(interval)) ==
        WAIT_OBJECT_0) {
      break;
    }
  }
}

}  // namespace

void Start() {
  if (g_running.exchange(true)) return;
  if (g_stop_event == nullptr) {
    g_stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  }
  ResetEvent(g_stop_event);
  {
    std::lock_guard<std::mutex> lock(g_cfg_mutex);
    g_cfg = config::Get();
  }
  g_thread = std::thread(WorkerProc);
}

void Stop() {
  if (!g_running.exchange(false)) return;
  if (g_stop_event != nullptr) SetEvent(g_stop_event);
  if (!g_thread.joinable()) return;
  if (gthread::IsGameThread()) {
    // 工作线程可能正在等待主线程执行任务，主线程 join 会造成卡顿/死锁，
    // 此处分离让其自行退出。
    g_thread.detach();
  } else {
    g_thread.join();
  }
}

void Toggle() {
  if (Running()) {
    Stop();
  } else {
    Start();
  }
}

bool Running() { return g_running.load(); }

void OnConfigChanged(const config::Settings& s) {
  std::lock_guard<std::mutex> lock(g_cfg_mutex);
  g_cfg = s;
}

}  // namespace pickup
