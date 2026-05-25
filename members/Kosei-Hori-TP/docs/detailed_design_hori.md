# 詳細設計書 — 組込み開発実習

<!-- 作成者: 堀航成 / 日付: 2026-05-22 / グループ: 1-L -->
<!-- ⏱️ 目安時間: 2〜3時間（作成）+ 1時間（AIレビュー・修正） -->

> **このドキュメントの目的**  
> 基本設計書（basic_design.md）で「どのような構造で作るか」を決めました。  
> この詳細設計書では「**各処理を具体的にどう実装するか**」を決めます。  
> 書き終わったとき、**コードの骨格がほぼ完成している**状態を目指してください。

> [!NOTE]
> 詳細設計書を書いた後にコーディングを始めると、書く内容が明確なため格段に速く進みます。  
> 詳細設計書はそのまま **コードのコメント** としても使えます。

---

## 0. 基本設計書との接続確認

| 項目 | 内容（basic_design.md から転記） |
|:--|:--|
| 作品タイトル | MemoFlash |
| 状態の種類（いくつの状態があるか） | 4種類（TITLE / SHOW / INPUT / OVER） |
| 実装する関数の数 | 13個 |

---

## 1. グローバル変数・定数の設計

> ※ プログラム全体で使う変数・定数を**型と初期値まで**決めます。  
> ここで設計した変数は、この後の関数設計でそのまま使います。

```cpp
#include <LiquidCrystal.h>

// =============================================
// ピン定義（基本設計書 2-1 から転記）
// =============================================
// --- LCD1602 (4-bit parallel) ---
const int PIN_LCD_RS = 7;
const int PIN_LCD_EN = 8;
const int PIN_LCD_D4 = 9;
const int PIN_LCD_D5 = 10;
const int PIN_LCD_D6 = 11;
const int PIN_LCD_D7 = 12;

// --- LED 4色 ---
const int PIN_LED_RED    = 2;
const int PIN_LED_YELLOW = 4;
const int PIN_LED_BLUE   = 5;
const int PIN_LED_GREEN  = 6;

// --- パッシブブザー ---
const int PIN_BUZZER = 3;

// --- メンブレンキーパッド (6ピンカスタムスキャン) ---
const int PIN_KEY_ROW0 = A0;   // A キーの行
const int PIN_KEY_ROW1 = A1;   // B キーの行
const int PIN_KEY_ROW2 = A2;   // C キーの行
const int PIN_KEY_ROW3 = A3;   // D / # キーの行
const int PIN_KEY_COL2 = A4;   // # キーの列
const int PIN_KEY_COL3 = A5;   // A/B/C/D キーの列

// =============================================
// ゲーム設定定数（基本設計書 3-3 主要定数から転記）
// =============================================
const int  MAX_PATTERN     = 20;   // pattern[] 配列の安全上限（要件「上限なし」に対する防御的設計）
const int  INITIAL_ROUND   = 5;    // 初期ラウンド長
const int  LED_ON_TIME     = 500;  // LED 点灯時間 (ms)
const int  LED_OFF_TIME    = 150;  // LED 消灯時間 (ms)
const int  DEBOUNCE_TIME   = 50;   // キー入力のデバウンス時間 (ms)
const int  KEY_SCAN_PERIOD = 50;   // キーパッドスキャン周期 (ms)

// =============================================
// 状態定数（基本設計書 1-2 状態遷移から転記）
// =============================================
const int STATE_TITLE = 0;
const int STATE_SHOW  = 1;
const int STATE_INPUT = 2;
const int STATE_OVER  = 3;

// =============================================
// 色定数（pattern[] の値・LED の識別用）
// =============================================
const int COLOR_RED    = 0;
const int COLOR_YELLOW = 1;
const int COLOR_BLUE   = 2;
const int COLOR_GREEN  = 3;

// =============================================
// LCD オブジェクト
// =============================================
LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);

// =============================================
// 状態管理
// =============================================
int gameState = STATE_TITLE;   // 現在の状態（電源ONで TITLE から開始）

// =============================================
// タイマー管理（millis() 用）
// =============================================
unsigned long lastKeyScanTime  = 0;   // キースキャン周期管理用
unsigned long lastLedTime      = 0;   // SHOW状態のLED切替用
unsigned long lastDebounceTime = 0;   // チャタリング判定用

// =============================================
// 入力管理（キーパッド）
// =============================================
char lastKey    = '\0';   // 前回検出したキー（リピート抑制用）
char currentKey = '\0';   // 現在のスキャンで検出したキー
char lastRawKey = '\0';   // scanKeypad 内のデバウンス比較用（前回スキャンの生値）

// =============================================
// ゲーム進行管理
// =============================================
int  pattern[MAX_PATTERN];     // 生成されたパターン（0〜3 の色番号）
int  patternLength = 0;        // 現在のラウンド長（initGame() で INITIAL_ROUND にセット）
int  inputIndex    = 0;        // INPUT状態での入力進捗（initGame() でリセット）
int  showIndex     = 0;        // SHOW状態でのLED表示進捗（initGame() でリセット）
int  bestScore     = 0;        // セッション中ハイスコア（最高到達ラウンド数・電源ONで0）
```

