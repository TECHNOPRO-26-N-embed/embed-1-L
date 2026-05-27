/*
  MemoFlash — 組込み開発実習
  作成者: 堀航成 / グループ: 1-L / 2026-05-25

  ランダムに光るLEDの順番を記憶して再現するゲーム。
  キーパッドのABCDでLED色に対応、#でゲーム開始・リセット。

  ピンアサイン:
    LCD1602   : RS=7, EN=8, D4=9, D5=10, D6=11, D7=12
    LED 赤    : D2
    LED 黄    : D4
    LED 青    : D5
    LED 緑    : D6
    パッシブブザー: D3
    キーパッド行  : A0(Row0), A1(Row1), A2(Row2), A3(Row3)
    キーパッド列  : A4(Col2=#), A5(Col3=ABCD)
*/

#include <LiquidCrystal.h>

// =============================================
// ピン定義
// =============================================
const int PIN_LCD_RS = 7;
const int PIN_LCD_EN = 8;
const int PIN_LCD_D4 = 9;
const int PIN_LCD_D5 = 10;
const int PIN_LCD_D6 = 11;
const int PIN_LCD_D7 = 12;

const int PIN_LED_RED    = 2;
const int PIN_LED_YELLOW = 4;
const int PIN_LED_BLUE   = 5;
const int PIN_LED_GREEN  = 6;

const int PIN_BUZZER = 3;

const int PIN_KEY_ROW0 = A0;
const int PIN_KEY_ROW1 = A1;
const int PIN_KEY_ROW2 = A2;
const int PIN_KEY_ROW3 = A3;
const int PIN_KEY_COL2 = A4;  // # のある列
const int PIN_KEY_COL3 = A5;  // A/B/C/D のある列

// =============================================
// ゲーム定数
// =============================================
const int MAX_PATTERN     = 20;   // パターン配列の最大長（防御的上限）
const int INITIAL_ROUND   = 5;    // 最初に覚えるLED数
const int LED_ON_TIME     = 500;  // LED点灯時間 [ms]
const int LED_OFF_TIME    = 150;  // LED消灯間隔 [ms]
const int DEBOUNCE_TIME   = 50;   // デバウンス時間 [ms]
const int KEY_SCAN_PERIOD = 50;   // キースキャン周期 [ms]

// =============================================
// 状態定数
// =============================================
const int STATE_TITLE       = 0;
const int STATE_SHOW        = 1;
const int STATE_INPUT       = 2;
const int STATE_OVER        = 3;
const int STATE_COUNTDOWN   = 4;
const int STATE_ROUND_CLEAR = 5;

// =============================================
// 色定数
// =============================================
const int COLOR_RED    = 0;
const int COLOR_YELLOW = 1;
const int COLOR_BLUE   = 2;
const int COLOR_GREEN  = 3;

// =============================================
// オブジェクト
// =============================================
LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_EN,
                  PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);

// =============================================
// グローバル変数
// =============================================
int gameState = STATE_TITLE;

// タイマー
unsigned long lastKeyScanTime  = 0;
unsigned long lastLedTime      = 0;
unsigned long lastDebounceTime = 0;

// キー入力管理
char lastKey    = '\0';   // 直前に確定したキー（リピート抑制用）
char currentKey = '\0';   // 今フレームで確定したキー
char lastRawKey = '\0';   // 直前のスキャン生値（デバウンス用）

// パターン管理
int pattern[MAX_PATTERN];
int patternLength = 0;   // initGame() で INITIAL_ROUND に設定
int inputIndex    = 0;   // プレイヤーの入力位置
int showIndex     = 0;   // 提示中のLEDインデックス

// スコア
int bestScore = 0;

// =============================================
// 前方宣言
// =============================================
void allLEDOff();
void lightLED(int color);
void initGame();
void updateLCD(int round, int best);
void playSound(int type);
char scanKeypad();
void generatePattern(int length);
void showTitle();
void showCountdown();
void showPattern();
void waitInput();
void showRoundClear();
void showGameOver();
int  keyToColor(char key);

