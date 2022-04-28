#pragma once

#include <functional>

struct Config {
  unsigned winemakers = 10;
  unsigned students = 20;
  unsigned safe_places = 5;
  unsigned max_wine_production = 10;
  unsigned max_wine_demand = 10;

  unsigned getTotalProcessesNumber() { return winemakers + students + 1; }

  void forEachWinemaker(std::function<void(int)> callback) {
    for (int i = 0; i < winemakers; i++) {
      callback(i + 1);
    }
  }

  void forEachStudent(std::function<void(int)> callback) {
    for (int i = 0; i < students; i++) {
      callback(i + 1 + winemakers);
    }
  }
};
