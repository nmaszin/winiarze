#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <mpi.h>
#include <thread>

struct ObserverMessagePayload {
  unsigned winemaker_id;
  unsigned student_id;
  unsigned safe_place_id;
  unsigned wine_amount;
};

enum class ObserverMessages {
  // Winiarz zakończył produkcję wina (id winiarza, ile jest tego wina)
  WINEMAKER_PRODUCTION_END,

  // Winiarz uzyskał dostęp do meliny (id winiarza, numer meliny)
  WINEMAKER_RESERVED_SAFE_PLACE,

  // Winiarz opuścił melinę (id winiarza, numer meliny)
  WINEMAKER_LEFT_SAFE_PLACE,

  // Winiarz dał wino studentowi (id winiarza, id studenta, id meliny, ile wina
  WINEMAKER_GAVE_WINE_TO_STUDENT,

  // Winiarz rozpoczął produkcję wina (id winiarza)
  WINEMAKER_PRODUCTION_STARTED,

  // Student jest gotowy do zbierania wina (id studenta, ile wina)
  STUDENT_WANT_TO_PARTY,

  // Student zarezerwował melinę (id winiarza, id studenta, id meliny)
  STUDENT_RESERVED_SAFE_PLACE,

  // Student opuścił melinę (id studenta, id meliny, id winiarza)
  STUDENT_LEFT_SAFE_PLACE,

  // Student nie chce już więcej imprezować i leczy kaca (id studenta)
  STUDENT_DOESNT_WANT_TO_PARTY_ANYMORE
};

enum class WinemakerMessages {
  // Winiarz chce uzyskać dostęp do sekcji krytycznej związanej z meliną
  WINEMAKER_SAFE_PLACE_REQUEST,

  // Zgoda na wejście do sekcji krytycznej związanej z meliną
  WINEMAKE_SAFE_PLACE_ACK,

  // Winiarz chce opuścić sekcję krytyczną związaną z meliną
  WINEMAKER_SAFE_PLACE_RELEASE,

  // Winiarz zarezerwował melinę
  WINEMAKER_SAFE_PLACE_RESERVED,

  // Winiarz opuścił melinę
  WINEMAKER_SAFE_PLACE_LEFT,

  // Winiarz wręcza studentowi porcję wina
  HERE_YOU_ARE
};

enum class StudentMessages {
  // Student chce uzyskać dostęp do sekcji krytycznej związanej z meliną
  STUDENT_SAFE_PLACE_REQUEST,

  // Student uzyskał zgodę na wejście do sekcji krytycznej związanej z meliną
  STUDENT_SAFE_PLACE_ACK,

  // Student opuszcza sekcję krytyczną związaną z meliną
  STUDENT_SAFE_PLACE_RELEASE,

  // Student ogłasza innym studentom, że zarezerwował melinę
  STUDENT_SAFE_PLACE_RESERVED,

  // Student opuszcza melinę
  STUDENT_SAFE_PLACE_LEFT,

  // Student prosi winiarza o wino w jakiejś ilości
  GIVE_ME_WINE,
};

unsigned randint(unsigned min, unsigned max) {
  return min + (rand() % (max - min));
}

struct Config {
  unsigned wine_makers = 10;
  unsigned students = 20;
  unsigned safe_places = 5;
  unsigned max_wine_production = 10;
  unsigned max_wine_demand = 10;

  unsigned total_processes() { return wine_makers + students + 1; }
};

class WorkingProcess {
public:
  void run() {
    thread = std::make_unique<std::thread>([this]() { backgroundTask(); });
    foregroundTask();
  }

  virtual void foregroundTask() = 0;
  virtual void backgroundTask() = 0;

private:
  std::unique_ptr<std::thread> thread;
};

class Observer : public WorkingProcess {
public:
  Observer(Config &config) : config(config) {}

