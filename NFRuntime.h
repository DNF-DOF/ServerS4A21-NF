// NFRuntime.h - 运行时数值数据
// 地址经 ClientAddress() 路由，抗 ASLR。
#pragma once

#include <cstdint>

namespace runtime {

// 人物对象指针，无效返回 0。
uint32_t PlayerPtr();

// 地图对象指针（人物 + 地图偏移），无效返回 0。
uint32_t MapPtr();

// 当前房间编号。
int32_t RoomNumber();

// 人物坐标（内存中为整数型存储）。
int32_t PosX();
int32_t PosY();
int32_t PosZ();

// 当前地图掉落物数量。
int32_t DropCount();

// 背包装备列表首地址，无效返回 0。
uint32_t EquipListBase();

}  // namespace runtime

namespace state {

// 角色数据是否就绪（人物指针有效）。
bool PlayerReady();

// 是否在副本内（读 地图基址 整数值，城镇为 0，副本内为正数）。
bool InDungeon();

}  // namespace state
