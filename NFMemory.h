// NFMemory.h - 进程内内存读写与游戏数据解密
// 地址经 ClientAddress() 路由，抗 ASLR。
#pragma once

#include <cstdint>
#include <string>

namespace mem {

// 假定基址。
constexpr uintptr_t kPreferredImageBase = 0x00400000;

// 将 preferred VA 转为运行时实际地址。
uintptr_t ClientAddress(uintptr_t preferredAddress);

// 判断地址是否可读。
bool IsReadable(uintptr_t address, size_t size = 4);

// 判断地址是否可写。
bool IsWritable(uintptr_t address, size_t size = 4);

// 安全读取 4 字节，失败返回 def。
uint32_t ReadDword(uintptr_t address, uint32_t def = 0);

// 安全读取 4 字节浮点，失败返回 def。
float ReadFloat(uintptr_t address, float def = 0.0f);

// 安全写入 4 字节，返回是否成功。
bool WriteDword(uintptr_t address, uint32_t value);

// 安全读取 size 字节到 buffer（调用方保证 buffer 足够），返回实际读取字节数（0=失败）。
size_t ReadBytes(uintptr_t address, void* buffer, size_t size);

// 安全写入 size 字节；代码段(PAGE_EXECUTE_READ)会临时提权后写入。返回是否成功。
bool WriteBytes(uintptr_t address, const void* data, size_t size);

// 读取宽字符串（最多 max_bytes 字节），失败返回空串。
std::wstring ReadWideString(uintptr_t address, size_t max_bytes);

// 游戏数据解密（对应易语言 超级解密）。失败返回 -1。
int32_t SuperDecrypt(uintptr_t address);

// 游戏数据加密（对应用户给的易语言"超级加密"）。
//   type < 0     → 视为"加密类型可空"，按 address & 0xF 走 4 种默认算法（0/4/8/12）
//   type ∈ {1..4} → 按加密类型 1/2/3/4 强制走指定算法
// 返回是否成功写入（地址可读+可写即视为成功）。
bool SuperEncrypt(uintptr_t address, int32_t value, int type = -1);

}  // namespace mem
