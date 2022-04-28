#pragma once

#include <cstdlib>

unsigned randint(unsigned min, unsigned max) {
  return min + (rand() % (max - min));
}
