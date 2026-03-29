// SPDX-License-Identifier: Apache-2.0

#include "sdkconfig.h"

#include <Arduino.h>
#include <Bluepad32.h>
#include <Wire.h>

namespace {

constexpr uint8_t kOledAddress = 0x3C;
constexpr int kOledSdaPin = 21;
constexpr int kOledSclPin = 22;
constexpr int kBuzzerPin = 32;

constexpr int kDisplayWidth = 128;
constexpr int kDisplayHeight = 64;
constexpr int kPageCount = 8;
constexpr int kCellSize = 4;

constexpr int kSnakeGridWidth = kDisplayWidth / kCellSize;
constexpr int kSnakeGridHeight = kDisplayHeight / kCellSize;
constexpr int kMaxSnakeLength = kSnakeGridWidth * kSnakeGridHeight;
constexpr uint32_t kSnakeStepMs = 140;

constexpr int kShooterHudHeight = 10;
constexpr int kShooterArenaTop = kShooterHudHeight;
constexpr int kShooterArenaBottom = kDisplayHeight - 2;
constexpr int kShooterPlayerX = 10;
constexpr int kShooterPlayerWidth = 9;
constexpr int kShooterPlayerHeight = 7;
constexpr int kShooterBulletMax = 5;
constexpr int kShooterEnemyMax = 6;
constexpr int kShooterStarCount = 18;
constexpr uint32_t kShooterBulletStepMs = 35;
constexpr uint32_t kShooterEnemyStepMs = 65;
constexpr uint32_t kShooterSpawnMs = 520;
constexpr uint32_t kShooterShotCooldownMs = 140;
constexpr uint32_t kShooterStarStepMs = 90;

uint8_t gDisplayBuffer[kDisplayWidth * kPageCount];
ControllerPtr gControllers[BP32_MAX_GAMEPADS];

enum class Screen : uint8_t {
    Waiting,
    Menu,
    Snake,
    Shooter,
};

enum class MenuItem : uint8_t {
    Snake,
    Shooter,
};

enum class Direction : uint8_t {
    Up,
    Down,
    Left,
    Right,
};

struct Point {
    int8_t x;
    int8_t y;
};

struct InputState {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool a = false;
    bool b = false;
    bool start = false;
};

struct SnakeGame {
    Point body[kMaxSnakeLength];
    int length = 0;
    Point food = {0, 0};
    Direction direction = Direction::Right;
    Direction pendingDirection = Direction::Right;
    bool alive = false;
    bool started = false;
    bool gameOverTonePlayed = false;
    uint32_t lastStepMs = 0;
    uint16_t score = 0;
};

struct Bullet {
    int16_t x = 0;
    int8_t y = 0;
    bool active = false;
};

struct Enemy {
    int16_t x = 0;
    int8_t y = 0;
    int8_t w = 0;
    int8_t h = 0;
    bool active = false;
};

struct Star {
    int16_t x = 0;
    int8_t y = 0;
};

struct ShooterGame {
    int8_t playerY = 0;
    Bullet bullets[kShooterBulletMax];
    Enemy enemies[kShooterEnemyMax];
    Star stars[kShooterStarCount];
    bool alive = false;
    bool started = false;
    bool gameOverTonePlayed = false;
    uint32_t score = 0;
    uint32_t lastBulletStepMs = 0;
    uint32_t lastEnemyStepMs = 0;
    uint32_t lastSpawnMs = 0;
    uint32_t lastShotMs = 0;
    uint32_t lastStarStepMs = 0;
};

Screen gScreen = Screen::Waiting;
MenuItem gMenuItem = MenuItem::Snake;
InputState gPreviousInput;
SnakeGame gSnake;
ShooterGame gShooter;
bool gDisplayReady = false;
bool gBuzzerReady = false;
bool gScreenDirty = true;

constexpr uint8_t glyphFor(char c) {
    return static_cast<uint8_t>(c);
}

const uint8_t* getGlyph(char c) {
    switch (glyphFor(c)) {
        case 'A': {
            static const uint8_t glyph[] = {0x1E, 0x05, 0x05, 0x1E, 0x00};
            return glyph;
        }
        case 'B': {
            static const uint8_t glyph[] = {0x1F, 0x15, 0x15, 0x0A, 0x00};
            return glyph;
        }
        case 'C': {
            static const uint8_t glyph[] = {0x0E, 0x11, 0x11, 0x11, 0x00};
            return glyph;
        }
        case 'D': {
            static const uint8_t glyph[] = {0x1F, 0x11, 0x11, 0x0E, 0x00};
            return glyph;
        }
        case 'E': {
            static const uint8_t glyph[] = {0x1F, 0x15, 0x15, 0x11, 0x00};
            return glyph;
        }
        case 'F': {
            static const uint8_t glyph[] = {0x1F, 0x05, 0x05, 0x01, 0x00};
            return glyph;
        }
        case 'G': {
            static const uint8_t glyph[] = {0x0E, 0x11, 0x15, 0x1D, 0x00};
            return glyph;
        }
        case 'H': {
            static const uint8_t glyph[] = {0x1F, 0x04, 0x04, 0x1F, 0x00};
            return glyph;
        }
        case 'I': {
            static const uint8_t glyph[] = {0x11, 0x1F, 0x11, 0x00, 0x00};
            return glyph;
        }
        case 'K': {
            static const uint8_t glyph[] = {0x1F, 0x04, 0x0A, 0x11, 0x00};
            return glyph;
        }
        case 'L': {
            static const uint8_t glyph[] = {0x1F, 0x10, 0x10, 0x10, 0x00};
            return glyph;
        }
        case 'M': {
            static const uint8_t glyph[] = {0x1F, 0x02, 0x04, 0x02, 0x1F};
            return glyph;
        }
        case 'N': {
            static const uint8_t glyph[] = {0x1F, 0x02, 0x04, 0x1F, 0x00};
            return glyph;
        }
        case 'O': {
            static const uint8_t glyph[] = {0x0E, 0x11, 0x11, 0x0E, 0x00};
            return glyph;
        }
        case 'P': {
            static const uint8_t glyph[] = {0x1F, 0x05, 0x05, 0x02, 0x00};
            return glyph;
        }
        case 'R': {
            static const uint8_t glyph[] = {0x1F, 0x05, 0x0D, 0x12, 0x00};
            return glyph;
        }
        case 'S': {
            static const uint8_t glyph[] = {0x12, 0x15, 0x15, 0x09, 0x00};
            return glyph;
        }
        case 'T': {
            static const uint8_t glyph[] = {0x01, 0x1F, 0x01, 0x00, 0x00};
            return glyph;
        }
        case 'U': {
            static const uint8_t glyph[] = {0x0F, 0x10, 0x10, 0x0F, 0x00};
            return glyph;
        }
        case 'W': {
            static const uint8_t glyph[] = {0x1F, 0x08, 0x04, 0x08, 0x1F};
            return glyph;
        }
        case '0': {
            static const uint8_t glyph[] = {0x0E, 0x19, 0x15, 0x13, 0x0E};
            return glyph;
        }
        case '1': {
            static const uint8_t glyph[] = {0x00, 0x12, 0x1F, 0x10, 0x00};
            return glyph;
        }
        case '2': {
            static const uint8_t glyph[] = {0x12, 0x19, 0x15, 0x12, 0x00};
            return glyph;
        }
        case '3': {
            static const uint8_t glyph[] = {0x11, 0x15, 0x15, 0x0A, 0x00};
            return glyph;
        }
        case '4': {
            static const uint8_t glyph[] = {0x07, 0x04, 0x04, 0x1F, 0x04};
            return glyph;
        }
        case '5': {
            static const uint8_t glyph[] = {0x17, 0x15, 0x15, 0x09, 0x00};
            return glyph;
        }
        case '6': {
            static const uint8_t glyph[] = {0x0E, 0x15, 0x15, 0x08, 0x00};
            return glyph;
        }
        case '7': {
            static const uint8_t glyph[] = {0x01, 0x01, 0x1D, 0x03, 0x00};
            return glyph;
        }
        case '8': {
            static const uint8_t glyph[] = {0x0A, 0x15, 0x15, 0x0A, 0x00};
            return glyph;
        }
        case '9': {
            static const uint8_t glyph[] = {0x02, 0x15, 0x15, 0x0E, 0x00};
            return glyph;
        }
        case ':': {
            static const uint8_t glyph[] = {0x00, 0x0A, 0x00, 0x00, 0x00};
            return glyph;
        }
        case '>': {
            static const uint8_t glyph[] = {0x00, 0x11, 0x0A, 0x04, 0x00};
            return glyph;
        }
        case ' ': {
            static const uint8_t glyph[] = {0x00, 0x00, 0x00, 0x00, 0x00};
            return glyph;
        }
        default: {
            static const uint8_t glyph[] = {0x1F, 0x11, 0x11, 0x1F, 0x00};
            return glyph;
        }
    }
}

void oledCommand(uint8_t command) {
    Wire.beginTransmission(kOledAddress);
    Wire.write(0x00);
    Wire.write(command);
    Wire.endTransmission();
}

void oledData(const uint8_t* data, size_t len) {
    Wire.beginTransmission(kOledAddress);
    Wire.write(0x40);
    for (size_t i = 0; i < len; ++i) {
        Wire.write(data[i]);
    }
    Wire.endTransmission();
}

void clearBuffer() {
    memset(gDisplayBuffer, 0, sizeof(gDisplayBuffer));
}

void drawPixel(int x, int y, bool on = true) {
    if (x < 0 || x >= kDisplayWidth || y < 0 || y >= kDisplayHeight) {
        return;
    }
    const int index = x + (y / 8) * kDisplayWidth;
    const uint8_t mask = 1U << (y & 7);
    if (on) {
        gDisplayBuffer[index] |= mask;
    } else {
        gDisplayBuffer[index] &= ~mask;
    }
}

void fillRect(int x, int y, int w, int h, bool on = true) {
    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            drawPixel(x + dx, y + dy, on);
        }
    }
}

