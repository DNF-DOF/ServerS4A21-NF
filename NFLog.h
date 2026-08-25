// NFLog.h - NF 插件诊断日志（写到 DLL 目录 NF.log，UTF-16LE）
#pragma once

#include <string>

namespace nflog {

// 初始化日志目录（DllMain 中调用，之后才能写日志）。
void Init(const std::wstring& dir);

// 总开关：debug=0 时 Write 直接返回（不打开文件，不写磁盘）。默认 1（写）。
void SetEnabled(bool enabled);

// 写一行日志（时间戳 + 换行），失败静默；总开关关闭时也静默。
void Write(const wchar_t* fmt, ...);

}  // namespace nflog
