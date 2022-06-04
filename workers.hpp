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
  Observer(Config &config, unsigned observer_pid)
      : config(config), winemakers_wine_amounts(config.winemakers, 0),
        students_wine_needs(config.students, 0),
        safe_places_winemakers_available(config.safe_places, false),
        safe_places_students_available(config.safe_places, false),
        safe_places_winemakers_ids(config.safe_places, 0),
        safe_places_students_ids(config.safe_places, 0),
        winemakers_working(config.winemakers, false),
        students_rest(config.students, false) {}

  void run() override {
    while (true) {
      auto response = et.receive<EntirePayload>(MPI_ANY_TAG, MPI_ANY_SOURCE);
      const auto &payload = response.payload;

      switch (response.message) {
      case ObserverMessage::WINEMAKER_PRODUCTION_END: {
        auto wid = config.getWinemakerIdFromPid(payload.winemaker_pid);
        winemakers_wine_amounts[wid] = payload.wine_amount;
        winemakers_working[wid] = false;

        std::cout << "Winiarz o id " << wid + 1
                  << " zakończył produkcję i wyprodukował "
                  << payload.wine_amount << " jednostek wina\n";
        break;
      }

      case ObserverMessage::WINEMAKER_SAFE_PLACE_RESERVED: {
        auto wid = config.getWinemakerIdFromPid(payload.winemaker_pid);
        auto spid = payload.safe_place_id;
        safe_places_winemakers_available[spid] = true;
        safe_places_winemakers_ids[spid] = wid + 1;

        std::cout << "Winiarz o id " << wid + 1 << " zarezerwował melinę o id "
                  << payload.safe_place_id + 1 << "\n";
        break;
      }

      case ObserverMessage::WINEMAKER_SAFE_PLACE_LEFT: {
        auto wid = config.getWinemakerIdFromPid(payload.winemaker_pid);
        auto spid = payload.safe_place_id;
        safe_places_winemakers_available[spid] = false;
        safe_places_winemakers_ids[spid] = 0;

        std::cout << "Winiarz o id " << wid + 1 << " opuścił melinę o id "
                  << spid + 1 << "\n";
        break;
      }
      case ObserverMessage::WINEMAKER_GAVE_WINE_TO_STUDENT: {
        auto sid = config.getStudentIdFromPid(payload.student_pid);
        auto wid = config.getWinemakerIdFromPid(payload.winemaker_pid);
        auto spid = payload.safe_place_id;

        students_wine_needs[sid] -= payload.wine_amount;
        winemakers_wine_amounts[wid] -= payload.wine_amount;

        std::cout << "Winiarz o id " << wid + 1 << " dał studentowi o id "
                  << sid + 1 << " wino w ilości " << payload.wine_amount
                  << " w melinie o id " << spid + 1 << "\n";
        break;
      }
      case ObserverMessage::WINEMAKER_PRODUCTION_STARTED: {
        auto wid = config.getWinemakerIdFromPid(payload.winemaker_pid);
        winemakers_working[wid] = true;
        std::cout << "Winiarz o id " << wid + 1 << " rozpoczął produkcję\n";
        break;
      }

      case ObserverMessage::STUDENT_WANT_TO_PARTY: {
        auto sid = config.getStudentIdFromPid(payload.student_pid);
        students_wine_needs[sid] = payload.wine_amount;
        students_rest[sid] = false;

        std::cout << "Student o id " << sid + 1
                  << " wyleczył kaca i potrzebuje " << payload.wine_amount
                  << " jednostek wina na kolejną imprezę\n";
        break;
      }
      case ObserverMessage::STUDENT_SAFE_PLACE_RESERVED: {
        auto sid = config.getStudentIdFromPid(payload.student_pid);
        auto spid = payload.safe_place_id;

        safe_places_students_available[spid] = true;
        safe_places_students_ids[spid] = sid + 1;

        std::cout << "Student o id " << sid + 1 << " zarezerwował melinę o id "
                  << spid + 1 << "\n";
        break;
      }
      case ObserverMessage::STUDENT_SAFE_PLACE_LEFT: {
        auto sid = config.getStudentIdFromPid(payload.student_pid);
        auto spid = payload.safe_place_id;

        safe_places_students_available[spid] = false;
        safe_places_students_ids[spid] = 0;
        std::cout << "Student o id " << sid + 1 << " opuścił melinę o id "
                  << spid + 1 << "\n";
        break;
      }
      case ObserverMessage::STUDENT_DOESNT_WANT_TO_PARTY_ANYMORE: {
        auto sid = config.getStudentIdFromPid(payload.student_pid);
        students_rest[sid] = true;
        std::cout << "Student o id " << sid + 1 << " ma kaca\n";
        break;
      }
      }

      printState();
      std::cout << "\n";
    }
  }

