#pragma once

#include "payload.hpp"
#include <mpi.h>
#include <mutex>

struct MessageTransmitter {
  struct Response {
    int message;
    int source;
    int previousClock;
    Payload payload;
  };

  int clock = 0;
  bool broadcast = false;
  std::mutex clock_mutex;

  void send(int message, Payload &&payload, int dest) {
    Payload p = payload;
    if (broadcast) {
      p.clock = this->clock;
    } else {
      updateClock(p);
    }

    auto serialized = p.serialize();
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    std::cout << "[" << rank << "] Clock przy wysyłce: " << p.clock << "\n";
    MPI_Send(serialized.data(), serialized.size(), MPI_INT, dest, message,
             MPI_COMM_WORLD);
  }

  void startBroadcast() {
    clock_mutex.lock();
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    std::cout << "[" << rank << "] Rozpoczyna broadcast"
              << "\n";
    broadcast = true;
    this->clock++;
  }

  void stopBroadcast() {
    broadcast = false;
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    std::cout << "[" << rank << "] Kończy broadcast"
              << "\n";
    clock_mutex.unlock();
  }

  void updateClock(Payload &payload) {
    std::lock_guard<std::mutex> lock(clock_mutex);
    this->clock++;
    payload.clock = this->clock;
  }

  Response receive(int message, int source) {
    Response response;
    MPI_Status status;

    auto array = response.payload.serialize();
    MPI_Recv(array.data(), array.size(), MPI_INT, source, message,
             MPI_COMM_WORLD, &status);

    response.payload.deserialize(array);
    response.message = status.MPI_TAG;
    response.source = status.MPI_SOURCE;
    response.previousClock = this->clock;

    {
      std::lock_guard<std::mutex> lock(clock_mutex);
      clock = std::max(clock, response.payload.clock) + 1;
    }

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    std::cout << "[" << rank << "] Clock po odbiorze: " << clock << "\n";

    return response;
  }
};
