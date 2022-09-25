#pragma once

#include <functional>
#include <vector>

// Zawsze musi być przynajmniej 1 winiarz i 1 student!
struct Config {
  bool dev = true;

  int observers = 1;
  int winemakers = 2;
  int students = 0;
  int safe_places = 100000;
  int max_wine_production = 10;
  int max_wine_demand = 10;
  int max_sleep_time = 10;

  int getTotalProcessesNumber() { return observers + winemakers + students; }

  int getWinemakerIdFromPid(int process_id) { return process_id - observers; }

  int getStudentIdFromPid(int process_id) {
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