---

## 2. 各関数の詳細設計

> ※ 基本設計書（3-2 関数一覧）で定義した各関数の「中身」を設計します。  
> **疑似コード**（日本語＋コード混じり）で書いてください。実際のコードは書かなくてOK。

---

### `setup()` — 初期化処理

```
【処理の流れ】

1. シリアル通信を開始する（デバッグ用）
   - Serial.begin(9600)

2. 乱数シードを初期化する（※ ピンモード設定より前に実行）
   - randomSeed(analogRead(A0))
   - 起動直後の A0 ピンは INPUT (floating) 状態で、ノイズによりランダム値が読める

3. ピンモードを設定する
   - LED 4本（PIN_LED_RED / YELLOW / BLUE / GREEN）→ OUTPUT、初期 LOW（消灯）
   - PIN_BUZZER → OUTPUT、初期 LOW
   - キーパッド行（PIN_KEY_ROW0〜ROW3）→ INPUT_PULLUP
   - キーパッド列（PIN_KEY_COL2, COL3）→ OUTPUT、初期 HIGH（非アクティブ）

4. LCD を初期化する
   - lcd.begin(16, 2)

5. 起動演出（電源ONフィードバック）
   - 赤LED → 黄LED → 青LED → 緑LED の順に、各 200ms ずつ点灯（直前の色は消灯）
   - 最後にすべての LED を消灯する
   - ※ setup() 内では delay() を使ってよい（並行処理が不要な一回限りの演出のため）

6. ゲーム変数を初期化する
   - initGame() を呼ぶ
   - patternLength = INITIAL_ROUND、inputIndex = 0、showIndex = 0
   - ※ bestScore はここではリセットしない（セッション中保持のため）
```

---

### `loop()` — メインループ

```
【処理の流れ】

gameState の値に応じて、対応する状態関数を呼び出す：

  switch (gameState):
    case STATE_TITLE → showTitle() を呼ぶ
    case STATE_SHOW  → showPattern() を呼ぶ
    case STATE_INPUT → waitInput() を呼ぶ
    case STATE_OVER  → showGameOver() を呼ぶ

【設計方針】
- loop() 自体には複雑なロジックを持たせず、状態関数へのディスパッチに徹する
- キーパッドのスキャン・デバウンス・リピート抑制は scanKeypad() 内に閉じ込め、
  各状態関数が必要に応じて scanKeypad() を呼び出す
- 状態の遷移（gameState の書き換え）は、各状態関数の内部で行う

```

---

### `allLEDOff()` — 全LED消灯

**引数：**
- なし

**戻り値：**
- なし

```
【処理の流れ】

1. 赤・黄・青・緑のすべての LED を LOW にする
   - digitalWrite(PIN_LED_RED,    LOW)
   - digitalWrite(PIN_LED_YELLOW, LOW)
   - digitalWrite(PIN_LED_BLUE,   LOW)
   - digitalWrite(PIN_LED_GREEN,  LOW)

```

---

### `lightLED(color)` — 指定色のLEDを点灯

**引数：**
- `color`（int）：点灯させる色（COLOR_RED / COLOR_YELLOW / COLOR_BLUE / COLOR_GREEN）