private:
  void printState() {
    std::cout << "Aktualny stan:\n";

    auto identifiers_number =
        std::max({config.safe_places, config.winemakers, config.students});

    std::cout << "\tId:      \t";
    for (unsigned i = 0; i < identifiers_number; i++) {
      std::cout << i + 1 << "\t";
    }
    std::cout << "\n------------------------------------------------\n";

    std::cout << "\tWiniarze:\t";
    for (unsigned i = 0; i < config.winemakers; i++) {
      if (winemakers_working[i]) {
        std::cout << "W\t";
      } else {
        std::cout << winemakers_wine_amounts[i] << "\t";
      }
    }
    std::cout << '\n';

    std::cout << "\tMeliny:  \t";
    for (unsigned i = 0; i < config.safe_places; i++) {
      if (safe_places_winemakers_available[i]) {
        std::cout << safe_places_winemakers_ids[i];
      } else {
        std::cout << "-";
      }
      std::cout << "/";
      if (safe_places_students_available[i]) {
        std::cout << safe_places_students_ids[i];
      } else {
        std::cout << "-";
      }
      std::cout << "\t";
    }
    std::cout << '\n';

    std::cout << "\tStudenci:\t";
    for (unsigned i = 0; i < config.students; i++) {
      if (students_rest[i]) {
        std::cout << "R\t";
      } else {
        std::cout << students_wine_needs[i] << "\t";
      }
    }
    std::cout << '\n';
  }

  Config &config;

  std::vector<unsigned> winemakers_wine_amounts;
  std::vector<unsigned> students_wine_needs;
  std::vector<unsigned> safe_places_winemakers_ids;
  std::vector<unsigned> safe_places_students_ids;
  std::vector<bool> winemakers_working;
  std::vector<bool> students_rest;
  std::vector<bool> safe_places_winemakers_available;
  std::vector<bool> safe_places_students_available;

  MessageTransmitter et;
};

class Winemaker : public WorkingProcess {
public:
  Winemaker(Config &config, unsigned pid)
      : config(config), pid(pid), safe_places_free(config.safe_places, true),
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
      auto response = et.receive<EntirePayload>(MPI_ANY_TAG, MPI_ANY_SOURCE);
      std::unique_lock<std::mutex>(m);
      switch (response.message) {
      case WinemakerMessage::WINEMAKER_SAFE_PLACE_REQUEST: {
        auto opponent_clock = response.payload.clock;
        auto my_clock = response.previousClock;
        auto opponent_pid = response.payload.winemaker_pid;

        if (!want_to_enter_critical_section || opponent_clock > my_clock ||
            (opponent_clock == my_clock && opponent_pid < pid)) {
          et.send(WinemakerMessage::WINEMAKER_SAFE_PLACE_ACK, EntirePayload(),
                  response.source);
        } else {
          wait_queue.push(response.source);
        }
        break;
      }

      case WinemakerMessage::WINEMAKER_SAFE_PLACE_ACK:
        critical_section_counter--;
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
        auto spid = response.payload.safe_place_id;
        safe_places_students_available[spid] = true;
        safe_places_students_ids[spid] = response.source;
        safe_places_students_wine_needs[spid] = response.payload.wine_amount;

        if (in_safe_place && spid == safe_place_id) {
          student_wait.notify_one();
        }

        break;
      }

      case StudentMessage::STUDENT_SAFE_PLACE_LEFT: {
        auto spid = response.payload.safe_place_id;
        safe_places_students_available[spid] = false;
        safe_places_students_ids[spid] = 0;
        safe_places_students_wine_needs[spid] = 0;
        break;
      }
      }
    }
  }

