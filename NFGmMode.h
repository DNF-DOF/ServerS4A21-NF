// NFGmMode.h - GM 模式开关（修补 GM 权限判定）
// 移植自 参考/新绝对地址.txt 的 #GM权限模式 与 易语言 GM 切换子程序。
#pragma once

namespace gm {

// 由面板“GM模式”开关点击触发：每次调用翻转一次。
//   开 -> 先记录原 16 字节，再写入补丁（改写 GM 权限判定逻辑）；
//   关 -> 写回原 16 字节，还原判定。
// 返回翻转后的开关状态（true=已开启）；失败时保持关闭并返回 false。
bool Toggle();

// 当前开关状态（用于状态栏展示）。
bool Enabled();

}  // namespace gm
