// NFLog.cpp - NF 插件诊断日志
// 每行打开-追加-关闭，进程崩溃也不丢已写内容。
#include "NFLog.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace nflog {
namespace {

std::wstring g_path;
std::mutex g_mutex;
volatile LONG g_enabled = 1;  // 1 = 写日志（默认），0 = 完全静默

}  // namespace

void Init(const std::wstring& dir) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!dir.empty() && dir.back() != L'\\' && dir.back() != L'/')
    g_path = dir + L"\\NF.log";
  else
    g_path = dir + L"NF.log";
}

void SetEnabled(bool enabled) {
  InterlockedExchange(&g_enabled, enabled ? 1 : 0);
}

void Write(const wchar_t* fmt, ...) {
  if (InterlockedCompareExchange(&g_enabled, 0, 0) == 0)
    return;
  std::lock_guard<std::mutex> lock(g_mutex);
  if (g_path.empty()) return;

  SYSTEMTIME t = {};
  GetLocalTime(&t);
  wchar_t head[64] = {0};
  _snwprintf_s(head, _TRUNCATE, L"[%02d:%02d:%02d.%03d] ", t.wHour,
               t.wMinute, t.wSecond, t.wMilliseconds);

  wchar_t body[1024] = {0};
  va_list args;
  va_start(args, fmt);
  _vsnwprintf_s(body, _TRUNCATE, fmt, args);
  va_end(args);

  FILE* f = nullptr;
  if (_wfopen_s(&f, g_path.c_str(), L"a, ccs=UTF-16LE") != 0 || !f) return;
  fwprintf(f, L"%s%s\r\n", head, body);
  fclose(f);
}

}  // namespace nflog
