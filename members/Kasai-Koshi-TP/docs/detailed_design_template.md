# 詳細設計書 — 組込み開発実習

<!-- 作成者: あなたの名前 / 日付: YYYY-MM-DD / グループ: 〇-〇 -->

> **このドキュメントの目的**
> 基本設計書（basic_design.md）で「**どのような構造で作るか**」を決めました。
> この詳細設計書では「**各処理を具体的にどう実装するか**」を決めます。
> 書き終わったとき、**コードの骨格がほぼ完成している**状態を目指してください。

> [!NOTE]
> **V字モデルにおける位置づけ**
> 詳細設計書 ←→ **単体テスト**（関数・部品ごとのテスト）が対応します。
> 「この関数が正しく動くか」の確認は Section 5 の単体テスト仕様書で計画します。
> ※ 必須機能全体が動くかの「結合テスト」は基本設計書（Section 6）に記載します。

---

## 0. 基本設計書との接続確認

| 項目 | basic_design.md から転記 |
|:--|:--|
| 作品タイトル | 簡易カラオケ |
| 状態の種類（1-2 状態遷移から） | 待機中,ボタン待機,ボタン検知,曲再生・歌詞表示,今日終了,終了 |
| 実装する関数の数（2-2 関数一覧から） | 　8個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 　約４１B（配列などで増える可能性あり） |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
const int PIN_BUTTON = 2;

const int PIN_BUZZER = 3;

const int PIN_LCD_SDA = A4;

const int PIN_LCD_SCL = A5;

【状態管理】（basic_design.md 1-2 の状態名から転記）
currentState = 0;
//1:曲再生・歌詞表示 2:終了

【タイマー（millis()用）】（basic_design.md 2-3 から転記）
lastMillis_Button = 0;
lastMillis_lyrics = 0;
lastMillis_Melody = 0;

【センサー・入力値】（basic_design.md 2-1 から転記）
buttonState = false
prevButtonState = false //デバウンス用

【その他のフラグ・カウンター】
  （自分のものを追加）
```
char lyrics[]
int melody[]
int noteDurations[] //各音の長さ

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — 初期化処理

```
【処理の流れ】
1. ピンモードを設定する
   - PIN_BUTTON  → INPUT_PULLUP
   - PIN_LED_*   → OUTPUT
   - PIN_BUZZER  → OUTPUT

2. ライブラリの初期化（使うものだけ）
   - 例: lcd.begin(16, 2)
   - 例: servo.attach(PIN_SERVO)

3. Serial.begin(9600)（デバッグ用）

4. 起動確認（任意）: 緑LEDを1秒点灯して消灯
```

**↓ 自分の setup() を設計してください**
```
【処理の流れ】
1. ピンモードを設定する
- PIN_BUTTON → INPUT_PULLUP
- PIN_BUZZER → OUTPUT

2. LCDを初期化する
- lcd.init()
- lcd.backlight()
- lcd.clear()
- １行目に初期メッセージを記入

3. 状態と制御変数を初期化
- crrentState = 0
- lyricsIndex = 0
- elodyIndex = 0
- lastMillis_lyrics = 0
- lastMillis_Melody = 0
```

---

### `loop()` — メインループ

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

```
【処理の流れ】

＜毎ループ実行すること＞
  - 入力を読む（readButton(), readSensor() などを呼ぶ）
  - 現在時刻を取得: now = millis()

＜currentState が 0（待機中）のとき＞
  - センサー値を監視する
  - 検知条件を満たしたら → currentState = 1

＜currentState が 1（動作中）のとき＞
  - メイン処理を行う
  - 終了条件を満たしたら → currentState = 2

＜currentState が 2（完了）のとき＞
  - 完了表示をする
  - リセットボタンが押されたら → currentState = 0

＜currentState が 3（エラー）のとき＞
  - エラー表示をする / リセットを待つ
```

**↓ 自分の loop() を設計してください**
```
【処理の流れ】

＜毎ループ実行すること＞
now = millis()
btnPressed = readButton()

＜currentState が 0(ボタン待機)のとき＞
- 待機画面を記入する
- btnPressedがtureならstart()を実行

