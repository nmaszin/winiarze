#pragma once

#include <array>
#include <mpi.h>

template <typename T, std::size_t N> struct Payload {
  virtual MPI_Datatype getType() const = 0;
  virtual std::array<T, N> serialize() const = 0;
  virtual void deserialize(const std::array<T, N> &serialized) = 0;
};

struct ObserverMessagePayload : public Payload<unsigned, 4> {
  unsigned winemaker_id;
  unsigned student_id;
  unsigned safe_place_id;
  unsigned wine_amount;

  MPI_Datatype getType() const override { return MPI_UNSIGNED; }

  std::array<unsigned, 4> serialize() const override {
    return {winemaker_id, student_id, safe_place_id, wine_amount};
  }

  void deserialize(const std::array<unsigned, 4> &serialized) override {
    winemaker_id = serialized[0];
    student_id = serialized[1];
    safe_place_id = serialized[2];
    wine_amount = serialized[3];
  }
};
