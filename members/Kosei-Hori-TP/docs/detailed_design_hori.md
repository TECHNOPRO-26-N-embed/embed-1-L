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
| 1 | LED 赤の点灯 | sketch で `digitalWrite(PIN_LED_RED, HIGH)` | 赤LED が点灯する | | [〇] |
| 2 | LED 黄の点灯 | 同上 (PIN_LED_YELLOW) | 黄LED が点灯する | | [〇] |
| 3 | LED 青の点灯 | 同上 (PIN_LED_BLUE) | 青LED が点灯する | | [〇] |
| 4 | LED 緑の点灯 | 同上 (PIN_LED_GREEN) | 緑LED が点灯する | | [〇] |
| 5 | lightLED() の排他制御 | lightLED(COLOR_RED) → lightLED(COLOR_BLUE) を連続呼出 | 赤が消え、青のみ点灯（複数同時点灯がない） | | [〇] |
| 6 | allLEDOff() の動作 | 4つのLEDを全点灯後、allLEDOff() を呼ぶ | 4つすべて消灯 | | [〇] |
| 7 | LCD 動作確認 | lcd.print("Hello") をテストスケッチで実行 | LCD に "Hello" が表示される | | [〇] |
| 8 | ブザー動作確認 | tone(PIN_BUZZER, 1500, 200) を実行 | 約 200ms 間、高音(1500Hz)が鳴る | | [〇] |
| 9 | キーパッド A 検出 | scanKeypad() を呼びながら A キーを押下 | 'A' が返ってくる（押下から最大 100ms 以内） | | [〇] |
| 10 | キーパッド B/C/D 検出 | 同上を B/C/D で繰り返す | それぞれ 'B'/'C'/'D' が返ってくる | | [〇] |
| 11 | キーパッド # 検出 | 同上を # キーで実行 | '#' が返ってくる | | [〇] |
| 12 | リピート抑制 | A を 3秒間押しっぱなしにする | 最初に1回だけ 'A' が返り、その後は '\0' のまま | | [〇] |
| 13 | generatePattern() 動作 | generatePattern(20) を実行し、pattern[0..19] を出力 | 0〜3 の範囲の整数が 20個 入っている | | [〇] |

### 4-2. 機能テスト（必須機能ごと）

> ※ requirements.md の「3-1. 必須機能」を1つずつ検証します。

| No | 必須機能（requirements.md から転記） | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | 4色のLEDをランダムな順序で1つずつ点灯しパターン提示 | ゲームを開始（# キー）し、SHOW フェーズを観察 | 5個の LED が1つずつ順番に点灯する（毎回違う順序） | | [〇] |
| 2 | キーパッドで色を順番に入力できる | パターンを覚えた後、見た順序通りに A/B/C/D を押す | 押すたびに対応色 LED が瞬間的に光る | | [〇] |
| 3 | キー対応の全色マッピング | INPUT 中に A → B → C → D の順に押し、各押下で点灯する LED を観察 | A=赤 / B=黄 / C=青 / D=緑 がそれぞれ点灯する | | [〇] |
| 4 | 入力順序の正誤判定 | わざと間違ったキーを押す | 即座に Game Over 画面に遷移する | | [〇] |
| 5 | 正解時は次のラウンドに進める | 5手を全部正解する | 次の SHOW で 6個の LED が表示される | | [〇] |
| 6 | LCD にラウンド数と最高到達ラウンド数（ハイスコア）を表示 | ゲーム中 LCD を観察 | "Round: X" "Best: Y" の形式で正しく表示される | | [〇] |
| 7 | 不正解時 GameOver 画面で最終ラウンドとハイスコアを表示 | わざと間違える | "Game Over!" と "Rnd:X Bst:Y" が表示される | | [〇] |
| 8 | 初期ラウンド長は 5、正解するたびに +1。実用上の上限なし（20手で打ち切り、防御的） | ゲーム開始時の SHOW を観察、5手連続クリアして次の SHOW を観察 | 1ラウンド目は 5手、2ラウンド目は 6手の LED 点灯 | | [〇] |
| 9 | 同色連続を許容する完全ランダム | シリアル出力でパターンをログ表示しながら 20回以上ゲームを開始し、生成された pattern[] を記録 | 20試行中で同色連続が複数回観測され、特定色への偏りが極端でない（各色 25% ± 15% 程度） | | [ ] |
| 10 | 失敗時のハイスコア不更新（回帰テスト） | 6手ラウンドをクリア後、7手目ラウンドで意図的にミスする | LCD に "Rnd:7 Bst:6" と表示される（Bst が 7 にならない） | | [ ] |
| 11 | 上限近傍 19→20 の遷移 | （テスト用に MAX_PATTERN=5, INITIAL_ROUND=3 に一時変更）4手ラウンドをクリア | 次ラウンドで 5手の SHOW が始まる | | [ ] |
| 12 | 上限到達時の挙動 | （同上のテスト設定で）5手ラウンドをクリア | 設計どおり OVER 状態に遷移し、Game Over 表示になる | | [ ] |
| 13 | Game Over 後の再開始フロー | ゲーム途中で意図的にミス → OVER 画面で # 押下 → TITLE → # 押下で再スタート | 新ゲームの 1ラウンド目が 5手で始まり、best は前ゲームのスコアが保持される | | [ ] |