// =============================================
// setup()
// =============================================
void setup() {
  // randomSeed は pinMode より前に（アナログピン浮遊値を使う）
  randomSeed(analogRead(A0));

  // ピンモード設定
  pinMode(PIN_LED_RED,    OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_BLUE,   OUTPUT);
  pinMode(PIN_LED_GREEN,  OUTPUT);
  pinMode(PIN_BUZZER,     OUTPUT);

  // キーパッド行：INPUT_PULLUP
  pinMode(PIN_KEY_ROW0, INPUT_PULLUP);
  pinMode(PIN_KEY_ROW1, INPUT_PULLUP);
  pinMode(PIN_KEY_ROW2, INPUT_PULLUP);
  pinMode(PIN_KEY_ROW3, INPUT_PULLUP);

  // キーパッド列：OUTPUT（デフォルト HIGH）
  pinMode(PIN_KEY_COL2, OUTPUT);
  pinMode(PIN_KEY_COL3, OUTPUT);
  digitalWrite(PIN_KEY_COL2, HIGH);
  digitalWrite(PIN_KEY_COL3, HIGH);

  // LCD 初期化
  lcd.begin(16, 2);

  // シリアル（デバッグ用）
  Serial.begin(9600);

  // 起動演出 Phase 1：順次点灯（加速・音なし・溜め）
  allLEDOff();
  int ledPins[]      = { PIN_LED_RED, PIN_LED_YELLOW, PIN_LED_BLUE, PIN_LED_GREEN };
  int ledDurations[] = { 500, 450, 400, 350 };  // 加速
  for (int i = 0; i < 4; i++) {
    digitalWrite(ledPins[i], HIGH);
    delay(ledDurations[i]);
    digitalWrite(ledPins[i], LOW);
  }

  // 起動演出 Phase 2：全消灯で「タメ」
  delay(120);

  // 起動演出 Phase 3：4LED同時点灯 + 上昇アルペジオ（クライマックス）
  // 音階：E5 → G5 → C6 → E6（C major arpeggio 第1転回上昇）
  digitalWrite(PIN_LED_RED,    HIGH);
  digitalWrite(PIN_LED_YELLOW, HIGH);
  digitalWrite(PIN_LED_BLUE,   HIGH);
  digitalWrite(PIN_LED_GREEN,  HIGH);
  tone(PIN_BUZZER, 659,  60);  delay(65);   // ミ
  tone(PIN_BUZZER, 784,  60);  delay(65);   // ソ
  tone(PIN_BUZZER, 1047, 90);  delay(95);   // ド↑
  tone(PIN_BUZZER, 1319, 180); delay(200);  // ミ↑（伸ばし）
  delay(300);                               // 余韻でLED残光
  allLEDOff();

  // ゲーム初期化（タイトル画面表示前）
  initGame();
}

// =============================================
// loop()
// =============================================
void loop() {
  currentKey = scanKeypad();

  switch (gameState) {
    case STATE_TITLE:       showTitle();       break;
    case STATE_COUNTDOWN:   showCountdown();   break;
    case STATE_SHOW:        showPattern();     break;
    case STATE_INPUT:       waitInput();       break;
    case STATE_ROUND_CLEAR: showRoundClear();  break;
    case STATE_OVER:        showGameOver();    break;
  }
}

// =============================================
// allLEDOff() — 全LED消灯
// =============================================
void allLEDOff() {
  digitalWrite(PIN_LED_RED,    LOW);
  digitalWrite(PIN_LED_YELLOW, LOW);
  digitalWrite(PIN_LED_BLUE,   LOW);
  digitalWrite(PIN_LED_GREEN,  LOW);
}

