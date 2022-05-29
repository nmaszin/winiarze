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
      auto response = et.receive(MPI_ANY_TAG, MPI_ANY_SOURCE);
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
  MessageTransmitter<EntirePayload> et;
};

class Winemaker : public WorkingProcess {
public:
  Winemaker(Config &config, unsigned id)
      : config(config), id(id), safe_places_free(config.safe_places, true),
        safe_places_students_available(config.safe_places, false),
        safe_places_students_ids(config.safe_places, 0),
        safe_places_students_wine_needs(config.safe_places, 0) {}

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
        if (!want_to_enter_critical_section || response.payload.clock > clock) {
          et.send(WinemakerMessage::WINEMAKER_SAFE_PLACE_ACK,
                  EntirePayload(clock), response.source);
          clock = std::max(clock, response.payload.clock);
        } else {
          wait_queue.push(response.source);
        }
        break;

      case WinemakerMessage::WINEMAKER_SAFE_PLACE_ACK:
        critical_section_counter--;
        clock = std::max(clock, response.payload.clock + 1);
        if (critical_section_counter == 0) {
          critical_section_wait.notify_one();
        }
        break;

      case WinemakerMessage::WINEMAKER_SAFE_PLACE_RESERVED:
        safe_places_free[response.payload.safe_place_id] = false;
        break;

      case WinemakerMessage::WINEMAKER_SAFE_PLACE_LEFT:
        safe_places_free[response.payload.safe_place_id] = true;
        break;

      case StudentMessage::STUDENT_SAFE_PLACE_RESERVED: {
        auto sp_id = response.payload.safe_place_id;
        safe_places_students_available[sp_id] = true;
        safe_places_students_ids[sp_id] = response.source;
        safe_places_students_wine_needs[sp_id] = response.payload.wine_amount;
        break;
      }

      case StudentMessage::STUDENT_SAFE_PLACE_LEFT: {
        auto sp_id = response.payload.safe_place_id;
        safe_places_students_available[sp_id] = false;
        safe_places_students_ids[sp_id] = 0;
        safe_places_students_wine_needs[sp_id] = 0;
        break;
      }
      }
    }
  }

