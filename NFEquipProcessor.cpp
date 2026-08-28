// NFEquipProcessor.cpp - 一键装备处理
// 移植自 参考/nf/EquipProcessor.cpp，公告改走 GameNative (NFNotice)。
#include "NFEquipProcessor.h"

#include <windows.h>

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "NFAddresses.h"
#include "NFConfig.h"
#include "NFGameThread.h"
#include "NFLog.h"
#include "NFMemory.h"
#include "NFNotice.h"
#include "NFPacket.h"
#include "NFRuntime.h"

namespace equip {
namespace {

std::atomic<bool> g_busy(false);
std::mutex g_result_mutex;
Result g_last;

constexpr DWORD kItemConfirmationTimeoutMs = 3000;
constexpr DWORD kInterItemDelayMs = 150;
constexpr DWORD kOrganizeCooldownMs = 1500;

enum class Phase {
  kIdle,
  kReady,
  kWaitingForSlotChange,
  kOrganizeCooldown,
};

struct Candidate {
  int32_t slot = -1;
  uint32_t item = 0;
  int32_t quality = -1;
  int32_t action = config::kKeep;
  std::wstring name;
};

Phase g_phase = Phase::kIdle;
std::vector<Candidate> g_candidates;
size_t g_next_candidate = 0;
Candidate g_inflight;
DWORD g_due_tick = 0;
Result g_current;
bool g_cooldown_completed = false;
std::wstring g_cooldown_reason;

bool TickReached(DWORD now, DWORD due) {
  return static_cast<LONG>(now - due) >= 0;
}

bool TryReadSlotItem(int32_t slot, uint32_t& item) {
  item = 0;
  const int32_t index = slot - addr::kEquipSlotOffset;
  if (index < 0 || index >= addr::kEquipSlotCount) return false;

  const uint32_t list = runtime::EquipListBase();
  if (list == 0) return false;
  constexpr uint32_t kReadFailed = 0xFFFFFFFFu;
  const uint32_t value = mem::ReadDword(
      list + static_cast<uint32_t>(index) * sizeof(uint32_t), kReadFailed);
  if (value == kReadFailed) return false;
  item = value;
  return true;
}

int32_t ReadQuality(uint32_t item) {
  if (item == 0) return -1;
  return static_cast<int32_t>(
      mem::ReadDword(item + addr::kOffEquipAttr, 0xFFFFFFFFu));
}

std::wstring ReadName(uint32_t item) {
  if (item == 0) return std::wstring();
  const uint32_t name_ptr =
      mem::ReadDword(item + addr::kOffEquipName, 0);
  return name_ptr != 0 ? mem::ReadWideString(name_ptr, 48)
                       : std::wstring();
}

// 名称是否命中过滤列表。
bool NameFiltered(const std::wstring& name,
                   const std::vector<std::wstring>& filters) {
  for (const std::wstring& f : filters) {
    if (f.empty()) continue;
    if (name.find(f) != std::wstring::npos) return true;
  }
  return false;
}

void PublishCurrentResult() {
  std::lock_guard<std::mutex> lock(g_result_mutex);
  g_last = g_current;
}

void SendSummaryNotice(const Result& result) {
  if (!notice::Available()) return;

  const wchar_t* names[config::kQualityCount] = {
      L"白装", L"蓝装", L"紫装", L"粉装", L"史诗", L"异界"};
  notice::Send(L"本次一键处理结果");
  for (int q = 0; q < config::kQualityCount; ++q) {
    if (result.sold[q] == 0 && result.disassembled[q] == 0) continue;
    wchar_t buf[128] = {0};
    _snwprintf_s(buf, _TRUNCATE, L"卖物-%s：%d 件  分解-%s：%d 件",
                 names[q], result.sold[q], names[q],
                 result.disassembled[q]);
    notice::Send(buf);
  }
}

void FinishBatch(bool completed, const wchar_t* reason) {
  g_current.aborted = !completed;
  PublishCurrentResult();
  if (completed) SendSummaryNotice(g_current);

  int confirmed = 0;
  for (int quality = 0; quality < config::kQualityCount; ++quality)
    confirmed += g_current.sold[quality] + g_current.disassembled[quality];

  nflog::Write(
      L"[一键处理] %s reason=%s scanned=%d candidates=%u confirmed=%u",
      completed ? L"完成" : L"中止", reason ? reason : L"?",
      g_current.scanned, static_cast<unsigned int>(g_candidates.size()),
      static_cast<unsigned int>(confirmed));

  g_candidates.clear();
  g_next_candidate = 0;
  g_inflight = Candidate();
  g_due_tick = 0;
  g_cooldown_completed = false;
  g_cooldown_reason.clear();
  g_phase = Phase::kIdle;
  g_busy.store(false);
}

void EnterCooldown(bool completed, const wchar_t* reason, DWORD now) {
  PublishCurrentResult();
  g_cooldown_completed = completed;
  g_cooldown_reason = reason ? reason : L"?";
  g_phase = Phase::kOrganizeCooldown;
  g_due_tick = now + kOrganizeCooldownMs;
}

bool RevalidateCandidate(const Candidate& candidate,
                         std::wstring& reject_reason,
                         bool& fatal) {
  fatal = false;
  uint32_t current_item = 0;
  if (!TryReadSlotItem(candidate.slot, current_item)) {
    reject_reason = L"装备背包暂时不可读";
    fatal = true;
    return false;
  }
  if (current_item == 0) {
    reject_reason = L"槽位已空";
    return false;
  }
  if (current_item != candidate.item) {
    reject_reason = L"槽位物品已变化";
    return false;
  }

  const int32_t current_quality = ReadQuality(current_item);
  if (current_quality != candidate.quality || current_quality < 0 ||
      current_quality >= config::kQualityCount) {
    reject_reason = L"品质已变化或不可识别";
    return false;
  }

  const std::wstring current_name = ReadName(current_item);
  if (current_name.empty() || current_name != candidate.name) {
    reject_reason = L"名称已变化或不可识别";
    return false;
  }

  const config::Settings current_cfg = config::Get();
  const int32_t current_action = current_cfg.equip_action[current_quality];
  if (current_action != candidate.action ||
      (current_action != config::kSell &&
       current_action != config::kDisassemble)) {
    reject_reason = L"处理规则已变化或当前为保留";
    return false;
  }

  const std::vector<std::wstring> filters =
      current_cfg.ParsedNameFilters();
  if (!filters.empty() && NameFiltered(current_name, filters)) {
    reject_reason = L"当前名称过滤规则命中";
    return false;
  }
  return true;
}

void StartBatch() {
  if (!g_busy.load()) return;

  const config::Settings cfg = config::Get();
  const std::vector<std::wstring> filters = cfg.ParsedNameFilters();
  g_current = Result();
  g_candidates.clear();
  g_next_candidate = 0;
  g_inflight = Candidate();
  g_cooldown_completed = false;
  g_cooldown_reason.clear();

  const uint32_t list = runtime::EquipListBase();
  if (list == 0) {
    FinishBatch(false, L"装备背包不可用");
    return;
  }

  for (int i = 1; i <= addr::kEquipSlotCount; ++i) {
    const uint32_t item =
        mem::ReadDword(list + static_cast<uint32_t>(i - 1) * 4, 0);
    if (item == 0) continue;

    const int32_t quality = ReadQuality(item);
    if (quality < 0 || quality >= config::kQualityCount) continue;

    const std::wstring name = ReadName(item);
    ++g_current.scanned;

    // 无法确认名称时宁可保留，避免内存读取异常时处理未知装备。
    if (name.empty()) continue;
    if (!filters.empty() && NameFiltered(name, filters)) continue;

    const int32_t action = cfg.equip_action[quality];
    if (action != config::kSell && action != config::kDisassemble)
      continue;

    Candidate candidate;
    candidate.slot = i - 1 + addr::kEquipSlotOffset;
    candidate.item = item;
    candidate.quality = quality;
    candidate.action = action;
    candidate.name = name;
    g_candidates.push_back(std::move(candidate));
  }

  nflog::Write(L"[一键处理] 开始 scanned=%d candidates=%u",
               g_current.scanned,
               static_cast<unsigned int>(g_candidates.size()));
  if (g_candidates.empty()) {
    FinishBatch(true, L"没有需要处理的装备");
    return;
  }

  g_phase = Phase::kReady;
  g_due_tick = GetTickCount();
}

}  // namespace

void ProcessAsync() {
  if (g_busy.exchange(true)) {
    nflog::Write(L"[一键处理] 忽略重复触发：上一批仍在等待确认");
    return;
  }
  if (!gthread::Installed()) {
    g_busy.store(false);
    return;
  }
  gthread::Post([] { StartBatch(); });
}

void Tick() {
  if (!g_busy.load() || !gthread::IsGameThread()) return;

  const DWORD now = GetTickCount();
  if (g_phase == Phase::kWaitingForSlotChange) {
    uint32_t current_item = 0;
    if (!TryReadSlotItem(g_inflight.slot, current_item)) {
      nflog::Write(
          L"[一键处理] 等待期间装备背包不可读，停止后续请求 slot=%d",
          g_inflight.slot);
      EnterCooldown(false, L"等待期间装备背包不可读", now);
      return;
    }
    if (current_item == 0) {
      if (g_inflight.action == config::kSell)
        ++g_current.sold[g_inflight.quality];
      else if (g_inflight.action == config::kDisassemble)
        ++g_current.disassembled[g_inflight.quality];

      nflog::Write(L"[一键处理] 已确认 slot=%d quality=%d action=%d",
                   g_inflight.slot, g_inflight.quality,
                   g_inflight.action);
      g_inflight = Candidate();
      g_phase = Phase::kReady;
      g_due_tick = now + kInterItemDelayMs;
      return;
    }

    if (current_item != g_inflight.item) {
      nflog::Write(
          L"[一键处理] 槽位被其他装备替换，停止后续请求 slot=%d old=%p new=%p",
          g_inflight.slot, reinterpret_cast<void*>(g_inflight.item),
          reinterpret_cast<void*>(current_item));
      EnterCooldown(false, L"等待期间目标槽被其他装备替换", now);
      return;
    }

    if (TickReached(now, g_due_tick)) {
      nflog::Write(
          L"[一键处理] 等待超时，停止后续请求 slot=%d quality=%d action=%d",
          g_inflight.slot, g_inflight.quality, g_inflight.action);
      EnterCooldown(false, L"服务端未确认当前槽位变化", now);
    }
    return;
  }

  if (g_phase == Phase::kOrganizeCooldown) {
    if (TickReached(now, g_due_tick))
      FinishBatch(g_cooldown_completed, g_cooldown_reason.c_str());
    return;
  }

  if (g_phase != Phase::kReady || !TickReached(now, g_due_tick)) return;

  while (g_next_candidate < g_candidates.size()) {
    const Candidate candidate = g_candidates[g_next_candidate++];
    std::wstring reject_reason;
    bool fatal = false;
    if (!RevalidateCandidate(candidate, reject_reason, fatal)) {
      nflog::Write(L"[一键处理] 跳过 slot=%d quality=%d reason=%s",
                   candidate.slot, candidate.quality,
                   reject_reason.c_str());
      if (fatal) {
        EnterCooldown(false, reject_reason.c_str(), now);
        return;
      }
      continue;
    }

    g_inflight = candidate;
    if (candidate.action == config::kSell)
      packet::SellItem(candidate.slot, 1);
    else
      packet::Disassemble(candidate.slot);

    nflog::Write(L"[一键处理] 已发送 slot=%d quality=%d action=%d name=%s",
                 candidate.slot, candidate.quality, candidate.action,
                 candidate.name.c_str());
    g_phase = Phase::kWaitingForSlotChange;
    g_due_tick = now + kItemConfirmationTimeoutMs;
    return;
  }

  // 所有目标槽都已逐一确认变化后才允许整理，避免整理与尚在途的槽位请求竞态。
  packet::OrganizeBag(1, 0);
  nflog::Write(L"[一键处理] 全部槽位已确认，发送整理并进入冷却");
  EnterCooldown(true, L"全部请求已确认并完成整理", now);
}

bool Busy() { return g_busy.load(); }

Result LastResult() {
  std::lock_guard<std::mutex> lock(g_result_mutex);
  Result r = g_last;
  r.busy = g_busy.load();
  return r;
}

}  // namespace equip
