#include <LiquidCrystal.h>

// ボタンとブザーのピン番号を定義
const int PIN_BUTTON = 2; // ボタン入力ピン
const int PIN_BUZZER = 3; // ブザー出力ピン

// LCDの配線ピン番号を定義
const int PIN_LCD_RS = 7;
const int PIN_LCD_EN = 8;
const int PIN_LCD_D4 = 9;
const int PIN_LCD_D5 = 10;
const int PIN_LCD_D6 = 11;
const int PIN_LCD_D7 = 12;

// ボタンのチャタリング防止用の遅延時間（ミリ秒）
const unsigned long DEBOUNCE_DELAY = 50;

// シリアルモニターの通信速度
const unsigned long SERIAL_BAUDRATE = 9600;

// アプリケーションの状態を表す列挙型
enum AppState {
  STATE_WAITING = 0, // ボタン待機中
  STATE_PLAYING = 1, // 再生中
  STATE_FINISHED = 2, // 再生終了
  STATE_ERROR = 3 // エラー状態
};

// このスケッチは「状態遷移」で動く。
// WAITING -> PLAYING -> FINISHED -> WAITING が基本ルート。
// 想定外が起きたら ERROR に入れ、ボタン押下で復帰する。

// LCDオブジェクトを初期化
LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);

// 歌詞1行と表示時間をひとまとめにした構造体
struct LyricEntry {
  const char* text;           // LCD/シリアルに出す歌詞1行
  unsigned long intervalMs;   // この1行を表示する時間[ms]
};

// 歌詞データ（英語の「ドレミの歌」）
const LyricEntry lyricEntries[] = {
  {"Do, a deer", 1931},          // 1行目
  {"a female deer", 1931},       // 2行目
  {"Re, a drop", 1931},          // 3行目
  {"of golden sun", 1931},       // 4行目
  {"Mi, a name", 1931},          // 5行目
  {"I call myself", 1931},       // 6行目
  {"Fa, a long", 1931},          // 7行目
  {"long way to run", 1931},     // 8行目
  {"So, a needle", 1931},        // 9行目
  {"pulling thread", 1931},      // 10行目
  {"La, a note", 1931},          // 11行目
  {"to follow So", 1931},        // 12行目
  {"Ti, a drink", 1931},         // 13行目
  {"with jam and bread", 1931},  // 14行目
  {"That will bring", 2420},     // 15行目
  {"us back to Do!", 2420}       // 16行目
};

// メロディ1音と長さをまとめた構造体
struct MelodyNote {
  int frequency;            // 周波数[Hz]。0なら休符（無音）
  unsigned long durationMs; // 音の長さ[ms]
};

// メロディデータ（音階の周波数と長さ）
const MelodyNote melodyNotes[] = {
  {262, 726}, {294, 242}, {330, 726}, {262, 242}, {330, 484}, {262, 484}, {330, 968},
  {294, 726}, {330, 242}, {349, 726}, {349, 242}, {330, 484}, {294, 484}, {349, 968},
  {330, 726}, {349, 242}, {392, 726}, {330, 242}, {392, 484}, {330, 484}, {392, 968},
  {349, 726}, {392, 242}, {440, 726}, {440, 242}, {392, 484}, {349, 484}, {440, 968},
  {392, 726}, {262, 242}, {294, 726}, {330, 242}, {349, 484}, {392, 484}, {440, 968},
  {440, 726}, {294, 242}, {330, 726}, {370, 242}, {392, 484}, {440, 484}, {494, 968},
  {494, 726}, {330, 242}, {370, 726}, {415, 242}, {440, 484}, {494, 484}, {523, 968},
  {440, 484}, {349, 484}, {587, 726}, {494, 242}, {523, 968}, {523, 968}, {0, 968}
};

// 配列の要素数を計算
const size_t lyricsCount = sizeof(lyricEntries) / sizeof(lyricEntries[0]);
const size_t melodyCount = sizeof(melodyNotes) / sizeof(melodyNotes[0]);

// 現在のアプリケーション状態
AppState currentState = STATE_WAITING;

