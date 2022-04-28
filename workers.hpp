#pragma once

#include "messages.hpp"
#include "payload.hpp"
#include "transmitter.hpp"
#include "utils.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <mpi.h>
#include <thread>
#include <vector>

struct Runnable {
  virtual void run() = 0;
};

class WorkingProcess : public Runnable {
public:
  void run() {
    thread = std::make_unique<std::thread>([this] { this->backgroundTask(); });
    foregroundTask();
  }

protected:
  virtual void foregroundTask() = 0;
  virtual void backgroundTask() = 0;

private:
  std::unique_ptr<std::thread> thread;
};

class Observer : public Runnable {
public:
  Observer(Config &config)
      : config(config), winemakers_wine_amounts(config.winemakers, 0),
        safe_places_membership(config.safe_places,
                               std::pair<unsigned, unsigned>(0, 0)),
        students_wine_needs(config.students, 0) {}

  void run() override {
    while (true) {
      MessageTransmitter<ObserverMessagePayload> transmitter;
      auto response = transmitter.receive(MPI_ANY_TAG, MPI_ANY_SOURCE);
      const auto &payload = response.payload;

      switch (response.message) {
      case ObserverMessage::WINEMAKER_PRODUCTION_END:
        winemakers_wine_amounts[payload.winemaker_id - 1] = payload.wine_amount;
        std::cout << "Winiarz o id " << payload.winemaker_id
                  << " zakończył produkcję i wyprodukował "
                  << payload.wine_amount << " jednostek wina\n";
        break;

      case ObserverMessage::WINEMAKER_RESERVED_SAFE_PLACE:
        safe_places_membership[payload.safe_place_id - 1].first =
            payload.winemaker_id;
        std::cout << "Winiarz o id " << payload.winemaker_id
                  << " zarezerwował melinę o id " << payload.safe_place_id
                  << "\n";
        break;

      case ObserverMessage::WINEMAKER_LEFT_SAFE_PLACE:
        safe_places_membership[payload.safe_place_id - 1].first = 0;
        std::cout << "Winiarz o id " << payload.winemaker_id
                  << " opuścił melinę o id " << payload.safe_place_id << "\n";
        break;

      case ObserverMessage::WINEMAKER_GAVE_WINE_TO_STUDENT:
        students_wine_needs[payload.student_id - 1] -= payload.wine_amount;
        winemakers_wine_amounts[payload.winemaker_id - 1] -=
            payload.wine_amount;

        std::cout << "Winiarz o id " << payload.winemaker_id
                  << " dał studentowi o id " << payload.student_id
                  << " wino w ilości " << payload.wine_amount
                  << " w melinie o id " << payload.safe_place_id << "\n";
        break;

      case ObserverMessage::WINEMAKER_PRODUCTION_STARTED:
        std::cout << "Winiarz o id " << payload.winemaker_id
                  << " rozpoczął produkcję\n";
        break;

      case ObserverMessage::STUDENT_WANT_TO_PARTY:
        students_wine_needs[payload.student_id - 1] = payload.wine_amount;

        std::cout << "Student o id " << payload.student_id
                  << " wyleczył kaca i potrzebuje " << payload.wine_amount
                  << " jednostek wina na kolejną imprezę\n";
        break;

      case ObserverMessage::STUDENT_RESERVED_SAFE_PLACE:
        safe_places_membership[payload.safe_place_id - 1].second =
            payload.student_id;
        std::cout << "Student o id " << payload.student_id
                  << " zarezerwował melinę o id " << payload.safe_place_id
                  << " w której siedzi winiarz o id " << payload.winemaker_id
                  << "\n";
        break;

      case ObserverMessage::STUDENT_LEFT_SAFE_PLACE:
        safe_places_membership[payload.safe_place_id - 1].second = 0;
        std::cout << "Student o id " << payload.student_id
                  << " opuścił melinę o id " << payload.safe_place_id
                  << " w której siedzi winiarz o id " << payload.winemaker_id
                  << "\n";
        break;

      case ObserverMessage::STUDENT_DOESNT_WANT_TO_PARTY_ANYMORE:
        std::cout << "Student o id " << payload.student_id << " ma kaca\n";
        break;
      }

      printState();
      std::cout << "\n";
    }
  }

private:
  void printState() {
    std::cout << "Aktualny stan:\n";

    auto identifiers_number =
        std::max({winemakers_wine_amounts.size(), safe_places_membership.size(),
                  students_wine_needs.size()});

    std::cout << "\tId:      \t";
    std::vector<unsigned> identifiers(identifiers_number, 0);
    for (unsigned i = 0; i < identifiers_number; i++) {
      identifiers[i] = i + 1;
    }
    printVector(identifiers);
    std::cout << '\n';

    std::cout << "\tWiniarze:\t";
    printVector(winemakers_wine_amounts);
    std::cout << '\n';

    std::cout << "\tMeliny:  \t";
    printVector(safe_places_membership);
    std::cout << '\n';

    std::cout << "\tStudenci:\t";
    printVector(students_wine_needs);
    std::cout << '\n';
  }

  template <typename T> void printScalar(T value) {
    if (value == 0) {
      std::cout << 'X';
    } else {
      std::cout << value;
    }
  }

  template <typename T, typename Q>
  void printVector(const std::vector<std::pair<T, Q>> &array) {
    for (const auto &e : array) {
      std::cout << '(';
      printScalar(e.first);
      std::cout << ", ";
      printScalar(e.second);
      std::cout << ")\t";
    }
  }

  template <typename T> void printVector(const std::vector<T> &array) {
    for (const auto &e : array) {
      printScalar(e);
      std::cout << '\t';
    }
  }

  Config &config;
  std::vector<unsigned> winemakers_wine_amounts;
  std::vector<std::pair<unsigned, unsigned>> safe_places_membership;
  std::vector<unsigned> students_wine_needs;
};

class Winemaker : public WorkingProcess {
public:
  Winemaker(Config &config, unsigned id) : config(config), id(id) {}

protected:
  void foregroundTask() override {
    while (true) {
      makeWine();
      reserveSafePlace();
      handleSafePlace();
      leaveSafePlace();
    }
  }

  void backgroundTask() override {}

private:
  Config &config;
  unsigned id;
  unsigned wine_available = 0;

  void makeWine() {
    auto duration = std::chrono::seconds(randint(1, 10));
    std::this_thread::sleep_for(duration);
    wine_available = randint(1, config.max_wine_production);

    ObserverMessagePayload payload;
    payload.winemaker_id = id;
    payload.wine_amount = wine_available;

    MessageTransmitter<ObserverMessagePayload> transmitter;
    transmitter.send(ObserverMessage::WINEMAKER_PRODUCTION_END, payload, 0);
  }

  void reserveSafePlace() {
    // TODO:
  }

  void handleSafePlace() {
    while (wine_available > 0) {
      // TODO:
      break;
    }
  }

  void leaveSafePlace() {
    // TODO
  }
};

class Student : public WorkingProcess {
public:
  Student(Config &config, unsigned id) : config(config), id(id) {}

protected:
  void foregroundTask() override {
    // TODO:
  }

  void backgroundTask() override {
    // TODO
  }

private:
  Config &config;
  unsigned id;
  unsigned wine_available = 0;

  void relax() {
    // TODO:
  }

  void reserveSafePlace() {
    // TODO:
  }

  void handleSafePlace() {
    // TODO:
  }

  void leaveSafePlace() {
    // TODO:
  }
};
