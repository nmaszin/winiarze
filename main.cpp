#include "config.hpp"
#include "workers.hpp"
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <mpi.h>

int main(int argc, char *argv[]) {
  Config config;
  MPI_Init(&argc, &argv);

  int current_number_of_processes;
  int expected_number_of_processes = config.total_processes();
  MPI_Comm_size(MPI_COMM_WORLD, &current_number_of_processes);
  if (current_number_of_processes != expected_number_of_processes) {
    std::cerr << "Invalid number of processes\n";
    std::cerr << "With current configuration you should run "
              << expected_number_of_processes << " processes\n";
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  int process_id;
  MPI_Comm_rank(MPI_COMM_WORLD, &process_id);
  srand(time(NULL) + process_id);

  std::unique_ptr<Runnable> process;
  if (process_id == 0) {
    process = std::make_unique<Observer>(config);
  } else if (process_id <= config.winemakers) {
    process = std::make_unique<Winemaker>(config, process_id);
  } else {
    process = std::make_unique<Student>(config, process_id - config.winemakers);
  }

  process->run();
  MPI_Finalize();
}