// =============================================
// lightLED(color) — 指定色のLEDを点灯（他は消灯）
// =============================================
void lightLED(int color) {
  allLEDOff();
  switch (color) {
    case COLOR_RED:    digitalWrite(PIN_LED_RED,    HIGH); break;
    case COLOR_YELLOW: digitalWrite(PIN_LED_YELLOW, HIGH); break;
    case COLOR_BLUE:   digitalWrite(PIN_LED_BLUE,   HIGH); break;
    case COLOR_GREEN:  digitalWrite(PIN_LED_GREEN,  HIGH); break;
    default: break; // 不正な色は無視
  }
}

// =============================================
// initGame() — ゲームデータを初期化
// =============================================
void initGame() {
  patternLength = INITIAL_ROUND;
  inputIndex    = 0;
  showIndex     = 0;
  generatePattern(MAX_PATTERN);
}

// =============================================
// updateLCD(round, best) — LCD表示更新
// =============================================
void updateLCD(int round, int best) {
  // 行全体を上書きすることで、残留文字を残さない
  char buf[17];

  snprintf(buf, sizeof(buf), "Round: %-9d", round);
  lcd.setCursor(0, 0);
  lcd.print(buf);

  snprintf(buf, sizeof(buf), "Best: %-9d", best);
  lcd.setCursor(0, 1);
  lcd.print(buf);
}

// =============================================
// playSound(type) — ブザー音を鳴らす
// =============================================
void playSound(int type) {
  if (type == 1) {
    // 正解音
    tone(PIN_BUZZER, 1500, 80);
  } else {
    // ゲームオーバー音：下降音階 → 低音持続
    tone(PIN_BUZZER, 262, 200); delay(250); //C4
    tone(PIN_BUZZER, 196, 200); delay(250); //G3
    tone(PIN_BUZZER, 165, 200); delay(250); //E3
    tone(PIN_BUZZER, 131, 1000); //C3
  }
}

// =============================================
// scanKeypad() — キー入力スキャン
//   戻り値: 確定したキー文字、なければ '\0'
// =============================================
char scanKeypad() {
  // KEY_SCAN_PERIOD ごとにしかスキャンしない
  unsigned long now = millis();
  if (now - lastKeyScanTime < KEY_SCAN_PERIOD) {
    return '\0';
  }
  lastKeyScanTime = now;

  // --- 6ピンカスタムスキャン ---
  // Col2(#キーのある列)を LOW にして行を読む
  char rawKey = '\0';
  int lowRows = 0;  // LOW になっている行の数（同時押し検出用）

  // Col2(#) をスキャン
  digitalWrite(PIN_KEY_COL2, LOW);
  delayMicroseconds(10);

  int row3_col2 = digitalRead(PIN_KEY_ROW3);  // #キー は Row3/Col2
  if (row3_col2 == LOW) {
    rawKey = '#';
    lowRows++;
  }
  digitalWrite(PIN_KEY_COL2, HIGH);

  // Col3(A/B/C/D) をスキャン
  digitalWrite(PIN_KEY_COL3, LOW);
  delayMicroseconds(10);

  int row0_col3 = digitalRead(PIN_KEY_ROW0);  // A
  int row1_col3 = digitalRead(PIN_KEY_ROW1);  // B
  int row2_col3 = digitalRead(PIN_KEY_ROW2);  // C
  int row3_col3 = digitalRead(PIN_KEY_ROW3);  // D

  if (row0_col3 == LOW) { rawKey = 'A'; lowRows++; }
  if (row1_col3 == LOW) { rawKey = 'B'; lowRows++; }
  if (row2_col3 == LOW) { rawKey = 'C'; lowRows++; }
  if (row3_col3 == LOW) { rawKey = 'D'; lowRows++; }

  digitalWrite(PIN_KEY_COL3, HIGH);

  // 同時押し検出：2つ以上 LOW → 無効
  if (lowRows >= 2) {
    lastRawKey = '\0';
    lastKey    = '\0';
    return '\0';
  }

  // デバウンス：生値が変わったらタイマーリセット
  if (rawKey != lastRawKey) {
    lastRawKey     = rawKey;
    lastDebounceTime = now;
    return '\0';
  }

  // デバウンス時間未満はまだ確定しない
  if (now - lastDebounceTime < DEBOUNCE_TIME) {
    return '\0';
  }

  // リピート抑制：前回と同じキーが押しっぱなし → 無効
  if (rawKey == lastKey) {
    return '\0';
  }

  // 新しいキーが確定
  lastKey = rawKey;

  // デバッグ出力
  if (rawKey != '\0') {
    Serial.print("[KEY] ");
    Serial.println(rawKey);
  }

  return rawKey;
}