  void foregroundTask() override {
    while (true) {
      ObserverMessagePayload payload;
      MPI_Status status;

      MPI_Recv(&payload, sizeof(payload), MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG,
               MPI_COMM_WORLD, NULL);

      ObserverMessages message = static_cast<ObserverMessages>(status.MPI_TAG);
      switch (message) {
      case ObserverMessages::WINEMAKER_PRODUCTION_END:
        std::cout << "Winiarz o id " << payload.winemaker_id
                  << " zakończył produkcję i wyprodukował "
                  << payload.wine_amount << " jednostek wina\n";
        break;

      case ObserverMessages::WINEMAKER_RESERVED_SAFE_PLACE:
        std::cout << "Winiarz o id " << payload.winemaker_id
                  << " zarezerwował melinę o id " << payload.safe_place_id
                  << "\n";
        break;

      case ObserverMessages::WINEMAKER_LEFT_SAFE_PLACE:
        std::cout << "Winiarz o id " << payload.winemaker_id
                  << " opuścił melinę o id " << payload.safe_place_id << "\n";
        break;

      case ObserverMessages::WINEMAKER_GAVE_WINE_TO_STUDENT:
        std::cout << "Winiarz o id " << payload.winemaker_id
                  << " dał studentowi o id " << payload.student_id
                  << " wino w ilości " << payload.wine_amount
                  << " w melinie o id " << payload.safe_place_id << "\n";
        break;

      case ObserverMessages::WINEMAKER_PRODUCTION_STARTED:
        std::cout << "Winiarz o id " << payload.winemaker_id
                  << " rozpoczął produkcję\n";
        break;

      case ObserverMessages::STUDENT_WANT_TO_PARTY:
        std::cout << "Student o id " << payload.student_id
                  << " wyleczył kaca i potrzebuje " << payload.wine_amount
                  << " jednostek wina na kolejną imprezę\n";
        break;

      case ObserverMessages::STUDENT_RESERVED_SAFE_PLACE:
        std::cout << "Student o id " << payload.student_id
                  << " zarezerwował melinę o id " << payload.safe_place_id
                  << " w której siedzi winiarz o id " << payload.winemaker_id
                  << "\n";
        break;

      case ObserverMessages::STUDENT_LEFT_SAFE_PLACE:
        std::cout << "Student o id " << payload.student_id
                  << " opuścił melinę o id " << payload.safe_place_id
                  << " w której siedzi winiarz o id " << payload.winemaker_id
                  << "\n";
        break;

      case ObserverMessages::STUDENT_DOESNT_WANT_TO_PARTY_ANYMORE:
        std::cout << "Student o id " << payload.student_id << " ma kaca\n";
        break;
      }
    }
  }

  void backgroundTask() override {}

private:
  Config &config;
};

class WineMaker : public WorkingProcess {
public:
  WineMaker(Config &config) : config(config) {}

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
  unsigned wine_available = 0;

  void makeWine() {
    auto duration = std::chrono::seconds(randint(1, 10));
    std::this_thread::sleep_for(duration);
    wine_available = randint(1, config.max_wine_production);
  }

  void reserveSafePlace() {
    // TODO:
  }

  void handleSafePlace() {
    while (wine_available > 0) {
      // TODO:
    }
  }

  void leaveSafePlace() {
    // TODO
  }
};

class Student : public WorkingProcess {
public:
  Student(Config &config) : config(config) {}

  void foregroundTask() override {
    // TODO:
  }

  void backgroundTask() override {
    // TODO
  }

private:
  Config &config;
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

int main(int argc, char *argv[]) {
  MPI_Init(&argc, &argv);

  Config config;

  int current_number_of_processes;
  int expected_number_of_processes = config.total_processes();
  MPI_Comm_size(MPI_COMM_WORLD, &current_number_of_processes);
  if (current_number_of_processes != expected_number_of_processes) {
    std::cerr << "Invalid number of processes\n";
    std::cerr << "With current configuration you should run "
              << expected_number_of_processes << " processes\n";
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  int process_id;
  MPI_Comm_rank(MPI_COMM_WORLD, &process_id);

  std::unique_ptr<WorkingProcess> process;
  if (process_id == 0) {
    // Observer
    process = std::make_unique<Observer>(config);
  } else if (process_id <= config.wine_makers) {
    process = std::make_unique<WineMaker>(config);
  } else {
    process = std::make_unique<Student>(config);
  }

  process->run();

  MPI_Finalize();
}
