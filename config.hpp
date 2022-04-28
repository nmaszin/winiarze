#pragma once

struct Config {
  unsigned winemakers = 10;
  unsigned students = 20;
  unsigned safe_places = 5;
  unsigned max_wine_production = 10;
  unsigned max_wine_demand = 10;

  unsigned total_processes() { return winemakers + students + 1; }
};