private:
  Config &config;
  MessageTransmitter et;
  std::queue<int> wait_queue;
  std::condition_variable critical_section_wait, student_wait;
  std::mutex critical_section_wait_mutex, student_wait_mutex, m;

  std::vector<bool> safe_places_free;
  std::vector<bool> safe_places_students_available;
  std::vector<unsigned> safe_places_students_ids;
  std::vector<unsigned> safe_places_students_wine_needs;

  bool want_to_enter_critical_section = false;
  bool wait_for_student;

  unsigned pid;
  unsigned wine_available = 0;
  unsigned safe_place_id = 0;
  bool in_safe_place = false;
  unsigned critical_section_counter;
  unsigned student_pid;

  void makeWine() {
    {
      std::unique_lock<std::mutex>(m);
      et.send(ObserverMessage::WINEMAKER_PRODUCTION_STARTED,
              EntirePayload().setWinemakerPid(pid), 0);
    }

    auto duration = std::chrono::seconds(randint(1, 10));
    std::this_thread::sleep_for(duration);

    {
      std::unique_lock<std::mutex>(m);
      wine_available = randint(1, config.max_wine_production);

      et.send(
          ObserverMessage::WINEMAKER_PRODUCTION_END,
          EntirePayload().setWinemakerPid(pid).setWineAmount(wine_available),
          0);
    }
  }

  void reserveSafePlace() {
    while (true) {
      {
        std::unique_lock<std::mutex>(m);
        want_to_enter_critical_section = true;
        critical_section_counter = config.winemakers - 1;

        config.forEachWinemaker([&](int process_id) {
          if (process_id != pid) {
            et.multicast(WinemakerMessage::WINEMAKER_SAFE_PLACE_REQUEST,
                         EntirePayload().setWinemakerPid(pid), process_id);
          }
        });
        et.updateClock();
      }

      if (critical_section_counter > 0) {
        std::unique_lock<std::mutex> lock(critical_section_wait_mutex);
        critical_section_wait.wait(lock);
      }
      {
        std::unique_lock<std::mutex>(m);
        // Critical section start
        want_to_enter_critical_section = false;

        bool ok = false;
        for (unsigned i = 0; i < config.safe_places; i++) {
          if (safe_places_free[i]) {
            std::unique_lock<std::mutex>(m);
            safe_places_free[i] = false;
            safe_place_id = i;
            wait_for_student = true;
            ok = true;
            in_safe_place = true;
            break;
          }
        }

        if (ok) {
          config.forEachWinemakerAndStudent([&](int process_id) {
            if (process_id != pid) {
              et.multicast(WinemakerMessage::WINEMAKER_SAFE_PLACE_RESERVED,
                           EntirePayload()
                               .setSafePlaceId(safe_place_id)
                               .setWineAmount(wine_available),
                           process_id);
            }
          });
          et.updateClock();

          if (safe_places_students_available[safe_place_id]) {
            wait_for_student = false;
          }
        }

        // Critical section end
        while (!wait_queue.empty()) {
          auto process_id = wait_queue.front();
          wait_queue.pop();
          et.send(WinemakerMessage::WINEMAKER_SAFE_PLACE_ACK, EntirePayload(),
                  process_id);
        }

        if (ok) {
          et.send(ObserverMessage::WINEMAKER_SAFE_PLACE_RESERVED,
                  EntirePayload().setWinemakerPid(pid).setSafePlaceId(
                      safe_place_id),
                  0);
          break;
        }
      }
    }
  }

  void handleSafePlace() {
    m.lock();
    while (wine_available > 0) {
      if (wait_for_student) {
        m.unlock();
        std::unique_lock<std::mutex> lock(student_wait_mutex);
        student_wait.wait(lock);
        m.lock();
      }

      wait_for_student = true;
      auto student_pid = safe_places_students_ids[safe_place_id];
      auto wine_requested = safe_places_students_wine_needs[safe_place_id];
      auto wine_given = std::min(wine_requested, wine_available);

      et.send(WinemakerMessage::HERE_YOU_ARE,
              EntirePayload().setWineAmount(wine_given), student_pid);

      et.send(ObserverMessage::WINEMAKER_GAVE_WINE_TO_STUDENT,
              EntirePayload()
                  .setWinemakerPid(pid)
                  .setStudentPid(student_pid)
                  .setSafePlaceId(safe_place_id)
                  .setWineAmount(wine_given),
              0);

      wine_available -= wine_given;
      safe_places_students_wine_needs[safe_place_id] -= wine_given;
    }

    m.unlock();
  }

  void leaveSafePlace() {
    std::unique_lock<std::mutex>(m);
    safe_places_free[safe_place_id] = true;
    in_safe_place = false;
    config.forEachWinemakerAndStudent([&](int process_id) {
      if (process_id != pid) {
        et.multicast(WinemakerMessage::WINEMAKER_SAFE_PLACE_LEFT,
                     EntirePayload().setSafePlaceId(safe_place_id), process_id);
      }
    });
    et.updateClock();

    et.send(ObserverMessage::WINEMAKER_SAFE_PLACE_LEFT,
            EntirePayload().setWinemakerPid(pid).setSafePlaceId(safe_place_id),
            0);
  }
};

