// NFScore.cpp - 自动评分
#include "NFScore.h"

#include <windows.h>

#include "NFAddresses.h"
#include "NFLog.h"
#include "NFMemory.h"
#include "NFRuntime.h"

namespace grade {
namespace {

constexpr int32_t kScoreThreshold = 5201314;   // 低于此值才写入
constexpr int32_t kScoreWriteValue = 9999999;  // 写入的固定评分

// 评分具体实现：先判定副本，再读评分并低于阈值时加密写入。
void RunOnce() {
  // 先检查是否副本内：不在副本直接返回。
  if (!state::InDungeon())
    return;
  if (!state::PlayerReady())
    return;

  // 评分地址 = 读内存整数型(进程ID, #评分基址)
  const uint32_t score_struct = mem::ReadDword(mem::ClientAddress(addr::kScoreBase), 0);
  if (score_struct == 0 || !mem::IsReadable(score_struct + 272 + 4, 4))
    return;

  // 解密(评分地址 + 272)
  const int32_t current = mem::SuperDecrypt(score_struct + 272);
  if (current < 0)
    return;  // 解密失败（-1）不写，避免破坏内存

  // 如果真(解密(...) < 5201314) 加密(..., 9999999)
  if (current < kScoreThreshold) {
    mem::SuperEncrypt(score_struct + 272, kScoreWriteValue);
    nflog::Write(L"[评分] 当前评分 %d < %d，已写入 %d", current, kScoreThreshold,
                 kScoreWriteValue);
  }
}

}  // namespace

void Tick(bool auto_grade_enabled) {
  static bool s_active = false;
  static DWORD s_enable_time = 0;
  static DWORD s_last_action = 0;

  if (!auto_grade_enabled) {
    s_active = false;  // 关闭即停止轮询（下次开启重新预热 5 秒）
    return;
  }

  const DWORD now = GetTickCount();
  if (!s_active) {
    s_active = true;
    s_enable_time = now;
    s_last_action = 0;
    return;  // 界面打开开关后预热 5 秒
  }
  if (now - s_enable_time < 5000)
    return;  // 开关后 5 秒才开始轮询
  if (now - s_last_action < 5000)
    return;  // 每 5 秒轮询一次
  s_last_action = now;
  RunOnce();
}

}  // namespace grade