void drawRect(int x, int y, int w, int h) {
    for (int dx = 0; dx < w; ++dx) {
        drawPixel(x + dx, y);
        drawPixel(x + dx, y + h - 1);
    }
    for (int dy = 0; dy < h; ++dy) {
        drawPixel(x, y + dy);
        drawPixel(x + w - 1, y + dy);
    }
}

void drawChar(int x, int y, char c, uint8_t scale = 1) {
    const uint8_t* glyph = getGlyph(c);
    for (int col = 0; col < 5; ++col) {
        const uint8_t bits = glyph[col];
        for (int row = 0; row < 7; ++row) {
            if (bits & (1U << row)) {
                fillRect(x + col * scale, y + row * scale, scale, scale);
            }
        }
    }
}

void drawText(int x, int y, const char* text, uint8_t scale = 1) {
    while (*text) {
        drawChar(x, y, *text, scale);
        x += 6 * scale;
        ++text;
    }
}

void flushDisplay() {
    for (uint8_t page = 0; page < kPageCount; ++page) {
        oledCommand(0xB0 + page);
        oledCommand(0x02);
        oledCommand(0x10);
        oledData(&gDisplayBuffer[page * kDisplayWidth], kDisplayWidth);
    }
}

void initDisplay() {
    Wire.begin(kOledSdaPin, kOledSclPin);
    delay(50);
    oledCommand(0xAE);
    oledCommand(0xD5);
    oledCommand(0x80);
    oledCommand(0xA8);
    oledCommand(0x3F);
    oledCommand(0xD3);
    oledCommand(0x00);
    oledCommand(0x40);
    oledCommand(0xAD);
    oledCommand(0x8B);
    oledCommand(0xA1);
    oledCommand(0xC8);
    oledCommand(0xDA);
    oledCommand(0x12);
    oledCommand(0x81);
    oledCommand(0x7F);
    oledCommand(0xD9);
    oledCommand(0x22);
    oledCommand(0xDB);
    oledCommand(0x20);
    oledCommand(0xA4);
    oledCommand(0xA6);
    oledCommand(0xAF);
    clearBuffer();
    flushDisplay();
    gDisplayReady = true;
}

