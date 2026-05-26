#include <LiquidCrystal.h>

// LCD1602: RS, E, D4, D5, D6, D7 
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// ピン定義 
const int PIN_BUZZER = 3; 
const int PIN_TRIG = 5; 
const int PIN_ECHO = 6;

// 距離判定用の定数 
const int CAUTION_DISTANCE = 50; // 50cm未満で注意 
const int DANGER_DISTANCE = 20; // 20cm未満で危険 
const int MAX_DISTANCE = 400; // 測定範囲外判定

// タイミング設定 
const unsigned long SENSOR_INTERVAL = 300; 
const unsigned long LCD_INTERVAL = 300; 
const unsigned long CAUTION_INTERVAL = 500; 
const unsigned long DANGER_INTERVAL = 200;

// 状態定義 
const int STATE_MEASURING = 1; 
const int STATE_SAFE = 2; 
const int STATE_CAUTION = 3; 
const int STATE_DANGER = 4; 
const int STATE_ERROR = 5;

// グローバル変数 
long duration = 0; 
float distanceCm = 0.0; 
int currentState = STATE_MEASURING;

unsigned long lastMillisSensor = 0; 
unsigned long lastMillisLCD = 0; 
unsigned long lastMillisBuzzer = 0;

bool buzzerOn = false;

void setup() { 
  // LCD初期化 
  lcd.begin(16, 2); 
  lcd.clear();

  // ピンモード設定 
  pinMode(PIN_TRIG, OUTPUT); 
  pinMode(PIN_ECHO, INPUT); 
  pinMode(PIN_BUZZER, OUTPUT);

  // ブザー停止 
  noTone(PIN_BUZZER);

  // デバッグ用 
  Serial.begin(9600);

  // 起動表示 
  lcd.setCursor(0, 0); 
  lcd.print("Back Sonar"); 
  lcd.setCursor(0, 1); 
  lcd.print("Starting..."); 
  delay(1000);

  lcd.clear(); 
}

void loop() { 
  unsigned long now = millis();

  // 一定周期ごとに距離測定 
  if (now - lastMillisSensor >= SENSOR_INTERVAL) { 
    lastMillisSensor = now;

    distanceCm = measureDistance();
    currentState = judgeState(distanceCm);

    Serial.print("Distance: ");
    Serial.print(distanceCm);
    Serial.print(" cm, State: ");
    Serial.println(currentState);
  }

  // 一定周期ごとにLCD表示更新 
  if (now - lastMillisLCD >= LCD_INTERVAL) { 
    lastMillisLCD = now; 
    updateLCD(distanceCm, currentState); 
  }

  // ブザー制御 
  updateBuzzer(currentState, now); 
}

// 距離を測定する関数 
float measureDistance() { 
  digitalWrite(PIN_TRIG, LOW); 
  delayMicroseconds(2);

  digitalWrite(PIN_TRIG, HIGH); 
  delayMicroseconds(10); 
  digitalWrite(PIN_TRIG, LOW);

  duration = pulseIn(PIN_ECHO, HIGH, 30000);

  if (duration == 0) { 
    return -1; 
  }

  float distance = duration * 0.0343 / 2; 
  return distance; 
}

// 距離に応じて状態を判定する関数 
int judgeState(float distance) { 
  if (distance <= 0 || distance > MAX_DISTANCE) { 
    return STATE_ERROR; 
  }

  if (distance < DANGER_DISTANCE) { 
    return STATE_DANGER; 
  }

  if (distance < CAUTION_DISTANCE) { 
    return STATE_CAUTION; 
  }

  return STATE_SAFE; 
  }

// LCD表示を更新する関数 
void updateLCD(float distance, int state) { 
  lcd.clear();

  if (state == STATE_ERROR) { 
    lcd.setCursor(0, 0); 
    lcd.print("Out of range"); 
    lcd.setCursor(0, 1); 
    lcd.print("No object"); 
    return; 
  }

  lcd.setCursor(0, 0); 
  lcd.print("Dist:"); 
  lcd.print((int)distance); 
  lcd.print("cm");

  lcd.setCursor(0, 1);

  if (state == STATE_SAFE) { 
    lcd.print("State:Safe"); 
  } else if (state == STATE_CAUTION) { 
    lcd.print("State:Caution"); 
  } else if (state == STATE_DANGER) { 
    lcd.print("State:Danger"); 
  } 
}

// 状態に応じてブザーを制御する関数 
void updateBuzzer(int state, unsigned long now) { 
  if (state == STATE_SAFE || state == STATE_ERROR) { 
    noTone(PIN_BUZZER); 
    buzzerOn = false; 
    return; 
  }

  if (state == STATE_CAUTION) { 
    cautionBeep(now); 
  } else if (state == STATE_DANGER) { 
    dangerBeep(now); 
  } 
}

// 注意状態のブザー制御 
void cautionBeep(unsigned long now) { 
  if (now - lastMillisBuzzer >= CAUTION_INTERVAL) { 
    lastMillisBuzzer = now; 
    buzzerOn = !buzzerOn;

    if (buzzerOn) {
      tone(PIN_BUZZER, 800);
    } else {
      noTone(PIN_BUZZER);
    }
  } 
}

// 危険状態のブザー制御 
void dangerBeep(unsigned long now) { 
  if (now - lastMillisBuzzer >= DANGER_INTERVAL) { 
    lastMillisBuzzer = now; 
    buzzerOn = !buzzerOn;

    if (buzzerOn) {
      tone(PIN_BUZZER, 1200);
    } else {
      noTone(PIN_BUZZER);
    }
  } 
}