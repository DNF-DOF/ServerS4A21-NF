// NFGmMode.cpp - GM 模式开关
// 对应 易语言：
//   GM开关 ＝ 取反 (GM开关)
//   如果真 (GM开关 ＝ 真)
//     gm数据 ＝ 读内存字节集 (进程id, #GM权限模式, 16)
//     写内存字节集 (进程id, #GM权限模式, {3B 0D 48 B3 A5 03 74 03 30 C0 C3 B0 01 C3 CC CC})
//   否则
//     写内存字节集 (进程id, #GM权限模式, gm数据)
#include "NFGmMode.h"

#include <cstring>

#include "NFAddresses.h"
#include "NFLog.h"
#include "NFMemory.h"

namespace gm {
namespace {

constexpr size_t kPatchSize = 16;
// 补丁：改写 GM 权限判定函数（恒比较 + 按比较结果返回 0/1）。
const unsigned char kPatch[kPatchSize] = {
    0x3B, 0x0D, 0x48, 0xB3, 0xA5, 0x03, 0x74, 0x03,
    0x30, 0xC0, 0xC3, 0xB0, 0x01, 0xC3, 0xCC, 0xCC,
};

bool s_enabled = false;
unsigned char s_original[kPatchSize] = {0};

}  // namespace

bool Toggle() {
  s_enabled = !s_enabled;
  const uintptr_t addr = mem::ClientAddress(addr::kGmPermissionMode);

  if (s_enabled) {
    // 先记录当前 16 字节（等同 易语言 读内存字节集），再写补丁。
    if (mem::ReadBytes(addr, s_original, kPatchSize) != kPatchSize) {
      nflog::Write(L"[GM] 读取原数据失败，未开启");
      s_enabled = false;  // 回滚，避免状态不一致
      return false;
    }
    if (!mem::WriteBytes(addr, kPatch, kPatchSize)) {
      nflog::Write(L"[GM] 写入补丁失败，未开启");
      s_enabled = false;
      return false;
    }
    nflog::Write(L"[本地GM] 已开启");
  } else {
    // 写回原数据，还原判定。
    if (!mem::WriteBytes(addr, s_original, kPatchSize)) {
      nflog::Write(L"[GM] 还原原数据失败");
      return false;
    }
    nflog::Write(L"[本地GM] 已关闭（已还原）");
  }
  return s_enabled;
}

bool Enabled() { return s_enabled; }

}  // namespace gm
