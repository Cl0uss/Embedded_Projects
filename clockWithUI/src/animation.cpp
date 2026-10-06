#include "connector.h"


// ======================================================
// SETTINGS
// ======================================================

constexpr float TWO_PI_F = 6.28318530718f;

constexpr int HEART_COUNT = 7;
constexpr int STAR_COUNT = 12;


// ======================================================
// HEART BITMAP
// ======================================================

constexpr int HEART_WIDTH = 11;
constexpr int HEART_HEIGHT = 9;

static const uint16_t HEART_BITMAP[HEART_HEIGHT] = {
    0b00110001100,
    0b01111011110,
    0b11111111111,
    0b11111111111,
    0b01111111110,
    0b00111111100,
    0b00011111000,
    0b00001110000,
    0b00000100000
};


// ======================================================
// STAR BITMAP
// ======================================================

constexpr int STAR_WIDTH = 9;
constexpr int STAR_HEIGHT = 9;

static const uint16_t STAR_BITMAP[STAR_HEIGHT] = {
    0b000010000,
    0b100010001,
    0b010010010,
    0b001111100,
    0b111111111,
    0b001111100,
    0b010010010,
    0b100010001,
    0b000010000
};


// ======================================================
// PARTICLES
// ======================================================

struct ParticlePoint
{
    int x;
    int y;
};

static ParticlePoint previousHearts[HEART_COUNT];
static ParticlePoint currentHearts[HEART_COUNT];

static ParticlePoint previousStars[STAR_COUNT];
static ParticlePoint currentStars[STAR_COUNT];

static bool havePreviousFrame = false;


// ======================================================
// FONT
// ======================================================

static const uint8_t* activeAnimationFont = ANIMATION_FONT_LARGE;


// ======================================================
// TEXT BADGE POSITION ON PHYSICAL SCREEN
// ======================================================

static int badgeScreenX = 0;
static int badgeScreenY = 0;

static int badgeScreenWidth = 0;
static int badgeScreenHeight = 0;


// ======================================================
// COLOR
// ======================================================

uint16_t colorWheel(uint8_t position)
{
    position = 255 - position;

    if (position < 85) {
        return tft.color565(
            255 - position * 3,
            0,
            position * 3
        );
    }

    if (position < 170) {
        position -= 85;

        return tft.color565(
            0,
            position * 3,
            255 - position * 3
        );
    }

    position -= 170;

    return tft.color565(
        position * 3,
        255 - position * 3,
        0
    );
}


// ======================================================
// ROUNDED RECTANGLE MASK
//
// Проверяет, находится ли конкретный пиксель
// внутри настоящей формы нашей скругленной рамки.
//
// Благодаря этому сердечки исчезают именно под
// визуальной рамкой, а не под невидимым прямоугольником.
// ======================================================

static bool isInsideRoundedBadge(int x, int y)
{
    int left = badgeScreenX;
    int top = badgeScreenY;

    int right = left + badgeScreenWidth - 1;
    int bottom = top + badgeScreenHeight - 1;

    if (x < left || x > right || y < top || y > bottom) {
        return false;
    }

    int radius = TEXT_BADGE_RADIUS;

    // Центральная вертикальная область.
    if (x >= left + radius && x <= right - radius) {
        return true;
    }

    // Центральная горизонтальная область.
    if (y >= top + radius && y <= bottom - radius) {
        return true;
    }

    // Четыре скругленных угла.
    int cornerX;
    int cornerY;

    if (x < left + radius) {
        cornerX = left + radius;
    } else {
        cornerX = right - radius;
    }

    if (y < top + radius) {
        cornerY = top + radius;
    } else {
        cornerY = bottom - radius;
    }

    int dx = x - cornerX;
    int dy = y - cornerY;

    return dx * dx + dy * dy <= radius * radius;
}


// ======================================================
// BITMAP HELPERS
// ======================================================

static bool bitmapPixel(
    const uint16_t* bitmap,
    int width,
    int height,
    int x,
    int y
)
{
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return false;
    }

    uint16_t mask = 1U << (width - 1 - x);

    return (bitmap[y] & mask) != 0;
}


static bool shapeContainsScreenPixel(
    const uint16_t* bitmap,
    int width,
    int height,
    int centerX,
    int centerY,
    int screenX,
    int screenY
)
{
    int left = centerX - width / 2;
    int top = centerY - height / 2;

    int localX = screenX - left;
    int localY = screenY - top;

    return bitmapPixel(
        bitmap,
        width,
        height,
        localX,
        localY
    );
}


