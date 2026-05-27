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
const int STATE_FINALE      = 6;

// =============================================
// 色定数
// =============================================
const int COLOR_RED    = 0;
const int COLOR_YELLOW = 1;
const int COLOR_BLUE   = 2;
const int COLOR_GREEN  = 3;

// 色ごとの音階（Cメジャーアルペジオ：ドミソド↑）
// インデックスは COLOR_* と一致：[赤, 黄, 青, 緑]
const int COLOR_TONES[4] = { 523, 659, 784, 1047 };  // C5, E5, G5, C6

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
void updateInputLCD(int round, int currentStep, int totalSteps);
void playSound(int type);
char scanKeypad();
void generatePattern(int length);
void showTitle();
void showCountdown();
void showPattern();
void waitInput();
void showRoundClear();
void showGameOver();
void showFinale();
int  keyToColor(char key);
char colorToKey(int color);

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
  lcd.clear();

  // シリアル（デバッグ用）
  Serial.begin(9600);

  // 起動演出 Phase 1：順次点灯（加速・音なし・溜め）
  allLEDOff();
  int ledPins[]      = { PIN_LED_RED, PIN_LED_YELLOW, PIN_LED_BLUE, PIN_LED_GREEN };
  int ledDurations[] = { 500, 450, 400, 350 };  // 加速：500 → 450 → 400 → 350ms
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

  // 起動直後にタイトルを強制表示（表示確認と初回無表示の防止）
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   MemoFlash");
  lcd.setCursor(0, 1);
  lcd.print("Press # to START");
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
    case STATE_FINALE:      showFinale();      break;
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
//   round: 現在のラウンド番号（1, 2, 3, ...）
//   best : ハイスコア（最高到達ラウンド番号）
// =============================================
void updateLCD(int round, int best) {
  // 行全体（16文字）を必ず上書きして残留文字を消す
  // "Round: " (7文字) + 数値を左寄せ9文字幅 = 計16文字
  char buf[17];

  snprintf(buf, sizeof(buf), "Round: %-9d", round);
  lcd.setCursor(0, 0);
  lcd.print(buf);

  snprintf(buf, sizeof(buf), "Best:  %-9d", best);
  lcd.setCursor(0, 1);
  lcd.print(buf);
}

// =============================================
// updateInputLCD(round, currentStep, totalSteps)
//   — INPUT状態専用のLCD表示（進捗バー付き）
//   上段: "Round N  X/Y"
//   下段: 16セルの進捗バー
// =============================================
void updateInputLCD(int round, int currentStep, int totalSteps) {
  char buf[17];

  // 上段: "Round N  X/Y" を16文字にパディング
  int len = snprintf(buf, sizeof(buf), "Round %d  %d/%d", round, currentStep, totalSteps);
  while (len < 16) buf[len++] = ' ';
  buf[16] = '\0';
  lcd.setCursor(0, 0);
  lcd.print(buf);

  // 下段: 16セル進捗バー
  // filled = (currentStep × 16) ÷ totalSteps
  int filled = (currentStep * 16) / totalSteps;
  lcd.setCursor(0, 1);
  for (int i = 0; i < 16; i++) {
    if (i < filled) {
      lcd.write((byte)0xFF);  // HD44780 内蔵フルブロック「█」
    } else {
      lcd.print(' ');
    }
  }
}

