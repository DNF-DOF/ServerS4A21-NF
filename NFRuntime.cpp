// NFRuntime.cpp - 运行时数值数据
#include "NFRuntime.h"

#include "NFAddresses.h"
#include "NFMemory.h"

namespace runtime {

uint32_t PlayerPtr() {
  const uint32_t p = mem::ReadDword(mem::ClientAddress(addr::kPlayerBase), 0);
  if (p == 0 || !mem::IsReadable(p, 4)) return 0;
  return p;
}

uint32_t MapPtr() {
  const uint32_t player = PlayerPtr();
  if (player == 0) return 0;
  const uint32_t m = mem::ReadDword(player + addr::kOffMap, 0);
  if (m == 0 || !mem::IsReadable(m, 4)) return 0;
  return m;
}

int32_t RoomNumber() {
  return static_cast<int32_t>(mem::ReadDword(mem::ClientAddress(addr::kRoomNoBase), 0));
}

int32_t PosX() {
  const uint32_t p = PlayerPtr();
  if (p == 0) return 0;
  return static_cast<int32_t>(mem::ReadDword(p + addr::kOffPosX, 0));
}

int32_t PosY() {
  const uint32_t p = PlayerPtr();
  if (p == 0) return 0;
  return static_cast<int32_t>(mem::ReadDword(p + addr::kOffPosY, 0));
}

int32_t PosZ() {
  const uint32_t p = PlayerPtr();
  if (p == 0) return 0;
  return static_cast<int32_t>(mem::ReadDword(p + addr::kOffPosZ, 0));
}

int32_t DropCount() {
  const uint32_t m = MapPtr();
  if (m == 0) return 0;
  const uint32_t head = mem::ReadDword(m + addr::kOffListHead, 0);
  const uint32_t tail = mem::ReadDword(m + addr::kOffListTail, 0);
  if (head == 0 || tail <= head) return 0;

  int32_t count = 0;
  for (uint32_t it = head; it + 4 <= tail; it += 4) {
    const uint32_t obj = mem::ReadDword(it, 0);
    if (obj == 0) continue;
    if (static_cast<int32_t>(mem::ReadDword(obj + addr::kOffEntityType, 0)) ==
        addr::kEntityTypeDrop) {
      ++count;
    }
  }
  return count;
}

uint32_t EquipListBase() {
  const uint32_t bag = mem::ReadDword(mem::ClientAddress(addr::kBagBase), 0);
  if (bag == 0) return 0;
  const uint32_t head = mem::ReadDword(bag + addr::kOffBagList, 0);
  if (head == 0) return 0;
  const uint32_t list = head + addr::kOffBagEquip;
  if (!mem::IsReadable(list, 4)) return 0;
  return list;
}

}  // namespace runtime

namespace state {

bool PlayerReady() { return runtime::PlayerPtr() != 0; }

bool InDungeon() {
  return static_cast<int32_t>(mem::ReadDword(mem::ClientAddress(addr::kMapBase), 0)) > 0;
}

}  // namespace state