**戻り値：**
- なし

```
【処理の流れ】
1. allLEDOff() を呼んで、まずすべての LED を消灯する（排他制御）

2. color の値に応じて、対応する LED だけを点灯する:
   - case COLOR_RED    → digitalWrite(PIN_LED_RED,    HIGH)
   - case COLOR_YELLOW → digitalWrite(PIN_LED_YELLOW, HIGH)
   - case COLOR_BLUE   → digitalWrite(PIN_LED_BLUE,   HIGH)
   - case COLOR_GREEN  → digitalWrite(PIN_LED_GREEN,  HIGH)
   - default (上記以外の値) → 何もしない（異常値は無視）

【エラーケース】
- color が 0〜3 以外の値: default で何もせず関数を抜ける（基本設計書 4章の「未定義値は無効入力扱い」と整合）
```

---

### `initGame()` — ゲーム変数のリセット

**引数：**
- なし

**戻り値：**
- なし

```
【処理の流れ】
1. ラウンド長を初期値にリセットする
   - patternLength = INITIAL_ROUND   // 5

2. 入力・表示の進捗をリセットする
   - inputIndex = 0
   - showIndex  = 0

3. パターン配列を新規生成する
   - generatePattern(MAX_PATTERN)
   - これにより 1ゲーム分（20手分）のランダム列が pattern[] に格納される
   - 各ラウンドでは pattern[0..patternLength-1] を使用する

【リセットしないもの】
- bestScore（セッション中保持のため）
```

---

### `updateLCD(round, best)` — LCD のラウンド/スコア表示更新

**引数：**
- `round`（int）：現在のラウンド数
- `best`（int）：セッション中のハイスコア

**戻り値：**
- なし

```
【処理の流れ】
1. LCD の1行目（上段）にラウンド数を表示する
   - lcd.setCursor(0, 0)
   - lcd.print("Round: ")
   - lcd.print(round)
   - lcd.print("   ")   // 古い桁を消すためのパディング（10→9 などの遷移対策）

2. LCD の2行目（下段）にハイスコアを表示する
   - lcd.setCursor(0, 1)
   - lcd.print("Best:  ")
   - lcd.print(best)
   - lcd.print("   ")   // 同上

【表示イメージ（round=12, best=15 の場合）】
   ┌────────────────┐
   │Round: 12       │
   │Best:  15       │
   └────────────────┘
```

---

### `playSound(type)` — ブザー音再生

**引数：**
- `type`（int）：1=正解音 / 0=不正解音

**戻り値：**
- なし

```
【処理の流れ】
1. type の値に応じて、tone() で音を鳴らす:
   - type == 1 (正解音):
     - tone(PIN_BUZZER, 1500, 80)    // 1500Hz を 80ms（短く高い音）
   - type == 0 (不正解音):
     - tone(PIN_BUZZER, 200, 500)    // 200Hz を 500ms（長く低い音）
   - 上記以外: 何もしない（無効値）

【補足】
- tone() の第3引数（持続時間）を指定しているため、noTone() による停止は不要
- tone() はノンブロッキング（バックグラウンドで鳴り続ける）。関数はすぐリターンする
- 呼び出し側でゲーム進行と並行して鳴らしたい場合に有利
```

---

### `scanKeypad()` — キーパッドのスキャンと入力イベント検出

**引数：**
- なし

**戻り値：**
- char：検出された新しいキー（'A'/'B'/'C'/'D'/'#'）、または '\0'（入力なし or イベント未確定）

