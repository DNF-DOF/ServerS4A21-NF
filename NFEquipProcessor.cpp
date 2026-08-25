// NFEquipProcessor.cpp - 一键装备处理
// 移植自 参考/nf/EquipProcessor.cpp，公告改走 GameNative (NFNotice)。
#include "NFEquipProcessor.h"

#include <windows.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "NFAddresses.h"
#include "NFConfig.h"
#include "NFGameThread.h"
#include "NFMemory.h"
#include "NFNotice.h"
#include "NFPacket.h"
#include "NFRuntime.h"

namespace equip {
namespace {

std::atomic<bool> g_busy(false);
std::mutex g_result_mutex;
Result g_last;

// 名称是否命中过滤列表。
bool NameFiltered(const std::wstring& name,
                  const std::vector<std::wstring>& filters) {
  for (const std::wstring& f : filters) {
    if (f.empty()) continue;
    if (name.find(f) != std::wstring::npos) return true;
  }
  return false;
}

void ProcessCore() {
  const config::Settings cfg = config::Get();
  const std::vector<std::wstring> filters = cfg.ParsedNameFilters();

  Result r;
  const uint32_t list = runtime::EquipListBase();
  if (list == 0) {
    std::lock_guard<std::mutex> lock(g_result_mutex);
    g_last = r;
    return;
  }

  for (int i = 1; i <= addr::kEquipSlotCount; ++i) {
    const uint32_t item =
        mem::ReadDword(list + static_cast<uint32_t>(i - 1) * 4, 0);
    if (item == 0) continue;

    const int32_t quality = static_cast<int32_t>(
        mem::ReadDword(item + addr::kOffEquipAttr, 0xFFFFFFFFu));
    if (quality < 0 || quality >= config::kQualityCount) continue;

    const uint32_t name_ptr = mem::ReadDword(item + addr::kOffEquipName, 0);
    const std::wstring name =
        name_ptr != 0 ? mem::ReadWideString(name_ptr, 48) : std::wstring();

    ++r.scanned;

    if (!filters.empty() && NameFiltered(name, filters)) continue;

    const int32_t slot = i - 1 + addr::kEquipSlotOffset;
    switch (cfg.equip_action[quality]) {
      case config::kSell:
        packet::SellItem(slot, 1);
        ++r.sold[quality];
        break;
      case config::kDisassemble:
        packet::Disassemble(slot);
        ++r.disassembled[quality];
        break;
      default:
        break;  // 保留
    }
  }

  // 处理完毕后整理人物背包装备栏（参考/补充.txt：组包整理背包(1,0)）。
  packet::OrganizeBag(1, 0);

  {
    std::lock_guard<std::mutex> lock(g_result_mutex);
    g_last = r;
  }

  if (notice::Available()) {
    const wchar_t* names[config::kQualityCount] = {
        L"白装", L"蓝装", L"紫装", L"粉装", L"史诗", L"异界"};
    notice::Send(L"本次一键处理结果");
    for (int q = 0; q < config::kQualityCount; ++q) {
      if (r.sold[q] == 0 && r.disassembled[q] == 0) continue;
      wchar_t buf[128] = {0};
      _snwprintf_s(buf, _TRUNCATE, L"卖物-%s：%d 件  分解-%s：%d 件",
                   names[q], r.sold[q], names[q], r.disassembled[q]);
      notice::Send(buf);
    }
  }
}

}  // namespace

void ProcessAsync() {
  if (g_busy.exchange(true)) return;  // 防重复执行
  gthread::Post([] {
    ProcessCore();
    g_busy.store(false);
  });
}

bool Busy() { return g_busy.load(); }

Result LastResult() {
  std::lock_guard<std::mutex> lock(g_result_mutex);
  Result r = g_last;
  r.busy = g_busy.load();
  return r;
}

}  // namespace equip
