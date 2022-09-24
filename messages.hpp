#pragma once

struct ObserverMessage {
  enum {
    // Winiarz rozpoczął produkcję wina
    // > Payload(_pid, clock)
    WINEMAKER_PRODUCTION_STARTED = 100,

    // Winiarz zakończył produkcję wina
    // > Payload(_pid, clock, wine_amount)
    WINEMAKER_PRODUCTION_END = 101,

    // Student nie chce już więcej imprezować i leczy kaca
    // > Payload(_pid, clock)
    STUDENT_DOESNT_WANT_TO_PARTY_ANYMORE = 102,

    // Student jest gotowy do zbierania wina
    // > Payload(_pid, clock, wine_amount)
    STUDENT_WANT_TO_PARTY = 103,

    // Winiarz zaniósł wino do meliny
    // > Payload(_pid, clock, safeplace_id, wine_amount)
    WINEMAKER_SAFE_PLACE_UPDATED = 104,

    // Student odebrał wino z meliny
    // > Payload(_pid, clock, safeplace_id, wine_amount)
    STUDENT_SAFE_PLACE_UPDATED = 105,
  };
};

struct CommonMessage {
  enum {
    // Winiarz/Student chce wejść do sekcji krytycznej
    // > Payload(_pid, clock)
    REQUEST = 200,

    // Zgoda na wejście do sekcji krytycznej
    // > Payload(clock)
    ACK = 201,

    // Zmiana ilości dostępnego wina w danej melinie
    // > Payload(clock, safe_place_id, wine_amount)
    // Uwaga: tu nie inkrementujemy/dekrementujemy, tylko przypisujemy
    SAFE_PLACE_UPDATED = 202,
  };
};