＜currentState が1(曲再生・歌詞表示)のとき＞
- updateLyrics(now)を実行
- playMelody(now)を実行
- 再生中のボタン入力は無視する
- Finish(now)がtureならcurrentState=2にする

＜currentState が2（終了）のとき＞
- stop()を実行
- currentState = 0 に戻す

```

---

### （関数ごとに以下のブロックをコピーして追加してください）

> ※ 基本設計書 2-2 の関数一覧に記載した関数を1つずつ設計します。

---

### `関数名()` — （役割を1行で書く）

**basic_design.md 2-2 との対応：** （基本設計書の関数一覧の説明を転記）

**引数：** `引数名`（型）: 何の値か

**戻り値：** 型（なしの場合は void）

```
【処理の流れ】
1.
2.
3.

【エラー・異常ケース】
- 異常な値が来た場合:
```

### `readButton()` — （ボタンの状態を読み取り押下判定を行う）

**basic_design.md 2-2 との対応：** （ボタンの状態を読み取り押下判定を行う）

**引数：** `引数名`（型）: なし

**戻り値：** 型（なしの場合は void）:bool

```
【処理の流れ】
1. digitalRead(PIN_BUTTON) で現在値を取得する
2. 前回値と異なる場合は lastDebounceTime を更新する
3. 経過時間が DEBOUNCE_DELAY 以上なら入力を確定する
4. 「未押下→押下（HIGH→LOW）」のときのみ true を返す（それ以外は false）

【エラー・異常ケース】
- チャタリングが発生した場合：50ms未満の変化は無視する
```
### `start()` — （始めるための準備をする関数）

**basic_design.md 2-2 との対応：** （再生開始の初期化）

**引数：** `引数名`（型）: now (unisigned long)現在時刻

**戻り値：** 型（なしの場合は void）:void

```
【処理の流れ】
1. lyricsIndex = 0、melodyIndex = 0 にする
2. 再生開始時刻を記録する（lastMillis_lyrics, lastMillis_Melody）
3. 最初の歌詞をLCDに表示する

【エラー・異常ケース】
- 歌詞配列またはメロディ配列が空の場合：currentState = 3（エラー）にする
```
### `updatelLyrics()` — （経過時間に応じて表示する歌詞を切り替える）

**basic_design.md 2-2 との対応：** （経過時間に応じて表示する歌詞を切り替える）

**引数：** `引数名`（型）: now(unisigned long)現在時刻

**戻り値：** 型（なしの場合は void）:void

```
【処理の流れ】
1. 現在の lyricsIndex に対応する表示間隔を取得する
2. now - lastMillis_lyrics が表示間隔以上なら次の歌詞へ進む
3. LCD表示を更新し、lastMillis_lyrics = now に更新する

【エラー・異常ケース】
- lyricsIndex が配列範囲外の場合：空表示にしてエラー状態へ遷移する
```
---

### `playMelody()` — （曲を流すための関数）

**basic_design.md 2-2 との対応：** （経過時間に応じてブザーの音程・長さを制御する）

**引数：** `引数名`（型）: now(unisigned long)現在時刻

**戻り値：** 型（なしの場合は void）:bool

```
【処理の流れ】
1. 現在の melodyIndex の音程と長さを取得する
2. now - lastMillis_Melody が音長以上なら次の音へ進む
3. tone(PIN_BUZZER, 周波数) で再生し、lastMillis_Melody を更新する

【エラー・異常ケース】
- 異常な周波数値の場合：その音をスキップする

```

---


### `Finish()` — （終了していいのかの処理をする）

**basic_design.md 2-2 との対応：** （曲終了の判定を行う）

**引数：** `引数名`（型）: now(unsigned long)現在時刻

**戻り値：** 型（なしの場合は void）:bool

```
【処理の流れ】
1. lyricsIndex が最終要素まで到達したか確認する
2. melodyIndex が最終要素まで到達したか確認する
3. 両方到達していれば true、未到達なら false を返す

【エラー・異常ケース】
- 片方だけ極端に進んだ場合：整合性エラーとして currentState = 3 にする