### 4-3. 異常系テスト

| No | 異常ケース（basic_design.md 4章から転記） | テスト手順 | 期待する動作 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | チャタリング（キーパッド） | A キーを素早く2回押す（50ms 以内に2回押すイメージ） | 1回の入力としてのみ認識される（inputIndex が1だけ進む） | | [〇] |
| 2 | 同時押し（複数キー） | A と B を同時に押す | 入力が無効化され、inputIndex は変わらない | | [〇] |
| 3 | SHOW 状態でのキー入力 | SHOW 中に適当なキーを連打する | 入力が完全に無視され、SHOW のリズムが乱れない | | [〇] |
| 4 | INPUT 状態での未定義キー（#） | INPUT 中に # キーを押す | 何も起こらず、入力待ちが継続する（GameOverにならない） | | [〇] |
| 5 | OVER 状態での # 以外のキー | Game Over 画面で A/B/C/D を押す | 何も起こらず、Game Over 画面のまま | | [〇] |
| 6 | SHOW→INPUT 遷移時のキー持ち越し | SHOW の最後の LED 点灯中にキーを押しっぱなしにする | INPUT に切り替わった瞬間にそのキーが入力として扱われない | | [×] |
| 7 | バッファクリアの確実な動作（修正#5 の検証） | SHOW の最終 LED 点灯中から INPUT 状態に切り替わる瞬間までキーを押しっぱなしにする | INPUT 開始直後にそのキーが「新しい入力」として扱われない（誤入力扱いされない） | | [×] |

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

【既存テストの強化】
- 4-2 No.3 を「A 対応のみ」から「A/B/C/D 全色マッピング検証」に修正し、4色すべての対応を1つのテストで網羅できるようにした。
- 4-2 No.9 のランダム性テストを「5回中1回以上同色連続」から「20試行 + シリアルログで統計確認」に変更した。偽陰性のリスクを低減し、定量評価を可能にした。

【新規テストの追加（4-2 に4件追加）】
- No.10 として「失敗時のハイスコア不更新」を追加。Q1 で修正したロジックバグ（showGameOver による best 過大計上）の回帰テストとして位置づけた。
- No.11, No.12 として「上限近傍（19→20）の遷移」「上限到達時の挙動」を追加。実機テストは MAX_PATTERN=5, INITIAL_ROUND=3 のテスト用定数に一時変更して実施するテストフィクスチャ方式を採用した。
- No.13 として「Game Over 後の再開始フロー」を追加。TITLE 経由での新ゲーム開始と best の保持を検証する。

【4-3 異常系への追加】
- Q1 修正#5（バッファクリアのロバスト化）の回帰テストとして、「SHOW→INPUT 遷移時のキー持ち越し対策の確実な動作」を異常系 No.7 として追加した。

---

## 6. グループレビュー記録

### 6-1. 指摘一覧

| No | 指摘内容 | 対応 |
|:---|:---|:---|
| 1 | 一番つまずきそうなポイントは？ | scanKeypad()の内容が難しく、理解しきれていない箇所もあるため、理解をしてから実装に努める |
| 2 | テスト仕様書の中で一番こだわったポイントは？ | 入力関連の処理でバグが起きやすいと予想したため、AIの回答に抜け漏れがないようにするために、プロンプトを工夫した。 |
| 3 |  |  |

