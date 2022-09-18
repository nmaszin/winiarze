#pragma once

#include <chrono>
#include <cstdlib>
#include <thread>

// Range: [min, max)
int randint(int min, int max) { return min + (rand() % (max - min)); }

void sleep(int seconds) {
  auto duration = std::chrono::seconds(seconds);
  // std::this_thread::sleep_for(duration);
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