// 歌詞とメロディの現在のインデックス
size_t lyricsIndex = 0;
size_t melodyIndex = 0;

// 最後に歌詞やメロディを更新した時間
unsigned long lastMillis_lyrics = 0;
unsigned long lastMillis_Melody = 0;

// ボタンの状態を管理
int lastButtonReading = HIGH;
int buttonState = HIGH;
int prevButtonState = HIGH;
unsigned long lastDebounceTime = 0;

// 待機画面が表示されたかどうか
bool waitingScreenShown = false;

// 関数のプロトタイプ宣言
bool readButton();
void start(unsigned long now);
void updateLyrics(unsigned long now);
void playMelody(unsigned long now);
bool Finish(unsigned long now);
void stopPlayback();
void showError(const char* message);
void showLyricsPair(size_t firstIndex);

// 初期化関数
void setup() {
  // INPUT_PULLUPなので、未押下=HIGH, 押下=LOW になる
  pinMode(PIN_BUTTON, INPUT_PULLUP); // ボタンをプルアップ入力に設定
  pinMode(PIN_BUZZER, OUTPUT); // ブザーを出力に設定

  Serial.begin(SERIAL_BAUDRATE); // シリアル通信を開始

  lcd.begin(16, 2); // LCDを16x2モードで初期化
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Press Button"); // 初期メッセージを表示

  currentState = STATE_WAITING; // 初期状態を待機に設定
  lyricsIndex = 0;
  melodyIndex = 0;
  lastMillis_lyrics = 0;
  lastMillis_Melody = 0;
}

// メインループ
void loop() {
  // 1ループ内で共通利用する現在時刻。millis()を何度も呼ばないため固定する
  const unsigned long now = millis(); // 現在の時間を取得
  const bool btnPressed = readButton(); // ボタンが押されたか確認

  switch (currentState) {
    case STATE_WAITING:
      // WAITINGに入るたび毎回clearするとちらつくので、最初の1回だけ描画
      if (!waitingScreenShown) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Press Button");
        waitingScreenShown = true;
      }

      if (btnPressed) {
        start(now); // 再生を開始
      }
      break;

    case STATE_PLAYING:
      // 歌詞更新とメロディ更新は独立タイミングで並行進行させる
      updateLyrics(now); // 歌詞を更新
      playMelody(now); // メロディを再生

      if (Finish(now)) {
        currentState = STATE_FINISHED; // 再生終了
      }
      break;

    case STATE_FINISHED:
      stopPlayback(); // 再生を停止
      currentState = STATE_WAITING; // 待機状態に戻る
      break;

    case STATE_ERROR:
      if (btnPressed) {
        stopPlayback();
        currentState = STATE_WAITING; // エラーから復帰
      }
      break;

    default:
      showError("State Error"); // 不明な状態
      break;
  }
}

// ボタンの状態を読み取る関数
bool readButton() {
  const unsigned long now = millis();
  const int reading = digitalRead(PIN_BUTTON);

  // 生の読み値が変化した瞬間を記録（チャタリング開始点の検出）
  if (reading != lastButtonReading) {
    lastDebounceTime = now;
    lastButtonReading = reading;
  }

  // 値が一定時間安定してから正式な状態更新を行う
  if ((now - lastDebounceTime) >= DEBOUNCE_DELAY) {
    if (reading != buttonState) {
      prevButtonState = buttonState;
      buttonState = reading;

      // ボタンが押された場合（HIGH -> LOW）
      if (prevButtonState == HIGH && buttonState == LOW) {
        return true;
      }
    }
  }

  return false;
}

// 再生を開始する関数
void start(unsigned long now) {
  // 最低限のデータチェック。空配列なら再生不可
  if (lyricsCount == 0 || melodyCount == 0) {
    showError("Data Empty");
    return;
  }

  // 先頭から再生し直すため、インデックスと基準時刻をリセット
  lyricsIndex = 0;
  melodyIndex = 0;

  lastMillis_lyrics = now;
  lastMillis_Melody = now;

  showLyricsPair(lyricsIndex); // 最初の歌詞を表示

  if (melodyNotes[melodyIndex].frequency > 0) {
    tone(PIN_BUZZER, melodyNotes[melodyIndex].frequency); // 最初の音を再生
  } else {
    noTone(PIN_BUZZER); // 先頭が休符なら無音を明示
  }

  waitingScreenShown = false;
  currentState = STATE_PLAYING;
}

