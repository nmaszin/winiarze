#pragma once

#include "payload.hpp"
#include "utils.hpp"
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
  std::mutex clock_mutex;

  void setClock(int value) {
    clock_mutex.lock();
    this->clock = value;
    clock_mutex.unlock();
  }

  int getClock() {
    clock_mutex.lock();
    int copy = this->clock;
    clock_mutex.unlock();
    return copy;
  }

  void send(int message, Payload &&payload, int dest) {
    clock_mutex.lock();
    this->clock++;
    sendBroadcast(message, std::move(payload), dest);
    clock_mutex.unlock();
  }

  void sendBroadcast(int message, Payload &&payload, int dest) {
    payload.clock = this->clock;

    auto serialized = payload.serialize();
    std::cerr << process::rank << "send() msg: " << message
              << ", dest: " << dest << ", clock: " << clock
              << ", payload: " << payload << "\n";
    MPI_Send(serialized.data(), serialized.size(), MPI_INT, dest, message,
             MPI_COMM_WORLD);
  }

  void startBroadcast() {
    clock_mutex.lock();
    std::cerr << process::rank << "Rozpoczyna broadcast\n";
    this->clock++;
  }

  void stopBroadcast() {
    std::cerr << process::rank << "Kończy broadcast\n";
    clock_mutex.unlock();
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

    {
      clock_mutex.lock();
      response.previousClock = this->clock;
      this->clock = std::max(this->clock, response.payload.clock) + 1;

      std::cerr << process::rank << " recv() mesg: " << response.message
                << ", src: " << response.source << ", clock: " << clock
                << ", payload: " << response.payload << "\n";
      clock_mutex.unlock();
    }

    return response;
  }
};