```

---

### `stop()` — （曲を停止して、LCDも止める）

**basic_design.md 2-2 との対応：** （音停止・LCDクリアなど終了処理を行い状態へ戻す）

**引数：** `引数名`（型）: なし

**戻り値：** 型（なしの場合は void）:void

```
【処理の流れ】
1. noTone(PIN_BUZZER) で音を止める
2. lcd.clear() で表示を消す
3. 待機メッセージ "Press Button" を表示する
4. インデックスと一時変数を次回再生用に初期化する

【エラー・異常ケース】
- LCD表示更新に失敗した場合：音停止だけは必ず実行する

```


---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
ボタンはINPUT_PULLUPで読むため、押していないときはHIGH,押したときはLOW
ただし押下直後は短時間で揺れるので、前回の変化から50ms未満は無視し、50ms以上同じ状態なら確定する。

【処理の流れ】
1.毎ループでボタンの生データを読む（digitalRead(PIN_BUTTON)）。
2.前回読んだ値と違ったら、lastDebounceTime を現在時刻 now に更新する。
3.now - lastDebounceTime が DEBOUNCE_DELAY（50ms）未満なら、まだ確定しない。
4.50ms 以上経過し、かつ値が安定していたら buttonState を更新する。
5.未押下 HIGH から 押下 LOW に変わった瞬間だけ「押された」と判定する。
6.再生中（currentState=1）は押下判定が出ても状態遷移しない（無視）。

【必要な変数（Section 1 に追加済みか確認）】
・lastDebounceTime : unsigned long = 0
・DEBOUNCE_DELAY : const unsigned long = 50
・lastButtonReading : bool = HIGH
・buttonState : bool = HIGH
・prevButtonState : bool = HIGH
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
このシステムでは、歌詞表示とメロディ再生を同時に進める必要がある。
delay() を使うと片方の処理中にもう片方が止まるため、millis() で
「前回実行時刻」を管理して、一定時間ごとに処理を実行する。


【処理の流れ】
  1. 毎ループでnow = milis() を取得する
　2. 歌詞表示更新:
   - now - lastMillis_lyrics が lyricsInterval[lyricsIndex] 以上なら
     次の歌詞へ進めて LCD を更新し、lastMillis_lyrics = now にする。
3. メロディ更新:
   - now - lastMillis_Melody が noteDurations[melodyIndex] 以上なら
     次の音へ進めて tone() を実行し、lastMillis_Melody = now にする。
4. ボタン入力は常時 readButton() で監視し、再生中の押下は無視する。

【自分のシステムで millis() を使う処理】
【自分のシステムで millis() を使う処理】
- 歌詞表示切替:
  変数: lastMillis_lyrics（unsigned long）
  判定: now - lastMillis_lyrics >= lyricsInterval[lyricsIndex]
- メロディ再生:
  変数: lastMillis_Melody（unsigned long）
  判定: now - lastMillis_Melody >= noteDurations[melodyIndex]
- デバウンス（ボタンのチャタリング対策）:
  変数: lastDebounceTime（unsigned long）
  判定: now - lastDebounceTime >= DEBOUNCE_DELAY（50ms）
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1.
2.
3.

【入力値と出力値の関係】

```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | ボタン押下が一回だけ認識させるか | `readButton()` | `BTN raw=LOW,confirmed=ture` |
| 2 | チャタリングが無視できているか | `readButton()` | `devounce elapsed=32ms,ignored` |
| 3 | 状態遷移が正しいか（待機→姿勢→終了） | `loop()` | `state:0>1,state:1>2` |
| 4 | 歌詞の切り替えタイミングが正しいか | `updateLyrics()` | `lyricsIndex=2,now=3500` |
| 5 | メロディの進行が正しいか | `playMelody()` | `melodyindex=5,freq=440,dur=250` |
| 6 | 終了判定が正しく出るか | `Finish()` | `finish=ture(lyrics end,melodyend)` |
| 7 | 停止処理が呼ばれているか | `stop()` | stop calied,buzzer off,lcd clear |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readButton() | タクトスイッチを1回押す | true が返る | | [ ] |
| 2 | readButton() | スイッチを素早く2回押す | 1回分だけ true になる | | [ ] |
| 3 | readSensor() | センサーを正常範囲で使う | 仕様範囲内の値が返る | | [ ] |
| 4 | readSensor() | センサーを遮蔽・範囲外に向ける | 誤動作しない | | [ ] |
| 6 |	updateLyrics() | lyricsIndex=配列長 |	エラー状態へ遷移する |  |	[ ] |
| 7 |	playMelody() | melodyIndex=配列長 |	音が鳴らずエラー状態 |  | [ ] |
| 8 |	readButton() | 50ms未満で連打 | 1回しかtrueにならない |  | [ ] |
| 9 |	stop() | LCD未接続 | 音停止だけは必ず実行 |  | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | updateLyrics() | 歌詞切り替えタイミングで呼ぶ | LCDに次の歌詞が表示される| | [ ] |
| 2 | playMelody() | 曲再生中に呼ぶ | ブザーから正しい音が鳴る | ブ | [ ] |
| 3 | stop() | 曲終了時に呼ぶ | LCDがクリアされ、音が止まる | | [ ] |
| 4 | loop() | currentState=0で待機 | LCdに初期メッセージが表示される | |


### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | 歌詞とメロディの平行動作 | 曲再生中にLCDとブザー両方を観察 | 歌詞と音がずれずに進む | | [ ] |
| 2 | millis()タイマーの周期精度 | 歌詞や音の切り替えタイミングをストップウォッチで測定 | 設計通りの周期で切り替わる | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
1. デバウンス処理の抜け・誤動作

ボタンのチャタリング対策（50ms判定）が正しく実装されていないと、意図しない多重押下や誤動作が発生しやすいです。
lastDebounceTimeやbuttonStateの初期化・更新タイミングに注意。
2. 配列の範囲外アクセス

歌詞やメロディの配列（lyrics[], melody[], noteDurations[]）で、インデックスが範囲外にならないように必ず長さチェックを行うこと。
lyricsIndexやmelodyIndexの更新時に最大値を超えないよう注意。
3. 型の不一致・初期化漏れ

unsigned long型（millis用）とint型（配列インデックスや音程）を混同しないこと。
変数の初期化漏れ（特にcurrentState, lyricsIndex, melodyIndexなど）に注意。
4. 状態遷移の抜け

currentStateの値が想定外になった場合の処理（エラー遷移）が抜けていると、無限ループやフリーズの原因になります。
状態ごとの処理分岐（switchやif）で全ての状態を網羅すること。
5. LCDやブザーの出力失敗時の考慮

LCD表示やブザー出力が失敗した場合のフォールバック処理（最低限、音停止やエラー表示など）を用意しておくと安全です。
6. millis()のオーバーフロー

millis()は約50日で0に戻るため、長時間動作させる場合はオーバーフロー対策が必要です（通常の学習用途なら大きな問題にはなりません）。

**対応した内容：**
実装のときに参考にする

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
【1. テストが不足しやすい項目】

配列の範囲外アクセス（歌詞・メロディの最初/最後/空配列）
ボタン連打や長押し時の挙動（デバウンスの限界テスト）
状態遷移の異常系（想定外のstate値、途中でエラー発生時）
LCDやブザーの出力失敗時の動作（例：LCD未接続時）
【2. 境界値テストの例】

lyricsIndex, melodyIndex = 0（最初の要素）
lyricsIndex, melodyIndex = 配列長-1（最後の要素）
lyricsIndex, melodyIndex = 配列長（範囲外アクセス時の動作）
noteDurations[]やlyricsInterval[]が0や極端に大きい場合
【3. 追加推奨テスト例】

No	テスト対象の関数	入力・操作	期待する結果	実際の結果	合否
6	updateLyrics()	lyricsIndex=配列長	エラー状態へ遷移する		[ ]
7	playMelody()	melodyIndex=配列長	音が鳴らずエラー状態		[ ]
8	readButton()	50ms未満で連打	1回しかtrueにならない		[ ]
9	stop()	LCD未接続	音停止だけは必ず実行		[ ]


**対応した内容：**
テスト項目を追加

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 |  |  |  |
| 2 |  |  |  |
| 3 |  |  |  |

### 7-2. レビューを受けて変更した点

-
-

---

*初版: YYYY-MM-DD / AIレビュー: YYYY-MM-DD / グループレビュー後更新: YYYY-MM-DD*
