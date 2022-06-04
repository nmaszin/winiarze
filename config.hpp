#pragma once

#include <functional>
#include <vector>

struct Config {
  unsigned observers = 1;
  unsigned winemakers = 2;
  unsigned students = 2;
  unsigned safe_places = 1;
  unsigned max_wine_production = 10;
  unsigned max_wine_demand = 10;

  unsigned getTotalProcessesNumber() {
    return observers + winemakers + students;
  }

  unsigned getObserverIdFromPid(unsigned process_id) { return process_id; }

  unsigned getWinemakerIdFromPid(unsigned process_id) {
    return process_id - observers;
  }

  unsigned getStudentIdFromPid(unsigned process_id) {
    return process_id - observers - winemakers;
  }

  void forEachWinemaker(std::function<void(int)> callback) {
    for (int i = 0; i < winemakers; i++) {
      callback(i + observers);
    }
  }

  void forEachStudent(std::function<void(int)> callback) {
    for (int i = 0; i < students; i++) {
      callback(i + observers + winemakers);
    }
  }

  void forEachWinemakerAndStudent(std::function<void(int)> callback) {
    for (int i = 0; i < winemakers + students; i++) {
      callback(i + observers);
    }
  }
};
