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
  Config &config;
  unsigned pid;
  MessageTransmitter t;

  std::vector<unsigned> winemakers_wine_amounts;
  std::vector<unsigned> students_wine_needs;
  std::vector<unsigned> safe_places_wine_amounts;
  std::vector<bool> winemakers_working;
  std::vector<bool> students_resting;
  unsigned free_safe_places = 0;

public:
  Observer(Config &config, unsigned pid)
      : config(config), pid(pid), winemakers_wine_amounts(config.winemakers, 0),
        students_wine_needs(config.students, 0),
        safe_places_wine_amounts(config.safe_places, 0),
        winemakers_working(config.winemakers, false),
        students_resting(config.students, false) {}

  void run() override {
    while (true) {
      auto response = t.receive<EntirePayload>(MPI_ANY_TAG, MPI_ANY_SOURCE);
      const auto &payload = response.paylod;

      switch (response.message) {
      case ObserverMessage::WINEMAKER_PRODUCTION_STARTED: {
        auto wid = config.getWinemakerIdFromPid(payload.winemaker_pid);
        winemakers_working[wid] = true;
        std::cout << "Winiarz o id " << wid + 1 << " rozpoczął produkcję\n";
        break;
      }

      case ObserverMessage::WINEMAKER_PRODUCTION_END: {
        auto wid = config.getWinemakerIdFromPid(payload.winemaker_pid);
        winemakers_working[wid] = false;
        winemakers_wine_amounts[wid] = payload.wine_amount;

        std::cout << "Winiarz o id " << wid + 1
                  << " zakończył produkcję i wyprodukował "
                  << payload.wine_amount << " jednostek wina\n";

        break;
      }

      case ObserverMessage::STUDENT_DOESNT_WANT_TO_PARTY_ANYMORE: {
        auto sid = config.getStudentIdFromPid(payload.student_pid);
        students_resting[sid] = true;
        std::cout << "Student o id " << sid + 1 << " ma kaca\n";
        break;
      }

      case ObserverMessage::STUDENT_WANT_TO_PARTY: {
        auto sid = config.getStudentIdFromPid(payload.student_pid);
        students_resting[sid] = false;
        students_wine_needs[sid] = payload.wine_amount;

        std::cout << "Student o id " << sid + 1
                  << " wyleczył kaca i potrzebuje " << payload.wine_amount
                  << " jednostek wina na kolejną imprezę\n";
        break;
      }

      case ObserverMessage::WINEMAKER_SAFE_PLACE_UPDATED: {
        auto wid = config.getWinemakerIdFromPid(payload.winemaker_pid);
        auto spid = payload.safe_place_id;
        auto &r = safe_places_wine_amounts[spid];

        auto old_value = r;
        r += payload.wine_amount;

        if (old_value == 0) {
          free_safe_places--;
        }

        std::cout << "Winiarz o id " << wid + 1 << " przyniósł "
                  << payload.wine_amount << " jednostek wina, do meliny nr "
                  << spid + 1 << "\n";

        std::cout << "Aktualna liczba pustych melin to " << free_safe_places
                  << "\n";
        break;
      }

      case ObserverMessage::STUDENT_SAFE_PLACE_UPDATED: {
        auto sid = config.getWinemakerIdFromPid(payload.student_pid);
        auto spid = payload.safe_place_id;
        auto &r = safe_places_wine_amounts[spid];

        r -= paylod.wine_amount;
        auto new_value = r;
        if (new_value == 0) {
          free_safe_places++;
        }

        std::cout << "Student o id " << sid + 1 << " zabrał "
                  << payload.wine_amount << " jednostek wina, z meliny nr "
                  << spid + 1 << "\n";

        std::cout << "Aktualna liczba pustych melin to " << free_safe_places
                  << "\n";
        break;
      }
      }

      printState();
      std::cout << "\n";
    }
  }

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

    std::cout << "\t Meliny:   \t";
    for (unsigned i = 0; i < config.safe_places; i++) {
      std::cout << safe_places_wine_amounts[i] << "\t";
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
};

class Winemaker : public WorkingProcess {
  Config &config;
  unsigned pid;
  MessageTransmitter t;

  unsigned wine_available = 0;
  unsigned safe_place_id;
  std::vector<unsigned> safe_places_wine_amounts(config.safe_places, 0);
  unsigned free_safe_places;