void initBuzzer() {
    gBuzzerReady = ledcAttach(kBuzzerPin, 2000, 10);
    if (gBuzzerReady) {
        ledcWriteTone(kBuzzerPin, 0);
    }
}

void beep(uint16_t freq, uint16_t durationMs) {
    if (!gBuzzerReady) {
        return;
    }
    ledcWriteTone(kBuzzerPin, freq);
    delay(durationMs);
    ledcWriteTone(kBuzzerPin, 0);
}

ControllerPtr activeGamepad() {
    for (auto controller : gControllers) {
        if (controller && controller->isConnected() && controller->isGamepad()) {
            return controller;
        }
    }
    return nullptr;
}

InputState readInput(ControllerPtr ctl) {
    InputState input;
    if (!ctl) {
        return input;
    }
    const int axisX = ctl->axisX();
    const int axisY = ctl->axisY();
    input.up = (ctl->dpad() & DPAD_UP) || axisY < -300;
    input.down = (ctl->dpad() & DPAD_DOWN) || axisY > 300;
    input.left = (ctl->dpad() & DPAD_LEFT) || axisX < -300;
    input.right = (ctl->dpad() & DPAD_RIGHT) || axisX > 300;
    input.a = ctl->a();
    input.b = ctl->b();
    input.start = ctl->miscButtons() != 0;
    return input;
}

