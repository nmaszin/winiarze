#pragma once

#include <mpi.h>

template <typename PayloadType> struct MessageTransmitter {
  struct Response {
    int message;
    int source;
    PayloadType payload;
  };

  void send(int message, const PayloadType &payload, int dest,
            MPI_Comm comm = MPI_COMM_WORLD) {
    auto serialized = payload.serialize();
    MPI_Send(serialized.data(), serialized.size(), payload.getType(), dest,
             message, comm);
  }

  Response receive(int message, int source, MPI_Comm comm = MPI_COMM_WORLD) {
    Response response;
    MPI_Status status;

    auto array = response.payload.serialize();
    MPI_Recv(array.data(), array.size(), response.payload.getType(), source,
             message, comm, &status);

    response.payload.deserialize(array);
    response.message = status.MPI_TAG;
    response.source = status.MPI_SOURCE;
    return response;
  }
};
