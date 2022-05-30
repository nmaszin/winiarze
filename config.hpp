#pragma once

#include <functional>

struct Config {
  unsigned winemakers = 5;
  unsigned students = 10;
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

  void forAll(std::function<void(int)> callback) {
    auto total = getTotalProcessesNumber();
    for (int i = 0; i < total; i++) {
      callback(i);
    }
  }

  unsigned getWinemakerId(unsigned process_id) { return process_id; }

  unsigned getStudentId(unsigned process_id) { return process_id - winemakers; }
};
