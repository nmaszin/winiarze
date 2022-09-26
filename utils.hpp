#pragma once

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <thread>

// Range: [min, max)
int randint(int min, int max) { return min + (rand() % (max - min)); }

void sleep(int milliseconds) {
  auto duration = std::chrono::milliseconds(milliseconds);
  // std::this_thread::sleep_for(duration);
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