// ======================================================
// DRAW PARTICLE SHAPE
// ======================================================

static void drawBitmapShape(
    const uint16_t* bitmap,
    int width,
    int height,
    int centerX,
    int centerY,
    uint16_t color
)
{
    int left = centerX - width / 2;
    int top = centerY - height / 2;

    for (int y = 0; y < height; y++) {
        int screenY = top + y;

        if (screenY < 0 || screenY >= TFT_HEIGHT) {
            continue;
        }

        for (int x = 0; x < width; x++) {
            if (!bitmapPixel(bitmap, width, height, x, y)) {
                continue;
            }

            int screenX = left + x;

            if (screenX < 0 || screenX >= TFT_WIDTH) {
                continue;
            }

            // Вот здесь частица обрезается точно
            // по форме скругленной рамки.
            if (isInsideRoundedBadge(screenX, screenY)) {
                continue;
            }

            tft.drawPixel(
                screenX,
                screenY,
                color
            );
        }
    }
}


// ======================================================
// NEW PARTICLES COVER PIXEL?
// ======================================================

static bool currentParticlesCoverPixel(int x, int y)
{
    for (int i = 0; i < HEART_COUNT; i++) {
        if (shapeContainsScreenPixel(
            HEART_BITMAP,
            HEART_WIDTH,
            HEART_HEIGHT,
            currentHearts[i].x,
            currentHearts[i].y,
            x,
            y
        )) {
            return true;
        }
    }

    for (int i = 0; i < STAR_COUNT; i++) {
        if (shapeContainsScreenPixel(
            STAR_BITMAP,
            STAR_WIDTH,
            STAR_HEIGHT,
            currentStars[i].x,
            currentStars[i].y,
            x,
            y
        )) {
            return true;
        }
    }

    return false;
}


// ======================================================
// ERASE OLD PARTICLE
// ======================================================

static void eraseOldShape(
    const uint16_t* bitmap,
    int width,
    int height,
    int centerX,
    int centerY
)
{
    int left = centerX - width / 2;
    int top = centerY - height / 2;

    for (int y = 0; y < height; y++) {
        int screenY = top + y;

        if (screenY < 0 || screenY >= TFT_HEIGHT) {
            continue;
        }

        for (int x = 0; x < width; x++) {
            if (!bitmapPixel(bitmap, width, height, x, y)) {
                continue;
            }

            int screenX = left + x;

            if (screenX < 0 || screenX >= TFT_WIDTH) {
                continue;
            }

            // Никогда не стираем область текста.
            if (isInsideRoundedBadge(screenX, screenY)) {
                continue;
            }

            // Если новый объект уже занимает пиксель,
            // тоже не стираем его.
            if (currentParticlesCoverPixel(screenX, screenY)) {
                continue;
            }

            tft.drawPixel(
                screenX,
                screenY,
                GC9A01A_BLACK
            );
        }
    }
}


// ======================================================
// FONT MEASUREMENT
// ======================================================

static void measureCurrentFont()
{
    animationTextWidth = textRenderer.getUTF8Width(
        outputText.c_str()
    );

    animationTextHeight =
        textRenderer.getFontAscent()
        -
        textRenderer.getFontDescent();

    if (animationTextWidth < 1) {
        animationTextWidth = 1;
    }

    if (animationTextHeight < 1) {
        animationTextHeight = 1;
    }
}


// ======================================================
// SELECT BIGGEST FONT
// ======================================================

static void selectBestFont()
{
    int maximumWidth =
        TFT_WIDTH
        -
        TEXT_BADGE_PADDING_X * 2
        -
        8;

    activeAnimationFont = ANIMATION_FONT_LARGE;

    textRenderer.setFont(activeAnimationFont);
    measureCurrentFont();

    if (animationTextWidth <= maximumWidth) {
        return;
    }

    activeAnimationFont = ANIMATION_FONT_MEDIUM;

    textRenderer.setFont(activeAnimationFont);
    measureCurrentFont();

    if (animationTextWidth <= maximumWidth) {
        return;
    }

    activeAnimationFont = ANIMATION_FONT_SMALL;

    textRenderer.setFont(activeAnimationFont);
    measureCurrentFont();
}


// ======================================================
// CALCULATE PARTICLES
// ======================================================

