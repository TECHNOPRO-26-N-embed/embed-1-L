#include <LiquidCrystal.h>

// LCDを使うためのライブラリを読み込みます。
// 16x2の文字LCDに文字を表示するために使います。

LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// ==============================
// 定数
// ==============================
// ボタンをつないでいるピン番号
const int PIN_BUTTON = 2;
// チャタリング除去用の時間（ms）。
// ボタンがこの時間だけ安定してから「押された」と判定します。
const unsigned long BUTTON_DEBOUNCE_MS = 30;
// LCDの横幅（列数）
const int LCD_COLS = 16;
// LCDの縦幅（行数）
const int LCD_ROWS = 2;
// 縦向き座標の最上段。16列なので 0..15 の 15 が最上段。
const int VERTICAL_TOP = LCD_COLS - 1;  // 縦向き座標の最上段(y=15)
// 縦向き座標の最下段
const int VERTICAL_BOTTOM = 0;          // 縦向き座標の最下段(y=0)
// 障害物が1段落ちる間隔（秒）
const unsigned long OBSTACLE_FALL_SEC = 1;   // スクロール速度（小さいほど速い）
// 秒をミリ秒へ変換した実際の比較用値
const unsigned long OBSTACLE_FALL_MS = OBSTACLE_FALL_SEC * 1000UL;
// 障害物を新しく作る間隔（ms）
const unsigned long OBSTACLE_SPAWN_MS = 1000; // 生成間隔（1段空きに見せるため落下周期に合わせる）
// 画面を描き直す間隔（ms）
const unsigned long DRAW_FRAME_MS = 60;

// ==============================
// グローバル変数（状態を記録する値）
// ==============================
// プレイヤーのいるレーン（0:左列, 1:右列）
int playerLane = 0;
// ボタンの前回値（生の値）。INPUT_PULLUPなので、押すとLOWになります。
bool prevRawButtonState = HIGH;
// デバウンス後に確定したボタン状態
bool stableButtonState = HIGH;
// ボタン状態が変わった時刻（millis）
unsigned long lastButtonChangeTime = 0;
// 最後に描いたプレイヤー位置（将来の差分描画用）
int lastDrawnLane = -1;
// 障害物マップ。 obstaclePos[レーン][高さ]
// 値が1なら障害物あり、0なら空き。
int obstaclePos[LCD_ROWS][LCD_COLS] = {0};
// 障害物を最後に落下させた時刻
unsigned long lastObstacleFallTime = 0;
// 障害物を最後に生成した時刻
unsigned long lastObstacleSpawnTime = 0;
// 画面を最後に描画した時刻
unsigned long lastDrawTime = 0;
// 直前に生成した障害物のレーン
int lastSpawnLane = -1;
// 同じレーンに連続生成した回数
int consecutiveSpawnCount = 0;

// ボタン入力を読む関数
// 押した瞬間だけ true を返し、押しっぱなしでは連続 true にならないようにします。
bool readButton() {
  // ボタンの現在値を読む（INPUT_PULLUPなので、押すとLOW）
  bool rawButtonState = digitalRead(PIN_BUTTON);

  // 生の値に変化があったら、変化時刻を記録
  if (rawButtonState != prevRawButtonState) {
    lastButtonChangeTime = millis();
    prevRawButtonState = rawButtonState;
  }

  // 一定時間安定していて、かつ確定状態が変わったときに入力確定
  if ((millis() - lastButtonChangeTime) >= BUTTON_DEBOUNCE_MS &&
      rawButtonState != stableButtonState) {
    stableButtonState = rawButtonState;
    // LOWになった瞬間（押下イベント）だけtrue
    if (stableButtonState == LOW) {
      return true;
    }
  }

  return false;
}

// プレイヤーのレーンを更新する関数
// ボタンが押されたときだけ 0<->1 を切り替えます。
void updatePlayer(bool pressed) {
  if (!pressed) {
    return;
  }

  // 想定外の値が入っていたら安全のため0へ戻す
  if (playerLane < 0 || playerLane > 1) {
    playerLane = 0;
    return;
  }

  // 0なら1へ、1なら0へ切り替える
  playerLane = 1 - playerLane;
}

// 指定した高さ y に障害物があるかを確認する関数
// 2レーンのどちらかに障害物があれば true。
bool hasObstacleAtHeight(int y) {
  // 画面外の高さは false
  if (y < VERTICAL_BOTTOM || y > VERTICAL_TOP) {
    return false;
  }

  for (int lane = 0; lane < LCD_ROWS; lane++) {
    if (obstaclePos[lane][y] == 1) {
      return true;
    }
  }

  return false;
}