private:
  Config &config;
  MessageTransmitter<EntirePayload> et;
  std::queue<int> wait_queue;
  std::condition_variable critical_section_wait, student_wait;
  std::mutex critical_section_wait_mutex, student_wait_mutex;

  std::vector<bool> safe_places_free;
  std::vector<bool> safe_places_students_available;
  std::vector<unsigned> safe_places_students_ids;
  std::vector<unsigned> safe_places_students_wine_needs;

  bool want_to_enter_critical_section = false;
  bool wait_for_student;

  unsigned id;
  unsigned wine_available = 0;
  unsigned safe_place_id = 0;
  unsigned clock = 0;
  unsigned critical_section_counter;
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
    while (true) {
      want_to_enter_critical_section = true;
      critical_section_counter = config.winemakers - 1;

      config.forEachWinemaker([&](int process_id) {
        if (process_id != id) {
          et.send(WinemakerMessage::WINEMAKER_SAFE_PLACE_REQUEST,
                  EntirePayload(clock), process_id);
        }
      });

      {
        std::unique_lock<std::mutex> lock(critical_section_wait_mutex);
        critical_section_wait.wait(lock);
      }

      // Critical section start
      bool ok = false;
      for (unsigned i = 0; i < config.safe_places; i++) {
        if (safe_places_free[i]) {
          safe_places_free[i] = false;
          safe_place_id = i;
          ok = true;
          break;
        }
      }

      wait_for_student = true;
      if (ok) {
        config.forAll([&](int process_id) {
          // Forall except me and observer
          if (process_id != id && process_id != 0) {
            et.send(WinemakerMessage::WINEMAKER_SAFE_PLACE_RESERVED,
                    EntirePayload()
                        .setSafePlaceId(safe_place_id)
                        .setWineAmount(wine_available),
                    process_id);
          }
        });

        if (safe_places_students_available[safe_place_id]) {
          wait_for_student = false;
        }
      }

      // Critical section end
      while (!wait_queue.empty()) {
        auto process_id = wait_queue.front();
        wait_queue.pop();
        et.send(WinemakerMessage::WINEMAKER_SAFE_PLACE_ACK,
                EntirePayload(clock), process_id);
      }
      want_to_enter_critical_section = false;

      if (ok) {
        et.send(
            ObserverMessage::WINEMAKER_RESERVED_SAFE_PLACE,
            EntirePayload().setWinemakerId(id).setSafePlaceId(safe_place_id),
            0);
        break;
      }
    }
  }

  void handleSafePlace() {
    while (wine_available > 0) {
      if (wait_for_student) {
        std::unique_lock<std::mutex> lock(student_wait_mutex);
        student_wait.wait(lock);
      }

      wait_for_student = true;
      auto student_id = safe_places_students_ids[safe_place_id];
      auto wine_requested = safe_places_students_wine_needs[safe_place_id];
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
    want_to_enter_critical_section = true;
    critical_section_counter = config.winemakers - 1;

    config.forEachWinemaker([&](int process_id) {
      if (process_id != id) {
        et.send(WinemakerMessage::WINEMAKER_SAFE_PLACE_REQUEST,
                EntirePayload(clock), process_id);
      }
    });

    {
      std::unique_lock<std::mutex> lock(critical_section_wait_mutex);
      critical_section_wait.wait(lock);
    }

    // Critical section start
    safe_places_free[safe_place_id] = true;
    config.forAll([&](int process_id) {
      et.send(WinemakerMessage::WINEMAKER_SAFE_PLACE_LEFT,
              EntirePayload().setSafePlaceId(safe_place_id), process_id);
    });

    // Critical section end
    while (!wait_queue.empty()) {
      auto process_id = wait_queue.front();
      wait_queue.pop();
      et.send(WinemakerMessage::WINEMAKER_SAFE_PLACE_ACK, EntirePayload(clock),
              process_id);
    }

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
    case StudentMessage::STUDENT_SAFE_PLACE_REQUEST:
      if (!want_to_enter_critical_section || response.payload.clock > clock) {
        et.send(StudentMessage::STUDENT_SAFE_PLACE_ACK, EntirePayload(clock),
                response.source);
        clock = std::max(clock, response.payload.clock + 1);
      }
      break;
    case StudentMessage::STUDENT_SAFE_PLACE_ACK:
      critical_section_counter--;
      if (critical_section_counter == 0) {
        critical_section_wait.notify_one();
      }
      break;
    case StudentMessage::STUDENT_SAFE_PLACE_RESERVED: {
      auto sp_id = response.payload.safe_place_id;
      safe_places_free[sp_id] = false;
      break;
    }
    case StudentMessage::STUDENT_SAFE_PLACE_LEFT: {
      auto sp_id = response.payload.safe_place_id;
      safe_places_free[sp_id] = true;
      break;
    }
    case WinemakerMessage::WINEMAKER_SAFE_PLACE_RESERVED: {
      auto sp_id = response.payload.safe_place_id;
      safe_places_winemakers_available[sp_id] = true;
      safe_places_winemakers_id[sp_id] = response.payload.winemaker_id;
      safe_places_winemakers_wine_available[sp_id] =
          response.payload.wine_amount;
      break;
    }
    case WinemakerMessage::WINEMAKER_SAFE_PLACE_LEFT: {
      auto sp_id = response.payload.safe_place_id;
      safe_places_winemakers_available[sp_id] = false;
      safe_places_winemakers_id[sp_id] = 0;
      safe_places_winemakers_wine_available[sp_id] = 0;
      break;
    }
    }
  }

