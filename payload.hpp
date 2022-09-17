#pragma once

#include <array>
#include <mpi.h>

template <typename T, std::size_t N> struct Payload {
  virtual MPI_Datatype getType() const = 0;
  virtual std::array<T, N> serialize() const = 0;
  virtual void deserialize(const std::array<T, N> &serialized) = 0;

  virtual T getClock() const = 0;
  virtual void setClock(T value) = 0;
};

struct EntirePayload : public Payload<unsigned, 5> {
  unsigned clock;
  unsigned winemaker_pid;
  unsigned student_pid;
  unsigned safe_place_id;
  unsigned wine_amount;

  EntirePayload &setWinemakerPid(unsigned winemaker_pid) {
    this->winemaker_pid = winemaker_pid;
    return *this;
  }

  EntirePayload &setStudentPid(unsigned student_pid) {
    this->student_pid = student_pid;
    return *this;
  }

  EntirePayload &setSafePlaceId(unsigned safe_place_id) {
    this->safe_place_id = safe_place_id;
    return *this;
  }

  EntirePayload &setWineAmount(unsigned wine_amount) {
    this->wine_amount = wine_amount;
    return *this;
  }

  MPI_Datatype getType() const override { return MPI_UNSIGNED; }

  std::array<unsigned, 5> serialize() const override {
    return {clock, winemaker_pid, student_pid, safe_place_id, wine_amount};
  }

  void deserialize(const std::array<unsigned, 5> &serialized) override {
    clock = serialized[0];
    winemaker_pid = serialized[1];
    student_pid = serialized[2];
    safe_place_id = serialized[3];
    wine_amount = serialized[4];
  }

  unsigned getClock() const override { return clock; }

  void setClock(unsigned value) override { clock = value; };
};
