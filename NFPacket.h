// NFPacket.h - 游戏发包封装
// 说明：所有接口必须在游戏主线程调用（通过 NFGameThread 派发）。
#pragma once

#include <cstdint>

namespace packet {

// 底层：写入包头（缓冲CALL）。
void BeginPacket(int32_t head);

// 底层：写入一个字段。length 仅支持 1 / 2 / 4 字节。
void WriteField(int32_t value, int32_t length);

// 底层：提交发送（发包CALL）。
void EndPacket();

// 业务：拾取指定掉落物。
//   item_addr: 物品地址（实体指针或ID值，低16位入包）
//   px, py: 人物坐标
//   ix, iy: 掉落物原始坐标
void Pickup(int32_t item_addr, int32_t px, int32_t py,
            int32_t ix, int32_t iy);

// 业务：卖出背包格子 slot 的 count 件物品。
void SellItem(int32_t slot, int32_t count = 1);

// 业务：分解背包格子 slot 的物品。
void Disassemble(int32_t slot);

// 业务：整理背包。id: 1=装备 2=消耗品 3=材料。bag_type: 0=人物 7=宠物。
void OrganizeBag(int32_t id = 1, int32_t bag_type = 0);

// 业务：强制过图（顺图CALL）。direction: 0=左 1=右 2=上 3=下。
void ForceChangeRoom(int32_t direction);

}  // namespace packet
