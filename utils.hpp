#pragma once

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <thread>

// Range: [min, max)
int randint(int min, int max) { return min + (rand() % (max - min)); }

void sleep(int seconds) {
  auto duration = std::chrono::seconds(seconds);
  // std::this_thread::sleep_for(duration);
  // std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

namespace process {
struct Rank {
} rank;
} // namespace process

std::ostream &operator<<(std::ostream &s, const process::Rank &) {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  s << "[" << rank << "] ";
  return s;
}

std::mutex print;