```
【処理の流れ】

[Step 1] タイミングチェック（スキャン周期の管理）
─────────────────────────────────────────
1. now = millis()
2. if (now - lastKeyScanTime < KEY_SCAN_PERIOD):
     return '\0'         // 50ms 経ってないので何もしない
3. lastKeyScanTime = now


[Step 2] 生スキャン（6ピン読み取り）
─────────────────────────────────────────
変数初期化:
  rawKey = '\0'
  rowsLowCount = 0       // 同時押し検出用カウンタ

▼ 列3 (A/B/C/D 列) を駆動して行を読む
  digitalWrite(PIN_KEY_COL3, LOW)
  digitalWrite(PIN_KEY_COL2, HIGH)
  delayMicroseconds(5)   // 信号安定化

  if (digitalRead(PIN_KEY_ROW0) == LOW): rawKey = 'A'; rowsLowCount++
  if (digitalRead(PIN_KEY_ROW1) == LOW): rawKey = 'B'; rowsLowCount++
  if (digitalRead(PIN_KEY_ROW2) == LOW): rawKey = 'C'; rowsLowCount++
  if (digitalRead(PIN_KEY_ROW3) == LOW): rawKey = 'D'; rowsLowCount++

▼ 列2 (# 列) を駆動して行3 を読む
  digitalWrite(PIN_KEY_COL3, HIGH)
  digitalWrite(PIN_KEY_COL2, LOW)
  delayMicroseconds(5)

  if (digitalRead(PIN_KEY_ROW3) == LOW):
    if (rawKey == '\0'): rawKey = '#'   // # 単独押し
    rowsLowCount++

▼ 列を非アクティブ状態に戻す
  digitalWrite(PIN_KEY_COL2, HIGH)
  digitalWrite(PIN_KEY_COL3, HIGH)


[Step 3] 同時押し検出（基本設計書 4章「1サイクル無効化」）
─────────────────────────────────────────
4. if (rowsLowCount >= 2):
     lastKey    = '\0'    // 状態をリセット
     lastRawKey = '\0'
     return '\0'           // このサイクルは無効


[Step 4] デバウンス（チャタリング防止）
─────────────────────────────────────────
5. if (rawKey != lastRawKey):
     // 値が変化した（バウンスの可能性）→ タイマー更新
     lastRawKey       = rawKey
     lastDebounceTime = now
     return '\0'           // まだ安定していないので未確定

6. if (now - lastDebounceTime < DEBOUNCE_TIME):
     return '\0'           // 安定時間に達していない


[Step 5] リピート抑制 + 新規キーイベントの発火
─────────────────────────────────────────
7. if (rawKey == lastKey):
     return '\0'           // 押しっぱなし or 入力なし継続

8. lastKey = rawKey         // 状態を更新

9. if (rawKey == '\0'):
     return '\0'           // 「離された」イベント（状態だけ更新して戻る）
   else:
     return rawKey         // 新しいキー押下イベント！

アルゴリズム図解

[毎ループ scanKeypad() 呼出]
       │
       ▼
[50ms 経過？] ── NO ──→ return '\0'
       │ YES
       ▼
[列3 LOW で行0-3 を読む]
[列2 LOW で行3 を読む]
       │
       ▼
[同時押し？] ── YES ──→ 状態リセット, return '\0'
       │ NO
       ▼
[前回と値が違う？] ── YES ──→ タイマー更新, return '\0'
       │ NO
       ▼
[50ms 安定した？] ── NO ──→ return '\0'
       │ YES
       ▼
[lastKey と同じ？] ── YES ──→ return '\0' (押しっぱなし)
       │ NO
       ▼
[lastKey 更新]
       │
       ▼
[キーあり？] ── NO ──→ return '\0' (release イベント)
       │ YES
       ▼
   return キー値
```

---

### `generatePattern(length)` — ランダムパターンの生成

**引数：**
- `length`（int）：生成する要素数（通常は MAX_PATTERN を渡す）

**戻り値：**
- なし（グローバル pattern[] 配列に書き込む）

```
【処理の流れ】

1. 入力バリデーション（範囲チェック）
   - if (length > MAX_PATTERN): length = MAX_PATTERN
   - if (length <= 0): return （何もせず終了）

2. length 個の要素をランダム生成
   - for (i = 0; i < length; i++):
     - pattern[i] = random(4)    // 0, 1, 2, 3 のいずれか（COLOR_RED〜COLOR_GREEN）

【補足】
- random(4) は 0〜3 の整数を返す（4は含まない）
- 同色連続を許容する仕様（要件定義書 3-1）に従い、特別な制約はかけない
- randomSeed() は setup() で初期化済み
```

---

### `showTitle()` — TITLE 状態の処理

**引数：**
- なし

**戻り値：**
- なし

