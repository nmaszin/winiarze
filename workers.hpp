#pragma once

#include "messages.hpp"
#include "payload.hpp"
#include "transmitter.hpp"
#include "utils.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mpi.h>
#include <queue>
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
      MessageTransmitter<EntirePayload> transmitter;
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
  Winemaker(Config &config, unsigned id)
      : config(config), id(id), safe_places_free(config.safe_places, true) {}

protected:
  void foregroundTask() override {
    while (true) {
      makeWine();
      reserveSafePlace();
      handleSafePlace();
      leaveSafePlace();
    }
  }

  void backgroundTask() override {
    while (true) {
      auto response = et.receive(MPI_ANY_TAG, MPI_ANY_SOURCE);
      switch (response.message) {
      case WinemakerMessage::WINEMAKER_SAFE_PLACE_REQUEST:
        // Czy tu nie powinien być mutex?
        if (response.payload.clock > clock) {
          et.send(WinemakerMessage::WINEMAKER_SAFE_PLACE_ACK,
                  EntirePayload(clock), response.source);
          clock = std::max(clock, response.payload.clock);
        } else {
          wait_queue.push(response.source);
        }
        break;

      case WinemakerMessage::WINEMAKER_SAFE_PLACE_ACK:
        critical_section_counter--;
        if (critical_section_counter == 0) {
          wait_for_message.notify_one();
        }
        break;

      case WinemakerMessage::WINEMAKER_SAFE_PLACE_RELEASE:
        // Czy to jest w ogóle potrzebne?
        break;

      case WinemakerMessage::WINEMAKER_SAFE_PLACE_RESERVED:
        safe_places_free[response.payload.safe_place_id] = false;
        break;

      case WinemakerMessage::WINEMAKER_SAFE_PLACE_LEFT:
        safe_places_free[response.payload.safe_place_id] = true;
        break;

      case StudentMessage::GIVE_ME_WINE:
        wine_requested = response.payload.wine_amount;
        student_id = response.payload.student_id;
        wait_for_message.notify_one();
        break;
      }
    }
  }

private:
  Config &config;
  unsigned id;
  unsigned wine_available = 0;
  unsigned safe_place_id = 0;
  unsigned clock = 0;

  std::vector<bool> safe_places_free;
  MessageTransmitter<EntirePayload> et;

  std::condition_variable wait_for_message;
  std::mutex wait_for_message_mutex;
  std::queue<int> wait_queue;
  unsigned critical_section_counter;
  unsigned wine_requested;
  unsigned student_id;

  void makeWine() {
    et.send(ObserverMessage::WINEMAKER_PRODUCTION_STARTED,
            EntirePayload().setWinemakerId(id), 0);

    auto duration = std::chrono::seconds(randint(1, 10));
    std::this_thread::sleep_for(duration);
    wine_available = randint(1, config.max_wine_production);

    et.send(ObserverMessage::WINEMAKER_PRODUCTION_END,
            EntirePayload().setWinemakerId(id).setWineAmount(wine_available),
            0);
  }

  void reserveSafePlace() {
    critical_section_counter = config.winemakers - 1;

    config.forEachWinemaker([&](int process_id) {
      et.send(WinemakerMessage::WINEMAKER_SAFE_PLACE_REQUEST,
              EntirePayload(clock), process_id);
    });

    {
      std::unique_lock<std::mutex> lock(wait_for_message_mutex);
      wait_for_message.wait(lock);
    }

    // Critical section start
    for (unsigned i = 0; i < config.safe_places; i++) {
      if (safe_places_free[i]) {
        safe_places_free[i] = false;
        safe_place_id = i;
        break;
      }
    }

    config.forAll([&](int process_id) {
      et.send(WinemakerMessage::WINEMAKER_SAFE_PLACE_RESERVED,
              EntirePayload()
                  .setSafePlaceId(safe_place_id)
                  .setWineAmount(wine_available),
              process_id);
    });

    // Critical section end
    config.forEachWinemaker([&](int process_id) {
      et.send(WinemakerMessage::WINEMAKER_SAFE_PLACE_RELEASE,
              EntirePayload(clock), process_id);
    });

    while (!wait_queue.empty()) {
      auto process_id = wait_queue.front();
      wait_queue.pop();
      et.send(WinemakerMessage::WINEMAKER_SAFE_PLACE_ACK, EntirePayload(clock),
              process_id);
    }

    et.send(ObserverMessage::WINEMAKER_RESERVED_SAFE_PLACE,
            EntirePayload().setWinemakerId(id).setSafePlaceId(safe_place_id),
            0);
  }

  void handleSafePlace() {
    while (wine_available > 0) {
      {
        std::unique_lock<std::mutex> lock(wait_for_message_mutex);
        wait_for_message.wait(lock);
      }

      auto wine_given = std::min(wine_requested, wine_available);

      et.send(WinemakerMessage::HERE_YOU_ARE,
              EntirePayload().setWineAmount(wine_given), student_id);

      et.send(ObserverMessage::WINEMAKER_GAVE_WINE_TO_STUDENT,
              EntirePayload()
                  .setWinemakerId(id)
                  .setStudentId(student_id)
                  .setSafePlaceId(safe_place_id)
                  .setWineAmount(wine_given),
              0);

      wine_available -= wine_given;
    }
  }

  void leaveSafePlace() {
    config.forEachWinemaker([&](int process_id) {
      et.send(WinemakerMessage::WINEMAKER_SAFE_PLACE_REQUEST,
              EntirePayload(clock), process_id);
    });

    for (unsigned i = 0; i < config.winemakers - 1; i++) {
      auto response = et.receive(WinemakerMessage::WINEMAKER_SAFE_PLACE_ACK,
                                 MPI_ANY_SOURCE);
      clock = std::max(clock, response.payload.clock + 1);
    }

    // Critical section start
    safe_places_free[safe_place_id] = true;

    config.forAll([&](int process_id) {
      et.send(WinemakerMessage::WINEMAKER_SAFE_PLACE_LEFT,
              EntirePayload().setSafePlaceId(safe_place_id), process_id);
    });

    // Critical section end
    config.forEachWinemaker([&](int process_id) {
      et.send(WinemakerMessage::WINEMAKER_SAFE_PLACE_RELEASE,
              EntirePayload(clock), process_id);
    });

    et.send(ObserverMessage::WINEMAKER_LEFT_SAFE_PLACE,
            EntirePayload().setWinemakerId(id).setSafePlaceId(safe_place_id),
            0);
  }
};

