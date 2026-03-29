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
constexpr int kGridWidth = kDisplayWidth / kCellSize;
constexpr int kGridHeight = kDisplayHeight / kCellSize;
constexpr int kMaxSnakeLength = kGridWidth * kGridHeight;
constexpr uint32_t kStepIntervalMs = 140;

uint8_t gDisplayBuffer[kDisplayWidth * kPageCount];
ControllerPtr gControllers[BP32_MAX_GAMEPADS];

struct Point {
    int8_t x;
    int8_t y;
};

enum class Direction : uint8_t {
    Up,
    Down,
    Left,
    Right,
};

struct SnakeGame {
    Point body[kMaxSnakeLength];
    int length = 0;
    Point food = {0, 0};
    Direction direction = Direction::Right;
    Direction pendingDirection = Direction::Right;
    bool alive = false;
    bool started = false;
    uint32_t lastStepMs = 0;
    uint16_t score = 0;
};

SnakeGame gGame;
bool gDisplayReady = false;
bool gBuzzerReady = false;
bool gWaitingScreenDirty = true;
bool gGameOverTonePlayed = false;

constexpr uint8_t glyphFor(char c) {
    return static_cast<uint8_t>(c);
}

const uint8_t* getGlyph(char c) {
    switch (glyphFor(c)) {
        case 'A': {
            static const uint8_t glyph[] = {0x1E, 0x05, 0x05, 0x1E, 0x00};
            return glyph;
        }
        case 'C': {
            static const uint8_t glyph[] = {0x0E, 0x11, 0x11, 0x11, 0x00};
            return glyph;
        }
        case 'E': {
            static const uint8_t glyph[] = {0x1F, 0x15, 0x15, 0x11, 0x00};
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

bool isOpposite(Direction a, Direction b) {
    return (a == Direction::Up && b == Direction::Down) || (a == Direction::Down && b == Direction::Up) ||
           (a == Direction::Left && b == Direction::Right) || (a == Direction::Right && b == Direction::Left);
}

bool pointEquals(const Point& a, const Point& b) {
    return a.x == b.x && a.y == b.y;
}

bool snakeOccupies(const Point& p, int length) {
    for (int i = 0; i < length; ++i) {
        if (pointEquals(gGame.body[i], p)) {
            return true;
        }
    }
    return false;
}

void spawnFood() {
    const uint32_t start = millis();
    Point candidate{};
    do {
        candidate.x = random(0, kGridWidth);
        candidate.y = random(0, kGridHeight);
    } while (snakeOccupies(candidate, gGame.length) && millis() - start < 1000);
    gGame.food = candidate;
}

void resetGame() {
    gGame.length = 4;
    gGame.body[0] = {12, 8};
    gGame.body[1] = {11, 8};
    gGame.body[2] = {10, 8};
    gGame.body[3] = {9, 8};
    gGame.direction = Direction::Right;
    gGame.pendingDirection = Direction::Right;
    gGame.alive = true;
    gGame.started = false;
    gGame.lastStepMs = millis();
    gGame.score = 0;
    gGameOverTonePlayed = false;
    spawnFood();
}

void drawCell(const Point& p, bool filled) {
    const int x = p.x * kCellSize;
    const int y = p.y * kCellSize;
    if (filled) {
        fillRect(x, y, kCellSize, kCellSize);
    } else {
        drawRect(x, y, kCellSize, kCellSize);
    }
}

void renderWaitingScreen() {
    clearBuffer();
    drawRect(0, 0, kDisplayWidth, kDisplayHeight);
    drawText(13, 14, "AWAITING", 2);
    drawText(7, 34, "CONTROLLER", 2);
    drawText(28, 56, "SNAKE", 1);
    flushDisplay();
}

void renderGame() {
    clearBuffer();

    for (int x = 0; x < kDisplayWidth; ++x) {
        drawPixel(x, 0);
        drawPixel(x, kDisplayHeight - 1);
    }
    for (int y = 0; y < kDisplayHeight; ++y) {
        drawPixel(0, y);
        drawPixel(kDisplayWidth - 1, y);
    }

    drawText(4, 2, "S:");
    char scoreText[8];
    snprintf(scoreText, sizeof(scoreText), "%u", gGame.score);
    drawText(18, 2, scoreText);

    drawCell(gGame.food, true);

    for (int i = gGame.length - 1; i >= 0; --i) {
        drawCell(gGame.body[i], i == 0);
    }

    if (!gGame.alive) {
        fillRect(14, 22, 100, 20, false);
        drawText(22, 26, "GAME", 2);
        drawText(70, 26, "OVER", 2);
        drawText(20, 48, "A TO RESTART", 1);
    } else if (!gGame.started) {
        drawText(18, 56, "PRESS DPAD", 1);
    }

    flushDisplay();
}

void drawWaitingIfNeeded() {
    if (!gDisplayReady || !gWaitingScreenDirty) {
        return;
    }
    renderWaitingScreen();
    gWaitingScreenDirty = false;
}

void onConnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; ++i) {
        if (gControllers[i] == nullptr) {
            ControllerProperties properties = ctl->getProperties();
            Console.printf("Controller connected, index=%d, VID=0x%04x, PID=0x%04x\n", i, properties.vendor_id,
                           properties.product_id);
            gControllers[i] = ctl;
            gWaitingScreenDirty = true;
            beep(1760, 60);
            return;
        }
    }
    Console.println("Controller connected, but there are no free slots");
}

void onDisconnectedController(ControllerPtr ctl) {
    for (int i = 0; i < BP32_MAX_GAMEPADS; ++i) {
        if (gControllers[i] == ctl) {
            Console.printf("Controller disconnected from index=%d\n", i);
            gControllers[i] = nullptr;
            gWaitingScreenDirty = true;
            resetGame();
            beep(220, 180);
            return;
        }
    }
}

void updateDirectionFromController(ControllerPtr ctl) {
    if (!ctl) {
        return;
    }

    Direction candidate = gGame.pendingDirection;
    const int axisX = ctl->axisX();
    const int axisY = ctl->axisY();

    if (ctl->dpad() & DPAD_UP || axisY < -300) {
        candidate = Direction::Up;
    } else if (ctl->dpad() & DPAD_DOWN || axisY > 300) {
        candidate = Direction::Down;
    } else if (ctl->dpad() & DPAD_LEFT || axisX < -300) {
        candidate = Direction::Left;
    } else if (ctl->dpad() & DPAD_RIGHT || axisX > 300) {
        candidate = Direction::Right;
    }

    if (!isOpposite(candidate, gGame.direction)) {
        gGame.pendingDirection = candidate;
    }

    if (!gGame.started && candidate != gGame.direction) {
        gGame.started = true;
    }
    if (!gGame.started && (ctl->dpad() != 0 || abs(axisX) > 300 || abs(axisY) > 300)) {
        gGame.started = true;
    }
}

void stepGame() {
    if (!gGame.alive || !gGame.started) {
        return;
    }

    gGame.direction = gGame.pendingDirection;
    Point next = gGame.body[0];
    switch (gGame.direction) {
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

    if (next.x < 0 || next.x >= kGridWidth || next.y < 0 || next.y >= kGridHeight) {
        gGame.alive = false;
        return;
    }

    for (int i = 0; i < gGame.length; ++i) {
        if (pointEquals(next, gGame.body[i])) {
            gGame.alive = false;
            return;
        }
    }

    const bool ateFood = pointEquals(next, gGame.food);
    const int newLength = min(gGame.length + (ateFood ? 1 : 0), kMaxSnakeLength);
    for (int i = newLength - 1; i > 0; --i) {
        gGame.body[i] = gGame.body[i - 1];
    }
    gGame.body[0] = next;
    gGame.length = newLength;

    if (ateFood) {
        ++gGame.score;
        spawnFood();
        beep(1320, 35);
    }
}

void handleGamepadButtons(ControllerPtr ctl) {
    if (!ctl) {
        return;
    }

    if (!gGame.alive && ctl->a()) {
        resetGame();
        beep(880, 50);
        delay(80);
        beep(1320, 50);
    }
}

void runSnakeFrame(ControllerPtr ctl) {
    updateDirectionFromController(ctl);
    handleGamepadButtons(ctl);

    const uint32_t now = millis();
    if (now - gGame.lastStepMs >= kStepIntervalMs) {
        gGame.lastStepMs = now;
        stepGame();
    }

    if (!gGame.alive && !gGameOverTonePlayed) {
        gGameOverTonePlayed = true;
        beep(330, 120);
        delay(40);
        beep(180, 180);
    }

    renderGame();
}

}  // namespace

void setup() {
    delay(200);
    randomSeed(esp_random());

    initDisplay();
    initBuzzer();
    resetGame();

    Console.printf("Firmware: %s\n", BP32.firmwareVersion());
    const uint8_t* addr = BP32.localBdAddress();
    Console.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

    BP32.setup(&onConnectedController, &onDisconnectedController, true);
    BP32.enableVirtualDevice(false);
    BP32.enableBLEService(false);

    renderWaitingScreen();
}

void loop() {
    BP32.update();

    ControllerPtr controller = activeGamepad();
    if (!controller) {
        drawWaitingIfNeeded();
        delay(30);
        return;
    }

    gWaitingScreenDirty = true;
    runSnakeFrame(controller);
    delay(20);
}