// =============================================
// generatePattern(length) — パターン配列を生成
// =============================================
void generatePattern(int length) {
  if (length > MAX_PATTERN) length = MAX_PATTERN;  // 防御的上限

  Serial.print("[PATTERN] length=");
  Serial.print(length);
  Serial.print(" : ");

  for (int i = 0; i < length; i++) {
    pattern[i] = random(4);  // 0〜3 の完全ランダム（同色連続あり）
    Serial.print(pattern[i]);
    Serial.print(' ');
  }
  Serial.println();
}

// =============================================
// keyToColor(key) — キー文字を色インデックスに変換
//   戻り値: 0=赤, 1=黄, 2=青, 3=緑, -1=無効
// =============================================
int keyToColor(char key) {
  switch (key) {
    case 'A': return COLOR_RED;
    case 'B': return COLOR_YELLOW;
    case 'C': return COLOR_BLUE;
    case 'D': return COLOR_GREEN;
    default:  return -1;
  }
}

// =============================================
// showTitle() — タイトル画面（STATE_TITLE）
// =============================================
void showTitle() {
  static bool entered = false;

  if (!entered) {
    entered = true;
    allLEDOff();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("   MemoFlash");
    lcd.setCursor(0, 1);
    lcd.print("Press # to START");

    Serial.println("[STATE] TITLE");
  }

  // # キーでゲーム開始
  if (currentKey == '#') {
    entered = false;  // 次回タイトルに戻ったとき再描画する
    initGame();
    gameState = STATE_COUNTDOWN;
  }
}

// =============================================
// showCountdown() — 3,2,1,GO! カウントダウン（STATE_COUNTDOWN）
// =============================================
void showCountdown() {
  static bool entered     = false;
  static int  step        = 0;     // 0=3, 1=2, 2=1, 3=GO!
  static unsigned long stepStart = 0;
  static bool soundPlayed = false;

  unsigned long now = millis();

  if (!entered) {
    entered     = true;
    step        = 0;
    stepStart   = now;
    soundPlayed = false;

    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print("Get Ready!");

    Serial.println("[STATE] COUNTDOWN");
  }

  // 各ステップ開始時に1回だけ：LCD更新 + 音
  if (!soundPlayed) {
    lcd.setCursor(0, 1);
    switch (step) {
      case 0:
        lcd.print("       3        ");
        tone(PIN_BUZZER, 1000, 100);
        break;
      case 1:
        lcd.print("       2        ");
        tone(PIN_BUZZER, 1000, 100);
        break;
      case 2:
        lcd.print("       1        ");
        tone(PIN_BUZZER, 1000, 100);
        break;
      case 3:
        lcd.print("      GO!       ");
        tone(PIN_BUZZER, 1500, 400);
        break;
    }
    soundPlayed = true;
  }

  // 各ステップの保持時間：数字は1000ms、GO! は短めの700ms
  int holdTime = (step == 3) ? 700 : 1000;

  if (now - stepStart >= holdTime) {
    step++;
    soundPlayed = false;
    stepStart   = now;

    if (step >= 4) {
      // カウントダウン完了 → SHOW へ
      entered = false;
      gameState = STATE_SHOW;
      Serial.println("[STATE] -> SHOW");
    }
  }
}