```
【処理の流れ】

[初回エントリー処理]
static bool entered = false

1. if (entered == false):
   a. 全 LED 消灯
      - allLEDOff()
   b. LCD に表示
      - lcd.setCursor(0, 0)
      - lcd.print("MemoFlash       ")    // 1行目（残りスペースでパディング）
      - lcd.setCursor(0, 1)
      - lcd.print("Press # to START")    // 2行目（ぴったり16文字）
   c. entered = true

[毎ループ処理]
2. char key = scanKeypad()
3. if (key == '#'):
   a. initGame()                  // ゲーム変数リセット + パターン新規生成
   b. entered = false             // 次回 TITLE に入る時のためリセット
   c. gameState = STATE_SHOW      // SHOW 状態へ遷移
```

---

### `showPattern()` — SHOW 状態の処理

**引数：**
- なし

**戻り値：**
- なし

```
【関数内の静的変数】
static bool entered = false        // 状態に入った直後かどうか
static bool ledIsOn = false        // 現在 LED が点灯フェーズか消灯フェーズか


【処理の流れ】

[初回エントリー処理]
1. if (entered == false):
   a. updateLCD(patternLength, bestScore)   // 現ラウンドとスコアを表示
   b. showIndex = 0                          // 表示インデックスをリセット
   c. allLEDOff()
   d. lightLED(pattern[0])                   // 最初のLEDを点灯
   e. ledIsOn   = true
   f. lastLedTime = millis()
   g. entered   = true
   return                                    // この回はここまで

[毎ループ処理]
2. now = millis()

3. if (ledIsOn):
   a. if (now - lastLedTime >= LED_ON_TIME):  // 500ms 経った
      - allLEDOff()
      - ledIsOn = false
      - lastLedTime = now

4. else (LED 消灯フェーズ):
   a. if (now - lastLedTime >= LED_OFF_TIME):  // 150ms 経った
      - showIndex++
      - if (showIndex >= patternLength):
        // ★ パターン提示完了 → INPUT へ遷移
            - inputIndex       = 0
            - lastKey          = '\0'     // 最後に報告したキー
            - lastRawKey       = '\0'     // 生スキャン値もリセット
            - lastDebounceTime = millis()  // デバウンスタイマーをリセット
            - entered          = false
            - gameState        = STATE_INPUT
            // ※ scanKeypad() の空読みは不要（明示リセットで確実にクリア）
      - else:
        // 次の LED を点灯
        - lightLED(pattern[showIndex])
        - ledIsOn = true
        - lastLedTime = now
```

---

### `waitInput()` — INPUT 状態の処理

**引数：**
- なし

**戻り値：**
- なし

```
【関数内の静的変数】
static bool entered            = false
static bool feedbackLedOn      = false      // 正解フィードバックLED が点灯中か
static unsigned long feedbackStartTime = 0  // フィードバック開始時刻


【処理の流れ】

[初回エントリー処理]
1. if (entered == false):
   a. allLEDOff()
   b. updateLCD(patternLength, bestScore)
   c. feedbackLedOn = false
   d. entered       = true

[毎ループ：フィードバック LED の自動消灯]
2. if (feedbackLedOn):
   a. if (millis() - feedbackStartTime >= LED_OFF_TIME):  // 150ms 経過
      - allLEDOff()
      - feedbackLedOn = false

[毎ループ：キー入力チェック]
3. char key = scanKeypad()
4. if (key == '\0'):
   - return                          // 入力イベントなし

5. if (key == '#'):
   - return                          // # は無効入力扱い（基本設計書 4章）

[A/B/C/D が押された場合の判定]
6. int pressedColor = keyToColor(key)        // 'A'→0, 'B'→1, 'C'→2, 'D'→3
7. int expectedColor = pattern[inputIndex]

8. if (pressedColor == expectedColor):
   // ★ 正解
   a. lightLED(pressedColor)                // 視覚フィードバック
   b. feedbackLedOn      = true
   c. feedbackStartTime  = millis()
   d. inputIndex++

   e. if (inputIndex >= patternLength):
      // ★★ このラウンドを完走
      - if (patternLength > bestScore):
        - bestScore = patternLength
      - patternLength++

      - if (patternLength > MAX_PATTERN):
        // パターン上限に到達 → ゲームクリア扱い（OVER で表示）
        - entered   = false
        - gameState = STATE_OVER
      - else:
        // 次のラウンド SHOW へ
        - entered   = false
        - gameState = STATE_SHOW
9. else:
   // ★ 不正解
   a. entered   = false
   b. gameState = STATE_OVER
```

