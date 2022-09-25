#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mpi.h>
#include <queue>
#include <thread>
#include <vector>

#include "messages.hpp"
#include "payload.hpp"
#include "transmitter.hpp"
#include "utils.hpp"

struct Runnable {
  virtual void run() = 0;
};

class Observer : public Runnable {
  Config &config;
  int pid;
  MessageTransmitter t;

  std::vector<int> winemakers_wine_amounts;
  std::vector<int> students_wine_needs;
  std::vector<int> safe_places_wine_amounts;
  std::vector<bool> winemakers_working;
  std::vector<bool> students_resting;
  int free_safe_places;

public:
  Observer(Config &config, int pid)
      : config(config), pid(pid), free_safe_places(config.safe_places),
        winemakers_wine_amounts(config.winemakers, 0),
        students_wine_needs(config.students, 0),
        safe_places_wine_amounts(config.safe_places, 0),
        winemakers_working(config.winemakers, false),
        students_resting(config.students, false) {}

  void run() override {
    while (true) {
    } // TODO: naprawić
    {
      auto response = t.receive(MPI_ANY_TAG, MPI_ANY_SOURCE);
      const auto &payload = response.payload;

      switch (response.message) {
      case ObserverMessage::WINEMAKER_PRODUCTION_STARTED: {
        auto wid = config.getWinemakerIdFromPid(response.source);
        winemakers_working[wid] = true;
        std::cout << "Winiarz o id " << wid + 1 << " rozpoczął produkcję\n";
        break;
      }

      case ObserverMessage::WINEMAKER_PRODUCTION_END: {
        auto wid = config.getWinemakerIdFromPid(response.source);
        winemakers_working[wid] = false;
        winemakers_wine_amounts[wid] = payload.wine_amount;

        std::cout << "Winiarz o id " << wid + 1
                  << " zakończył produkcję i wyprodukował "
                  << payload.wine_amount << " jednostek wina\n";

        break;
      }

      case ObserverMessage::STUDENT_DOESNT_WANT_TO_PARTY_ANYMORE: {
        auto sid = config.getStudentIdFromPid(response.source);
        students_resting[sid] = true;
        std::cout << "Student o id " << sid + 1 << " ma kaca\n";
        break;
      }

      case ObserverMessage::STUDENT_WANT_TO_PARTY: {
        auto sid = config.getStudentIdFromPid(response.source);
        students_resting[sid] = false;
        students_wine_needs[sid] = payload.wine_amount;

        std::cout << "Student o id " << sid + 1
                  << " wyleczył kaca i potrzebuje " << payload.wine_amount
                  << " jednostek wina na kolejną imprezę\n";
        break;
      }

      case ObserverMessage::WINEMAKER_SAFE_PLACE_UPDATED: {
        auto wid = config.getWinemakerIdFromPid(response.source);
        auto spid = payload.safe_place_id;
        auto &r = safe_places_wine_amounts[spid];

        auto increase = payload.wine_amount - r;
        if (r == 0 && increase > 0) {
          free_safe_places--;
        }
        r = payload.wine_amount;
        winemakers_wine_amounts[wid] -= increase;

        std::cout << "Winiarz o id " << wid + 1 << " przyniósł " << increase
                  << " jednostek wina, do meliny nr " << spid + 1 << "\n";

        std::cout << "Aktualna liczba pustych melin to " << free_safe_places
                  << "\n";
        break;
      }

      case ObserverMessage::STUDENT_SAFE_PLACE_UPDATED: {
        auto sid = config.getStudentIdFromPid(response.source);
        auto spid = payload.safe_place_id;
        auto &r = safe_places_wine_amounts[spid];

        auto decrease = r - payload.wine_amount;
        r = payload.wine_amount;
        students_wine_needs[sid] -= decrease;
        if (r == 0 && decrease > 0) {
          free_safe_places++;
        }

        std::cout << "Student o id " << sid + 1 << " zabrał " << decrease
                  << " jednostek wina, z meliny nr " << spid + 1 << "\n";

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
    for (int i = 0; i < identifiers_number; i++) {
      std::cout << i + 1 << "\t";
    }
    std::cout << "\n------------------------------------------------\n";

    std::cout << "\tWiniarze:\t";
    for (int i = 0; i < config.winemakers; i++) {
      if (winemakers_working[i]) {
        std::cout << "W\t";
      } else {
        std::cout << winemakers_wine_amounts[i] << "\t";
      }
    }
    std::cout << '\n';

    std::cout << "\t Meliny:   \t";
    for (int i = 0; i < config.safe_places; i++) {
      std::cout << safe_places_wine_amounts[i] << "\t";
    }
    std::cout << '\n';

    std::cout << "\tStudenci:\t";
    for (int i = 0; i < config.students; i++) {
      if (students_resting[i]) {
        std::cout << "R\t";
      } else {
        std::cout << students_wine_needs[i] << "\t";
      }
    }
    std::cout << '\n';
  }
};

class WorkingProcess : public Runnable {
public:
  void run() {
    thread = std::move(std::thread(&WorkingProcess::backgroundTask, this));
    foregroundTask();
  }

protected:
  virtual void foregroundTask() = 0;
  virtual void backgroundTask() = 0;

private:
  std::thread thread;
};

struct Winemaker : public WorkingProcess {
  Config &config;
  int pid;
  MessageTransmitter t, ot;

  int wine_available = 0;
  std::vector<int> safe_places_wine_amounts;

  bool want_to_enter_critical_section = false;
  bool wait_ready = false;
  std::mutex wait_ready_mutex;

  int ack_counter;
  std::queue<int> wait_queue;
  std::mutex data_mutex, critical_section_wait_mutex;
  std::condition_variable critical_section_wait;

  Winemaker(Config &config, int pid)
      : config(config), pid(pid),
        safe_places_wine_amounts(config.safe_places, 0) {}

  void foregroundTask() override {
    while (true) {
      makeWine();
      while (wine_available > 0) {
        deliverWine();
      }
    }
  }

  void makeWine() {
    ot.send(ObserverMessage::WINEMAKER_PRODUCTION_STARTED, Payload(), 0);

    sleep(randint(1, config.max_sleep_time));
    {
      std::lock_guard<std::mutex> lock(data_mutex);
      wine_available = randint(1, config.max_wine_production);
    }

    ot.send(ObserverMessage::WINEMAKER_PRODUCTION_END,
            Payload().setWineAmount(wine_available), 0);
  }

  void deliverWine() {
    {
      std::lock_guard<std::mutex> lock(data_mutex);
      want_to_enter_critical_section = true;
      ack_counter = config.winemakers + config.students - 1;

      t.startBroadcast();
      config.forEachWinemakerAndStudent([&](int process_id) {
        if (process_id != pid) {
          t.sendBroadcast(CommonMessage::REQUEST, Payload(), process_id);
        }
      });
      t.stopBroadcast();
    }

    print.lock();
    std::cerr << process::rank << "Czekam se, w morde jeża\n";
    print.unlock();

    {
      // std::unique_lock<std::mutex> lock(critical_section_wait_mutex);
      // critical_section_wait.wait(lock);
      while (true) {
        wait_ready_mutex.lock();
        bool x = wait_ready;
        wait_ready_mutex.unlock();

        if (x) {
          break;
        }
      }

      wait_ready_mutex.lock();
      wait_ready = false;
      wait_ready_mutex.unlock();
    }

    print.lock();
    std::cerr << process::rank << "Już se nie czekam, w morde jeża\n";
    print.unlock();

    {
      std::lock_guard<std::mutex> lock(data_mutex);
      want_to_enter_critical_section = false;
      // CRITICAL SECTION START
      sleep(randint(1, config.max_sleep_time));
      for (int i = 0; i < config.safe_places; i++) {
        if (safe_places_wine_amounts[i] == 0) {
          safe_places_wine_amounts[i] = wine_available;
          wine_available = 0;

          auto payload = Payload().setSafePlaceId(i).setWineAmount(
              safe_places_wine_amounts[i]);

          auto payload_copy = payload;
          ot.send(ObserverMessage::WINEMAKER_SAFE_PLACE_UPDATED,
                  std::move(payload_copy), 0);

          t.startBroadcast();
          config.forEachWinemakerAndStudent([&](int process_id) {
            if (process_id != pid) {
              auto payload_copy = payload;
              t.sendBroadcast(CommonMessage::SAFE_PLACE_UPDATED,
                              std::move(payload_copy), process_id);
            }
          });
          t.stopBroadcast();
          break;
        }
      }

      sleep(randint(1, config.max_sleep_time));
      // CRITICAL SECTION END
      t.startBroadcast();
      while (!wait_queue.empty()) {
        auto process_id = wait_queue.front();
        wait_queue.pop();
        t.sendBroadcast(CommonMessage::ACK, Payload(), process_id);
      }
      t.stopBroadcast();
    }
  }

  void backgroundTask() override {
    while (true) {
      auto response = t.receive(MPI_ANY_TAG, MPI_ANY_SOURCE);
      std::lock_guard<std::mutex> lock(data_mutex);
      const auto &payload = response.payload;

      switch (response.message) {
      case CommonMessage::REQUEST: {
        auto my_clock = response.previousClock;
        auto opponent_clock = payload.clock;
        auto opponent_pid = response.source;

        print.lock();
        std::cerr << process::rank
                  << "PRZYSZEDŁ REQUEST (mój zegar: " << my_clock
                  << ", jego zegar: " << opponent_clock << "): " << std::flush;

        if ((want_to_enter_critical_section &&
             (my_clock < opponent_clock ||
              (my_clock == opponent_clock && pid < opponent_pid)))) {
          std::cerr << "Czekam... " << std::flush;
          std::cerr << "Musisz poczekać!\n";
          print.unlock();
          wait_queue.push(response.source);
        } else {
          std::cerr << "Zgadzam się\n";
          print.unlock();
          t.send(CommonMessage::ACK, Payload(), response.source);
        }

        break;
      }

      case CommonMessage::ACK: {
        ack_counter--;
        if (ack_counter == 0) {
          // critical_section_wait.notify_one();
          wait_ready_mutex.lock();
          print.lock();
          std::cout << process::rank << "ACK_COUNTER = " << ack_counter << "\n";
          print.unlock();
          wait_ready = true;
          wait_ready_mutex.unlock();
        }
        break;
      }

      case CommonMessage::SAFE_PLACE_UPDATED: {
        auto spid = payload.safe_place_id;
        safe_places_wine_amounts[spid] = payload.wine_amount;
        break;
      }
      }
    }
  }
};

/*
struct Student : public WorkingProcess {
  Config &config;
  int pid;
  MessageTransmitter t, ot;

  int wine_demand = 0;
  std::vector<int> safe_places_wine_amounts;

  std::atomic<bool> want_to_enter_critical_section;
  int ack_counter;
  std::queue<int> wait_queue;
  std::mutex data_mutex, critical_section_wait_mutex;
  std::condition_variable critical_section_wait;
  std::atomic<bool> wait_ready;

  Student(Config &config, int pid)
      : config(config), pid(pid),
        safe_places_wine_amounts(config.safe_places, 0), wait_ready(false),
        want_to_enter_critical_section(false) {}

  void foregroundTask() override {
    while (true) {
      drinkWine();
      while (wine_demand > 0) {
        receiveWine();
      }
    }
  }

  void drinkWine() {
    ot.send(ObserverMessage::STUDENT_DOESNT_WANT_TO_PARTY_ANYMORE, Payload(),
            0);

    sleep(randint(1, config.max_sleep_time));
    wine_demand = randint(1, config.max_wine_demand);

    ot.send(ObserverMessage::STUDENT_WANT_TO_PARTY,
            Payload().setWineAmount(wine_demand), 0);
  }

  void receiveWine() {
    want_to_enter_critical_section.store(true, std::memory_order_release);
    {
      std::lock_guard<std::mutex> lock(data_mutex);
      ack_counter = config.winemakers + config.students - 1;
    }

    t.startBroadcast();
    config.forEachWinemakerAndStudent([&](int process_id) {
      if (process_id != pid) {
        t.sendBroadcast(CommonMessage::REQUEST, Payload(), process_id);
      }
    });
    t.stopBroadcast();

    print.lock();
    std::cerr << process::rank << "Czekam se, w morde jeża\n";
    print.unlock();
    {
      // std::unique_lock<std::mutex> lock(critical_section_wait_mutex);
      // critical_section_wait.wait(lock);
      while (!wait_ready.load(std::memory_order_acquire)) {
      }
      wait_ready.store(false, std::memory_order_release);
    }
    print.lock();
    std::cerr << process::rank << "Już se nie czekam, w morde jeża\n";
    print.unlock();

    // CRITICAL SECTION START
    sleep(randint(1, config.max_sleep_time));
    data_mutex.lock();
    for (int i = 0; i < config.safe_places; i++) {
      if (wine_demand == 0) {
        break;
      }

      if (safe_places_wine_amounts[i] > 0) {
        auto quantity = std::min(wine_demand, safe_places_wine_amounts[i]);
        wine_demand -= quantity;
        safe_places_wine_amounts[i] -= quantity;

        auto payload = Payload().setSafePlaceId(i).setWineAmount(
            safe_places_wine_amounts[i]);

        auto payload_copy = payload;
        ot.send(ObserverMessage::STUDENT_SAFE_PLACE_UPDATED,
                std::move(payload_copy), 0);

        t.startBroadcast();
        config.forEachWinemakerAndStudent([&](int process_id) {
          if (process_id != pid) {
            auto payload_copy = payload;
            t.sendBroadcast(CommonMessage::SAFE_PLACE_UPDATED,
                            std::move(payload_copy), process_id);
          }
        });
        t.stopBroadcast();
      }
    }
    data_mutex.unlock();
    sleep(randint(1, config.max_sleep_time));
    want_to_enter_critical_section.store(false, std::memory_order_release);
    // CRITICAL SECTION END

    data_mutex.lock();
    t.startBroadcast();
    while (!wait_queue.empty()) {
      auto process_id = wait_queue.front();
      wait_queue.pop();
      t.sendBroadcast(CommonMessage::ACK, Payload(), process_id);
    }
    t.stopBroadcast();
    data_mutex.unlock();
  }

  void backgroundTask() override {
    while (true) {
      auto response = t.receive(MPI_ANY_TAG, MPI_ANY_SOURCE);
      const auto &payload = response.payload;

      switch (response.message) {
      case CommonMessage::REQUEST: {
        auto my_clock = response.previousClock;
        auto opponent_clock = payload.clock;
        auto opponent_pid = response.source;

        print.lock();
        std::cerr << process::rank
                  << "PRZYSZEDŁ REQUEST (mój zegar: " << my_clock
                  << ", jego zegar: " << opponent_clock << "): " << std::flush;

        if (!want_to_enter_critical_section.load(std::memory_order_acquire) ||
            opponent_clock < my_clock ||
            (opponent_clock == my_clock && opponent_pid < pid)) {
          std::cerr << "Zgadzam się\n";
          print.unlock();
          t.send(CommonMessage::ACK, Payload(), response.source);
        } else {
          std::cerr << "Czekam... " << std::flush;
          print.unlock();
          std::lock_guard<std::mutex> lock(data_mutex);
          print.lock();
          std::cerr << "Musisz poczekać!\n";
          print.unlock();
          wait_queue.push(response.source);
        }

        break;
      }

      case CommonMessage::ACK: {
        std::lock_guard<std::mutex> lock(data_mutex);
        ack_counter--;
        if (ack_counter == 0) {
          // critical_section_wait.notify_one();
          wait_ready.store(true, std::memory_order_release);
        }
        break;
      }

      case CommonMessage::SAFE_PLACE_UPDATED: {
        std::lock_guard<std::mutex> lock(data_mutex);
        auto spid = payload.safe_place_id;
        safe_places_wine_amounts[spid] = payload.wine_amount;
        break;
      }
      }
    }
  }
};

*/