// =============================================
// showPattern() — パターン提示（STATE_SHOW）
// =============================================
void showPattern() {
  static bool entered   = false;
  static bool ledIsOn   = false;

  unsigned long now = millis();

  if (!entered) {
    entered   = true;
    ledIsOn   = false;
    showIndex = 0;
    lastLedTime = now;
    allLEDOff();

    // LCD 更新
    updateLCD(patternLength, bestScore);

    Serial.print("[STATE] SHOW round=");
    Serial.println(patternLength);
  }

  // 全提示完了 → INPUT へ遷移
  if (showIndex >= patternLength) {
    entered = false;
    allLEDOff();

    // 注意: lastKey/lastRawKey は意図的にリセットしない。
    // リセットすると SHOW 中に押しっぱなしのキーが INPUT 直後に
    // 「新しい入力」と誤判定される。
    // scanKeypad() のリピート抑制で「一度離して再度押した時のみ
    // 新入力として確定」する動作にする。

    inputIndex = 0;
    gameState  = STATE_INPUT;

    Serial.println("[STATE] -> INPUT");
    return;
  }

  if (!ledIsOn) {
    // 消灯インターバル待ち
    if (now - lastLedTime >= LED_OFF_TIME) {
      lightLED(pattern[showIndex]);
      ledIsOn     = true;
      lastLedTime = now;

      Serial.print("[SHOW] LED=");
      Serial.println(pattern[showIndex]);
    }
  } else {
    // 点灯時間待ち
    if (now - lastLedTime >= LED_ON_TIME) {
      allLEDOff();
      ledIsOn     = false;
      lastLedTime = now;
      showIndex++;
    }
  }
}

// =============================================
// waitInput() — プレイヤー入力待ち（STATE_INPUT）
// =============================================
void waitInput() {
  static bool entered       = false;
  static bool feedbackLedOn = false;
  static unsigned long feedbackStart = 0;

  if (!entered) {
    entered       = true;
    feedbackLedOn = false;
    Serial.print("[STATE] INPUT round=");
    Serial.println(patternLength);
  }

  // フィードバックLED 消灯処理（200ms）
  if (feedbackLedOn && millis() - feedbackStart >= 200) {
    allLEDOff();
    feedbackLedOn = false;
  }

  // フィードバック中はキー受付しない
  if (feedbackLedOn) return;

  // キー取得
  char key = currentKey;
  int color = keyToColor(key);

  if (color == -1) return;  // ABCD 以外は無視（# も無視）

  // 正誤判定
  if (color == pattern[inputIndex]) {
    // ---- 正解 ----
    playSound(1);

    // フィードバック LED 点灯
    lightLED(color);
    feedbackLedOn = true;
    feedbackStart = millis();

    inputIndex++;

    Serial.print("[INPUT] OK idx=");
    Serial.println(inputIndex);

    // ラウンドクリア判定
    if (inputIndex >= patternLength) {
      // ハイスコア更新（クリア時のラウンド長で更新）
      if (patternLength > bestScore) {
        bestScore = patternLength;
      }

      // 次ラウンドへ（patternLength を進める）
      patternLength++;
      if (patternLength > MAX_PATTERN) patternLength = MAX_PATTERN;

      entered = false;
      gameState = STATE_ROUND_CLEAR;  // 演出を挟む

      Serial.print("[ROUND CLEAR] next=");
      Serial.println(patternLength);
    }
  } else {
    // ---- 不正解 ----
    Serial.println("[INPUT] WRONG -> GAME OVER");
    entered    = false;
    gameState  = STATE_OVER;
  }
}

