#pragma once

struct ObserverMessage {
  enum {
    // Winiarz rozpoczął produkcję wina (id winiarza)
    WINEMAKER_PRODUCTION_STARTED,

    // Winiarz zakończył produkcję wina (id winiarza, ile jest tego wina)
    WINEMAKER_PRODUCTION_END = 100,

    // Winiarz uzyskał dostęp do meliny (id winiarza, numer meliny)
    WINEMAKER_SAFE_PLACE_RESERVED,

    // Winiarz opuścił melinę (id winiarza, numer meliny)
    WINEMAKER_SAFE_PLACE_LEFT,

    // Winiarz dał wino studentowi
    // (id winiarza, id studenta, id meliny, ile wina)
    WINEMAKER_GAVE_WINE_TO_STUDENT,

    // Student nie chce już więcej imprezować i leczy kaca (id studenta)
    STUDENT_DOESNT_WANT_TO_PARTY_ANYMORE,

    // Student jest gotowy do zbierania wina (id studenta, ile wina)
    STUDENT_WANT_TO_PARTY,

    // Student zarezerwował melinę (id studenta, id meliny)
    STUDENT_SAFE_PLACE_RESERVED,

    // Student opuścił melinę (id studenta, id meliny)
    STUDENT_SAFE_PLACE_LEFT
  };
};

struct WinemakerMessage {
  enum {
    // Winiarz chce uzyskać dostęp do sekcji krytycznej związanej z meliną
    // (clock, winemaker_id)
    WINEMAKER_SAFE_PLACE_REQUEST = 200,

    // Zgoda na wejście do sekcji krytycznej związanej z meliną
    WINEMAKER_SAFE_PLACE_ACK,

    // Winiarz zarezerwował melinę (id meliny, ile wina)
    WINEMAKER_SAFE_PLACE_RESERVED,

    // Winiarz opuścił melinę (id meliny)
    WINEMAKER_SAFE_PLACE_LEFT,

    // Winiarz wręcza studentowi porcję wina (ile wina)
    HERE_YOU_ARE
  };
};

struct StudentMessage {
  enum {
    // Student chce uzyskać dostęp do sekcji krytycznej związanej z meliną
    // (clock, winemaker_id)
    STUDENT_SAFE_PLACE_REQUEST = 300,

    // Student uzyskał zgodę na wejście do sekcji krytycznej związanej z meliną
    STUDENT_SAFE_PLACE_ACK,

    // Student ogłasza innym studentom, że zarezerwował melinę (id meliny, ile
    // wina)
    STUDENT_SAFE_PLACE_RESERVED,

    // Student opuszcza melinę (id meliny)
    STUDENT_SAFE_PLACE_LEFT,
  };
};
