// NFNativeUi.h - 原生 XUI 配置面板
// 复用 EquipmentSwap 已标定的 S4A21 窗口创建 / 控件查找 / 文本写入地址，
// 去掉物品格子与页签逻辑，仅保留按钮 + 文本控件。
#pragma once

#include <array>
#include <string>

#include <windows.h>

namespace nf_ui {

enum class Command {
  TogglePickup,      // 自动拾取 开/关
  ToggleAutoGrade,   // 自动评分 开/关（功能实现后续提供，这里仅开关占位）
  ToggleGmMode,      // GM模式 开/关（修补 GM 权限判定）
  ToggleSkillFullscreen,  // 技能全屏 开/关
  SetPickupMode,     // 拾取模式，argument = 0(吸物) 1(发包)
  SetFilterMode,     // 过滤模式，argument = 0(黑名单) 1(白名单)
  SetMoveMode,       // 顺图模式，argument = 0(坐标) 1(强制)
  ProcessEquip,      // 一键装备处理
  MapMove,           // 顺图，argument = mapmove::Direction
  SetQualityAction,  // 品质处理，argument = quality*3 + action(0保留 1卖 2分)
  Diagnostic,
};

using CommandHandler = void (*)(Command command, int argument);

// 面板显示状态。动态文字只写 CNUIControlText（按钮文字无法运行时改写，
// S4A21 上按钮 vtable 写入路径不可用，按钮一律静态命名，状态显示在标签里）。
struct UiState {
  std::wstring filterIdsText;     // 过滤ID只读展示（编辑走 NF.ini）
  std::wstring nameFilterText;    // 名称过滤只读展示
  std::wstring statusText;

  // 控件选中状态（呼出面板时，模拟点击正确项让选中=当前配置）。
  // -1 表示"本次刷新不改动"，其他值表示匹配到对应 controlId 后模拟点击一次。
  bool pickupEnabled = false;       // kPickupButton 是否勾选
  bool autoGradeEnabled = false;    // kAutoGradeButton 是否勾选（自动评分开关占位）
  bool gmEnabled = false;           // kGmButton 是否勾选（GM模式开关）
  bool skillFullscreenEnabled = false;  // kSkillFullscreenButton 是否勾选
  int pickupMode = -1;              // 0吸物 1发包 / -1不刷新
  int filterMode = -1;              // 0黑名单 1白名单 / -1不刷新
  int moveMode = -1;                // 0坐标 1强制 / -1不刷新
  int equipActions[6] = {-1,-1,-1,-1,-1,-1};  // 0保留 1卖 2分 / -1不刷新
};

// 校验客户端地址并安装（失败返回 false，界面功能不可用）。
bool Install(const wchar_t* layoutPath, int windowId, CommandHandler handler);
void Uninstall();
void* __cdecl WindowFactory(int windowId, void* manager, void* context);

// 开关面板（游戏线程调用）。
void Toggle();
void Close();
bool IsOpen();

// 游戏线程周期调用：窗口已关时回收、应用待刷新状态。
void Poll();

// 更新显示状态（任意线程调用，游戏线程在 Poll 中生效）。
void Refresh(const UiState& state);

}  // namespace nf_ui
