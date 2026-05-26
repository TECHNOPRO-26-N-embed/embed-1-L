#include <LiquidCrystal.h>

// initialize the library with the numbers of the interface pins

LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// ボタンピン番号（例: D2）
const int BUTTON_PIN = 2;
// フラグ変数（0/1でトグル）
int flag = 0;
// 前回のボタン状態
bool prevButtonState = HIGH;

// ボタンでflagをトグルし返す関数
int toggleFlag() {
  bool buttonState = digitalRead(BUTTON_PIN);
  // ボタンが押された瞬間だけトグル
  if (prevButtonState == HIGH && buttonState == LOW) {
    flag = 1 - flag;
  }
  prevButtonState = buttonState;
  return flag;
}

void setup() {
    pinMode(BUTTON_PIN, INPUT_PULLUP); // プルアップでボタン入力
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  //lcd.print("*");
}

void loop() {
  // 毎回画面をクリア
  lcd.clear();
  // flag=0なら(0,0)、flag=1なら(0,1)に表示
  int pos = toggleFlag();
  lcd.setCursor(0, pos);
  lcd.print("*");
  delay(100); // チラつき防止のため少し待つ
}

