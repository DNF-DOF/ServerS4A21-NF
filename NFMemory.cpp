// NFMemory.cpp - 进程内内存读写与游戏数据解密
#include "NFMemory.h"

#include <windows.h>

#include <cstring>

#include "NFAddresses.h"

namespace mem {

uintptr_t ClientAddress(uintptr_t preferredAddress) {
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
  return base + (preferredAddress - kPreferredImageBase);
}

namespace {

bool QueryProtect(uintptr_t address, size_t size, DWORD mask) {
  if (address < 0x10000) return false;
  MEMORY_BASIC_INFORMATION mbi;
  if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == 0) {
    return false;
  }
  if (mbi.State != MEM_COMMIT) return false;
  if ((mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) return false;
  if ((mbi.Protect & mask) == 0) return false;
  const uintptr_t region_end =
      reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
  return address + size <= region_end;
}

constexpr DWORD kReadMask = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                            PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                            PAGE_EXECUTE_WRITECOPY;

constexpr DWORD kWriteMask = PAGE_READWRITE | PAGE_WRITECOPY |
                             PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

}  // namespace

bool IsReadable(uintptr_t address, size_t size) {
  return QueryProtect(address, size, kReadMask);
}

bool IsWritable(uintptr_t address, size_t size) {
  return QueryProtect(address, size, kWriteMask);
}

uint32_t ReadDword(uintptr_t address, uint32_t def) {
  if (!IsReadable(address, 4)) return def;
  return *reinterpret_cast<volatile uint32_t*>(address);
}

float ReadFloat(uintptr_t address, float def) {
  if (!IsReadable(address, 4)) return def;
  return *reinterpret_cast<volatile float*>(address);
}

bool WriteDword(uintptr_t address, uint32_t value) {
  if (!IsWritable(address, 4)) return false;
  *reinterpret_cast<volatile uint32_t*>(address) = value;
  return true;
}

size_t ReadBytes(uintptr_t address, void* buffer, size_t size) {
  if (!buffer || size == 0) return 0;
  if (!IsReadable(address, size)) return 0;
  __try {
    memcpy(buffer, reinterpret_cast<const void*>(address), size);
    return size;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return 0;
  }
}

bool WriteBytes(uintptr_t address, const void* data, size_t size) {
  if (!data || size == 0) return false;
  if (!IsWritable(address, size)) {
    // 代码段常仅 PAGE_EXECUTE_READ：临时改保护后写入（等效 WriteProcessMemory）。
    DWORD old_protect = 0;
    if (!VirtualProtect(reinterpret_cast<LPVOID>(address), size,
                        PAGE_EXECUTE_READWRITE, &old_protect))
      return false;
    bool ok = false;
    __try {
      memcpy(reinterpret_cast<void*>(address), data, size);
      ok = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      ok = false;
    }
    DWORD restored = 0;
    VirtualProtect(reinterpret_cast<LPVOID>(address), size, old_protect,
                  &restored);
    return ok;
  }
  __try {
    memcpy(reinterpret_cast<void*>(address), data, size);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

std::wstring ReadWideString(uintptr_t address, size_t max_bytes) {
  if (max_bytes < 2) return std::wstring();
  if (!IsReadable(address, 2)) return std::wstring();
  const size_t max_chars = max_bytes / sizeof(wchar_t);
  std::wstring out;
  out.reserve(max_chars);
  const wchar_t* p = reinterpret_cast<const wchar_t*>(address);
  for (size_t i = 0; i < max_chars; ++i) {
    if (!IsReadable(address + i * sizeof(wchar_t), sizeof(wchar_t))) break;
    const wchar_t c = p[i];
    if (c == L'\0') break;
    out.push_back(c);
  }
  return out;
}

// 对应易语言 超级解密：
//   eax  = [address]                      加密ID
//   ecx8 = [address + 4]                  密文
//   esi  = [解密基址]
//   edx  = [ (eax >> 16) * 4 + esi + 36 ]
//   key  = [ (eax & 0xFFFF) * 4 + edx + 8468 ]
//   k16  = (uint16)key;  mask = (k16 << 16) | k16;  return mask ^ ecx8
int32_t SuperDecrypt(uintptr_t address) {
  if (!IsReadable(address, 8)) return -1;

  const uint32_t enc_id = ReadDword(address, 0xFFFFFFFFu);
  if (enc_id == 0xFFFFFFFFu) return -1;

  const uint32_t cipher = ReadDword(address + 4, 0xFFFFFFFFu);
  if (cipher == 0xFFFFFFFFu) return -1;

  const uint32_t table = ReadDword(ClientAddress(addr::kDecryptBase), 0);
  if (table == 0) return -1;

  const uint32_t node =
      ReadDword(table + ((enc_id >> 16) * 4) + 36, 0xFFFFFFFFu);
  if (node == 0xFFFFFFFFu || node == 0) return -1;

  const uint32_t key =
      ReadDword(node + ((enc_id & 0xFFFFu) * 4) + 8468, 0xFFFFFFFFu);
  if (key == 0xFFFFFFFFu) return -1;

  const uint32_t k16 = key & 0xFFFFu;
  const uint32_t mask = (k16 << 16) | k16;
  return static_cast<int32_t>(mask ^ cipher);
}

// 对应易语言 超级加密（写）：
//   edi  = [address]                                  种子
//   ecx  = [ [解密基址] + (edi>>16)*4 + 36 ]
//   ebx  = ecx + (edi & 0xFFFF)*4 + 8468
//   ecx  = [ebx];  k16 = ecx & 0xFFFF
//   eax  = (k16<<16) | k16                            mask
//   edx  = ecx & 0xFFFF                              (原始 edx)
//   按 type / (address & 0xF) 计算 si，再：
//     [ebx + 2] = (uint16)(si ^ edx)                 key 表演化（与游戏一致）
//     [address + (type != 4 ? 4 : 8)] = eax ^ value  密文
// 返回是否成功写入（地址可写即视为成功）。
bool SuperEncrypt(uintptr_t address, int32_t value, int type) {
  if (!IsReadable(address, 4)) return false;

  const uint32_t enc_id = ReadDword(address, 0);
  const uint32_t table = ReadDword(ClientAddress(addr::kDecryptBase), 0);
  if (table == 0) return false;
  const uint32_t node = ReadDword(table + ((enc_id >> 16) * 4) + 36, 0);
  if (node == 0) return false;
  const uint32_t ebx = node + ((enc_id & 0xFFFFu) * 4) + 8468;
  if (!IsReadable(ebx, 4)) return false;
  const uint32_t key = ReadDword(ebx, 0);

  const uint32_t k16 = key & 0xFFFFu;
  const uint32_t eax = (k16 << 16) | k16;   // mask = (k16 << 16) | k16
  const uint32_t edx0 = key & 0xFFFFu;      // 原始 edx（k16）

  const uint32_t v = static_cast<uint32_t>(value);
  uint16_t si = 0;
  if (type < 0) {
    // 加密类型可空：按 address & 0xF 选择 4 种默认算法（0/4/8/12）
    switch (static_cast<int>(address & 0xF)) {
      case 0:
        si = static_cast<uint16_t>(((v >> 16) - edx0 + v) & 0xFFFFu);
        break;
      case 4:
        si = static_cast<uint16_t>(((v & 0xFFFFu) - (v >> 16)) & 0xFFFFu);
        break;
      case 8:
        si = static_cast<uint16_t>(((v >> 16) * v) & 0xFFFFu);
        break;
      case 12:
        si = static_cast<uint16_t>(((v >> 16) + v + edx0) & 0xFFFFu);
        break;
      default:
        return false;
    }
  } else {
    switch (type) {
      case 1:
        si = static_cast<uint16_t>(v & 0xFFu);
        break;
      case 2:
        si = static_cast<uint16_t>(v & 0xFFFFu);
        break;
      case 3:
        si = static_cast<uint16_t>(((v >> 16) + v) & 0xFFFFu);
        break;
      case 4:
        si = static_cast<uint16_t>(((v >> 16) + (v & 0xFFFFu)) & 0xFFFFu);
        break;
      default:
        return false;
    }
  }

  const uint32_t xored = (static_cast<uint32_t>(si) ^ edx0) & 0xFFFFu;

  // 写 key 表（ebx+2 起 2 字节），与游戏自身加密一致，保持 key 演化同步。
  if (IsWritable(ebx + 2, 2))
    *reinterpret_cast<volatile uint16_t*>(ebx + 2) = static_cast<uint16_t>(xored);

  // 写密文：mask ^ value 到 address + (type != 4 ? 4 : 8)
  const uintptr_t write_addr = address + (type != 4 ? 4 : 8);
  return WriteDword(write_addr, eax ^ v);
}

}  // namespace mem
