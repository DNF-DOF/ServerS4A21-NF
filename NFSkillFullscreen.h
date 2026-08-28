// NFSkillFullscreen.h - 技能全屏
// 遍历地图对象表，过滤怪物目标后写参数槽并调用技能CALL(0x162C5E0)，
// 使技能直接命中全图怪物（参考 86-jp-DnfHelper 已验证实现 + S4A21 demo 复刻）。
// 参数（技能代码/伤害/频率/每轮目标数）来自 NF.ini [技能全屏]，热重载生效。
#pragma once

namespace skillfs {

// 当前是否处于技能全屏开启状态。
bool Enabled();

// 切换开关（任意线程；Tick 在游戏线程生效，勾选后下一 Tick 立即执行一轮）。
void SetEnabled(bool enabled);

// 控制线程周期调用（100ms 节拍），按配置频率执行技能CALL 轮；
// 调用方必须已在游戏线程泵消息（与 grade::Tick / pickup::Tick 同位置）。
void Tick();

}  // namespace skillfs