private:
  Config &config;
  unsigned id;
  unsigned wine_demand = 0;

  std::vector<bool> safe_places_free;
  std::vector<bool> safe_places_winemakers_available;
  std::vector<unsigned> safe_places_winemakers_id;
  std::vector<unsigned> safe_places_winemakers_wine_available;

  MessageTransmitter<EntirePayload> et;
  std::mutex critical_section_wait_mutex, winemaker_wait_mutex,
      wine_gave_wait_mutex;
  std::condition_variable critical_section_wait, winemaker_wait, wine_gave_wait;
  unsigned clock = 0;
  unsigned safe_place_id;
  unsigned critical_section_counter;
  bool want_to_enter_critical_section = false;
  bool wait_for_winemaker = true;

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
    while (true) {
      want_to_enter_critical_section = true;
      critical_section_counter = config.students - 1;

      config.forEachStudent([&](int process_id) {
        if (process_id != id) {
          et.send(StudentMessage::STUDENT_SAFE_PLACE_REQUEST,
                  EntirePayload(clock), process_id);
        }
      });

      {
        std::unique_lock<std::mutex> lock(critical_section_wait_mutex);
        critical_section_wait.wait(lock);
      }

      // Critical section start
      want_to_enter_critical_section = false;
      bool ok = false;
      for (int i = 0; i < safe_places_free.size(); i++) {
        if (safe_places_free[i]) {
          safe_places_free[i] = false;
          safe_place_id = i;
          ok = true;
          break;
        }
      }

      if (ok) {
        config.forEachStudent([&](int process_id) {
          if (process_id != id) {
            et.send(StudentMessage::STUDENT_SAFE_PLACE_RESERVED,
                    EntirePayload(clock).setSafePlaceId(safe_place_id),
                    process_id);
          }
        });

        if (safe_places_winemakers_available[safe_place_id]) {
          wait_for_winemaker = false;
        }
      }

      // Critical section end
      while (!wait_queue.empty()) {
        unsigned process_id = wait_queue.front();
        wait_queue.pop();

        et.send(StudentMessage::STUDENT_SAFE_PLACE_ACK, EntirePayload(clock),
                process_id);
      }

      if (ok) {
        et.send(ObserverMessage::STUDENT_RESERVED_SAFE_PLACE,
                EntirePayload().setStudentId(id).setSafePlaceId(safe_place_id),
                0);
        break;
      }
    }
  }

  void handleSafePlace() {
    while (wine_demand > 0) {
      if (wait_for_winemaker) {
        std::unique_lock<std::mutex> lock(winemaker_wait_mutex);
        winemaker_wait.wait(lock);
      }

      wait_for_winemaker = true;

      {
        std::unique_lock<std::mutex> lock(wine_gave_wait_mutex);
        wine_gave_wait.wait(lock);
      }
    }
  }

  void leaveSafePlace() {
    want_to_enter_critical_section = true;
    critical_section_counter = config.students - 1;

    config.forEachStudent([&](int process_id) {
      if (process_id != id) {
        et.send(StudentMessage::STUDENT_SAFE_PLACE_REQUEST,
                EntirePayload(clock), process_id);
      }
    });

    {
      std::unique_lock<std::mutex> lock(critical_section_wait_mutex);
      critical_section_wait.wait(lock);
    }

    // Critical section start
    safe_places_free[safe_place_id] = true;
    config.forEachStudent([&](int process_id) {
      if (process_id != id) {
        et.send(StudentMessage::STUDENT_SAFE_PLACE_LEFT,
                EntirePayload(clock).setSafePlaceId(safe_place_id), process_id);
      }
    });

    // Critical section end
    while (!wait_queue.empty()) {
      unsigned process_id = wait_queue.front();
      wait_queue.pop();
      et.send(StudentMessage::STUDENT_SAFE_PLACE_ACK, EntirePayload(clock),
              process_id);
    }

    et.send(ObserverMessage::STUDENT_LEFT_SAFE_PLACE,
            EntirePayload().setStudentId(id).setSafePlaceId(safe_place_id), 0);
  }
};