// =============================================
// showRoundClear() — ラウンドクリア演出（STATE_ROUND_CLEAR）
// =============================================
void showRoundClear() {
  static bool entered      = false;
  static int  step         = 0;     // 0〜5: 6ステップで演出
  static unsigned long stepStart = 0;
  static bool actionTaken  = false;

  unsigned long now = millis();

  if (!entered) {
    entered     = true;
    step        = 0;
    stepStart   = now;
    actionTaken = false;

    // patternLength は既にインクリメント済み（クリアしたラウンド+1）
    int clearedRoundNum = patternLength - INITIAL_ROUND;     // クリアしたラウンド番号
    int nextRoundNum    = patternLength - INITIAL_ROUND + 1; // 次のラウンド番号

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Round ");
    lcd.print(clearedRoundNum);
    lcd.print(" Clear!");
    lcd.setCursor(0, 1);
    lcd.print("Next: Round ");
    lcd.print(nextRoundNum);

    Serial.print("[STATE] ROUND CLEAR (cleared=");
    Serial.print(clearedRoundNum);
    Serial.println(")");
  }

  // 各ステップ開始時に1回だけ：LED + 音
  if (!actionTaken) {
    switch (step) {
      case 0:  // フラッシュ1 + C5
        digitalWrite(PIN_LED_RED,    HIGH);
        digitalWrite(PIN_LED_YELLOW, HIGH);
        digitalWrite(PIN_LED_BLUE,   HIGH);
        digitalWrite(PIN_LED_GREEN,  HIGH);
        tone(PIN_BUZZER, 523, 150);  // C5「ド」
        break;
      case 1:  // 消灯（間）
        allLEDOff();
        break;
      case 2:  // フラッシュ2 + E5
        digitalWrite(PIN_LED_RED,    HIGH);
        digitalWrite(PIN_LED_YELLOW, HIGH);
        digitalWrite(PIN_LED_BLUE,   HIGH);
        digitalWrite(PIN_LED_GREEN,  HIGH);
        tone(PIN_BUZZER, 659, 150);  // E5「ミ」
        break;
      case 3:  // 消灯（間）
        allLEDOff();
        break;
      case 4:  // フラッシュ3（クライマックス） + G5（伸ばし）
        digitalWrite(PIN_LED_RED,    HIGH);
        digitalWrite(PIN_LED_YELLOW, HIGH);
        digitalWrite(PIN_LED_BLUE,   HIGH);
        digitalWrite(PIN_LED_GREEN,  HIGH);
        tone(PIN_BUZZER, 784, 400);  // G5「ソ」（長め）
        break;
      case 5:  // 全消灯 + LCD表示維持
        allLEDOff();
        break;
    }
    actionTaken = true;
  }

  // ステップ保持時間
  int holdTime;
  switch (step) {
    case 4:  holdTime = 450; break;  // G5 鳴らしながら長め
    case 5:  holdTime = 400; break;  // LCD余韻
    default: holdTime = 200; break;  // フラッシュON/OFF
  }

  if (now - stepStart >= holdTime) {
    step++;
    actionTaken = false;
    stepStart   = now;

    if (step >= 6) {
      entered = false;
      gameState = STATE_SHOW;
      Serial.println("[STATE] -> SHOW");
    }
  }
}

// =============================================
// showGameOver() — ゲームオーバー画面（STATE_OVER）
// =============================================
void showGameOver() {
  static bool entered = false;

  if (!entered) {
    entered = true;
    allLEDOff();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  Game Over!");
    lcd.setCursor(0, 1);
    lcd.print("Rnd:");
    lcd.print(patternLength);
    lcd.print(" Bst:");
    lcd.print(bestScore);

    playSound(0);  // 不正解音

    Serial.print("[STATE] GAME OVER Rnd=");
    Serial.print(patternLength);
    Serial.print(" Bst=");
    Serial.println(bestScore);
  }

  // # キーでタイトルに戻る（bestScore はセッション中保持・リセットしない）
  if (currentKey == '#') {
    entered   = false;
    gameState = STATE_TITLE;

    Serial.println("[STATE] -> TITLE");
  }
}
