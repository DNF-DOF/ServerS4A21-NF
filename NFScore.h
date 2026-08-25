// NFScore.h - 自动评分（写入战力评分）
// 移植自 参考/新绝对地址.txt 的 #评分基址 与 易语言 超级加密 子程序。
#pragma once

namespace grade {

// 由控制线程周期调用。auto_grade_enabled 即面板“自动评分”开关状态。
// 开启后预热 5 秒；之后每 5 秒轮询一次：
//   不在副本内 -> 直接返回；
//   在副本且评分低于阈值 -> 加密写入固定值（默认 9999999）。
void Tick(bool auto_grade_enabled);

}  // namespace grade