### 6-2. レビューを受けて変更した点


---

## 7. 実装後の改善履歴（初版 → 改良版 → 拡張版）

> 実装中・実装後に発見した課題と追加機能を時系列で記録。
> 実装→実機プレイ→フィードバック→改善、を繰り返した。

### 7-1. 改善フェーズの全体像

| フェーズ | 内容 | 主な作業 |
|:---|:---|:---|
| **初版** | 基本設計書通りの初版実装 | 14関数、4状態（TITLE/SHOW/INPUT/OVER）でゲーム成立 |
| **改良版** | プレイテスト後の7つの改善 | バグ修正＋UX向上＋状態2つ追加 |
| **拡張版** | さらなる追加機能 | 色音連動・進捗バー・特大フィナーレ |

---

### 7-2. 改良版で追加した改善（初版 → 改良版）

#### 改善 #1：SHOW中の押しっぱなしバグ修正
- **現象**：SHOW中にキーを押しっぱなしにすると、INPUT切り替わり瞬間に「新しい入力」と誤判定される
- **原因**：`showPattern()` のSHOW→INPUT遷移時に `lastKey='\0'` でリセットしていた
- **対策**：リセットを廃止し、`scanKeypad()` のリピート抑制で「一度離して再度押した時のみ確定」する動作に
- **影響**：`showPattern()`

#### 改善 #2：起動LED速度の調整
- 各色 200ms → **1000ms**（後にPhase別に再設計）

#### 改善 #3：起動演出の3段構成化
LEDと音が直列だったのを、**溜め→タメ→クライマックス** に再設計：
- **Phase 1**：順次点灯（500→400→300→200msと加速、音なし）
- **Phase 2**：全消灯（120msの間）
- **Phase 3**：4LED同時点灯 + 上昇アルペジオ（E5→G5→C6→E6）
- 約2.25秒の起動チャイム

#### 改善 #4：カウントダウン演出（STATE_COUNTDOWN 追加）
- #押下後、心の準備期間を提供
- 「3, 2, 1, GO!」を1秒間隔で表示、各タイミングでブザー音
- 新状態 `STATE_COUNTDOWN`、新関数 `showCountdown()`

#### 改善 #5：ラウンドクリア演出（STATE_ROUND_CLEAR 追加）
- 1ラウンド完了→即次ラウンド開始だったのを、達成感のある演出を挟む
- LED3回フラッシュ + Cメジャー上昇音階（C5→E5→G5）+ LCD「Round N Clear!」
- 新状態 `STATE_ROUND_CLEAR`、新関数 `showRoundClear()`

#### 改善 #6：ラウンド表記統一・LCD修正・音差別化（複合）
4つのサブ改善：
- **6a**：「Round 1, 2, 3...」表記に統一（旧：LED数表示 "Round: 5"）
  - `bestScore` のセマンティクスも「最高LED数」→「最高ラウンド番号」に変更
- **6b**：LCD残留文字バグ修正（行末まで上書きされず前画面の文字が残る）
  - `snprintf` で16文字幅にパディングして完全上書き
- **6c**：Game Over画面を Best 表示のみに簡素化（失敗ラウンド情報は不要と判断）
- **6d**：ゲームオーバー音を下降音階＋低音持続（C4→G3→E3→C3）に変更
  - クリア音（上昇）と明確に差別化

#### 改善 #7：INPUT中の#でタイトル復帰
- 発表時の「ランダム性検証」用：発表中でも任意のタイミングでリセットして新パターンを見せられる
- INPUT状態中の # 押下を「強制中断 → TITLE」に
- `bestScore` は保持（セッション継続）

---

### 7-3. 拡張版で追加した機能（改良版 → 拡張版）

#### A1：パターン提示中の色音
- SHOW中、各LED点灯と同時に色固有の音階を鳴らす
- 赤=C5(523Hz) / 黄=E5(659Hz) / 青=G5(784Hz) / 緑=C6(1047Hz)
- パターンが「メロディ」として聞こえるようになり、Simon Saysらしさが強化
- 新定数 `COLOR_TONES[4]`、影響箇所：`showPattern()`

#### A2：入力時の色音（正解のみ）
- 正解キー押下時、押した色の音を150ms鳴らす
- 不正解時は無音（ゲームオーバー音と被らないため）
- 入力フィードバックが視覚＋聴覚で強化
- 影響箇所：`waitInput()`