// 障害物を生成する関数
// ルールに従って、最上段(y=15)のどちらかのレーンに1つだけ生成します。
void spawnObstacle() {
  // 生成する高さ（最上段固定）
  int spawnY = VERTICAL_TOP;

  // 生成予定位置の前後1段（両レーン）に障害物がある場合は生成しない
  if (hasObstacleAtHeight(spawnY + 1) || hasObstacleAtHeight(spawnY - 1)) {
    return;
  }

  // 最上段で2レーン同時に埋まらないよう、すでに最上段に障害物があれば生成しない
  if (hasObstacleAtHeight(spawnY)) {
    return;
  }

  // まずはランダムでレーン選択
  int lane = random(0, LCD_ROWS);

  // 片側に3連続で出ているとき、次は反対側へ強制する
  if (lastSpawnLane == lane && consecutiveSpawnCount >= 3) {
    lane = 1 - lane;
  }

  // 選んだレーンが埋まっている場合は反対レーンを試す
  if (obstaclePos[lane][spawnY] == 1) {
    lane = 1 - lane;
  }

  if (obstaclePos[lane][spawnY] == 1) {
    return;
  }

  // 生成を確定
  obstaclePos[lane][spawnY] = 1;

  // 連続生成回数の更新
  if (lane == lastSpawnLane) {
    consecutiveSpawnCount++;
  } else {
    lastSpawnLane = lane;
    consecutiveSpawnCount = 1;
  }
}

// 障害物を1段下へ移動する関数
// 例: (lane, 15) -> (lane, 14)
void updateObstacles() {
  for (int lane = 0; lane < LCD_ROWS; lane++) {
    // 最下段(y=0)の障害物は画面外へ消える
    obstaclePos[lane][VERTICAL_BOTTOM] = 0;

    // 上側から下側へ順にコピーして1段落とす
    for (int y = VERTICAL_BOTTOM + 1; y <= VERTICAL_TOP; y++) {
      obstaclePos[lane][y - 1] = obstaclePos[lane][y];
    }

    // 最上段(y=15)は移動後に空ける
    obstaclePos[lane][VERTICAL_TOP] = 0;
  }
}

// 画面を描画する関数
// 障害物を描画したあと、最後にプレイヤーを描いて見やすくします。
void drawFrame() {
  for (int row = 0; row < LCD_ROWS; row++) {
    for (int col = 0; col < LCD_COLS; col++) {
      lcd.setCursor(col, row);
      if (obstaclePos[row][col] == 1) {
        lcd.print("|");
      } else {
        lcd.print(" ");
      }
    }
  }

  // プレイヤーは障害物より手前に描画する
  lcd.setCursor(0, playerLane);
  lcd.print("*");
  lastDrawnLane = playerLane;
}

// 衝突判定関数
// プレイヤーと障害物が同じ位置にいるかを確認します。
bool checkCollision() {
  // プレイヤーの現在位置を取得
  int playerRow = playerLane;
  int playerCol = 0; // プレイヤーは常に左端にいると仮定

  // プレイヤー位置に障害物があるか確認
  if (obstaclePos[playerRow][playerCol] == 1) {
    return true;
  }

  return false;
}

// ゲームオーバー画面を表示する関数
void showGameOver() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("gameover");
}

// 初期化処理（電源ON直後に1回だけ実行）
void setup() {
  // ボタンピンをプルアップ入力に設定
  pinMode(PIN_BUTTON, INPUT_PULLUP); // プルアップでボタン入力
  // LCDを 16x2 で初期化
  lcd.begin(LCD_COLS, LCD_ROWS);
  // 乱数の偏りを減らすための種設定
  randomSeed(analogRead(A0));
  // 各タイマーの開始時刻をそろえる
  lastObstacleFallTime = millis();
  lastObstacleSpawnTime = millis();
  lastDrawTime = millis();
  // 起動時に最初の画面を表示
  drawFrame();
}

// メインループ（setup後に繰り返し実行）
void loop() {
  // 現在時刻（ms）を取得
  unsigned long now = millis();
  // ボタン入力の更新
  bool pressed = readButton();
  // プレイヤー移動
  updatePlayer(pressed);

  // 一定時間ごとに障害物を1段落とす
  if ((now - lastObstacleFallTime) >= OBSTACLE_FALL_MS) {
    updateObstacles();
    lastObstacleFallTime = now;
  }

  // 一定時間ごとに障害物を生成
  if ((now - lastObstacleSpawnTime) >= OBSTACLE_SPAWN_MS) {
    spawnObstacle();
    lastObstacleSpawnTime = now;
  }

  // 描画は別タイマーで制御してちらつきを抑える
  if ((now - lastDrawTime) >= DRAW_FRAME_MS) {
    drawFrame();
    lastDrawTime = now;
  }

  // 衝突判定を実行
  if (checkCollision()) {
    showGameOver();
    while (true) {
      // ゲームオーバー後は無限ループで停止
      delay(100);
    }
  }

  // ループを少しだけ休ませる
  delay(10);
}

