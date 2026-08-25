// NFPacket.cpp - 游戏发包封装
// 调用约定对齐 CE 反汇编（DNF.exe+234D2E0 / 234D1E0 / 5390FE9）：
//   this = [kSendBase]（1层引用，不再 [[base]]-8）
//   BeginPacket：push 0 / push header / ecx=this / call kBufferCall
//   WriteField：检查[this+8]、累加[this+0x2BCC2C]、push length/push &值
//                ecx=this / call kEncryptRawCall（统一底层入口，不按长度选偏移）
//   EndPacket：ecx=this / call kSendCall
#include "NFPacket.h"

#include <windows.h>

#include "NFAddresses.h"
#include "NFMemory.h"

namespace packet {
namespace {

// 发包对象是否有效。
bool SendObjectReady() {
  return mem::ReadDword(mem::ClientAddress(addr::kSendBase), 0) != 0;
}

// [kSendBase]（1层解引用）= 发包管理器 this 指针。
// 失败返回 0。
inline uint32_t FetchSendThis() {
  return mem::ReadDword(mem::ClientAddress(addr::kSendBase), 0);
}

}  // namespace

// 缓冲CALL（BuildPacket）：push 0/包头，ecx=this，调用客户端高层函数。
void BeginPacket(int32_t head) {
  const uint32_t send_this = FetchSendThis();
  if (send_this == 0) return;
  const uintptr_t call_addr = mem::ClientAddress(addr::kBufferCall);

  __try {
    __asm {
      push 0
      push head
      mov ecx, send_this
      call call_addr
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
}

// 加密写buffer：参考易语言"加密call"子程序：
//   长度=1 → CALL = kEncryptCall + 0
//   长度=2 → CALL = kEncryptCall + 48
//   长度=4 → CALL = kEncryptCall + 96
//   push value（值本身，不是指针）
//   ecx = [[kSendBase]]
//   call eax
void WriteField(int32_t value, int32_t length) {
  if (length != 1 && length != 2 && length != 4) return;
  const uint32_t send_this = FetchSendThis();
  if (send_this == 0) return;

  uintptr_t call_addr = mem::ClientAddress(addr::kEncryptCall);
  if (length == 2)       call_addr += 48;
  else if (length == 4)  call_addr += 96;

  const int32_t val = value;
  __try {
    __asm {
      push val
      mov  ecx, send_this
      call call_addr
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
}

// 发包CALL（SendBuffer）：ecx=this 调用高层入队+发送函数。
void EndPacket() {
  const uint32_t send_this = FetchSendThis();
  if (send_this == 0) return;
  const uintptr_t call_addr = mem::ClientAddress(addr::kSendCall);

  __try {
    __asm {
      mov ecx, send_this
      call call_addr
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
}

void Pickup(int32_t item_addr, int32_t px, int32_t py,
            int32_t ix, int32_t iy) {
  BeginPacket(addr::kPacketHeadPickup);
  WriteField(item_addr, 2);
  WriteField(0, 1);
  WriteField(1, 1);
  WriteField(px, 2);
  WriteField(py, 2);
  WriteField(0, 2);
  WriteField(ix, 2);
  WriteField(iy, 2);
  WriteField(0, 2);
  WriteField(0, 2);
  EndPacket();
}

void SellItem(int32_t slot, int32_t) {
  BeginPacket(addr::kPacketHeadSell);
  WriteField(0, 1);
  WriteField(slot, 2);
  WriteField(1, 2);
  WriteField(317, 4);
  WriteField(slot + 1, 4);
  EndPacket();
}

void Disassemble(int32_t slot) {
  BeginPacket(addr::kPacketHeadDisas);
  WriteField(slot, 2);
  WriteField(0, 1);
  WriteField(65535, 2);
  WriteField(317, 4);
  EndPacket();
}

// 组包整理背包：参考/补充.txt
void OrganizeBag(int32_t id, int32_t bag_type) {
  BeginPacket(addr::kPacketHeadOrganize);
  WriteField(bag_type, 1);
  WriteField(id, 1);
  EndPacket();
}

void ForceChangeRoom(int32_t direction) {
  // this = [[[[kMapBase] + 0x20A024] + 0x8C]（参考易语言置入代码）
  uint32_t p = mem::ReadDword(mem::ClientAddress(addr::kMapBase), 0);
  if (p == 0) return;
  p = mem::ReadDword(p + 0x20A024, 0);
  if (p == 0) return;
  const uint32_t this_ptr = mem::ReadDword(p + 0x8C, 0);
  if (this_ptr == 0) return;

  const uintptr_t call_addr = mem::ClientAddress(addr::kChangeMapCall);
  __try {
    __asm {
      mov  ecx, this_ptr
      push -1
      push -1
      push 0
      push 0
      push 0
      push 0
      push 0
      push direction
      mov  eax, call_addr
      call eax
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
}

}  // namespace packet