bool pressed(bool current, bool previous) {
    return current && !previous;
}

void setScreen(Screen screen) {
    gScreen = screen;
    gScreenDirty = true;
    gPreviousInput = {};
}

bool pointEquals(const Point& a, const Point& b) {
    return a.x == b.x && a.y == b.y;
}

bool isOpposite(Direction a, Direction b) {
    return (a == Direction::Up && b == Direction::Down) || (a == Direction::Down && b == Direction::Up) ||
           (a == Direction::Left && b == Direction::Right) || (a == Direction::Right && b == Direction::Left);
}

bool snakeOccupies(const Point& p, int length) {
    for (int i = 0; i < length; ++i) {
        if (pointEquals(gSnake.body[i], p)) {
            return true;
        }
    }
    return false;
}

void spawnSnakeFood() {
    Point candidate{};
    do {
        candidate.x = random(0, kSnakeGridWidth);
        candidate.y = random(0, kSnakeGridHeight);
    } while (snakeOccupies(candidate, gSnake.length));
    gSnake.food = candidate;
}

void resetSnake() {
    gSnake.length = 4;
    gSnake.body[0] = {12, 8};
    gSnake.body[1] = {11, 8};
    gSnake.body[2] = {10, 8};
    gSnake.body[3] = {9, 8};
    gSnake.direction = Direction::Right;
    gSnake.pendingDirection = Direction::Right;
    gSnake.alive = true;
    gSnake.started = false;
    gSnake.gameOverTonePlayed = false;
    gSnake.lastStepMs = millis();
    gSnake.score = 0;
    spawnSnakeFood();
}

void drawSnakeCell(const Point& p, bool filled) {
    const int x = p.x * kCellSize;
    const int y = p.y * kCellSize;
    if (filled) {
        fillRect(x, y, kCellSize, kCellSize);
    } else {
        drawRect(x, y, kCellSize, kCellSize);
    }
}

void renderSnake() {
    clearBuffer();
    drawRect(0, 0, kDisplayWidth, kDisplayHeight);
    drawText(4, 2, "S:");
    char scoreText[8];
    snprintf(scoreText, sizeof(scoreText), "%u", gSnake.score);
    drawText(18, 2, scoreText);
    drawSnakeCell(gSnake.food, true);
    for (int i = gSnake.length - 1; i >= 0; --i) {
        drawSnakeCell(gSnake.body[i], i == 0);
    }
    if (!gSnake.alive) {
        drawText(22, 22, "GAME", 2);
        drawText(70, 22, "OVER", 2);
        drawText(12, 48, "A RESTART B BACK", 1);
    } else if (!gSnake.started) {
        drawText(18, 56, "PRESS DPAD", 1);
    }
    flushDisplay();
}

void updateSnakeDirection(const InputState& input) {
    Direction candidate = gSnake.pendingDirection;
    if (input.up) {
        candidate = Direction::Up;
    } else if (input.down) {
        candidate = Direction::Down;
    } else if (input.left) {
        candidate = Direction::Left;
    } else if (input.right) {
        candidate = Direction::Right;
    }
    if (!isOpposite(candidate, gSnake.direction)) {
        gSnake.pendingDirection = candidate;
    }
    if (!gSnake.started && (input.up || input.down || input.left || input.right)) {
        gSnake.started = true;
    }
}

