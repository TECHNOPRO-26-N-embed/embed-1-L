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



// ドレミの歌 1番（英語歌詞・全フレーズ）
const char* lyrics[] = {
  "Do, a deer",            // 1
  "a female deer",         // 2
  "Re, a drop",            // 3
  "of golden sun",         // 4
  "Mi, a name",            // 5
  "I call myself",         // 6
  "Fa, a long",            // 7
  "long way to run",       // 8
  "So, a needle",          // 9
  "pulling thread",        // 10
  "La, a note",            // 11
  "to follow So",          // 12
  "Ti, a drink",           // 13
  "with jam and bread",    // 14
  "That will bring",       // 15
  "us back to Do!"         // 16
};

// 各歌詞の表示間隔（ミリ秒）
const unsigned long lyricsIntervals[] = {
  3862, 3862, 1200, 1200, 1200, 1200, 1200, 1200,
  1200, 1200, 1200, 1200, 1200, 1200, 1200, 1800
};

// メロディ（Cメジャー基準、フレーズ感を出すために休符を追加）
const int melody[] = {
  // Do, a deer, a female deer
  262, 294, 330, 262, 330, 262, 330, 0,
  // Re, a drop of golden sun
  294, 330, 349, 349, 330, 294, 349, 0,
  // Mi, a name I call myself
  330, 349, 392, 330, 392, 330, 392, 0,
  // Fa, a long long way to run
  349, 392, 440, 440, 392, 349, 440, 0,
  // So, a needle pulling thread
  392, 262, 294, 330, 349, 392, 440, 0,
  // La, a note to follow So
  440, 294, 330, 370, 392, 440, 494, 0,
  // Ti, a drink with jam and bread
  494, 330, 370, 415, 440, 494, 523, 0,
  // That will bring us back to Do!
  440, 349, 587, 494, 523, 523, 0
};

// 各音符の長さ（ミリ秒）
const unsigned long noteDurations[] = {
  // Do, a deer, a female deer
  726, 242, 726, 242, 484, 484, 968, 180,
  // Re, a drop of golden sun
  //726, 242, 726, 242, 484, 484, 958, 180,
  500,250,500,250,500,500,1000,
  // Mi, a name I call myself
  726, 242, 726, 242, 484, 484, 968, 180,
  // Fa, a long long way to run
  726, 242, 726, 242, 484, 484, 968, 180,
  // So, a needle pulling thread
  726, 242, 726, 242, 484, 484, 968, 180,
  // La, a note to follow So
  726, 242, 726, 242, 484, 484, 968, 180,
  // Ti, a drink with jam and bread
  726, 242, 726, 242, 484, 484, 968, 180,
  // That will bring us back to Do!
  484, 484, 726, 242, 968, 968, 968
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
void showLyricsPair(size_t firstIndex);

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

  showLyricsPair(lyricsIndex);

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

  unsigned long displayInterval = lyricsIntervals[lyricsIndex];
  if ((lyricsIndex + 1) < lyricsCount) {
    displayInterval += lyricsIntervals[lyricsIndex + 1];
  }

  if ((now - lastMillis_lyrics) >= displayInterval) {
    lyricsIndex += 2;
    lastMillis_lyrics = now;

    if (lyricsIndex < lyricsCount) {
      showLyricsPair(lyricsIndex);
    }
  }
}

void showLyricsPair(size_t firstIndex) {
  lcd.clear();

  if (firstIndex < lyricsCount) {
    lcd.setCursor(0, 0);
    lcd.print(lyrics[firstIndex]);
  }

  if ((firstIndex + 1) < lyricsCount) {
    lcd.setCursor(0, 1);
    lcd.print(lyrics[firstIndex + 1]);
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