class Student : public WorkingProcess {
public:
  Student(Config &config, unsigned pid)
      : config(config), pid(pid), safe_places_free(config.safe_places, true),
        safe_places_winemakers_available(config.safe_places, false),
        safe_places_winemakers_id(config.safe_places, 0),
        safe_places_winemakers_wine_available(config.safe_places, 0) {}

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
    while (true) {
      std::unique_lock<std::mutex>(m);
      auto response = et.receive<EntirePayload>(MPI_ANY_TAG, MPI_ANY_SOURCE);
      switch (response.message) {
      case StudentMessage::STUDENT_SAFE_PLACE_REQUEST: {
        auto my_clock = response.previousClock;
        auto opponent_clock = response.payload.clock;
        auto opponent_pid = response.payload.student_pid;

        if (!want_to_enter_critical_section || opponent_clock > my_clock ||
            (opponent_clock == my_clock && opponent_pid > pid)) {
          et.send(StudentMessage::STUDENT_SAFE_PLACE_ACK, EntirePayload(),
                  response.source);
        } else {
          wait_queue.push(response.source);
        }
        break;
      }
      case StudentMessage::STUDENT_SAFE_PLACE_ACK: {
        critical_section_counter--;
        if (critical_section_counter == 0) {
          critical_section_wait.notify_one();
        }
        break;
      }
      case StudentMessage::STUDENT_SAFE_PLACE_RESERVED: {
        auto spid = response.payload.safe_place_id;
        safe_places_free[spid] = false;
        break;
      }
      case StudentMessage::STUDENT_SAFE_PLACE_LEFT: {
        auto spid = response.payload.safe_place_id;
        safe_places_free[spid] = true;
        break;
      }
      case WinemakerMessage::WINEMAKER_SAFE_PLACE_RESERVED: {
        auto spid = response.payload.safe_place_id;
        safe_places_winemakers_available[spid] = true;
        safe_places_winemakers_id[spid] = response.payload.winemaker_pid;
        safe_places_winemakers_wine_available[spid] =
            response.payload.wine_amount;

        if (in_safe_place && safe_place_id == spid) {
          winemaker_wait.notify_one();
        }

        break;
      }
      case WinemakerMessage::WINEMAKER_SAFE_PLACE_LEFT: {
        auto spid = response.payload.safe_place_id;
        safe_places_winemakers_available[spid] = false;
        safe_places_winemakers_id[spid] = 0;
        safe_places_winemakers_wine_available[spid] = 0;
        break;
      }

      case WinemakerMessage::HERE_YOU_ARE: {
        auto wine = response.payload.wine_amount;
        wine_demand -= wine;
        safe_places_winemakers_wine_available[safe_place_id] -= wine;
        wine_gave_wait.notify_one();
        break;
      }
      }
    }
  }