void stepSnake() {
    if (!gSnake.alive || !gSnake.started) {
        return;
    }
    gSnake.direction = gSnake.pendingDirection;
    Point next = gSnake.body[0];
    switch (gSnake.direction) {
        case Direction::Up:
            --next.y;
            break;
        case Direction::Down:
            ++next.y;
            break;
        case Direction::Left:
            --next.x;
            break;
        case Direction::Right:
            ++next.x;
            break;
    }
    if (next.x < 0 || next.x >= kSnakeGridWidth || next.y < 0 || next.y >= kSnakeGridHeight) {
        gSnake.alive = false;
        return;
    }
    for (int i = 0; i < gSnake.length; ++i) {
        if (pointEquals(next, gSnake.body[i])) {
            gSnake.alive = false;
            return;
        }
    }
    const bool ateFood = pointEquals(next, gSnake.food);
    const int newLength = min(gSnake.length + (ateFood ? 1 : 0), kMaxSnakeLength);
    for (int i = newLength - 1; i > 0; --i) {
        gSnake.body[i] = gSnake.body[i - 1];
    }
    gSnake.body[0] = next;
    gSnake.length = newLength;
    if (ateFood) {
        ++gSnake.score;
        spawnSnakeFood();
        beep(1320, 35);
    }
}

void runSnake(const InputState& input) {
    if (pressed(input.b, gPreviousInput.b) || pressed(input.start, gPreviousInput.start)) {
        setScreen(Screen::Menu);
        return;
    }
    if (!gSnake.alive && pressed(input.a, gPreviousInput.a)) {
        resetSnake();
        beep(880, 50);
        delay(70);
        beep(1320, 50);
    }
    updateSnakeDirection(input);
    const uint32_t now = millis();
    if (now - gSnake.lastStepMs >= kSnakeStepMs) {
        gSnake.lastStepMs = now;
        stepSnake();
    }
    if (!gSnake.alive && !gSnake.gameOverTonePlayed) {
        gSnake.gameOverTonePlayed = true;
        beep(330, 120);
        delay(40);
        beep(180, 180);
    }
    renderSnake();
}

bool rectsOverlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

void resetShooter() {
    gShooter.playerY = (kShooterArenaTop + kShooterArenaBottom - kShooterPlayerHeight) / 2;
    gShooter.alive = true;
    gShooter.started = false;
    gShooter.gameOverTonePlayed = false;
    gShooter.score = 0;
    gShooter.lastBulletStepMs = millis();
    gShooter.lastEnemyStepMs = millis();
    gShooter.lastSpawnMs = millis();
    gShooter.lastShotMs = 0;
    gShooter.lastStarStepMs = millis();
    for (auto& bullet : gShooter.bullets) {
        bullet = {};
    }
    for (auto& enemy : gShooter.enemies) {
        enemy = {};
    }
    for (auto& star : gShooter.stars) {
        star.x = random(0, kDisplayWidth);
        star.y = random(kShooterArenaTop + 1, kShooterArenaBottom);
    }
}

void drawShooterPlayer() {
    const int x = kShooterPlayerX;
    const int y = gShooter.playerY;
    fillRect(x, y + 2, 5, 3);
    fillRect(x + 4, y + 1, 2, 5);
    drawPixel(x + 6, y);
    fillRect(x + 6, y + 2, 3, 3);
    drawPixel(x + 6, y + 6);
}

void drawShooterEnemy(const Enemy& enemy) {
    fillRect(enemy.x, enemy.y + 1, enemy.w - 1, enemy.h - 2);
    drawPixel(enemy.x + enemy.w - 1, enemy.y);
    drawPixel(enemy.x + enemy.w - 1, enemy.y + enemy.h - 1);
}