---

### `showGameOver()` — OVER 状態の処理

**引数：**
- なし

**戻り値：**
- なし

```
【処理の流れ】

[初回エントリー処理]
static bool entered = false

1. if (entered == false):
   a. 全 LED 消灯
      - allLEDOff()
   b. LCD に表示
      - lcd.setCursor(0, 0)
      - lcd.print("Game Over!      ")
      - lcd.setCursor(0, 1)
      - lcd.print("Rnd:")
      - lcd.print(patternLength)        // 失敗したラウンド
      - lcd.print(" Bst:")
      - lcd.print(bestScore)            // セッション中の最高完走ラウンド
      - lcd.print("    ")               // 末尾パディング
   c. 不正解音を鳴らす（任意機能採用時）
      - playSound(0)
   d. entered = true

   ※ ハイスコア更新は waitInput() の「ラウンド完走時」にのみ行う（ここでは更新しない）

[毎ループ処理]
2. char key = scanKeypad()
3. if (key == '#'):
   a. entered = false
   b. gameState = STATE_TITLE
   c. ※ initGame() はここでは呼ばない（次の TITLE → SHOW 遷移で showTitle() が呼ぶ）
```

---

### `keyToColor(key)` — キーから色番号への変換

**引数：**
- `key`（char）：A/B/C/D のいずれかのキー

**戻り値：**
- int：対応する色番号（0〜3）、未定義キーの場合は -1

```
【処理の流れ】
1. switch (key):
   - case 'A': return COLOR_RED;     // 0
   - case 'B': return COLOR_YELLOW;  // 1
   - case 'C': return COLOR_BLUE;    // 2
   - case 'D': return COLOR_GREEN;   // 3
   - default:  return -1;            // 未定義（呼出側でガード推奨）

【補足】
- 呼出側（waitInput）は事前に '#' を除外しているため、-1 は基本発生しない
- ただし防御的に書いて、想定外の入力での誤動作を防ぐ
```

---

## 3. 重要ロジックの設計（必要な場合）

> ※ 複雑なロジック（ゲーム判定、センサー計算、LED点灯パターンなど）は  
> ここでフローチャートや疑似コードで詳しく設計しておきます。

### 3-1. （ロジック名を書く）

> ※ 例：「チャタリング防止処理」「距離の判定ロジック」「スネークゲームの衝突判定」など

```
【入力】：

【処理】：
1. 
2. 
3. 

【出力】：
```

---

## 4. テスト仕様書

> ※ 実装が終わったら、各機能が「正しく動いているか」を確認するテストを定義します。  
> テスト実施後に「実際の結果」と「合否」を記入してください。

### 4-1. 単体テスト（部品・関数ごと）

| No | テスト対象 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | LED 赤の点灯 | sketch で `digitalWrite(PIN_LED_RED, HIGH)` | 赤LED が点灯する | | [ ] |
| 2 | LED 黄の点灯 | 同上 (PIN_LED_YELLOW) | 黄LED が点灯する | | [ ] |
| 3 | LED 青の点灯 | 同上 (PIN_LED_BLUE) | 青LED が点灯する | | [ ] |
| 4 | LED 緑の点灯 | 同上 (PIN_LED_GREEN) | 緑LED が点灯する | | [ ] |
| 5 | lightLED() の排他制御 | lightLED(COLOR_RED) → lightLED(COLOR_BLUE) を連続呼出 | 赤が消え、青のみ点灯（複数同時点灯がない） | | [ ] |
| 6 | allLEDOff() の動作 | 4つのLEDを全点灯後、allLEDOff() を呼ぶ | 4つすべて消灯 | | [ ] |
| 7 | LCD 動作確認 | lcd.print("Hello") をテストスケッチで実行 | LCD に "Hello" が表示される | | [ ] |
| 8 | ブザー動作確認 | tone(PIN_BUZZER, 1500, 200) を実行 | 約 200ms 間、高音(1500Hz)が鳴る | | [ ] |
| 9 | キーパッド A 検出 | scanKeypad() を呼びながら A キーを押下 | 'A' が返ってくる（押下から最大 100ms 以内） | | [ ] |
| 10 | キーパッド B/C/D 検出 | 同上を B/C/D で繰り返す | それぞれ 'B'/'C'/'D' が返ってくる | | [ ] |
| 11 | キーパッド # 検出 | 同上を # キーで実行 | '#' が返ってくる | | [ ] |
| 12 | リピート抑制 | A を 3秒間押しっぱなしにする | 最初に1回だけ 'A' が返り、その後は '\0' のまま | | [ ] |
| 13 | generatePattern() 動作 | generatePattern(20) を実行し、pattern[0..19] を出力 | 0〜3 の範囲の整数が 20個 入っている | | [ ] |

