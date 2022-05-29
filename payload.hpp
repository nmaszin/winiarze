#pragma once

#include <array>
#include <mpi.h>

template <typename T, std::size_t N> struct Payload {
  virtual MPI_Datatype getType() const = 0;
  virtual std::array<T, N> serialize() const = 0;
  virtual void deserialize(const std::array<T, N> &serialized) = 0;
};

struct EntirePayload : public Payload<unsigned, 4> {
  unsigned winemaker_id;
  unsigned student_id;
  unsigned safe_place_id;
  unsigned wine_amount;

  EntirePayload &setWinemakerId(unsigned value) {
    winemaker_id = value;
    return *this;
  }

  EntirePayload &setStudentId(unsigned value) {
    student_id = value;
    return *this;
  }

  EntirePayload &setSafePlaceId(unsigned value) {
    safe_place_id = value;
    return *this;
  }
  EntirePayload &setWineAmount(unsigned value) {
    wine_amount = value;
    return *this;
  }

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

struct ClockOnlyPayload : public Payload<unsigned, 1> {
  unsigned clock;

  ClockOnlyPayload() = default;
  ClockOnlyPayload(unsigned clock) : clock(clock) {}

  MPI_Datatype getType() const override { return MPI_UNSIGNED; }

  std::array<unsigned, 1> serialize() const override { return {clock}; }

  void deserialize(const std::array<unsigned, 1> &serialized) override {
    clock = serialized[0];
  }
};