private:
  Config &config;
  unsigned pid;
  unsigned wine_demand = 0;

  std::vector<bool> safe_places_free;
  std::vector<bool> safe_places_winemakers_available;
  std::vector<unsigned> safe_places_winemakers_id;
  std::vector<unsigned> safe_places_winemakers_wine_available;

  MessageTransmitter et;
  std::mutex critical_section_wait_mutex, winemaker_wait_mutex,
      wine_gave_wait_mutex, m;
  std::condition_variable critical_section_wait, winemaker_wait, wine_gave_wait;
  unsigned safe_place_id;
  unsigned critical_section_counter;
  bool want_to_enter_critical_section = false;
  bool wait_for_winemaker = true;
  bool in_safe_place = false;

  std::queue<unsigned> wait_queue;

  void relax() {
    {
      std::unique_lock<std::mutex>(m);
      et.send(ObserverMessage::STUDENT_DOESNT_WANT_TO_PARTY_ANYMORE,
              EntirePayload().setStudentPid(pid), 0);
    }

    auto duration = std::chrono::seconds(randint(1, 10));
    std::this_thread::sleep_for(duration);

    {
      std::unique_lock<std::mutex>(m);
      wine_demand = randint(1, config.max_wine_demand);
      et.send(ObserverMessage::STUDENT_WANT_TO_PARTY,
              EntirePayload().setStudentPid(pid).setWineAmount(wine_demand), 0);
    }
  }

  void reserveSafePlace() {
    while (true) {
      {
        std::unique_lock<std::mutex>(m);
        want_to_enter_critical_section = true;
        critical_section_counter = config.students - 1;

        config.forEachStudent([&](int process_id) {
          if (process_id != pid) {
            et.multicast(StudentMessage::STUDENT_SAFE_PLACE_REQUEST,
                         EntirePayload().setStudentPid(pid), process_id);
          }
        });
        et.updateClock();
      }

      if (critical_section_counter > 0) {
        std::unique_lock<std::mutex> lock(critical_section_wait_mutex);
        critical_section_wait.wait(lock);
      }

      {
        // Critical section start
        std::unique_lock<std::mutex>(m);
        want_to_enter_critical_section = false;
        bool ok = false;
        for (int i = 0; i < safe_places_free.size(); i++) {
          if (safe_places_free[i]) {
            safe_places_free[i] = false;
            safe_place_id = i;
            ok = true;
            in_safe_place = true;
            wait_for_winemaker = true;
            break;
          }
        }

        if (ok) {
          config.forEachWinemakerAndStudent([&](int process_id) {
            if (process_id != pid) {
              et.multicast(StudentMessage::STUDENT_SAFE_PLACE_RESERVED,
                           EntirePayload()
                               .setSafePlaceId(safe_place_id)
                               .setWineAmount(wine_demand),
                           process_id);
            }
          });
          et.updateClock();

          if (safe_places_winemakers_available[safe_place_id]) {
            wait_for_winemaker = false;
          }
        }

        // Critical section end
        while (!wait_queue.empty()) {
          unsigned process_id = wait_queue.front();
          wait_queue.pop();
          et.send(StudentMessage::STUDENT_SAFE_PLACE_ACK, EntirePayload(),
                  process_id);
        }

        if (ok) {
          et.send(
              ObserverMessage::STUDENT_SAFE_PLACE_RESERVED,
              EntirePayload().setStudentPid(pid).setSafePlaceId(safe_place_id),
              0);
          break;
        }
      }
    }
  }

  void handleSafePlace() {
    m.lock();
    while (wine_demand > 0) {
      if (wait_for_winemaker) {
        m.unlock();
        std::unique_lock<std::mutex> lock(winemaker_wait_mutex);
        winemaker_wait.wait(lock);
        m.lock();
      }
      wait_for_winemaker = true;

      m.unlock();
      {
        std::unique_lock<std::mutex> lock(wine_gave_wait_mutex);
        wine_gave_wait.wait(lock);
      }
      m.lock();
    }
    m.unlock();
  }

  void leaveSafePlace() {
    // Critical section start
    std::unique_lock<std::mutex>(m);
    safe_places_free[safe_place_id] = true;
    in_safe_place = false;
    config.forEachWinemakerAndStudent([&](int process_id) {
      if (process_id != pid) {
        et.multicast(StudentMessage::STUDENT_SAFE_PLACE_LEFT,
                     EntirePayload().setSafePlaceId(safe_place_id), process_id);
      }
    });
    et.updateClock();

    et.send(ObserverMessage::STUDENT_SAFE_PLACE_LEFT,
            EntirePayload().setStudentPid(pid).setSafePlaceId(safe_place_id),
            0);
  }
};