### 4-2. 機能テスト（必須機能ごと）

> ※ requirements.md の「3-1. 必須機能」を1つずつ検証します。

| No | 必須機能（requirements.md から転記） | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | 4色のLEDをランダムな順序で1つずつ点灯しパターン提示 | ゲームを開始（# キー）し、SHOW フェーズを観察 | 5個の LED が1つずつ順番に点灯する（毎回違う順序） | | [ ] |
| 2 | キーパッドで色を順番に入力できる | パターンを覚えた後、見た順序通りに A/B/C/D を押す | 押すたびに対応色 LED が瞬間的に光る | | [ ] |
| 3 | A=赤、B=黄、C=青、D=緑 の対応 | INPUT 状態で A を押す | 赤 LED がピカッと光る（黄/青/緑は光らない） | | [ ] |
| 4 | 入力順序の正誤判定 | わざと間違ったキーを押す | 即座に Game Over 画面に遷移する | | [ ] |
| 5 | 正解時は次のラウンドに進める | 5手を全部正解する | 次の SHOW で 6個の LED が表示される | | [ ] |
| 6 | LCD にラウンド数と最高到達ラウンド数（ハイスコア）を表示 | ゲーム中 LCD を観察 | "Round: X" "Best: Y" の形式で正しく表示される | | [ ] |
| 7 | 不正解時 GameOver 画面で最終ラウンドとハイスコアを表示 | わざと間違える | "Game Over!" と "Rnd:X Bst:Y" が表示される | | [ ] |
| 8 | 初期ラウンド長は 5、正解するたびに +1。実用上の上限なし（20手で打ち切り、防御的） | ゲーム開始時の SHOW を観察、5手連続クリアして次の SHOW を観察 | 1ラウンド目は 5手、2ラウンド目は 6手の LED 点灯 | | [ ] |
| 9 | 同色連続を許容する完全ランダム | ゲームを5回程度プレイし、SHOW の表示を記録 | 少なくとも一度は「同じ色が連続するパターン」が出現する | | [ ] |

### 4-3. 異常系テスト

| No | 異常ケース（basic_design.md 4章から転記） | テスト手順 | 期待する動作 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | チャタリング（キーパッド） | A キーを素早く2回押す（50ms 以内に2回押すイメージ） | 1回の入力としてのみ認識される（inputIndex が1だけ進む） | | [ ] |
| 2 | 同時押し（複数キー） | A と B を同時に押す | 入力が無効化され、inputIndex は変わらない | | [ ] |
| 3 | SHOW 状態でのキー入力 | SHOW 中に適当なキーを連打する | 入力が完全に無視され、SHOW のリズムが乱れない | | [ ] |
| 4 | INPUT 状態での未定義キー（#） | INPUT 中に # キーを押す | 何も起こらず、入力待ちが継続する（GameOverにならない） | | [ ] |
| 5 | OVER 状態での # 以外のキー | Game Over 画面で A/B/C/D を押す | 何も起こらず、Game Over 画面のまま | | [ ] |
| 6 | SHOW→INPUT 遷移時のキー持ち越し | SHOW の最後の LED 点灯中にキーを押しっぱなしにする | INPUT に切り替わった瞬間にそのキーが入力として扱われない | | [ ] |

