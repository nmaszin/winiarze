#pragma once

#include <mpi.h>
#include <mutex>

struct MessageTransmitter {
  template <typename PayloadType> struct Response {
    int message;
    int source;
    unsigned previousClock;
    PayloadType payload;
  };

  template <typename PayloadType>
  void send(int message, PayloadType &&payload, int dest,
            MPI_Comm comm = MPI_COMM_WORLD) {

    updateClock(payload);
    multicast(message, payload, dest, comm);
  }

  template <typename PayloadType>
  void multicast(int message, PayloadType &&payload, int dest,
                 MPI_Comm comm = MPI_COMM_WORLD) {
    auto serialized = payload.serialize();
    std::cerr << "[clock] " << payload.clock << "\n";
    MPI_Send(serialized.data(), serialized.size(), payload.getType(), dest,
             message, comm);
  }

  template <typename PayloadType>
  Response<PayloadType> receive(int message, int source,
                                MPI_Comm comm = MPI_COMM_WORLD) {
    Response<PayloadType> response;
    MPI_Status status;

    auto array = response.payload.serialize();
    MPI_Recv(array.data(), array.size(), response.payload.getType(), source,
             message, comm, &status);
    response.payload.deserialize(array);
    response.message = status.MPI_TAG;
    response.source = status.MPI_SOURCE;
    response.previousClock = clock;

    {
      std::lock_guard<std::mutex> lock(clock_mutex);
      clock = std::max(clock, response.payload.getClock()) + 1;
    }

    return response;
  }

  template <typename PayloadType> void updateClock(PayloadType &payload) {
    std::lock_guard<std::mutex> lock(clock_mutex);
    clock++;
    payload.setClock(clock);
  }

  unsigned getClock() { return clock; }

private:
  unsigned clock = 0;
  std::mutex clock_mutex;
};
