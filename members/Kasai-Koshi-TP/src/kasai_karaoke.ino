#include <LiquidCrystal.h>

const int PIN_BUTTON = 2;
const int PIN_BUZZER = 3;

// LCD wiring follows the working reference sketch.
const int PIN_LCD_RS = 7;
const int PIN_LCD_EN = 8;
const int PIN_LCD_D4 = 9;
const int PIN_LCD_D5 = 10;
const int PIN_LCD_D6 = 11;
const int PIN_LCD_D7 = 12;

const unsigned long DEBOUNCE_DELAY = 50;

enum AppState {
  STATE_WAITING = 0,
  STATE_PLAYING = 1,
  STATE_FINISHED = 2,
  STATE_ERROR = 3
};

LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);

const char* lyrics[] = {
  "Twinkle twinkle",
  "little star",
  "how I wonder",
  "what you are",
  "up above the",
  "world so high",
  "like a diamond",
  "in the sky"
};

const unsigned long lyricsIntervals[] = {
  1200, 1200, 1200, 1200, 1200, 1200, 1200, 1200
};

const int melody[] = {
  262, 262, 392, 392, 440, 440, 392,
  349, 349, 330, 330, 294, 294, 262
};

const unsigned long noteDurations[] = {
  400, 400, 400, 400, 400, 400, 800,
  400, 400, 400, 400, 400, 400, 800
};

const size_t lyricsCount = sizeof(lyrics) / sizeof(lyrics[0]);
const size_t lyricsIntervalCount = sizeof(lyricsIntervals) / sizeof(lyricsIntervals[0]);
const size_t melodyCount = sizeof(melody) / sizeof(melody[0]);
const size_t noteDurationCount = sizeof(noteDurations) / sizeof(noteDurations[0]);

AppState currentState = STATE_WAITING;

size_t lyricsIndex = 0;
size_t melodyIndex = 0;

unsigned long lastMillis_lyrics = 0;
unsigned long lastMillis_Melody = 0;

int lastButtonReading = HIGH;
int buttonState = HIGH;
int prevButtonState = HIGH;
unsigned long lastDebounceTime = 0;

bool waitingScreenShown = false;

bool readButton();
void start(unsigned long now);
void updateLyrics(unsigned long now);
void playMelody(unsigned long now);
bool Finish(unsigned long now);
void stopPlayback();
void showError(const char* message);

void setup() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Press Button");

  currentState = STATE_WAITING;
  lyricsIndex = 0;
  melodyIndex = 0;
  lastMillis_lyrics = 0;
  lastMillis_Melody = 0;
}

void loop() {
  const unsigned long now = millis();
  const bool btnPressed = readButton();

  switch (currentState) {
    case STATE_WAITING:
      if (!waitingScreenShown) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Press Button");
        waitingScreenShown = true;
      }

      if (btnPressed) {
        start(now);
      }
      break;

    case STATE_PLAYING:
      updateLyrics(now);
      playMelody(now);

      if (Finish(now)) {
        currentState = STATE_FINISHED;
      }
      break;

    case STATE_FINISHED:
      stopPlayback();
      currentState = STATE_WAITING;
      break;

    case STATE_ERROR:
      if (btnPressed) {
        stopPlayback();
        currentState = STATE_WAITING;
      }
      break;

    default:
      showError("State Error");
      break;
  }
}

bool readButton() {
  const unsigned long now = millis();
  const int reading = digitalRead(PIN_BUTTON);

  if (reading != lastButtonReading) {
    lastDebounceTime = now;
    lastButtonReading = reading;
  }

  if ((now - lastDebounceTime) >= DEBOUNCE_DELAY) {
    if (reading != buttonState) {
      prevButtonState = buttonState;
      buttonState = reading;

      // INPUT_PULLUP: HIGH -> LOW means a press event.
      if (prevButtonState == HIGH && buttonState == LOW) {
        return true;
      }
    }
  }

  return false;
}

void start(unsigned long now) {
  if (lyricsCount == 0 || melodyCount == 0) {
    showError("Data Empty");
    return;
  }

  if (lyricsCount != lyricsIntervalCount || melodyCount != noteDurationCount) {
    showError("Data Mismatch");
    return;
  }

  lyricsIndex = 0;
  melodyIndex = 0;

  lastMillis_lyrics = now;
  lastMillis_Melody = now;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(lyrics[lyricsIndex]);

  if (melody[melodyIndex] > 0) {
    tone(PIN_BUZZER, melody[melodyIndex]);
  }

  waitingScreenShown = false;
  currentState = STATE_PLAYING;
}

void updateLyrics(unsigned long now) {
  if (lyricsIndex >= lyricsCount) {
    return;
  }

  if ((now - lastMillis_lyrics) >= lyricsIntervals[lyricsIndex]) {
    lyricsIndex++;
    lastMillis_lyrics = now;

    lcd.clear();
    lcd.setCursor(0, 0);

    if (lyricsIndex < lyricsCount) {
      lcd.print(lyrics[lyricsIndex]);
    }
  }
}

void playMelody(unsigned long now) {
  if (melodyIndex >= melodyCount) {
    return;
  }

  if ((now - lastMillis_Melody) >= noteDurations[melodyIndex]) {
    melodyIndex++;
    lastMillis_Melody = now;

    if (melodyIndex < melodyCount) {
      if (melody[melodyIndex] > 0) {
        tone(PIN_BUZZER, melody[melodyIndex]);
      } else {
        noTone(PIN_BUZZER);
      }
    } else {
      noTone(PIN_BUZZER);
    }
  }
}

bool Finish(unsigned long now) {
  (void)now;
  const bool lyricsDone = (lyricsIndex >= lyricsCount);
  const bool melodyDone = (melodyIndex >= melodyCount);

  if (lyricsIndex > lyricsCount || melodyIndex > melodyCount) {
    showError("Index Error");
    return false;
  }

  return lyricsDone && melodyDone;
}

void stopPlayback() {
  noTone(PIN_BUZZER);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Press Button");

  lyricsIndex = 0;
  melodyIndex = 0;
  waitingScreenShown = true;
}

void showError(const char* message) {
  noTone(PIN_BUZZER);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ERROR");
  lcd.setCursor(0, 1);
  lcd.print(message);
  currentState = STATE_ERROR;
}
