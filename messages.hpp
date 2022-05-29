#pragma once

struct ObserverMessage {
  enum {
    // Winiarz zakończył produkcję wina (id winiarza, ile jest tego wina)
    WINEMAKER_PRODUCTION_END = 100,

    // Winiarz uzyskał dostęp do meliny (id winiarza, numer meliny)
    WINEMAKER_RESERVED_SAFE_PLACE,

    // Winiarz opuścił melinę (id winiarza, numer meliny)
    WINEMAKER_LEFT_SAFE_PLACE,

    // Winiarz dał wino studentowi (id winiarza, id studenta, id meliny, ile
    // wina
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
};

struct WinemakerMessage {
  enum {
    // Winiarz chce uzyskać dostęp do sekcji krytycznej związanej z meliną
    // (clock)
    WINEMAKER_SAFE_PLACE_REQUEST = 200,

    // Zgoda na wejście do sekcji krytycznej związanej z meliną
    WINEMAKER_SAFE_PLACE_ACK,

    // Winiarz zarezerwował melinę
    WINEMAKER_SAFE_PLACE_RESERVED,

    // Winiarz opuścił melinę
    WINEMAKER_SAFE_PLACE_LEFT,

    // Winiarz wręcza studentowi porcję wina
    HERE_YOU_ARE
  };
};

struct StudentMessage {
  enum {
    // Student chce uzyskać dostęp do sekcji krytycznej związanej z meliną
    STUDENT_SAFE_PLACE_REQUEST = 300,

    // Student uzyskał zgodę na wejście do sekcji krytycznej związanej z meliną
    STUDENT_SAFE_PLACE_ACK,

    // Student ogłasza innym studentom, że zarezerwował melinę
    STUDENT_SAFE_PLACE_RESERVED,

    // Student opuszcza melinę
    STUDENT_SAFE_PLACE_LEFT,
  };
};
