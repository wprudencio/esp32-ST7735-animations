#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    3
#define TFT_MOSI  2
#define TFT_SCLK  1

#define TOUCH_PIN 0
#define LED_PIN   10
#define NUMPIXELS 1

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

#define WIDTH  160
#define HEIGHT 128
#define CX (WIDTH / 2)
#define CY (HEIGHT / 2)

// ─── Player ────────────────────────
float playerX = CX;
float playerY = HEIGHT - 20;
float playerVX = 0;
#define PLAYER_SPEED 2.5
#define PLAYER_SIZE 5

// ─── Stars (3D parallax) ───────────
#define NUM_STARS 60
float starX[NUM_STARS], starY[NUM_STARS], starZ[NUM_STARS];
int prevStarX[NUM_STARS], prevStarY[NUM_STARS];

// ─── Asteroids ─────────────────────
#define MAX_ASTEROIDS 6
struct Asteroid {
  float x, y, z;     // 3D position (z=1 near, z=5 far)
  float size;
  bool active;
  int prevX, prevY, prevR;
};
Asteroid asteroids[MAX_ASTEROIDS];

// ─── Shots ─────────────────────────
#define MAX_SHOTS 4
struct Shot {
  float x, y;
  bool active;
  int prevX, prevY;
};
Shot shots[MAX_SHOTS];
int shotCooldown = 0;

// ─── State ─────────────────────────
int score = 0;
int lives = 3;
bool gameOver = false;
unsigned long gameStartTime;
bool firstFrame = true;

// ─── Touch ─────────────────────────
bool lastTouch = false;
float targetX = CX;  // where player wants to go

// ─── Colors ────────────────────────
#define COL_SHIP   0x07FF  // Cyan
#define COL_LASER  0xFFE0  // Yellow
#define COL_STAR   0xFFFF  // White
#define COL_ASTEROID 0x7BEF // Gray
#define COL_SCORE  0x07E0  // Green
#define COL_GAMEOVER 0xF800 // Red

uint16_t astColorRamp[8];

// ─── Helper: 3D projection ─────────
void project3D(float x, float y, float z, int* sx, int* sy, float* scale) {
  float d = 2.5;  // camera distance
  float p = d / (z + d);
  *sx = CX + x * p * (CX * 0.8);
  *sy = CY + y * p * (CX * 0.8);
  *scale = p;
}

// ─── Reset game ────────────────────
void resetGame() {
  playerX = CX;
  playerY = HEIGHT - 20;
  playerVX = 0;
  score = 0;
  lives = 3;
  gameOver = false;
  gameStartTime = millis();
  firstFrame = true;

  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    asteroids[i].active = false;
    asteroids[i].prevR = 0;
  }
  for (int i = 0; i < MAX_SHOTS; i++) {
    shots[i].active = false;
  }
  for (int i = 0; i < NUM_STARS; i++) {
    starZ[i] = random(10, 100) * 0.1;
    starX[i] = random(-80, 80);
    starY[i] = random(-60, 60);
    prevStarX[i] = prevStarY[i] = 0;
  }
}

// ─── Spawn asteroid ────────────────
void spawnAsteroid() {
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!asteroids[i].active) {
      asteroids[i].x = random(-50, 50);
      asteroids[i].y = random(-40, 30);
      asteroids[i].z = 5.0;  // far
      asteroids[i].active = true;
      asteroids[i].prevR = 0;
      break;
    }
  }
}

// ─── Setup ─────────────────────────
void setup() {
  pixels.begin();
  pixels.setBrightness(20);
  pixels.clear();
  pixels.show();

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST7735_BLACK);

  pinMode(TOUCH_PIN, INPUT);

  // Pre-compute asteroid ramp colors
  for (int i = 0; i < 8; i++) {
    uint8_t v = 40 + i * 25;
    astColorRamp[i] = tft.color565(v, v, v);
  }

  resetGame();
}