---

## 5. AIレビュー記録（詳細設計版）

### Q1: 実装上の問題確認
> 「この詳細設計書に書いた関数と処理フローをもとにArduinoコードを書きます。  
> バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**

- 指摘1: `scanKeypad()` で `lastRawKey` を使用しているが、グローバル変数定義がなくコンパイルエラーになる。
- 指摘2: `waitInput()` で `keyToColor()` を呼んでいるが、関数仕様が未定義でコンパイルエラーになる。
- 指摘3: `bestScore` の更新が `waitInput()` と `showGameOver()` の両方にあり、`patternLength++` 後の値を拾って過大記録するリスクがある。
- 指摘4: 仕様面では「上限なし」と `MAX_PATTERN=20` が不一致。どちらかに統一が必要。
- 指摘5: SHOW→INPUT のキー持ち越し対策は `scanKeypad()` の空読み依存のため、タイミングによって取りこぼしや誤検知の余地がある。

**対応した内容：**

- 指摘1への対応:
   scanKeypad() 内で参照していた lastRawKey が未定義だったため、
   chapter 1「入力管理」セクションにグローバル変数として追加した。

- 指摘2への対応:
   waitInput() で使用していた keyToColor() が関数として定義されていなかったため、
   chapter 2 に独立した関数（2-14）として正式に追加し、関数一覧の合計を13個に更新した。

- 指摘3への対応:
   showGameOver() で patternLength（=失敗したラウンド）を使って bestScore を
   上書きしており、未到達のラウンドをハイスコアとして記録するバグがあった。
   そこで showGameOver() からハイスコア更新ロジックを削除し、waitInput() の
   「ラウンド完走時」にのみ更新する設計に統一した。
   これにより、ハイスコア=「完走した最高ラウンド」で一貫する。

- 指摘4への対応:
   要件「上限なし」と設計 MAX_PATTERN=20 の関係を明文化した。
   chapter 1 のコメントで「人間の短期記憶限界（7±2）を考慮した到達不可能な防御的上限」
   と位置づけ、要件と矛盾しないことを明示した。
   4-2 機能テスト No.8 の文言も「実用上の上限なし（防御的に20まで）」に整合した。

- 指摘5への対応:
   SHOW→INPUT 遷移時のバッファクリアが scanKeypad() の空読みに依存しており、
   KEY_SCAN_PERIOD のゲートによって実行されない可能性があった。
   グローバル化した lastRawKey と lastDebounceTime を遷移時に直接リセットすることで、
   タイミング非依存の確実なクリアに変更した。

---

### Q2: テスト仕様の確認
> 「このテスト仕様書で、必須機能がすべて検証できていますか？  
> テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**

- 結論: 必須機能の大枠は検証できているが、合否を確定するには不足が残る。

- 不足している主な項目:
   - 色対応テストが A のみで、B/C/D の機能テストが不足している。
   - ハイスコアの境界（失敗時に best が更新されないこと）の確認が不足している。
   - ラウンド長の境界（19→20、20 到達後の遷移）が未確認。
   - GameOver 後の再開始フロー（# で TITLE に戻り再スタート）が未確認。
   - 同色連続のランダム性テストは 5 回試行だと偽陰性の可能性がある。

- 境界値・追加推奨テスト:
   - B/C/D の色対応を個別に検証する。
   - 6 手完走後に 7 手目でミスし、Rnd=7・Bst=6 のままか確認する。
   - 19 手完走で次ラウンドが 20 になることを確認する。
   - 20 手完走後の遷移（設計どおり OVER など）を確認する。
   - OVER 画面で # 押下後、TITLE 経由で初期ラウンド再開・best セッション保持を確認する。
   - ランダム性確認は試行回数を 20 回以上に増やして評価する。

**対応した内容：**

---

## 6. グループレビュー記録

### 6-1. 指摘一覧

| No | 指摘内容 | 対応 |
|:---|:---|:---|
| 1 |  |  |
| 2 |  |  |
| 3 |  |  |

### 6-2. レビューを受けて変更した点

- 

---

*初版: 2026-05-22 / グループレビュー後更新: 2026-05-22*