  bool want_to_enter_critical_section = false;
  std::queue<int> wait_queue;
  std::mutex m, critical_section_wait_mutex;
  std::condition_variable critical_section_wait;
  unsigned ack_counter = 0;

public:
  Winemaker(Config &config, unsigned pid)
      : config(config), pid(pid), free_safe_places(config.safe_places) {}

protected:
  void foregroundTask() override {
    while (true) {
      makeWine();
      handleSafePlace();
    }
  }

  void backgroundTask() override {
    while (true) {
      auto response = et.receive<EntirePayload>(MPI_ANY_TAG, MPI_ANY_SOURCE);
      const auto &payload = response.paylod;
      std::lock_guard<std::mutex>(m);

      switch (response.mesage) {
      case CommonMessage::REQUEST: {
        auto my_clock = response.previousClock;
        auto opponent_clock = payload.clock;
        auto opponent_pid = payload.winemaker_pid;

        if (!want_to_enter_critical_section || opponent_clock < my_clock ||
            (opponent_clock == my_clock && opponent_pid < pid)) {
          t.send(CommonMessage::ACK, EntirePayload(), response.source);
        } else {
          wait_queue.push(response.source);
        }

        break;
      }

      case CommonMessage::ACK: {
        ack_counter--;
        if (ack_counter == 0) {
          critical_section_wait.notify_one();
        }
        break;
      }

      case CommonMessage::SAFE_PLACE_UPDATED: {
        auto spid = payload.safe_place_id;
        auto &r = safe_places_wine_amounts[spid];

        auto old_value = r;
        r = payload.wine_amount;
        auto new_value = r;

        if (old_value == 0 && new_value > 0) {
          free_safe_places--;
        } else if (old_value > 0 && new_value == 0) {
          free_safe_places++;
          free_safe_places_wait.notify_one();
        }

        break;
      }
      }
    }
  }

  void makeWine() {
    m.lock();
    t.send(ObserverMessage::WINEMAKER_PRODUCTION_STARTED,
           EntirePayload().setWinemakerPid(pid), 0);
    m.unlock();

    auto duration = std::chrono::seconds(randint(1, 10));
    std::this_thread::sleep_for(duration);

    m.lock();
    wine_available = randint(1, config.max_wine_production);
    t.send(ObserverMessage::WINEMAKER_PRODUCTION_END,
           EntirePayload().setWinemakerPid(pid).setWineAmount(wine_available),
           0);
    m.unlock();
  }

  void handleSafePlace() {
    m.lock();

    want_to_enter_critical_section = true;
    ack_counter = config.winemakers + config.students - 1;

    auto payload = EntirePayload().setWinemakerPid(pid).setStudentPid(pid);
    t.updateClock(payload);
    config.forEachWinemakerAndStudent([&](int process_id) {
      if (process_id != pid) {
        auto payload_copy = payload;
        t.multicast(CommonMessage::REQUEST, payload_copy, process_id);
      }
    });

    if (ack_counter > 0) {
      m.unlock();
      critical_section_wait_mutex.lock();
      critical_section_wait.wait();
      critical_section_wait_mutex.unlock();
      m.lock();
    }

    // Critical section start
    want_to_enter_critical_section = false;
    bool ok = false;
    for (unsigned i = 0; i < config.safe_places; i++) {
      if (safe_places_wine_amounts[i] == 0) {
        ok = true;
        safe_place_id = i;
      }
    }
    if (!ok) {
      safe_place_id = randint(0, config.safe_places);
    }

    auto payload = EntirePayload()
                       .setSafePlaceId(safe_place_id)
                       .setWineAmount(wine_available);

    t.updateClock(payload);
    config.forEachWinemakerAndStudent([&](int process_id) {
      if (process_id != pid) {
        auto payload_copy = payload;
        t.multicast(CommonMessage::SAFE_PLACE_UPDATED, payload_copy,
                    process_id);
      }
    });

    wine_available = 0;

    std::this_thread::sleep_for(std::chrono::seconds(randint(1, 3)));

    // Critical section end
    while (!wait_queue.empty()) {
      auto process_id = wait_queue.front();
      wait_queue.pop();
      t.send(CommonMessage::ACK, EntirePayload(), process_id);
    }

    m.unlock();
  }
};

class Student : public WorkingProces {
  Config &config;
  unsigned pid;
  MessageTransmitter t;

  unsigned wine_demand;
  std::vector<unsigned> safe_places_wine_amounts;

