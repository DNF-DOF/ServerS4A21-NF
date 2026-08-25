// NFNotice.h - 游戏内公告输出（NF 自身调喇叭 CALL）
// 不依赖 GameNative 的 postChatNotice/postLoadNotice 导出，
// 直接走 NFAddresses.h 的 kNoticeHorn 喇叭 CALL，游戏线程安全投递。
// 原有 GameNative API 保留不动，NF 自身也生成一份公告。
#pragma once

#include <string>

namespace notice {

// 初始化（空实现保留，为后向兼容；任何线程可随时调 Send）。
void Bind();

// 喇叭 CALL 是否已可解析。
bool Available();

// 发送聊天栏公告（自动投递到游戏线程）。
// color=-1 为随机鲜艳颜色；notice_type 为喇叭类型，不传默认 38（原硬编码 30 可显式传）。
void Send(const std::wstring& text, int color = -1, int notice_type = 38);

// 发送加载公告（自动投递到游戏线程）。
// color=-1 为随机鲜艳颜色；notice_type 为喇叭类型，不传默认 38。
void SendLoad(const std::wstring& text, int color = -1, int notice_type = 38);

}  // namespace notice
