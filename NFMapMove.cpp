// NFMapMove.cpp - 顺图
// 移植自 参考/nf/MapMove.cpp，地址改为 ClientAddress() 路由。
#include "NFMapMove.h"

#include <windows.h>

#include <thread>

#include "NFAddresses.h"
#include "NFConfig.h"
#include "NFGameThread.h"
#include "NFMemory.h"
#include "NFPacket.h"
#include "NFRuntime.h"

namespace mapmove {
namespace {

// 坐标CALL（易语言置入代码版本）：对象虚表单调用，无前置调用。
//   esi = 触发指针（人物）, edi = [esi] = vtable
//   push Z / push Y / push X
//   eax = [edi + 172 (0xAC)] ; ecx = esi ; call eax
// 无 pushad/popad：避免 __try 的 SEH 栈帧因 ESP 变动失效。
void CallCoordRaw(uint32_t player, int32_t x, int32_t y, int32_t z) {
  __asm {
    mov  esi, player
    mov  eax, [esi]
    mov  edi, eax
    push z
    push y
    push x
    mov  eax, [edi + 172]
    mov  ecx, esi
    call eax
  }
}

void CallCoord(uint32_t player, int32_t x, int32_t y, int32_t z) {
  if (player == 0) return;
  if (!mem::IsReadable(player, 4)) return;
  const uint32_t vt = mem::ReadDword(player, 0);
  if (vt == 0 || !mem::IsReadable(vt + 172, 4)) return;
  if (x < -1000000 || x > 1000000 || y < -1000000 || y > 1000000) return;

  __try {
    CallCoordRaw(player, x, y, z);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
}

// 尝试从 L2 之后的 p + l3_off 解引用得到坐标数组基址；成功返回 true 并输出 cx/cy。
bool ProbeL3(uint32_t l2_p, int l3_off, int yi_dir, int32_t& out_cx,
             int32_t& out_cy, int step) {
  if (!mem::IsReadable(l2_p + l3_off, 4)) return false;
  const uint32_t p = mem::ReadDword(l2_p + l3_off, 0);
  if (p < 65536 || (p & 3) != 0) return false;
  if (!mem::IsReadable(p + 4548, 4)) return false;
  const uint32_t node =
      p + static_cast<uint32_t>((yi_dir + yi_dir * 8) * 4) + 4428;
  if (!mem::IsReadable(node, 16)) return false;
  const int32_t x = static_cast<int32_t>(mem::ReadDword(node + 0, 0));
  const int32_t y = static_cast<int32_t>(mem::ReadDword(node + 4, 0));
  const int32_t w = static_cast<int32_t>(mem::ReadDword(node + 8, 0));
  const int32_t h = static_cast<int32_t>(mem::ReadDword(node + 12, 0));
  if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return false;

  int32_t cx, cy;
  if (step != 0) {
    cx = x + w / 2;
    cy = y;
  } else {
    switch (yi_dir) {
      case 0: cx = x - 20;     cy = y + h / 2;     break;  // 左 (kLeft=0)
      case 1: cx = x + w + 20; cy = y + h / 2;     break;  // 右 (kRight=1)
      case 2: cx = x + w / 2;  cy = y - 20;        break;  // 上 (kUp=2)
      case 3: cx = x + w / 2;  cy = y + h + 20;    break;  // 下 (kDown=3)
      default: return false;
    }
  }
  out_cx = cx; out_cy = cy;
  return true;
}

// 坐标顺图单步，移植自易语言「顺图使用」子程序。
// step 0: 移动到门口外侧坐标；step 1: 回到房间中心，完成过图。
void CoordMoveStep(int dir, int step) {
  const uint32_t player = runtime::PlayerPtr();
  if (player == 0 || !mem::IsReadable(player, 4)) return;
  if (runtime::MapPtr() == 0) return;

  // L1: [[ShopBase-8]]
  const uint32_t base = mem::ClientAddress(addr::kShopBase) - 8;
  if (!mem::IsReadable(base, 4)) return;
  const uint32_t p1 = mem::ReadDword(base, 0);
  if (p1 == 0) return;

  // L2: [p1 + kOffTimeBase]
  if (!mem::IsReadable(p1 + addr::kOffTimeBase, 4)) return;
  const uint32_t l2_p = mem::ReadDword(p1 + addr::kOffTimeBase, 0);
  if (l2_p == 0) return;

  // NF Direction（kLeft=0 kRight=1 kUp=2 kDown=3）直接作为参_标识使用
  const int yi_dir = dir;

  int32_t cx, cy;
  bool ok = false;
  ok = ProbeL3(l2_p, 140, yi_dir, cx, cy, step);
  if (!ok)
    ok = ProbeL3(l2_p,  84, yi_dir, cx, cy, step);
  if (!ok) return;

  __try {
    CallCoord(player, cx, cy, 0);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
}

}  // namespace

// 坐标顺图走虚表调用，不依赖外部地址，始终可用。
bool CoordModeAvailable() { return true; }

void Move(Direction dir) {
  // 城镇（地图指针为 0）不执行顺图。
  if (runtime::MapPtr() == 0) return;

  const config::Settings cfg = config::Get();
  if (cfg.move_mode == 1) {
    // 强制顺图：过图CALL
    gthread::Post(
        [dir] { packet::ForceChangeRoom(static_cast<int32_t>(dir)); });
    return;
  }

  // 坐标顺图：两步之间需要间隔，放到工作线程避免阻塞渲染。
  std::thread([dir] {
    gthread::RunSync([dir] { CoordMoveStep(static_cast<int>(dir), 0); }, 2000);
    Sleep(200);
    gthread::RunSync([dir] { CoordMoveStep(static_cast<int>(dir), 1); }, 2000);
  }).detach();
}

}  // namespace mapmove
