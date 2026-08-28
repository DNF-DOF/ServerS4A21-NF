// NFSkillFullscreen.cpp - 技能全屏
// 实现方式参考 86-jp-DnfHelper（已验证 0x117AFF0 技能CALL）与 adddemo
// 的 sub_40D75E/sub_40DAEC（S4A21 版目标过滤与参数槽逐值复刻）：
//   1) 遍历 人物+0xB8(地图) -> +0xC0/+0xC4 对象表；
//   2) 过滤：类型∈{529,273,545} 且 阵营∈{100,110,120,101,50} 且 血量>0；
//   3) 坐标 = (int)(float)[obj+0x1B8/0x1BC/0x1C0]；
//   4) 每目标：写参数槽（+0=玩家对象,+4=技能代码,+8=伤害,+18/1C/20=坐标，
//      +58/+5C=0xFFFF 与 demo 一致），esi=槽，CALL 技能CALL(0x162C5E0)。
// 参数来自 NF.ini [技能全屏]：技能代码/伤害/频率/每轮目标数（热重载生效）。
// 旧方案（技能栏 +0x5B9C 连续写 float）实测闪退，已弃用。
#include "NFSkillFullscreen.h"

#include <windows.h>

#include <atomic>

#include "NFAddresses.h"
#include "NFConfig.h"
#include "NFMemory.h"
#include "NFRuntime.h"

namespace skillfs {
namespace {

std::atomic<bool> g_enabled{false};

// 参数槽：首次使用时分配一次并缓存复用（游戏侧技能CALL 读槽内字段）。
uint32_t g_slot = 0;
DWORD g_last_run = 0;

bool InList(int32_t value, const int32_t* list, size_t count) {
  for (size_t i = 0; i < count; ++i)
    if (list[i] == value) return true;
  return false;
}

// 读取实体坐标（内存为 float 位模式，转 float 后向零取整，与 demo 一致）。
int32_t ReadCoord(uint32_t address) {
  union {
    uint32_t u;
    float f;
  } cvt;
  cvt.u = mem::ReadDword(address, 0);
  return static_cast<int32_t>(cvt.f);
}

uint32_t EnsureSlot() {
  if (g_slot != 0) return g_slot;
  // 进程内分配即可：游戏侧技能CALL 只按槽地址读字段，进程退出时系统回收。
  const uint32_t slot =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(
          VirtualAlloc(nullptr, addr::kSkillSlotSize, MEM_COMMIT | MEM_RESERVE,
                       PAGE_READWRITE)));
  if (slot == 0 || !mem::IsWritable(slot, addr::kSkillSlotSize)) {
    if (slot != 0) VirtualFree(reinterpret_cast<void*>(slot), 0, MEM_RELEASE);
    return 0;
  }
  g_slot = slot;
  return slot;
}

// 对单个目标执行技能CALL（必须在游戏线程执行）。
// 调用约定与参考实现逐字节一致：esi = 参数槽，eax = 技能CALL 地址。
void CallSkillOnTarget(uint32_t player, uint32_t slot, int32_t code,
                       int32_t damage, int32_t x, int32_t y, int32_t z) {
  mem::WriteDword(slot + addr::kSkillSlotSelf, player);
  mem::WriteDword(slot + addr::kSkillSlotCode, static_cast<uint32_t>(code));
  mem::WriteDword(slot + addr::kSkillSlotDamage,
                  static_cast<uint32_t>(damage));
  mem::WriteDword(slot + 0x0C, 0);
  mem::WriteDword(slot + 0x10, 0);
  mem::WriteDword(slot + 0x14, 0);
  mem::WriteDword(slot + addr::kSkillSlotX, static_cast<uint32_t>(x));
  mem::WriteDword(slot + addr::kSkillSlotY, static_cast<uint32_t>(y));
  mem::WriteDword(slot + addr::kSkillSlotZ, static_cast<uint32_t>(z));
  // demo sub_40DAEC 每次调用额外写 +0x58/+0x5C = 0xFFFF（S4A21 特有）。
  mem::WriteDword(slot + addr::kSkillSlotMaxTgt, 0xFFFFu);
  mem::WriteDword(slot + addr::kSkillSlotExtra, 0xFFFFu);

  const uintptr_t call_addr = mem::ClientAddress(addr::kSkillCall);
  if (!mem::IsReadable(call_addr, 4)) return;

  __try {
    __asm {
      mov  esi, slot
      mov  eax, call_addr
      call eax
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
}

// 单轮执行：遍历对象表，对最多 cfg.skill_count 个有效目标各调用一次技能CALL。
void RunRound(const config::Settings& cfg) {
  const uint32_t player = runtime::PlayerPtr();
  if (player == 0) return;
  const uint32_t map = runtime::MapPtr();
  if (map == 0) return;  // 城镇没有地图对象，跳过

  const uint32_t head = mem::ReadDword(map + addr::kOffListHead, 0);
  const uint32_t tail = mem::ReadDword(map + addr::kOffListTail, 0);
  if (head == 0 || tail <= head) return;

  const uint32_t slot = EnsureSlot();
  if (slot == 0) return;

  int32_t remaining = cfg.skill_count;
  if (remaining < 1) remaining = 1;
  if (remaining > 20) remaining = 20;

  for (uint32_t it = head; it + 4 <= tail && remaining > 0; it += 4) {
    const uint32_t obj = mem::ReadDword(it, 0);
    if (obj == 0) continue;

    const int32_t type =
        static_cast<int32_t>(mem::ReadDword(obj + addr::kOffEntityType, 0));
    if (!InList(type, addr::kSkillTargetTypes,
                sizeof(addr::kSkillTargetTypes) / sizeof(int32_t))) {
      continue;
    }
    const int32_t camp =
        static_cast<int32_t>(mem::ReadDword(obj + addr::kOffCamp, 0));
    if (!InList(camp, addr::kSkillTargetCamps,
                sizeof(addr::kSkillTargetCamps) / sizeof(int32_t))) {
      continue;
    }
    // 血量 <= 0 的目标（尸体）跳过。
    if (static_cast<int32_t>(mem::ReadDword(obj + addr::kOffMonsterHp, 0)) <=
        0) {
      continue;
    }

    const int32_t x = ReadCoord(obj + addr::kOffPosX);
    const int32_t y = ReadCoord(obj + addr::kOffPosY);
    const int32_t z = ReadCoord(obj + addr::kOffPosZ);

    CallSkillOnTarget(player, slot, cfg.skill_code, cfg.skill_damage, x, y, z);
    --remaining;
  }
}

}  // namespace

bool Enabled() { return g_enabled.load(); }

void SetEnabled(bool enabled) {
  g_enabled.store(enabled);
  g_last_run = 0;  // 勾选后下一 Tick 立即执行一轮
}

void Tick() {
  if (!g_enabled.load()) return;
  const config::Settings cfg = config::Get();
  const DWORD now = GetTickCount();
  int interval = cfg.skill_interval;
  if (interval < 50) interval = 50;
  if (static_cast<DWORD>(now - g_last_run) < static_cast<DWORD>(interval)) {
    return;
  }
  g_last_run = now;
  RunRound(cfg);
}

}  // namespace skillfs