void renderShooter() {
    clearBuffer();
    drawRect(0, 0, kDisplayWidth, kDisplayHeight);
    drawText(4, 2, "S:");
    char scoreText[10];
    snprintf(scoreText, sizeof(scoreText), "%lu", static_cast<unsigned long>(gShooter.score));
    drawText(18, 2, scoreText);
    drawText(72, 2, "SHOOTER");
    for (const auto& star : gShooter.stars) {
        drawPixel(star.x, star.y);
    }
    drawRect(0, kShooterArenaTop, kDisplayWidth, kDisplayHeight - kShooterArenaTop);
    if (gShooter.alive) {
        drawShooterPlayer();
    }
    for (const auto& bullet : gShooter.bullets) {
        if (bullet.active) {
            fillRect(bullet.x, bullet.y, 4, 2);
        }
    }
    for (const auto& enemy : gShooter.enemies) {
        if (enemy.active) {
            drawShooterEnemy(enemy);
        }
    }
    if (!gShooter.alive) {
        drawText(26, 24, "GAME", 2);
        drawText(74, 24, "OVER", 2);
        drawText(18, 50, "A RESTART B BACK");
    } else if (!gShooter.started) {
        drawText(18, 50, "DPAD A FIRE B BACK");
    }
    flushDisplay();
}

void spawnShooterEnemy() {
    for (auto& enemy : gShooter.enemies) {
        if (!enemy.active) {
            enemy.active = true;
            enemy.w = random(6, 11);
            enemy.h = random(6, 11);
            enemy.x = kDisplayWidth - enemy.w - 2;
            enemy.y = random(kShooterArenaTop + 1, kShooterArenaBottom - enemy.h);
            return;
        }
    }
}

void fireShooterBullet() {
    for (auto& bullet : gShooter.bullets) {
        if (!bullet.active) {
            bullet.active = true;
            bullet.x = kShooterPlayerX + kShooterPlayerWidth;
            bullet.y = gShooter.playerY + (kShooterPlayerHeight / 2);
            beep(1760, 12);
            return;
        }
    }
}

void updateShooterStars() {
    for (auto& star : gShooter.stars) {
        --star.x;
        if (star.x <= 1) {
            star.x = kDisplayWidth - 2;
            star.y = random(kShooterArenaTop + 1, kShooterArenaBottom);
        }
    }
}

void updateShooterBullets() {
    for (auto& bullet : gShooter.bullets) {
        if (!bullet.active) {
            continue;
        }
        bullet.x += 4;
        if (bullet.x >= kDisplayWidth - 2) {
            bullet.active = false;
            continue;
        }
        for (auto& enemy : gShooter.enemies) {
            if (!enemy.active) {
                continue;
            }
            if (rectsOverlap(bullet.x, bullet.y, 4, 2, enemy.x, enemy.y, enemy.w, enemy.h)) {
                bullet.active = false;
                enemy.active = false;
                ++gShooter.score;
                beep(1320, 18);
                break;
            }
        }
    }
}

void updateShooterEnemies() {
    for (auto& enemy : gShooter.enemies) {
        if (!enemy.active) {
            continue;
        }
        enemy.x -= 2;
        if (enemy.x + enemy.w <= 1) {
            enemy.active = false;
            continue;
        }
        if (rectsOverlap(kShooterPlayerX, gShooter.playerY, kShooterPlayerWidth, kShooterPlayerHeight, enemy.x, enemy.y,
                         enemy.w, enemy.h)) {
            gShooter.alive = false;
            return;
        }
    }
}

void runShooter(const InputState& input) {
    if (pressed(input.b, gPreviousInput.b) || pressed(input.start, gPreviousInput.start)) {
        setScreen(Screen::Menu);
        return;
    }
    if (!gShooter.alive && pressed(input.a, gPreviousInput.a)) {
        resetShooter();
        beep(880, 40);
        delay(60);
        beep(1320, 40);
    }
    if (gShooter.alive) {
        if (input.up) {
            gShooter.playerY = max(kShooterArenaTop + 1, gShooter.playerY - 2);
            gShooter.started = true;
        }
        if (input.down) {
            gShooter.playerY = min(kShooterArenaBottom - kShooterPlayerHeight, gShooter.playerY + 2);
            gShooter.started = true;
        }
        const uint32_t now = millis();
        if (pressed(input.a, gPreviousInput.a) && now - gShooter.lastShotMs >= kShooterShotCooldownMs) {
            gShooter.lastShotMs = now;
            fireShooterBullet();
            gShooter.started = true;
        }
        if (now - gShooter.lastStarStepMs >= kShooterStarStepMs) {
            gShooter.lastStarStepMs = now;
            updateShooterStars();
        }
        if (now - gShooter.lastBulletStepMs >= kShooterBulletStepMs) {
            gShooter.lastBulletStepMs = now;
            updateShooterBullets();
        }
        if (gShooter.alive && now - gShooter.lastEnemyStepMs >= kShooterEnemyStepMs) {
            gShooter.lastEnemyStepMs = now;
            updateShooterEnemies();
        }
        if (gShooter.alive && now - gShooter.lastSpawnMs >= kShooterSpawnMs) {
            gShooter.lastSpawnMs = now;
            spawnShooterEnemy();
        }
    }
    if (!gShooter.alive && !gShooter.gameOverTonePlayed) {
        gShooter.gameOverTonePlayed = true;
        beep(300, 120);
        delay(40);
        beep(180, 180);
    }
    renderShooter();
}