static void calculateParticlePositions(float time)
{
    float centerX = TFT_WIDTH / 2.0f;
    float centerY = TFT_HEIGHT / 2.0f;

    // ==================================================
    // HEARTS
    // ==================================================

    for (int i = 0; i < HEART_COUNT; i++) {
        float direction = (i % 2 == 0) ? 1.0f : -1.0f;

        float baseRadius = 42.0f + i * 9.2f;

        float radius =
            baseRadius
            +
            sinf(
                time * (0.38f + i * 0.017f)
                +
                i * 1.31f
            ) * 7.0f;

        if (radius > 105.0f) {
            radius = 105.0f;
        }

        float angle =
            direction
            *
            time
            *
            (0.31f + i * 0.020f)
            +
            i * (TWO_PI_F / HEART_COUNT);

        currentHearts[i].x = roundf(
            centerX + cosf(angle) * radius
        );

        currentHearts[i].y = roundf(
            centerY + sinf(angle) * radius
        );
    }


    // ==================================================
    // STARS
    // ==================================================

    for (int i = 0; i < STAR_COUNT; i++) {
        float direction = (i % 2 == 0) ? -1.0f : 1.0f;

        float baseRadius = 24.0f + i * 6.5f;

        float radius =
            baseRadius
            +
            sinf(
                time * (0.46f + i * 0.013f)
                +
                i * 0.91f
            ) * 10.0f;

        radius = constrain(
            radius,
            18.0f,
            105.0f
        );

        float angle =
            direction
            *
            time
            *
            (0.23f + i * 0.014f)
            +
            i * (TWO_PI_F / STAR_COUNT);

        currentStars[i].x = roundf(
            centerX + cosf(angle) * radius
        );

        currentStars[i].y = roundf(
            centerY + sinf(angle) * radius
        );
    }
}


// ======================================================
// DRAW PARTICLES
// ======================================================

static void drawCurrentParticles(unsigned long now)
{
    for (int i = 0; i < STAR_COUNT; i++) {
        uint16_t color = colorWheel(
            now / 18 + i * 21
        );

        drawBitmapShape(
            STAR_BITMAP,
            STAR_WIDTH,
            STAR_HEIGHT,
            currentStars[i].x,
            currentStars[i].y,
            color
        );
    }

    for (int i = 0; i < HEART_COUNT; i++) {
        uint16_t color = colorWheel(
            now / 17 + i * 38
        );

        drawBitmapShape(
            HEART_BITMAP,
            HEART_WIDTH,
            HEART_HEIGHT,
            currentHearts[i].x,
            currentHearts[i].y,
            color
        );
    }
}


// ======================================================
// ERASE PREVIOUS PARTICLES
// ======================================================

static void erasePreviousParticles()
{
    if (!havePreviousFrame) {
        return;
    }

    for (int i = 0; i < HEART_COUNT; i++) {
        eraseOldShape(
            HEART_BITMAP,
            HEART_WIDTH,
            HEART_HEIGHT,
            previousHearts[i].x,
            previousHearts[i].y
        );
    }

    for (int i = 0; i < STAR_COUNT; i++) {
        eraseOldShape(
            STAR_BITMAP,
            STAR_WIDTH,
            STAR_HEIGHT,
            previousStars[i].x,
            previousStars[i].y
        );
    }
}


// ======================================================
// SAVE CURRENT POSITIONS
// ======================================================

static void saveCurrentPositions()
{
    for (int i = 0; i < HEART_COUNT; i++) {
        previousHearts[i] = currentHearts[i];
    }

    for (int i = 0; i < STAR_COUNT; i++) {
        previousStars[i] = currentStars[i];
    }

    havePreviousFrame = true;
}


// ======================================================
// SETUP
// ======================================================

