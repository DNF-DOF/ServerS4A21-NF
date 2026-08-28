// NFAddresses.h - S4A21 游戏基址与偏移常量表
// 数据来源: 参考/新绝对地址.txt (易语言常量, 十进制)
// 所有值为假定基址 0x00400000 的 VA，运行时经 ClientAddress() 路由。
#pragma once

#include <cstdint>

namespace addr {

// ---------------------------------------------------------------------------
// 绝对地址 (VA, 假定基址 0x00400000)
// ---------------------------------------------------------------------------
constexpr uintptr_t kPlayerBase   = 62119864;   // 人物基址  0x3B3DFB8
constexpr uintptr_t kMapBase      = 61196720;   // 地图基址  0x3A5C9B0 (房间编号)
constexpr uintptr_t kShopBase     = 61196728;   // 商店基址  0x3A5C9B8
constexpr uintptr_t kRoomNoBase   = 61196720;   // 房间编号  0x3A5C9B0
constexpr uintptr_t kBagBase      = 61196732;   // 背包基址  0x3A5C9BC
constexpr uintptr_t kScoreBase    = 61189444;   // 评分基址  0x3A5AD44
constexpr uintptr_t kSendBase     = 62377700;   // 发包基址  0x3B7CEE4
constexpr uintptr_t kDecryptBase  = 62428696;   // 解密基址  0x3B89618
constexpr uintptr_t kAutoPickFlag = 26326926;   // 自动捡物标志  0x191B78E
constexpr uintptr_t kGmPermissionMode = 0x01763ED0;  // GM权限模式（代码段，开启即赋予GM权限）
constexpr uint32_t kOffTimeBase  = 2138148;    // 时间基址(作为偏移使用)  0x20A0E4

// ---------------------------------------------------------------------------
// 技能全屏（参考 86-jp 已验证 + adddemo sub_40DAEC 逐值复刻）
// 数据来源: 参考/新绝对地址.txt + adddemo(S4A21-86CN.exe) 分析 + 实机测试。
// ---------------------------------------------------------------------------
constexpr uintptr_t kSkillCall       = 23250400;  // 技能CALL  0x162C5E0
constexpr uint32_t  kOffCamp         = 1872;      // 实体 -> 阵营  0x750
constexpr uint32_t  kOffMonsterHp    = 13276;     // 实体 -> 血量  0x33DC
constexpr uint32_t  kSkillSlotSize   = 4096;      // 技能参数槽大小（分配一次复用）
constexpr uint32_t  kSkillSlotSelf   = 0x00;      // 槽: 当前玩家对象
constexpr uint32_t  kSkillSlotCode   = 0x04;      // 槽: 技能代码
constexpr uint32_t  kSkillSlotDamage = 0x08;      // 槽: 伤害
constexpr uint32_t  kSkillSlotX      = 0x18;      // 槽: 目标X
constexpr uint32_t  kSkillSlotY      = 0x1C;      // 槽: 目标Y
constexpr uint32_t  kSkillSlotZ      = 0x20;      // 槽: 目标Z
constexpr uint32_t  kSkillSlotMaxTgt = 0x58;      // 槽: 目标数上限(demo 写 0xFFFF)
constexpr uint32_t  kSkillSlotExtra  = 0x5C;      // 槽: 附加(demo 写 0xFFFF)

// 技能全屏目标过滤（demo sub_40D75E 逐值复刻）：类型 + 阵营双白名单。
constexpr int32_t kSkillTargetTypes[] = {529, 273, 545};        // 目标实体类型
constexpr int32_t kSkillTargetCamps[] = {100, 110, 120, 101, 50};  // 敌对阵营

// ---------------------------------------------------------------------------
// CALL 地址 (VA)
// ---------------------------------------------------------------------------
constexpr uintptr_t kSendCall     = 41215696;   // 发包CALL  0x274E6D0
constexpr uintptr_t kEncryptCall  = 41210592;   // 加密包CALL(高层入口)  0x274D2E0
constexpr uintptr_t kEncryptRawCall = 91754473; // 加密写缓冲(底层统一入口)  0x5790FE9
constexpr uintptr_t kBufferCall   = 41210336;   // 缓冲CALL  0x274D1E0
constexpr uintptr_t kChangeMapCall = 12788240;  // 过图CALL  0xC32210
constexpr uintptr_t kNoticeHorn   = 27657760;   // 喇叭公告CALL  0x1A60620

// ---------------------------------------------------------------------------
// 偏移
// ---------------------------------------------------------------------------
constexpr uint32_t kOffMap        = 184;        // 人物 -> 地图对象  0xB8
constexpr uint32_t kOffListHead   = 192;        // 地图对象 -> 实体列表首地址  0xC0
constexpr uint32_t kOffListTail   = 196;        // 地图对象 -> 实体列表尾地址  0xC4
constexpr uint32_t kOffEntityType = 148;        // 实体 -> 类型  0x90 (289 = 掉落物)
constexpr uint32_t kOffEntityId   = 152;        // 实体 -> 加密ID结构  0x94
constexpr uint32_t kOffPickupAddress = 156;    // 组包拾取偏移(实体->解密物品地址入包)  0xA0
constexpr uint32_t kOffDir        = 168;        // 实体 -> 坐标结构指针  0xA8
constexpr uint32_t kOffPosX       = 440;        // 人物 -> X  0x1B8
constexpr uint32_t kOffPosY       = 444;        // 人物 -> Y  0x1BC
constexpr uint32_t kOffPosZ       = 448;        // 人物 -> Z  0x1C0
constexpr uint32_t kOffEquipAttr  = 0x160;     // 装备 -> 品质属性
constexpr uint32_t kOffEquipName  = 36;        // 装备 -> 名称指针  0x24
constexpr uint32_t kOffBagList    = 88;        // 背包 -> 列表首地址  0x58
constexpr uint32_t kOffBagEquip   = 36;        // 列表首地址 -> 装备栏起始  0x24

// ---------------------------------------------------------------------------
// 常量值
// ---------------------------------------------------------------------------
constexpr int32_t  kEntityTypeDrop   = 289;         // 掉落物实体类型
constexpr int32_t  kPickFlagSuck     = 1300955444;  // 吸物模式写入值
constexpr int32_t  kPickFlagPacket   = 1301004336;  // 发包模式写入值
constexpr int32_t  kEquipSlotOffset  = 9;           // 装备栏索引 -> 背包格子索引
constexpr int32_t  kEquipSlotCount   = 56;          // 装备栏格子数

// 包头
constexpr int32_t  kPacketHeadPickup    = 43;  // 拾取
constexpr int32_t  kPacketHeadSell      = 22;  // 卖物
constexpr int32_t  kPacketHeadDisas     = 26;  // 分解
constexpr int32_t  kPacketHeadOrganize  = 20;  // 整理背包

}  // namespace addr