#### B3：INPUT中の進捗バー
- 上段：`Round N  X/Y`（ラウンド番号と現在の入力進捗）
- 下段：16セルの進捗バー（HD44780内蔵フルブロック `0xFF` 使用）
- 公式：`filled = (currentStep × 16) ÷ totalSteps`
- 新関数 `updateInputLCD(round, currentStep, totalSteps)`

#### D1：特大フィナーレ演出（STATE_FINALE 追加）
- **トリガー**：最高ラウンド（MAX_PATTERN=20、ラウンド16）クリア時
- **構成**：約10.2秒の5段階演出
  1. 上昇フラッシュ4回（1.65s）
  2. **サイクロン5周加速**（150→130→110→90→70ms、計2.2s）
  3. 高速全点滅 E6→G6→C7（0.5s）
  4. 勝利ファンファーレ：C5→C7上昇 + 全LED点灯（2.8s）
  5. LCD「CHAMPION!!!」+ Best表示 + 全LED 3秒ホールド（3.0s）
- `#` でいつでも途中スキップ可能
- 新状態 `STATE_FINALE`、新関数 `showFinale()`

---

### 7-4. その他の小改善

- **シリアル出力のキー表示**：パターン配列のシリアル出力を数値（0,1,2,3）→ アルファベット（A,B,C,D）に変更。テスト時に「カンニングしながらプレイ」が可能に
  - 新関数 `colorToKey(int color)`

---

### 7-5. 最終的な状態一覧（初版から追加された分を含む）

| 状態 | 値 | 用途 | 追加時期 |
|:---|:---:|:---|:---:|
| `STATE_TITLE` | 0 | タイトル画面 | 初版 |
| `STATE_SHOW` | 1 | パターン提示 | 初版 |
| `STATE_INPUT` | 2 | 入力待ち | 初版 |
| `STATE_OVER` | 3 | ゲームオーバー | 初版 |
| `STATE_COUNTDOWN` | 4 | 3,2,1,GO! カウントダウン | 改良版 |
| `STATE_ROUND_CLEAR` | 5 | ラウンドクリア演出 | 改良版 |
| `STATE_FINALE` | 6 | 全クリア時フィナーレ | 拡張版 |

合計 **7状態**（初版から +3）

---

### 7-6. 最終的な関数一覧

| 関数 | 用途 | 追加/変更 |
|:---|:---|:---|
| `setup()` | 初期化 + 3段階起動演出 | 初版（改良版で起動演出拡張） |
| `loop()` | 状態ディスパッチ | 初版 |
| `allLEDOff()` | 全LED消灯 | 初版 |
| `lightLED(color)` | 指定色LED点灯 | 初版 |
| `initGame()` | ゲーム状態初期化 | 初版 |
| `updateLCD(round, best)` | SHOW中LCD更新（snprintf で16文字保証） | 初版（改良版でバグ修正） |
| `updateInputLCD(round, step, total)` | INPUT中LCD更新（進捗バー） | **拡張版** |
| `playSound(type)` | 効果音再生（正解音/ゲームオーバー音） | 初版（改良版で強化） |
| `scanKeypad()` | キーパッドスキャン（6ピンカスタム） | 初版 |
| `generatePattern(length)` | ランダムパターン生成 | 初版 |
| `showTitle()` | タイトル画面 | 初版 |
| `showCountdown()` | カウントダウン演出 | **改良版** |
| `showPattern()` | パターン提示（色音連動） | 初版（改良版でバグ修正、拡張版で色音） |
| `waitInput()` | 入力待ち＋判定（色音、進捗バー、#リセット） | 初版（改良版と拡張版で機能追加） |
| `showRoundClear()` | ラウンドクリア演出 | **改良版** |
| `showFinale()` | 全クリア時フィナーレ | **拡張版** |
| `showGameOver()` | ゲームオーバー画面 | 初版（改良版で表示簡素化） |
| `keyToColor(key)` | キー文字 → 色変換 | 初版 |
| `colorToKey(color)` | 色 → キー文字変換（シリアル表示用） | **改良版** |

合計 **19関数**（初版14 + 改良版2 + 拡張版2 + 初版2拡張）

---



*初版: 2026-05-22 / グループレビュー後更新: 2026-05-22*