bool setupAnimation()
{
    if (animationSprite != nullptr) {
        delete animationSprite;
        animationSprite = nullptr;
    }

    tft.fillScreen(GC9A01A_BLACK);

    havePreviousFrame = false;

    // Используем TFT только для измерения шрифта.
    textRenderer.begin(tft);

    textRenderer.setFontMode(1);
    textRenderer.setFontDirection(0);

    selectBestFont();

    // Sprite теперь точно совпадает с размерами badge.
    animationSpriteWidth =
        animationTextWidth
        +
        TEXT_BADGE_PADDING_X * 2;

    animationSpriteHeight =
        animationTextHeight
        +
        TEXT_BADGE_PADDING_Y * 2;

    animationSpriteWidth = min(
        animationSpriteWidth,
        TFT_WIDTH
    );

    animationSpriteHeight = min(
        animationSpriteHeight,
        TFT_HEIGHT
    );

    animationSprite = new GFXcanvas16(
        animationSpriteWidth,
        animationSpriteHeight
    );

    if (
        animationSprite == nullptr
        ||
        animationSprite->getBuffer() == nullptr
    ) {
        if (animationSprite != nullptr) {
            delete animationSprite;
            animationSprite = nullptr;
        }

        Serial.println(
            "Animation sprite allocation failed"
        );

        return false;
    }

    animationSprite->fillScreen(
        GC9A01A_BLACK
    );


    // Теперь U8g2 рисует в маленький sprite.
    textRenderer.begin(
        *animationSprite
    );

    textRenderer.setFont(
        activeAnimationFont
    );

    textRenderer.setFontMode(1);
    textRenderer.setFontDirection(0);

    textRenderer.setBackgroundColor(
        GC9A01A_BLACK
    );


    // Физические координаты badge на GC9A01.
    badgeScreenX =
        (TFT_WIDTH - animationSpriteWidth) / 2;

    badgeScreenY =
        (TFT_HEIGHT - animationSpriteHeight) / 2;

    badgeScreenWidth =
        animationSpriteWidth;

    badgeScreenHeight =
        animationSpriteHeight;


    lastAnimationFrame = micros();


    Serial.println();
    Serial.println("Animation initialized");

    Serial.print("Text: ");
    Serial.println(outputText);

    Serial.print("Text size: ");
    Serial.print(animationTextWidth);
    Serial.print(" x ");
    Serial.println(animationTextHeight);

    Serial.print("Badge size: ");
    Serial.print(animationSpriteWidth);
    Serial.print(" x ");
    Serial.println(animationSpriteHeight);

    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());

    return true;
}


// ======================================================
// TEXT LAYER
// ======================================================

static void drawTextLayer(unsigned long now)
{
    if (animationSprite == nullptr) {
        return;
    }

    animationSprite->fillScreen(
        GC9A01A_BLACK
    );


    // ==================================================
    // COLORS
    // ==================================================

    uint16_t borderColor = colorWheel(
        now / 24 + 90
    );

    uint16_t textColor = colorWheel(
        now / 22
    );


    // ==================================================
    // BADGE
    //
    // Рамка начинается прямо на границе sprite.
    // Защитная маска использует точно такую же геометрию.
    // ==================================================

    animationSprite->fillRoundRect(
        0,
        0,
        animationSpriteWidth,
        animationSpriteHeight,
        TEXT_BADGE_RADIUS,
        GC9A01A_BLACK
    );

    animationSprite->drawRoundRect(
        0,
        0,
        animationSpriteWidth,
        animationSpriteHeight,
        TEXT_BADGE_RADIUS,
        borderColor
    );

    if (
        animationSpriteWidth > 6
        &&
        animationSpriteHeight > 6
    ) {
        animationSprite->drawRoundRect(
            2,
            2,
            animationSpriteWidth - 4,
            animationSpriteHeight - 4,
            max(1, TEXT_BADGE_RADIUS - 2),
            borderColor
        );
    }


    // ==================================================
    // TEXT
    // ==================================================

    int textX =
        (animationSpriteWidth - animationTextWidth) / 2;

    int textTop =
        (animationSpriteHeight - animationTextHeight) / 2;

    int baseline =
        textTop
        +
        textRenderer.getFontAscent();


    textRenderer.setForegroundColor(
        textColor
    );

    textRenderer.setCursor(
        textX,
        baseline
    );

    textRenderer.print(
        outputText
    );


    // ==================================================
    // PUSH
    // ==================================================

    tft.drawRGBBitmap(
        badgeScreenX,
        badgeScreenY,
        animationSprite->getBuffer(),
        animationSpriteWidth,
        animationSpriteHeight
    );
}


// ======================================================
// UPDATE
// ======================================================

void updateAnimation()
{
    if (animationSprite == nullptr) {
        return;
    }

    unsigned long nowMicros = micros();

    if (
        nowMicros - lastAnimationFrame
        <
        ANIMATION_FRAME_INTERVAL_US
    ) {
        return;
    }

    lastAnimationFrame +=
        ANIMATION_FRAME_INTERVAL_US;

    unsigned long now = millis();
    float time = now / 1000.0f;


    // Новые координаты.
    calculateParticlePositions(time);


    // Сначала рисуем новое положение.
    //
    // Это уменьшает визуальный момент,
    // когда объект полностью отсутствует.
    drawCurrentParticles(now);


    // Потом удаляем остатки старого положения.
    erasePreviousParticles();


    // А текст и рамку всегда рисуем последними.
    drawTextLayer(now);


    saveCurrentPositions();
}