#pragma once

#include <cstdlib>

// Range: [min, max)
unsigned randint(unsigned min, unsigned max) {
  return min + (rand() % (max - min));
}