  std::mutex m, critical_section_wait_mutex;
  std::condition_variable critical_section_wait;
  bool want_to_enter_critical_section = false;
  std::queue<int> wait_queue;
  unsigned ack_counter;
  unsigned free_safe_places;

public:
  Student(Config &config, unsigned pid)
      : config(config), pid(pid),
        free_safe_places(config.safe_places)
            safe_places_wine_amounts(config.safe_places, 0) {}

protected:
  void backgroundTask() override {
    while (true) {
      auto response = t.receive<EntirePayload>(MPI_ANY_TAG, MPI_ANY_SOURCE);
      const auto &payload = response.payload;
      std::lock_guard<std::mutex>(m);

      switch (response.message) {
      case CommonMessage::REQUEST: {
        auto my_clock = response.previousClock;
        auto opponent_clock = payload.clock;
        auto opponent_pid = payload.winemaker_pid;

        if (!want_to_enter_critical_section || opponent_clock < my_clock ||
            (opponent_clock == my_clock && opponent_pid < pid)) {
          t.send(CommonMessage::ACK, EntirePayload(), response.source);
        } else {
          wait_queue.push(response.source);
        }

        break;
      }

      case CommonMessage::ACK: {
        ack_counter--;
        if (ack_counter == 0) {
          critical_section_wait.notify_one();
        }
        break;
      }

      case CommonMessage::SAFE_PLACE_UPDATED: {
        auto spid = payload.safe_place_id;
        auto &r = safe_places_wine_amounts[spid];

        auto old_value = r;
        r = payload.wine_amount;
        auto new_value = r;

        if (old_value == 0 && new_value > 0) {
          free_safe_places--;
        } else if (old_value > 0 && new_value == 0) {
          free_safe_places++;
          free_safe_places_wait.notify_one();
        }

        break;
      }
      }
    }
  }

  void foregroundTask() override {
    relax();
    handleSafePlace();
  }

  void relax();
}

// ----------------------

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

        std::cerr << "[Student] wine_demand = " << wine_demand << "\n";
        safe_places_winemakers_wine_available[safe_place_id] -= wine;

        auto payload =
            EntirePayload().setSafePlaceId(safe_place_id).setWineAmount(wine);
        et.updateClock(payload);
        config.forEachStudent([&](int process_id) {
          if (process_id != pid) {
            auto payload_copy = payload;
            et.multicast(StudentMessage::WINEMAKER_WINE_AMOUNT_DECREASED,
                         payload_copy, process_id);
          }
        });

        std::cerr << "[Student] Powiadamiam\n";
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
  unsigned critical_section_counter, process_id;
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

        auto payload = EntirePayload();
        et.updateClock(payload);
        config.forEachStudent([&](int process_id) {
          if (process_id != pid) {
            auto payload_copy = payload;
            et.multicast(StudentMessage::STUDENT_SAFE_PLACE_REQUEST,
                         payload_copy.setStudentPid(pid), process_id);
          }
        });
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
          auto payload = EntirePayload()
                             .setSafePlaceId(safe_place_id)
                             .setWineAmount(wine_demand);
          et.updateClock(payload);
          config.forEachWinemakerAndStudent([&](int process_id) {
            if (process_id != pid) {
              auto payload_copy = payload;
              et.multicast(StudentMessage::STUDENT_SAFE_PLACE_RESERVED,
                           payload_copy, process_id);
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
        std::cerr << "[xD] start\n";
        std::unique_lock<std::mutex> lock(wine_gave_wait_mutex);
        std::cerr << "[xD] środek\n";
        wine_gave_wait.wait(lock);
        std::cerr << "[xD] koniec\n";
      }
      m.lock();
    }
    std::cerr << "[Student] No to kończymy\n";
    m.unlock();
  }

  void leaveSafePlace() {
    std::cerr << "[Student] Czekam na mutex\n";
    std::lock_guard<std::mutex> lock(m);
    std::cerr << "[Student] już nie czekam na mutex\n";
    safe_places_free[safe_place_id] = true;
    in_safe_place = false;

    auto payload = EntirePayload().setSafePlaceId(safe_place_id);
    et.updateClock(payload);
    config.forEachWinemakerAndStudent([&](int process_id) {
      if (process_id != pid) {
        auto payload_copy = payload;
        et.multicast(StudentMessage::STUDENT_SAFE_PLACE_LEFT, payload_copy,
                     process_id);
      }
    });

    et.send(ObserverMessage::STUDENT_SAFE_PLACE_LEFT,
            EntirePayload().setStudentPid(pid).setSafePlaceId(safe_place_id),
            0);
  }
};