class Student : public WorkingProcess {
public:
  Student(Config &config, unsigned id)
      : config(config), id(id), safe_places_free(config.safe_places, true) {}

protected:
  void foregroundTask() override {
    while (true) {
      relax();
      reserveSafePlace();
      handleSafePlace();
      leaveSafePlace();
    }
  }

  void backgroundTask() override {
    auto response = et.receive(MPI_ANY_TAG, MPI_ANY_SOURCE);
    switch (response.message) {
      // TODO
    }
  }

private:
  Config &config;
  unsigned id;
  unsigned wine_demand = 0;

  MessageTransmitter<EntirePayload> et;
  std::mutex wait_for_message_mutex;
  std::condition_variable wait_for_message;
  unsigned clock = 0;
  unsigned safe_place_id;
  unsigned critical_section_counter;
  std::vector<bool> safe_places_free;
  std::queue<unsigned> wait_queue;

  void relax() {
    et.send(ObserverMessage::STUDENT_DOESNT_WANT_TO_PARTY_ANYMORE,
            EntirePayload().setStudentId(id), 0);

    auto duration = std::chrono::seconds(randint(1, 10));
    std::this_thread::sleep_for(duration);
    wine_demand = randint(1, config.max_wine_demand);

    et.send(ObserverMessage::STUDENT_WANT_TO_PARTY,
            EntirePayload().setStudentId(id).setWineAmount(wine_demand), 0);
  }

  void reserveSafePlace() {
    critical_section_counter = config.students - 1;

    config.forEachStudent([&](int process_id) {
      et.send(StudentMessage::STUDENT_SAFE_PLACE_REQUEST, EntirePayload(clock),
              process_id);
    });

    {
      std::unique_lock<std::mutex> lock(wait_for_message_mutex);
      wait_for_message.wait(lock);
    }

    // Critical section start
    for (int i = 0; i < safe_places_free.size(); i++) {
      if (safe_places_free[i]) {
        safe_places_free[i] = false;
        safe_place_id = i;
        break;
      }
    }

    config.forEachStudent([&](int process_id) {
      et.send(StudentMessage::STUDENT_SAFE_PLACE_RESERVED,
              EntirePayload(clock).setSafePlaceId(safe_place_id), process_id);
    });

    config.forEachStudent([&](int process_id) {
      et.send(StudentMessage::STUDENT_SAFE_PLACE_RELEASE, EntirePayload(clock),
              process_id);
    });

    while (!wait_queue.empty()) {
      unsigned process_id = wait_queue.front();
      wait_queue.pop();

      et.send(StudentMessage::STUDENT_SAFE_PLACE_ACK, EntirePayload(clock),
              process_id);
    }

    et.send(ObserverMessage::STUDENT_RESERVED_SAFE_PLACE,
            EntirePayload().setWinemakerId(997).setStudentId(id).setSafePlaceId(
                safe_place_id),
            0);
  }

  void handleSafePlace() {
    // TODO:
  }

  void leaveSafePlace() {
    // TODO:
  }
};
