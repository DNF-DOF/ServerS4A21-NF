// NFNotice.cpp - 游戏内公告输出（NF 自身调喇叭 CALL）
// 完全对齐 GameNativeNotice.cpp 的 CallOfficialNotice：
//   chat = [[kChatManagerPtr]] → kGetChatWindow(manager)
//   kNoticeHorn(thiscall ecx=chat, text, color, kNoticeType=30, 0, 0, 0, 0)
#include "NFNotice.h"

#include <windows.h>

#include <mutex>

#include "NFAddresses.h"
#include "NFGameThread.h"
#include "NFMemory.h"

namespace notice {
namespace {

// 和 GameNativeNotice 保持一致的基址/偏移/CALL
constexpr uintptr_t kChatManagerPtr = 0x03A5C9B8;
constexpr uintptr_t kGetChatWindow  = 0x00A9AF50;
constexpr int kNoticeType = 30;

// 预定义鲜艳颜色，公告随机选一个。
struct Rgb { int r, g, b; };
constexpr Rgb kRandomColors[] = {
  {255, 72, 220}, {72, 220, 255}, {255, 220, 72}, {72, 255, 128},
  {255, 128, 72}, {220, 72, 255}, {255, 72, 72},  {72, 128, 255},
};
constexpr size_t kRandomColorCount = sizeof(kRandomColors) / sizeof(kRandomColors[0]);

// RGB 顺序：对齐 GameNative 的 chatRgb（= COLORREF BGR：0x00BBGGRR）
int ChatRgb(int r, int g, int b) {
  r &= 0xFF; g &= 0xFF; b &= 0xFF;
  return (b << 16) | (g << 8) | r;
}

int RandomColor() {
  const Rgb& c = kRandomColors[GetTickCount() % kRandomColorCount];
  return ChatRgb(c.r, c.g, c.b);
}

// thiscall：ecx=chat，栈参数从右向左 push，最后一个是 unsigned char。
// 8 参数：this(ecx) + text, color, noticeType(30), 0, 0, 0, 0 → 7 项入栈
// thiscall 由被调方清栈（ret 7*4=28），调用方不用 add esp。
void* GetChatWindow() {
  const uint32_t manager_ptr = mem::ClientAddress(kChatManagerPtr);
  if (manager_ptr == 0) return nullptr;
  void* manager = *reinterpret_cast<void* const volatile*>(manager_ptr);
  if (manager == nullptr) return nullptr;
  // IsBadReadPtr 等价检查（NF 没 expose，用 VirtualQuery 过一下）
  MEMORY_BASIC_INFORMATION mbi = {};
  if (VirtualQuery(manager, &mbi, sizeof(mbi)) < sizeof(mbi)) return nullptr;
  if ((mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                      PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                      PAGE_EXECUTE_WRITECOPY)) == 0) return nullptr;
  if (reinterpret_cast<uintptr_t>(mbi.BaseAddress) + 0x50 >
      reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize &&
      reinterpret_cast<uintptr_t>(manager) + 0x50 >
      reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize) {
    return nullptr;
  }
  const uintptr_t get_chat = mem::ClientAddress(kGetChatWindow);
  if (get_chat == 0) return nullptr;

  void* chat = nullptr;
  __try {
    __asm {
      mov  ecx, manager
      call get_chat
      mov  chat, eax
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return nullptr;
  }

  if (chat == nullptr) return nullptr;
  MEMORY_BASIC_INFORMATION mbi2 = {};
  if (VirtualQuery(chat, &mbi2, sizeof(mbi2)) < sizeof(mbi2)) return nullptr;
  if ((mbi2.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                       PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                       PAGE_EXECUTE_WRITECOPY)) == 0) return nullptr;
  return chat;
}

void CallHornNotice(const wchar_t* text, int color, int notice_type) {
  void* chat = GetChatWindow();
  if (chat == nullptr || text == nullptr) return;

  const uintptr_t call_addr = mem::ClientAddress(addr::kNoticeHorn);
  if (call_addr == 0) return;

  __try {
    __asm {
      push 0           ; arg8 unsigned char h
      push 0           ; arg7 unsigned char g
      push 0           ; arg6 int f
      push 0           ; arg5 int e
      push notice_type ; arg4 int d (喇叭类型：默认38，老样式显式传30)
      push color       ; arg3 int c
      push text        ; arg2 wchar_t* b
      mov  ecx, chat   ; thiscall this=chat (arg1)
      call call_addr
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
}

void PostText(const std::wstring& text, int color, int notice_type) {
  const std::wstring prefixed = std::wstring(L"DouBi:  ") + text;
  gthread::Post([prefixed, color, notice_type] {
    const int actual = (color == -1) ? RandomColor() : color;
    CallHornNotice(prefixed.c_str(), actual, notice_type);
  });
}

}  // namespace

void Bind() {
  // 空实现保留以兼容现有调用点。
}

bool Available() {
  return mem::ClientAddress(addr::kNoticeHorn) != 0 &&
         mem::ClientAddress(kChatManagerPtr) != 0 &&
         mem::ClientAddress(kGetChatWindow) != 0;
}

void Send(const std::wstring& text, int color, int notice_type) {
  PostText(text, color, notice_type);
}

void SendLoad(const std::wstring& text, int color, int notice_type) {
  PostText(text, color, notice_type);
}

}  // namespace notice
