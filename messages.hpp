#pragma once

struct ObserverMessage {
  enum {
    // Winiarz rozpoczął produkcję wina
    // > EntirePayload(clock, winemaker_pid)
    WINEMAKER_PRODUCTION_STARTED = 100,

    // Winiarz zakończył produkcję wina
    // > EntirePayload(clock, winemaker_pid, wine_amount)
    WINEMAKER_PRODUCTION_END,

    // Student nie chce już więcej imprezować i leczy kaca
    // > EntirePayload(clock, student_pid)
    STUDENT_DOESNT_WANT_TO_PARTY_ANYMORE,

    // Student jest gotowy do zbierania wina
    // > EntirePayload(clock, student_pid, wine_amount)
    STUDENT_WANT_TO_PARTY,

    // Winiarz zaniósł wino do meliny
    // > EntirePayload(clock, winemaker_pid, safeplace_id, wine_amount)
    WINEMAKER_SAFE_PLACE_UPDATED,

    // Student odebrał wino z meliny
    // > EntirePayload(clock, student_pid, safeplace_id, wine_amount)
    STUDENT_SAFE_PLACE_UPDATED,
  };
};

struct CommonMessage {
  enum {
    // Winiarz/Student chce wejść do sekcji krytycznej
    // > EntirePayload(clock, pid)
    // UWAGA: student_pid i winemaker_pid muszą być takie same!
    REQUEST = 200,

    // Zgoda na wejście do sekcji krytycznej
    // > EntirePayload(clock)
    ACK,

    // Zmiana ilości dostępnego wina w danej melinie
    // > EntirePayload(clock, safe_place_id, wine_amount)
    // Uwaga: tu nie inkrementujemy/dekrementujemy, tylko przypisujemy
    SAFE_PLACE_UPDATED,
  }
}