// =============================================
// playSound(type) — ブザー音を鳴らす
//   type=1: 正解音（1500Hz, 80ms 短く高い「ピッ」）
//   type=0: ゲームオーバー音（下降+低音持続「ピロロロ↓ブーーー」）
// =============================================
void playSound(int type) {
  if (type == 1) {
    // 正解音
    tone(PIN_BUZZER, 1500, 80);
  } else {
    // ゲームオーバー音：下降音階 → 超低音持続
    tone(PIN_BUZZER, 262, 200);  delay(250);  // C4
    tone(PIN_BUZZER, 196, 200);  delay(250);  // G3
    tone(PIN_BUZZER, 165, 200);  delay(250);  // E3
    tone(PIN_BUZZER, 131, 1000);              // C3（超低音持続・非ブロッキング）
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
    Serial.print(colorToKey(pattern[i]));  // A/B/C/D で表示
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
// colorToKey(color) — 色インデックスをキー文字に変換（シリアル表示用）
//   戻り値: 'A'=赤, 'B'=黄, 'C'=青, 'D'=緑, '?'=無効
// =============================================
char colorToKey(int color) {
  switch (color) {
    case COLOR_RED:    return 'A';
    case COLOR_YELLOW: return 'B';
    case COLOR_BLUE:   return 'C';
    case COLOR_GREEN:  return 'D';
    default:           return '?';
  }
}

// =============================================
// showTitle() — タイトル画面（STATE_TITLE）
// =============================================
void showTitle() {
  static bool entered = false;
  static unsigned long lastRefresh = 0;

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

    // 現在のラウンド番号を計算
    int currentRound = patternLength - INITIAL_ROUND + 1;

    // LCD 更新（ラウンド番号で表示）
    updateLCD(currentRound, bestScore);

    Serial.print("[STATE] SHOW round=");
    Serial.print(currentRound);
    Serial.print(" (length=");
    Serial.print(patternLength);
    Serial.println(")");
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
      // 色に対応する音を鳴らす（A1: パターン提示音）
      tone(PIN_BUZZER, COLOR_TONES[pattern[showIndex]], 400);
      ledIsOn     = true;
      lastLedTime = now;

      Serial.print("[SHOW] LED=");
      Serial.println(colorToKey(pattern[showIndex]));  // A/B/C/D で表示
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

    // 進捗バー初期表示（0/N）
    int currentRound = patternLength - INITIAL_ROUND + 1;
    updateInputLCD(currentRound, 0, patternLength);

    Serial.print("[STATE] INPUT round=");
    Serial.println(patternLength);
  }

  // 改善#7: INPUT中の # キーでタイトル画面に強制復帰
  // （発表時のランダム性検証用。bestScore は保持される）
  if (currentKey == '#') {
    allLEDOff();
    entered       = false;
    feedbackLedOn = false;
    gameState     = STATE_TITLE;
    Serial.println("[STATE] -> TITLE (interrupted by #)");
    return;
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
    // 押した色に対応する音を鳴らす（A2: 入力時の色音、短く軽快に）
    tone(PIN_BUZZER, COLOR_TONES[color], 150);

    // フィードバック LED 点灯
    lightLED(color);
    feedbackLedOn = true;
    feedbackStart = millis();

    inputIndex++;

    // 進捗バー更新
    int currentRound = patternLength - INITIAL_ROUND + 1;
    updateInputLCD(currentRound, inputIndex, patternLength);

    Serial.print("[INPUT] OK idx=");
    Serial.println(inputIndex);

    // ラウンドクリア判定
    if (inputIndex >= patternLength) {
      // クリアしたラウンド番号を計算（5LED=1, 6LED=2, ...）
      int clearedRound = patternLength - INITIAL_ROUND + 1;
      int maxRound     = MAX_PATTERN - INITIAL_ROUND + 1;  // 最高ラウンド=16

      // ハイスコア更新（ラウンド番号で記録）
      if (clearedRound > bestScore) {
        bestScore = clearedRound;
      }

      entered = false;

      // 最高ラウンドクリア → FINALE 演出へ
      if (clearedRound >= maxRound) {
        gameState = STATE_FINALE;
        Serial.print("[ROUND CLEAR] *** ALL CLEAR! Round ");
        Serial.print(clearedRound);
        Serial.println(" -> FINALE ***");
      } else {
        // 通常のラウンドクリア → 演出 → 次ラウンドへ
        patternLength++;
        if (patternLength > MAX_PATTERN) patternLength = MAX_PATTERN;
        gameState = STATE_ROUND_CLEAR;

        Serial.print("[ROUND CLEAR] cleared=");
        Serial.print(clearedRound);
        Serial.print(" next=");
        Serial.println(clearedRound + 1);
      }
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
// showFinale() — 全クリア時の特大フィナーレ（STATE_FINALE）
//   約50秒の演出。# でスキップ可能。
// =============================================
void showFinale() {
  static bool entered     = false;
  static int  step        = 0;
  static unsigned long stepStart = 0;
  static bool actionTaken = false;

  // フィナーレ構成
  //   Phase 1 (steps 0-7)   : 上昇フラッシュ4回
  //   Phase 2 (steps 8-27)  : サイクロン 5周（加速）
  //   Phase 3 (steps 28-32) : 高速全点滅
  //   Phase 4 (steps 33-39) : 勝利ファンファーレ
  //   Phase 5 (steps 40-151): 追加クライマックス（約40秒）
  //   Phase 6 (steps 152-153): CHAMPION 表示 + ホールド
  //   合計 154 ステップ

  // サイクロン定数（5周分の保持時間と色テーブル）
  static const int CYCLONE_DURATIONS[5] = { 150, 130, 110, 90, 70 };
  static const int CYCLONE_LED_PINS[4]  = { PIN_LED_RED, PIN_LED_YELLOW, PIN_LED_BLUE, PIN_LED_GREEN };
  static const int CYCLONE_TONES[4]     = { 523, 659, 784, 1047 };
  // 追加クライマックス用
  static const int ENCORE_TONES[4]      = { 784, 988, 1175, 1568 };  // G5, B5, D6, G6

  unsigned long now = millis();

  if (!entered) {
    entered     = true;
    step        = 0;
    stepStart   = now;
    actionTaken = false;

    allLEDOff();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" ** PERFECT! **");
    lcd.setCursor(0, 1);
    lcd.print(" ALL CLEAR! ");

    Serial.print("[STATE] FINALE (Best=");
    Serial.print(bestScore);
    Serial.println(")");
  }

  // # でTITLEへ即時スキップ
  if (currentKey == '#') {
    entered = false;
    allLEDOff();
    gameState = STATE_TITLE;
    Serial.println("[STATE] -> TITLE (finale skipped)");
    return;
  }

  // 各ステップ開始時のアクション
  if (!actionTaken) {
    actionTaken = true;

    if (step >= 8 && step <= 27) {
      // ===== Phase 2: サイクロン（計算ベース・5周） =====
      int cycle = (step - 8) / 4;   // 0〜4（どの周か）
      int color = (step - 8) % 4;   // 0〜3（赤黄青緑）
      allLEDOff();
      digitalWrite(CYCLONE_LED_PINS[color], HIGH);
      tone(PIN_BUZZER, CYCLONE_TONES[color], CYCLONE_DURATIONS[cycle] - 20);

    } else if (step >= 40 && step <= 55) {
      // ===== Phase 5-A: 追加サイクロン 4周 =====
      int color = (step - 40) % 4;

      allLEDOff();
      digitalWrite(CYCLONE_LED_PINS[color], HIGH);
      tone(PIN_BUZZER, ENCORE_TONES[color], 150);

    } else if (step >= 56 && step <= 79) {
      // ===== Phase 5-B: しつこい追加サイクロン 6周 =====
      int color = (step - 56) % 4;

      allLEDOff();
      digitalWrite(CYCLONE_LED_PINS[color], HIGH);

      // だんだん高くして、無駄に盛り上げる
      int freq = 700 + (step - 56) * 35;
      tone(PIN_BUZZER, freq, 130);

    } else if (step >= 80 && step <= 95) {
      // ===== Phase 5-C: 左右交互点滅 =====
      bool leftSide = ((step - 80) % 2 == 0);

      allLEDOff();

      if (leftSide) {
        digitalWrite(PIN_LED_RED, HIGH);
        digitalWrite(PIN_LED_YELLOW, HIGH);
        tone(PIN_BUZZER, 880, 180);
      } else {
        digitalWrite(PIN_LED_BLUE, HIGH);
        digitalWrite(PIN_LED_GREEN, HIGH);
        tone(PIN_BUZZER, 1175, 180);
      }

    } else if (step >= 96 && step <= 103) {
      // ===== Phase 5-D: 全LED高速フラッシュ =====
      bool on = ((step - 96) % 2 == 0);

      if (on) {
        digitalWrite(PIN_LED_RED, HIGH);
        digitalWrite(PIN_LED_YELLOW, HIGH);
        digitalWrite(PIN_LED_BLUE, HIGH);
        digitalWrite(PIN_LED_GREEN, HIGH);
        tone(PIN_BUZZER, 1568, 90);
      } else {
        allLEDOff();
        noTone(PIN_BUZZER);
      }

    } else if (step >= 116 && step <= 131) {
      // ===== Phase 5-G: さらに延長サイクロン 4周 =====
      int color = (step - 116) % 4;

      allLEDOff();
      digitalWrite(CYCLONE_LED_PINS[color], HIGH);

      // 高めの音から少しずつ下げて、変な粘りを出す
      int freq = 1800 - (step - 116) * 35;
      tone(PIN_BUZZER, freq, 180);

    } else if (step >= 132 && step <= 143) {
      // ===== Phase 5-H: 全LEDゆっくり鼓動 =====
      bool on = ((step - 132) % 2 == 0);

      if (on) {
        digitalWrite(PIN_LED_RED, HIGH);
        digitalWrite(PIN_LED_YELLOW, HIGH);
        digitalWrite(PIN_LED_BLUE, HIGH);
        digitalWrite(PIN_LED_GREEN, HIGH);
        tone(PIN_BUZZER, 1047, 250);
      } else {
        allLEDOff();
        tone(PIN_BUZZER, 392, 180);
      }
    } else {
      switch (step) {
        // ===== Phase 1: 4回の上昇フラッシュ =====
        case 0:  // C5
          digitalWrite(PIN_LED_RED,    HIGH);
          digitalWrite(PIN_LED_YELLOW, HIGH);
          digitalWrite(PIN_LED_BLUE,   HIGH);
          digitalWrite(PIN_LED_GREEN,  HIGH);
          tone(PIN_BUZZER, 523, 200);
          break;
        case 1: allLEDOff(); break;
        case 2:  // E5
          digitalWrite(PIN_LED_RED,    HIGH);
          digitalWrite(PIN_LED_YELLOW, HIGH);
          digitalWrite(PIN_LED_BLUE,   HIGH);
          digitalWrite(PIN_LED_GREEN,  HIGH);
          tone(PIN_BUZZER, 659, 200);
          break;
        case 3: allLEDOff(); break;
        case 4:  // G5
          digitalWrite(PIN_LED_RED,    HIGH);
          digitalWrite(PIN_LED_YELLOW, HIGH);
          digitalWrite(PIN_LED_BLUE,   HIGH);
          digitalWrite(PIN_LED_GREEN,  HIGH);
          tone(PIN_BUZZER, 784, 200);
          break;
        case 5: allLEDOff(); break;
        case 6:  // C6 (大きく)
          digitalWrite(PIN_LED_RED,    HIGH);
          digitalWrite(PIN_LED_YELLOW, HIGH);
          digitalWrite(PIN_LED_BLUE,   HIGH);
          digitalWrite(PIN_LED_GREEN,  HIGH);
          tone(PIN_BUZZER, 1047, 350);
          break;
        case 7: allLEDOff(); break;

        // ===== Phase 3: 高速全点滅 =====
        case 28:  // E6
          digitalWrite(PIN_LED_RED,    HIGH);
          digitalWrite(PIN_LED_YELLOW, HIGH);
          digitalWrite(PIN_LED_BLUE,   HIGH);
          digitalWrite(PIN_LED_GREEN,  HIGH);
          tone(PIN_BUZZER, 1319, 80);
          break;
        case 29: allLEDOff(); break;
        case 30:  // G6
          digitalWrite(PIN_LED_RED,    HIGH);
          digitalWrite(PIN_LED_YELLOW, HIGH);
          digitalWrite(PIN_LED_BLUE,   HIGH);
          digitalWrite(PIN_LED_GREEN,  HIGH);
          tone(PIN_BUZZER, 1568, 80);
          break;
        case 31: allLEDOff(); break;
        case 32:  // C7（クライマックス入口）
          digitalWrite(PIN_LED_RED,    HIGH);
          digitalWrite(PIN_LED_YELLOW, HIGH);
          digitalWrite(PIN_LED_BLUE,   HIGH);
          digitalWrite(PIN_LED_GREEN,  HIGH);
          tone(PIN_BUZZER, 2093, 130);
          break;

        // ===== Phase 4: 勝利ファンファーレ（全LED ON継続） =====
        case 33:  // C5
          digitalWrite(PIN_LED_RED,    HIGH);
          digitalWrite(PIN_LED_YELLOW, HIGH);
          digitalWrite(PIN_LED_BLUE,   HIGH);
          digitalWrite(PIN_LED_GREEN,  HIGH);
          tone(PIN_BUZZER, 523, 200);
          break;
        case 34: tone(PIN_BUZZER, 659,  200); break;  // E5
        case 35: tone(PIN_BUZZER, 784,  200); break;  // G5
        case 36: tone(PIN_BUZZER, 1047, 200); break;  // C6
        case 37: tone(PIN_BUZZER, 1319, 200); break;  // E6
        case 38: tone(PIN_BUZZER, 1568, 380); break;  // G6
        case 39: tone(PIN_BUZZER, 2093, 1200); break; // C7 ロングホールド

        // ===== Phase 5-E: じわじわ溜める全点灯 =====
        case 104:
          digitalWrite(PIN_LED_RED, HIGH);
          digitalWrite(PIN_LED_YELLOW, LOW);
          digitalWrite(PIN_LED_BLUE, LOW);
          digitalWrite(PIN_LED_GREEN, LOW);
          tone(PIN_BUZZER, 523, 300);
          break;

        case 105:
          digitalWrite(PIN_LED_RED, HIGH);
          digitalWrite(PIN_LED_YELLOW, HIGH);
          digitalWrite(PIN_LED_BLUE, LOW);
          digitalWrite(PIN_LED_GREEN, LOW);
          tone(PIN_BUZZER, 659, 300);
          break;

        case 106:
          digitalWrite(PIN_LED_RED, HIGH);
          digitalWrite(PIN_LED_YELLOW, HIGH);
          digitalWrite(PIN_LED_BLUE, HIGH);
          digitalWrite(PIN_LED_GREEN, LOW);
          tone(PIN_BUZZER, 784, 300);
          break;

        case 107:
          digitalWrite(PIN_LED_RED, HIGH);
          digitalWrite(PIN_LED_YELLOW, HIGH);
          digitalWrite(PIN_LED_BLUE, HIGH);
          digitalWrite(PIN_LED_GREEN, HIGH);
          tone(PIN_BUZZER, 1047, 700);
          break;

        // ===== Phase 5-F: 最後の大ファンファーレ =====
        case 108:
          tone(PIN_BUZZER, 523, 250);   // C5
          break;

        case 109:
          tone(PIN_BUZZER, 659, 250);   // E5
          break;

        case 110:
          tone(PIN_BUZZER, 784, 250);   // G5
          break;

        case 111:
          tone(PIN_BUZZER, 1047, 350);  // C6
          break;

        case 112:
          tone(PIN_BUZZER, 1319, 250);  // E6
          break;

        case 113:
          tone(PIN_BUZZER, 1568, 350);  // G6
          break;

        case 114:
          tone(PIN_BUZZER, 2093, 600);  // C7
          break;

        case 115:
          tone(PIN_BUZZER, 2637, 1600); // E7 超ロング
          break;

        // steps 116-131 は actionTaken 側の「さらに延長サイクロン」で処理

        // steps 132-143 は actionTaken 側の「全LEDゆっくり鼓動」で処理

        // ===== Phase 5-I: 最後の最後のファンファーレ =====
        case 144:
          digitalWrite(PIN_LED_RED, HIGH);
          digitalWrite(PIN_LED_YELLOW, HIGH);
          digitalWrite(PIN_LED_BLUE, HIGH);
          digitalWrite(PIN_LED_GREEN, HIGH);
          tone(PIN_BUZZER, 523, 350);    // C5
          break;

        case 145:
          tone(PIN_BUZZER, 659, 350);    // E5
          break;

        case 146:
          tone(PIN_BUZZER, 784, 350);    // G5
          break;

        case 147:
          tone(PIN_BUZZER, 1047, 500);   // C6
          break;

        case 148:
          tone(PIN_BUZZER, 1319, 350);   // E6
          break;

        case 149:
          tone(PIN_BUZZER, 1568, 500);   // G6
          break;

        case 150:
          tone(PIN_BUZZER, 2093, 700);   // C7
          break;

        case 151:
          tone(PIN_BUZZER, 2637, 2200);  // E7 超ロング締め
          break;

        // ===== Phase 6: CHAMPION 表示 + ホールド =====
        case 152: {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("  CHAMPION!!!  ");
          lcd.setCursor(0, 1);
          char buf[17];
          snprintf(buf, sizeof(buf), "   Best: %-3d   ", bestScore);
          lcd.print(buf);
          break;
        }

        case 153:
          break;  // ホールドのみ
      }
    }
  }

  // ステップの保持時間
  int duration;
  if (step >= 8 && step <= 27) {
    // サイクロン：周によって加速
    int cycle = (step - 8) / 4;
    duration = CYCLONE_DURATIONS[cycle];
  } else {
    switch (step) {
      case 0: case 2: case 4:           duration = 250; break;
      case 6:                            duration = 400; break;
      case 1: case 3: case 5:           duration = 100; break;
      case 7:                            duration = 200; break;
      case 28: case 30:                  duration = 100; break;
      case 29: case 31:                  duration = 80; break;
      case 32:                            duration = 150; break;
      case 33: case 34: case 35: case 36: case 37: duration = 220; break;
      case 38:                            duration = 400; break;
      case 39:                            duration = 1300; break;

      // Phase 5-A: 追加サイクロン 4周
      case 40: case 41: case 42: case 43:
      case 44: case 45: case 46: case 47:
      case 48: case 49: case 50: case 51:
      case 52: case 53: case 54: case 55:
        duration = 220;
        break;

      // Phase 5-B: 追加サイクロン 6周
      case 56: case 57: case 58: case 59:
      case 60: case 61: case 62: case 63:
      case 64: case 65: case 66: case 67:
      case 68: case 69: case 70: case 71:
      case 72: case 73: case 74: case 75:
      case 76: case 77: case 78: case 79:
        duration = 170;
        break;

      // Phase 5-C: 左右交互点滅
      case 80: case 81: case 82: case 83:
      case 84: case 85: case 86: case 87:
      case 88: case 89: case 90: case 91:
      case 92: case 93: case 94: case 95:
        duration = 260;
        break;

      // Phase 5-D: 全LED高速フラッシュ
      case 96: case 97: case 98: case 99:
      case 100: case 101: case 102: case 103:
        duration = 150;
        break;

      // Phase 5-E: じわじわ全点灯
      case 104: duration = 500; break;
      case 105: duration = 650; break;
      case 106: duration = 800; break;
      case 107: duration = 1200; break;

      // Phase 5-F: 大ファンファーレ
      case 108: duration = 300; break;
      case 109: duration = 300; break;
      case 110: duration = 300; break;
      case 111: duration = 420; break;
      case 112: duration = 300; break;
      case 113: duration = 420; break;
      case 114: duration = 700; break;
      case 115: duration = 1800; break;

      // Phase 5-G: 延長サイクロン 4周
      case 116: case 117: case 118: case 119:
      case 120: case 121: case 122: case 123:
      case 124: case 125: case 126: case 127:
      case 128: case 129: case 130: case 131:
        duration = 260;
        break;

      // Phase 5-H: 全LED鼓動
      case 132: case 133: case 134: case 135:
      case 136: case 137: case 138: case 139:
      case 140: case 141: case 142: case 143:
        duration = 750;
        break;

      // Phase 5-I: 最後のファンファーレ
      case 144: duration = 400; break;
      case 145: duration = 400; break;
      case 146: duration = 400; break;
      case 147: duration = 600; break;
      case 148: duration = 400; break;
      case 149: duration = 600; break;
      case 150: duration = 800; break;
      case 151: duration = 2400; break;

      // Phase 6: CHAMPION 表示 + ホールド
      case 152: duration = 0; break;
      case 153: duration = 5000; break;

      default:                            duration = 100; break;
    }
  }

  if (now - stepStart >= duration) {
    step++;
    actionTaken = false;
    stepStart   = now;

    if (step >= 154) {
      // フィナーレ完了 → TITLE へ
      entered = false;
      allLEDOff();
      gameState = STATE_TITLE;
      Serial.println("[STATE] -> TITLE (finale complete)");
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

    // 失敗したラウンド番号（シリアルデバッグ用）
    int failedRound = patternLength - INITIAL_ROUND + 1;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  Game Over!");
    lcd.setCursor(0, 1);
    lcd.print("   Best: ");
    lcd.print(bestScore);

    playSound(0);  // 不正解音

    Serial.print("[STATE] GAME OVER Rnd=");
    Serial.print(failedRound);
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