// 歌詞を更新する関数
void updateLyrics(unsigned long now) {
  // 全歌詞の表示が終わっていれば何もしない
  if (lyricsIndex >= lyricsCount) {
    return;
  }

  // 2行同時表示なので、次の切り替えまでの時間は「1行目+2行目」を合算
  unsigned long displayInterval = lyricEntries[lyricsIndex].intervalMs;
  if ((lyricsIndex + 1) < lyricsCount) {
    displayInterval += lyricEntries[lyricsIndex + 1].intervalMs;
  }

  if ((now - lastMillis_lyrics) >= displayInterval) {
    lyricsIndex += 2; // 次の2行に進む
    lastMillis_lyrics = now;

    if (lyricsIndex < lyricsCount) {
      showLyricsPair(lyricsIndex); // 次の歌詞を表示
    }
  }
}

// 2行の歌詞をLCDに表示する関数
void showLyricsPair(size_t firstIndex) {
  lcd.clear();

  // デモ時にLCDが見えづらくても追えるよう、同じ内容をシリアルにも出す
  Serial.println("[Lyrics]");

  if (firstIndex < lyricsCount) {
    lcd.setCursor(0, 0);
    lcd.print(lyricEntries[firstIndex].text); // 1行目を表示
    Serial.print("1: ");
    Serial.println(lyricEntries[firstIndex].text); // 1行目をシリアルにも出力
  }

  if ((firstIndex + 1) < lyricsCount) {
    lcd.setCursor(0, 1);
    lcd.print(lyricEntries[firstIndex + 1].text); // 2行目を表示
    Serial.print("2: ");
    Serial.println(lyricEntries[firstIndex + 1].text); // 2行目をシリアルにも出力
  }

  Serial.println();
}

// メロディを再生する関数
void playMelody(unsigned long now) {
  // 全音符の再生が終わっていれば何もしない
  if (melodyIndex >= melodyCount) {
    return;
  }

  // 現在の音の長さが経過したら、次の音へ進む
  if ((now - lastMillis_Melody) >= melodyNotes[melodyIndex].durationMs) {
    melodyIndex++;
    lastMillis_Melody = now;

    if (melodyIndex < melodyCount) {
      if (melodyNotes[melodyIndex].frequency > 0) {
        tone(PIN_BUZZER, melodyNotes[melodyIndex].frequency); // 次の音を再生
      } else {
        noTone(PIN_BUZZER); // 無音
      }
    } else {
      noTone(PIN_BUZZER); // 再生終了
    }
  }
}

// 再生が終了したか確認する関数
bool Finish(unsigned long now) {
  (void)now;
  // 歌詞とメロディが両方終わったら完了
  const bool lyricsDone = (lyricsIndex >= lyricsCount);
  const bool melodyDone = (melodyIndex >= melodyCount);

  // 開発時の安全チェック（インデックスが配列外に出たらエラー）
  if (lyricsIndex > lyricsCount || melodyIndex > melodyCount) {
    showError("Index Error");
    return false;
  }

  return lyricsDone && melodyDone;
}

// 再生を停止する関数
void stopPlayback() {
  noTone(PIN_BUZZER);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Press Button");

  lyricsIndex = 0;
  melodyIndex = 0;
  // loop()内で待機画面を再描画しないようにフラグを立てる
  waitingScreenShown = true;
}

// エラーを表示する関数
void showError(const char* message) {
  // エラー時は音を止め、原因をLCDに表示してユーザーに知らせる
  noTone(PIN_BUZZER);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ERROR");
  lcd.setCursor(0, 1);
  lcd.print(message);
  currentState = STATE_ERROR;
}