void renderWaiting() {
    clearBuffer();
    drawRect(0, 0, kDisplayWidth, kDisplayHeight);
    drawText(13, 14, "AWAITING", 2);
    drawText(7, 34, "CONTROLLER", 2);
    drawText(16, 56, "CONNECT STADIA", 1);
    flushDisplay();
}

void renderMenu() {
    clearBuffer();
    drawRect(0, 0, kDisplayWidth, kDisplayHeight);
    drawText(28, 6, "SELECT", 2);
    drawText(40, 22, "GAME", 2);
    drawText(16, 44, gMenuItem == MenuItem::Snake ? "> SNAKE" : "  SNAKE", 1);
    drawText(16, 54, gMenuItem == MenuItem::Shooter ? "> SHOOTER" : "  SHOOTER", 1);
    flushDisplay();
}

void onConnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; ++i) {
        if (gControllers[i] == nullptr) {
            ControllerProperties properties = ctl->getProperties();
            Console.printf("Controller connected, index=%d, VID=0x%04x, PID=0x%04x\n", i, properties.vendor_id,
                           properties.product_id);
            gControllers[i] = ctl;
            beep(1760, 50);
            if (gScreen == Screen::Waiting) {
                setScreen(Screen::Menu);
            } else {
                gScreenDirty = true;
            }
            return;
        }
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; ++i) {
        if (gControllers[i] == ctl) {
            gControllers[i] = nullptr;
            Console.printf("Controller disconnected from index=%d\n", i);
            resetSnake();
            resetShooter();
            beep(220, 140);
            setScreen(Screen::Waiting);
            return;
        }
    }
}

void runMenu(const InputState& input) {
    if (pressed(input.up, gPreviousInput.up) || pressed(input.down, gPreviousInput.down)) {
        gMenuItem = (gMenuItem == MenuItem::Snake) ? MenuItem::Shooter : MenuItem::Snake;
        beep(1240, 20);
        gScreenDirty = true;
    }
    if (pressed(input.a, gPreviousInput.a)) {
        beep(1560, 30);
        if (gMenuItem == MenuItem::Snake) {
            resetSnake();
            setScreen(Screen::Snake);
        } else {
            resetShooter();
            setScreen(Screen::Shooter);
        }
    }
    if (gScreenDirty) {
        renderMenu();
        gScreenDirty = false;
    }
}

}  // namespace

void setup() {
    delay(200);
    randomSeed(esp_random());
    initDisplay();
    initBuzzer();
    resetSnake();
    resetShooter();

    Console.printf("Firmware: %s\n", BP32.firmwareVersion());
    const uint8_t* addr = BP32.localBdAddress();
    Console.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    BP32.setup(&onConnectedController, &onDisconnectedController, true);
    BP32.enableVirtualDevice(false);
    BP32.enableBLEService(false);

    renderWaiting();
}

void loop() {
    BP32.update();

    ControllerPtr controller = activeGamepad();
    if (!controller) {
        if (gScreen != Screen::Waiting || gScreenDirty) {
            setScreen(Screen::Waiting);
            renderWaiting();
            gScreenDirty = false;
        }
        delay(30);
        return;
    }

    const InputState input = readInput(controller);
    switch (gScreen) {
        case Screen::Waiting:
            setScreen(Screen::Menu);
            break;
        case Screen::Menu:
            runMenu(input);
            break;
        case Screen::Snake:
            runSnake(input);
            break;
        case Screen::Shooter:
            runShooter(input);
            break;
    }

    gPreviousInput = input;
    delay(20);
}
