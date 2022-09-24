#pragma once

#include <array>
#include <iostream>
#include <mpi.h>

struct Payload {
  int clock;
  int safe_place_id;
  int wine_amount;

  std::array<int, 3> serialize() const {
    return {clock, safe_place_id, wine_amount};
  }

  void deserialize(const std::array<int, 3> &serialized) {
    clock = serialized[0];
    safe_place_id = serialized[1];
    wine_amount = serialized[2];
  }

  Payload &&setClock(int clock) {
    this->clock = clock;
    return std::move(*this);
  }

  Payload &&setSafePlaceId(int safe_place_id) {
    this->safe_place_id = safe_place_id;
    return std::move(*this);
  }

  Payload &&setWineAmount(int wine_amount) {
    this->wine_amount = wine_amount;
    return std::move(*this);
  }
};

std::ostream &operator<<(std::ostream &s, const Payload &p) {
  return s << "Payload(clock: " << p.clock
           << ", safe_place_id: " << p.safe_place_id
           << ", wine_amount: " << p.wine_amount << ")";
}