// ─── Main loop ─────────────────────
void loop() {
  unsigned long t = millis();

  // ─── Input ───
  bool touched = (digitalRead(TOUCH_PIN) == HIGH);
  if (touched && !lastTouch && gameOver) {
    resetGame();
  }
  lastTouch = touched;

  if (!gameOver) {
    // Touch = thrust/accelerate (move up-right), release = drift left
    if (touched) {
      targetX = CX + 25;
      playerY -= 0.3;  // slight upward thrust
      if (playerY < 15) playerY = 15;
    } else {
      targetX = CX - 15;
      playerY += 0.15;  // gravity
      if (playerY > HEIGHT - 12) playerY = HEIGHT - 12;
    }
    // Smooth follow
    playerVX += (targetX - playerX) * 0.03;
    playerVX *= 0.92;  // friction
    playerX += playerVX;
    if (playerX < PLAYER_SIZE) playerX = PLAYER_SIZE;
    if (playerX > WIDTH - PLAYER_SIZE) playerX = WIDTH - PLAYER_SIZE;

    // Auto-fire
    if (shotCooldown <= 0) {
      for (int i = 0; i < MAX_SHOTS; i++) {
        if (!shots[i].active) {
          shots[i].x = playerX;
          shots[i].y = playerY - 8;
          shots[i].active = true;
          shots[i].prevX = shots[i].prevY = 0;
          break;
        }
      }
      shotCooldown = 8;
    }
    if (shotCooldown > 0) shotCooldown--;

    // Score over time
    if (t % 30 == 0) score++;
  }

  // ─── Update stars ───
  for (int i = 0; i < NUM_STARS; i++) {
    starZ[i] -= 0.08;
    if (starZ[i] < 0.5) {
      starZ[i] = 10.0;
      starX[i] = random(-80, 80);
      starY[i] = random(-60, 60);
    }
  }

  // ─── Update asteroids ───
  if (!gameOver) {
    // Spawn
    if (random(100) < 2 && t > 2000) {
      spawnAsteroid();
    }

    for (int i = 0; i < MAX_ASTEROIDS; i++) {
      if (!asteroids[i].active) continue;
      asteroids[i].z -= 0.03;
      if (asteroids[i].z < 0.3) {
        // Missed asteroid = lose life
        asteroids[i].active = false;
        if (!gameOver) {
          lives--;
          if (lives <= 0) {
            gameOver = true;
          }
        }
        continue;
      }
    }
  }

  // ─── Update shots ───
  for (int i = 0; i < MAX_SHOTS; i++) {
    if (!shots[i].active) continue;
    shots[i].y -= 3;
    if (shots[i].y < -5) {
      shots[i].active = false;
      continue;
    }

    // Check collision with asteroids
    for (int j = 0; j < MAX_ASTEROIDS; j++) {
      if (!asteroids[j].active) continue;
      int asx, asy;
      float ascale;
      project3D(asteroids[j].x, asteroids[j].y, asteroids[j].z, &asx, &asy, &ascale);
      float aSize = 3 + (1.0 - asteroids[j].z / 5.0) * 8;
      float dx = shots[i].x - asx;
      float dy = shots[i].y - asy;
      if (dx * dx + dy * dy < aSize * aSize) {
        shots[i].active = false;
        asteroids[j].active = false;
        score += 50;
        break;
      }
    }
  }

  // ─── DRAW ────────────────────────────
  if (firstFrame) {
    tft.fillScreen(ST7735_BLACK);
    firstFrame = false;
  }

  // --- Erase everything ---
  // Stars
  for (int i = 0; i < NUM_STARS; i++) {
    if (prevStarX[i] != 0 || prevStarY[i] != 0) {
      tft.drawPixel(prevStarX[i], prevStarY[i], ST7735_BLACK);
    }
  }
  // Asteroids
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (asteroids[i].prevR > 0) {
      int asx, asy;
      float ascale;
      project3D(asteroids[i].x, asteroids[i].y, asteroids[i].z, &asx, &asy, &ascale);
      // Erase old asteroid area
      tft.fillCircle(asteroids[i].prevX, asteroids[i].prevY, asteroids[i].prevR + 1, ST7735_BLACK);
    }
  }
  // Shots
  for (int i = 0; i < MAX_SHOTS; i++) {
    if (shots[i].prevX != 0 || shots[i].prevY != 0) {
      tft.fillRect(shots[i].prevX - 1, shots[i].prevY - 2, 3, 5, ST7735_BLACK);
    }
  }
  // Player (erase old)
  int px = playerX;
  int py = playerY;
  // Simple ship: triangle
  int shipPoints[6] = { px, (int)(py - 7), px - 5, py + 3, px + 5, py + 3 };
  tft.fillTriangle(shipPoints[0], shipPoints[1], shipPoints[2], shipPoints[3],
                   shipPoints[4], shipPoints[5], ST7735_BLACK);

  // --- Draw everything ---

  // 1. Stars (depth-attenuated)
  for (int i = 0; i < NUM_STARS; i++) {
    int sx, sy;
    float sc;
    project3D(starX[i], starY[i], starZ[i], &sx, &sy, &sc);
    if (sx >= 0 && sx < WIDTH && sy >= 0 && sy < HEIGHT) {
      uint8_t b = (1.0 - starZ[i] / 10.0) * 255;
      tft.drawPixel(sx, sy, tft.color565(b, b, b));
      prevStarX[i] = sx;
      prevStarY[i] = sy;
    }
  }

  // 2. Asteroids (with 3D perspective)
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!asteroids[i].active) continue;
    int asx, asy;
    float ascale;
    project3D(asteroids[i].x, asteroids[i].y, asteroids[i].z, &asx, &asy, &ascale);
    float aSize = 3 + (1.0 - asteroids[i].z / 5.0) * 8;
    if (aSize < 1) aSize = 1;
    int r = aSize;
    int colorIdx = (1.0 - asteroids[i].z / 5.0) * 7;
    if (colorIdx < 0) colorIdx = 0;
    if (colorIdx > 7) colorIdx = 7;
    tft.fillCircle(asx, asy, r, astColorRamp[colorIdx]);
    tft.drawCircle(asx, asy, r, tft.color565(180, 180, 180));
    asteroids[i].prevX = asx;
    asteroids[i].prevY = asy;
    asteroids[i].prevR = r;

    // Collision with player
    float dx = asx - playerX;
    float dy = asy - playerY;
    if (dx * dx + dy * dy < (r + PLAYER_SIZE) * (r + PLAYER_SIZE)) {
      asteroids[i].active = false;
      lives--;
      if (lives <= 0) {
        gameOver = true;
      }
    }
  }

  // 3. Shots
  for (int i = 0; i < MAX_SHOTS; i++) {
    if (!shots[i].active) continue;
    tft.fillRect(shots[i].x - 1, shots[i].y - 2, 3, 5, COL_LASER);
    shots[i].prevX = shots[i].x;
    shots[i].prevY = shots[i].y;
  }

  // 4. Player ship
  {
    int sx = playerX;
    int sy = playerY;
    int shipPts[6] = { sx, (int)(sy - 7), sx - 5, sy + 3, sx + 5, sy + 3 };
    tft.fillTriangle(shipPts[0], shipPts[1], shipPts[2], shipPts[3],
                     shipPts[4], shipPts[5], COL_SHIP);
    // Engine glow
    if (touched) {
      tft.fillRect(sx - 2, sy + 3, 4, 3, tft.color565(255, 100, 0));
    }
  }

  // 5. HUD (score + lives) — always redraw
  tft.fillRect(0, 0, WIDTH, 10, ST7735_BLACK);
  tft.setTextColor(COL_SCORE, ST7735_BLACK);
  tft.setTextSize(1);
  tft.setCursor(2, 1);
  tft.print("SCORE:");
  tft.print(score);

  // Lives as small triangles
  for (int i = 0; i < lives; i++) {
    int lx = WIDTH - 15 - i * 12;
    tft.fillTriangle(lx, 3, lx - 3, 10, lx + 3, 10, COL_SHIP);
  }

  // 6. Game Over overlay
  if (gameOver) {
    tft.fillRect(0, HEIGHT / 2 - 15, WIDTH, 40, ST7735_BLACK);
    tft.setTextColor(COL_GAMEOVER, ST7735_BLACK);
    tft.setTextSize(2);
    tft.setCursor(25, HEIGHT / 2 - 12);
    tft.print("GAME OVER");
    tft.setTextSize(1);
    tft.setTextColor(COL_SCORE, ST7735_BLACK);
    tft.setCursor(30, HEIGHT / 2 + 8);
    tft.print("Touch to restart");
  }

  // ─── WS2812 ───
  if (!gameOver) {
    float beat = (sin(t * 0.005) + 1) * 0.5;
    pixels.setPixelColor(0, pixels.Color(0, (uint8_t)(beat * 80), (uint8_t)(beat * 255)));
  } else {
    pixels.setPixelColor(0, pixels.Color(255, 0, 0));
  }
  pixels.show();

  delay(20);
}
