#include "epd_driver.h"
#include "config.h"

#if defined(EPD_PANEL_42_SSD1683_BW) || defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)

// ── Software SPI (bit-bang) for 4.2" panels ──
// Avoids Busy Timeout on ESP32-C3 with non-default pins; no GxEPD2 dependency.

static void spiWriteByte(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        digitalWrite(PIN_EPD_MOSI, (data & 0x80) ? HIGH : LOW);
        data <<= 1;
        digitalWrite(PIN_EPD_SCK, HIGH);
        digitalWrite(PIN_EPD_SCK, LOW);
    }
}

static void epdSendCommand(uint8_t cmd) {
    digitalWrite(PIN_EPD_DC, LOW);   // DC low = command
    digitalWrite(PIN_EPD_CS, LOW);
    spiWriteByte(cmd);
    digitalWrite(PIN_EPD_CS, HIGH);
}

static void epdSendData(uint8_t data) {
    digitalWrite(PIN_EPD_DC, HIGH);  // DC high = data
    digitalWrite(PIN_EPD_CS, LOW);
    spiWriteByte(data);
    digitalWrite(PIN_EPD_CS, HIGH);
}

static void epdWaitBusy(unsigned long maxMs = 0) {
    unsigned long t0 = millis();
    unsigned long timeoutMs = maxMs > 0 ? maxMs :
#if defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)
        45000;
#else
        10000;
#endif
#if defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)
    while (digitalRead(PIN_EPD_BUSY) == LOW) {
#else
    while (digitalRead(PIN_EPD_BUSY) == HIGH) {
#endif
        delay(10);
        if (millis() - t0 > timeoutMs) {
            Serial.println("EPD busy TIMEOUT!");
            return;
        }
    }
#if defined(EPD_PANEL_42_DKE_RY683)
    delay(100);
#endif
}

static void epdReset() {
#if defined(EPD_PANEL_42_GDEM042F52)
    delay(20);
    digitalWrite(PIN_EPD_RST, LOW);  delay(40);
    digitalWrite(PIN_EPD_RST, HIGH); delay(50);
#elif defined(EPD_PANEL_42_DKE_RY683)
    digitalWrite(PIN_EPD_RST, LOW);  delay(10);
    digitalWrite(PIN_EPD_RST, HIGH); delay(10);
#else
    digitalWrite(PIN_EPD_RST, HIGH); delay(100);
    digitalWrite(PIN_EPD_RST, LOW);  delay(2);
    digitalWrite(PIN_EPD_RST, HIGH); delay(100);
#endif
}

// ── Helper: configure RAM window for full screen ────────────

static void epdSetFullWindow() {
    epdSendCommand(0x11);  // Data Entry Mode Setting
    epdSendData(0x03);     //   X increment, Y increment

    epdSendCommand(0x44);  // Set RAM X address range
    epdSendData(0x00);
    epdSendData((W - 1) / 8);

    epdSendCommand(0x45);  // Set RAM Y address range
    epdSendData(0x00);
    epdSendData(0x00);
    epdSendData((H - 1) & 0xFF);
    epdSendData(((H - 1) >> 8) & 0xFF);

    epdSendCommand(0x4E);  // Set RAM X address counter
    epdSendData(0x00);

    epdSendCommand(0x4F);  // Set RAM Y address counter
    epdSendData(0x00);
    epdSendData(0x00);
}

// ── GPIO initialization ─────────────────────────────────────

void gpioInit() {
    pinMode(PIN_EPD_BUSY, INPUT);
    pinMode(PIN_EPD_RST,  OUTPUT);
    pinMode(PIN_EPD_DC,   OUTPUT);
    pinMode(PIN_EPD_CS,   OUTPUT);
    pinMode(PIN_EPD_SCK,  OUTPUT);
    pinMode(PIN_EPD_MOSI, OUTPUT);
    pinMode(PIN_CFG_BTN,  INPUT_PULLUP);
    digitalWrite(PIN_EPD_RST, HIGH);
    digitalWrite(PIN_EPD_CS,  HIGH);
    digitalWrite(PIN_EPD_SCK, LOW);
}

// ── EPD full init (standard mode) ──

void epdInit() {
#if defined(EPD_PANEL_42_DKE_RY683)
    Serial.printf("[EPD-init] begin BUSY=%d\n", digitalRead(PIN_EPD_BUSY));
    epdReset();
    epdWaitBusy();
    Serial.printf("[EPD-init] wait1 done BUSY=%d\n", digitalRead(PIN_EPD_BUSY));

    epdSendCommand(0x06);
    epdSendData(0x0F);
    epdSendData(0x8B);
    epdSendData(0x9C);
    epdSendData(0x96);

    epdSendCommand(0x00);
    epdSendData(0x2F);
    epdSendData(0x69);
    epdWaitBusy();
    Serial.printf("[EPD-init] wait2 done BUSY=%d\n", digitalRead(PIN_EPD_BUSY));

    epdSendCommand(0x01);
    epdSendData(0x07);
    epdSendData(0xF0);

    epdSendCommand(0x50);
    epdSendData(0x37);

    epdSendCommand(0x61);
    epdSendData(0x01);
    epdSendData(0x90);
    epdSendData(0x01);
    epdSendData(0x2C);

    epdSendCommand(0x62);
    epdSendData(0x64);
    epdSendData(0x53);

    epdSendCommand(0x65);
    epdSendData(0x00);
    epdSendData(0x00);
    epdSendData(0x00);
    epdSendData(0x00);

    epdSendCommand(0x30);
    epdSendData(0x08);

    epdSendCommand(0xE9);
    epdSendData(0x01);
#elif defined(EPD_PANEL_42_GDEM042F52)
    epdReset();

    epdSendCommand(0x4D);
    epdSendData(0x78);

    epdSendCommand(0x00);
    epdSendData(0x0F);
    epdSendData(0x29);

    epdSendCommand(0x06);
    epdSendData(0x0D);
    epdSendData(0x12);
    epdSendData(0x24);
    epdSendData(0x25);
    epdSendData(0x12);
    epdSendData(0x29);
    epdSendData(0x10);

    epdSendCommand(0x30);
    epdSendData(0x08);

    epdSendCommand(0x50);
    epdSendData(0x37);

    epdSendCommand(0x61);
    epdSendData(W >> 8);
    epdSendData(W & 0xFF);
    epdSendData(H >> 8);
    epdSendData(H & 0xFF);

    epdSendCommand(0xAE);
    epdSendData(0xCF);

    epdSendCommand(0xB0);
    epdSendData(0x13);

    epdSendCommand(0xBD);
    epdSendData(0x07);

    epdSendCommand(0xBE);
    epdSendData(0xFE);

    epdSendCommand(0xE9);
    epdSendData(0x01);

    epdSendCommand(0x04);
    epdWaitBusy();
#else
    epdReset();
    epdWaitBusy();

    epdSendCommand(0x12);  // Software Reset
    epdWaitBusy();

    epdSendCommand(0x21);  // Display Update Control 1
    epdSendData(0x40);     //   Source output mode
    epdSendData(0x00);

    epdSendCommand(0x3C);  // Border Waveform Control
    epdSendData(0x05);

    epdSetFullWindow();
    epdWaitBusy();
#endif
}

// ── EPD fast init (loads fast-refresh LUT via temperature register) ──
// Based on official Waveshare epd4in2_V2 Init_Fast() implementation.
// The 0x1A register sets a temperature value that selects faster internal LUT.
// 0x6E = ~1.5s refresh, 0x5A = ~1s refresh.

void epdInitFast() {
#if defined(EPD_PANEL_42_DKE_RY683)
    epdInit();
#elif defined(EPD_PANEL_42_GDEM042F52)
    delay(100);
    epdReset();
    epdWaitBusy();

    epdSendCommand(0x4D);
    epdSendData(0x78);

    epdSendCommand(0x00);
    epdSendData(0x0F);
    epdSendData(0x29);

    epdSendCommand(0x01);
    epdSendData(0x07);
    epdSendData(0x00);

    epdSendCommand(0x03);
    epdSendData(0x10);
    epdSendData(0x54);
    epdSendData(0x44);

    epdSendCommand(0x06);
    epdSendData(0x0F);
    epdSendData(0x0A);
    epdSendData(0x2F);
    epdSendData(0x25);
    epdSendData(0x22);
    epdSendData(0x2E);
    epdSendData(0x21);

    epdSendCommand(0x50);
    epdSendData(0x37);

    epdSendCommand(0x61);
    epdSendData(W >> 8);
    epdSendData(W & 0xFF);
    epdSendData(H >> 8);
    epdSendData(H & 0xFF);

    epdSendCommand(0xE3);
    epdSendData(0x22);

    epdSendCommand(0xB6);
    epdSendData(0x6F);

    epdSendCommand(0xB4);
    epdSendData(0xD0);

    epdSendCommand(0xE9);
    epdSendData(0x01);

    epdSendCommand(0x30);
    epdSendData(0x08);

    epdSendCommand(0x04);
    epdWaitBusy();

    epdSendCommand(0xE0);
    epdSendData(0x02);

    epdSendCommand(0xE6);
    epdSendData(0x5A);

    epdSendCommand(0xA5);
    epdSendData(0x00);
    epdWaitBusy();
#else
    epdReset();
    epdWaitBusy();

    epdSendCommand(0x12);  // Software Reset
    epdWaitBusy();

    epdSendCommand(0x21);  // Display Update Control 1
    epdSendData(0x40);
    epdSendData(0x00);

    epdSendCommand(0x3C);  // Border Waveform Control
    epdSendData(0x05);

    epdSendCommand(0x1A);  // Write to temperature register
    epdSendData(0x6E);     //   Value for ~1.5s fast refresh

    epdSendCommand(0x22);  // Display Update Control 2
    epdSendData(0x91);     //   Load temperature + Load LUT, then power down
    epdSendCommand(0x20);  // Master Activation
    epdWaitBusy();

    epdSetFullWindow();
    epdWaitBusy();
#endif
}

// ── EPD full-screen display (standard full refresh, 0xF7) ───
// Clears all ghosting but has visible black-white flash (~3-4s).

static uint8_t packMonoPixelGroup(const uint8_t *image, int rowBytes, int y, int x) {
    uint8_t packed = 0;
    for (int bit = 0; bit < 4; bit++) {
        int px = x + bit;
        bool isBlack = (image[y * rowBytes + px / 8] & (0x80 >> (px % 8))) == 0;
        uint8_t color = isBlack ? 0x00 : 0x01;
        packed |= color << (6 - bit * 2);
    }
    return packed;
}

#if defined(EPD_PANEL_42_GDEM042F52)
static uint8_t epdRemap2bppColor(uint8_t color) {
    return color & 0x03;
}

static void epdWriteMapped2bpp(const uint8_t *buf2bpp) {
    epdSendCommand(0x10);
    for (int i = 0; i < COLOR_BUF_LEN; i++) {
        uint8_t src = buf2bpp[i];
        uint8_t dst = 0;
        dst |= epdRemap2bppColor((src >> 6) & 0x03) << 6;
        dst |= epdRemap2bppColor((src >> 4) & 0x03) << 4;
        dst |= epdRemap2bppColor((src >> 2) & 0x03) << 2;
        dst |= epdRemap2bppColor(src & 0x03);
        epdSendData(dst);
    }
}

static void epdPowerOff() {
    epdSendCommand(0x02);
    epdSendData(0x00);
    epdWaitBusy();
}
#endif

static void epdSend2bppAndRefresh(const uint8_t *buf2bpp) {
    for (int attempt = 0; attempt < 3; attempt++) {
        unsigned long t0 = millis();
        Serial.printf("[EPD] attempt %d start BUSY=%d\n", attempt, digitalRead(PIN_EPD_BUSY));
        epdInit();
        Serial.printf("[EPD] init done %lums BUSY=%d\n", millis()-t0, digitalRead(PIN_EPD_BUSY));
#if defined(EPD_PANEL_42_GDEM042F52)
        epdWriteMapped2bpp(buf2bpp);
#else
        epdSendCommand(0x10);
        for (int i = 0; i < COLOR_BUF_LEN; i++) {
            epdSendData(buf2bpp[i]);
        }
#endif
        Serial.printf("[EPD] data done %lums\n", millis()-t0);
#if defined(EPD_PANEL_42_GDEM042F52)
        epdSendCommand(0x12);
        epdSendData(0x00);
        epdWaitBusy();
        Serial.printf("[EPD] refresh done %lums\n", millis()-t0);
        epdPowerOff();
        Serial.printf("[EPD] all done %lums\n", millis()-t0);
        return;
#else
        epdSendCommand(0x04);
        delay(100);
        if (digitalRead(PIN_EPD_BUSY) == HIGH) {
            Serial.printf("[EPD] cmd04 no response, retry\n");
            continue;
        }
        epdWaitBusy();
        Serial.printf("[EPD] power-on done %lums\n", millis()-t0);
        epdSendCommand(0x12);
        epdSendData(0x00);
        epdWaitBusy();
        Serial.printf("[EPD] refresh done %lums\n", millis()-t0);
        epdSendCommand(0x02);
        epdSendData(0x00);
        epdWaitBusy();
        Serial.printf("[EPD] all done %lums\n", millis()-t0);
        return;
#endif
    }
    Serial.println("[EPD] display failed after 3 attempts");
}

void epdDisplay(const uint8_t *image) {
#if defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)
    if (!ensureColorBuf()) { Serial.println("[EPD] colorBuf alloc failed"); return; }
    int rowBytes = W / 8;
    int out = 0;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x += 4) {
            colorBuf[out++] = packMonoPixelGroup(image, rowBytes, y, x);
        }
    }
    epdSend2bppAndRefresh(colorBuf);
#else
    epdInit();

    int w = W / 8;

    epdSendCommand(0x24);  // Write Black/White RAM
    for (int j = 0; j < H; j++)
        for (int i = 0; i < w; i++)
            epdSendData(image[i + j * w]);

    epdSendCommand(0x26);  // Write RED RAM (old data for refresh)
    for (int j = 0; j < H; j++)
        for (int i = 0; i < w; i++)
            epdSendData(image[i + j * w]);

    epdSendCommand(0x22);  // Display Update Control 2
    epdSendData(0xF7);     //   Full update sequence
    epdSendCommand(0x20);  // Activate Display Update Sequence
    epdWaitBusy();
#endif
}

void epdDisplay2bpp(const uint8_t *image2bpp) {
#if defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)
    epdSend2bppAndRefresh(image2bpp);
#else
    (void)image2bpp;
    epdDisplay(imgBuf);
#endif
}

// ── EPD deep clear (multi-cycle anti-ghosting) ──────────────
// SSD1683 BW: cycles all-black/all-white via 0x24/0x26 registers.
// 4-color panels: falls back to epdDisplay (no register-level deep clear).

void epdDisplayDeepClear(const uint8_t *image) {
#if defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)
    epdDisplay(image);
#else
    epdInit();
    int w = W / 8;
    int total = w * H;

    for (int pass = 0; pass < 4; pass++) {
        uint8_t fill = (pass % 2 == 0) ? 0x00 : 0xFF;

        epdSendCommand(0x24);
        for (int i = 0; i < total; i++)
            epdSendData(fill);

        epdSendCommand(0x26);
        for (int i = 0; i < total; i++)
            epdSendData(fill);

        epdSendCommand(0x22);
        epdSendData(0xF7);
        epdSendCommand(0x20);
        epdWaitBusy();
    }

    epdSendCommand(0x24);
    for (int j = 0; j < H; j++)
        for (int i = 0; i < w; i++)
            epdSendData(image[i + j * w]);

    epdSendCommand(0x26);
    for (int j = 0; j < H; j++)
        for (int i = 0; i < w; i++)
            epdSendData(image[i + j * w]);

    epdSendCommand(0x22);
    epdSendData(0xF7);
    epdSendCommand(0x20);
    epdWaitBusy();
#endif
}

// ── EPD full-screen display (fast refresh, 0xC7) ────────────

void epdDisplayFast(const uint8_t *image) {
#if defined(EPD_PANEL_42_GDEM042F52)
    if (!ensureColorBuf()) { Serial.println("[EPD] colorBuf alloc failed"); return; }
    int rowBytes = W / 8;
    int out = 0;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x += 4) {
            colorBuf[out++] = packMonoPixelGroup(image, rowBytes, y, x);
        }
    }
    epdInitFast();
    epdWriteMapped2bpp(colorBuf);
    epdSendCommand(0x12);
    epdSendData(0x00);
    epdWaitBusy();
    epdPowerOff();
#elif defined(EPD_PANEL_42_DKE_RY683)
    epdDisplay(image);
#else
    epdInitFast();

    int w = W / 8;

    epdSendCommand(0x24);  // Write Black/White RAM
    for (int j = 0; j < H; j++)
        for (int i = 0; i < w; i++)
            epdSendData(image[i + j * w]);

    epdSendCommand(0x26);  // Write RED RAM
    for (int j = 0; j < H; j++)
        for (int i = 0; i < w; i++)
            epdSendData(image[i + j * w]);

    epdSendCommand(0x22);  // Display Update Control 2
    epdSendData(0xC7);     //   Fast update: skip LUT load (already loaded by InitFast)
    epdSendCommand(0x20);  // Activate Display Update Sequence
    epdWaitBusy();
#endif
}

// ── EPD partial refresh ─────────────────────────────────────

void epdPartialDisplay(uint8_t *data, int xStart, int yStart, int xEnd, int yEnd) {
    epdPartialDisplayWithOld(data, nullptr, xStart, yStart, xEnd, yEnd);
}

bool epdSupportsPartialRefresh() {
#if defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)
    return false;
#else
    return true;
#endif
}

void epdPartialDisplayWithOld(uint8_t *data, const uint8_t *oldData, int xStart, int yStart, int xEnd, int yEnd) {
#if defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)
    (void)data;
    (void)oldData;
    (void)xStart;
    (void)yStart;
    (void)xEnd;
    (void)yEnd;
    epdDisplay(imgBuf);
#else
    int xS = xStart / 8;
    int xE = (xEnd - 1) / 8;
    int width = xE - xS + 1;
    int count = width * (yEnd - yStart);

    epdSendCommand(0x3C);  // Border Waveform Control
    epdSendData(0x80);

    epdSendCommand(0x21);  // Display Update Control 1
    epdSendData(0x00);
    epdSendData(0x00);

    epdSendCommand(0x3C);  // Border Waveform Control
    epdSendData(0x80);

    epdSendCommand(0x44);  // Set RAM X address range
    epdSendData(xS & 0xFF);
    epdSendData(xE & 0xFF);

    epdSendCommand(0x45);  // Set RAM Y address range
    epdSendData(yStart & 0xFF);
    epdSendData((yStart >> 8) & 0xFF);
    epdSendData((yEnd - 1) & 0xFF);
    epdSendData(((yEnd - 1) >> 8) & 0xFF);

    epdSendCommand(0x4E);  // Set RAM X address counter
    epdSendData(xS & 0xFF);

    epdSendCommand(0x4F);  // Set RAM Y address counter
    epdSendData(yStart & 0xFF);
    epdSendData((yStart >> 8) & 0xFF);

    epdSendCommand(0x24);  // Write Black/White RAM
    for (int i = 0; i < count; i++)
        epdSendData(data[i]);

    if (oldData) {
        epdSendCommand(0x4E);  // Set RAM X address counter
        epdSendData(xS & 0xFF);

        epdSendCommand(0x4F);  // Set RAM Y address counter
        epdSendData(yStart & 0xFF);
        epdSendData((yStart >> 8) & 0xFF);

        epdSendCommand(0x26);  // Write old/secondary RAM for stable partial inversion
        for (int i = 0; i < count; i++)
            epdSendData(oldData[i]);
    }

    epdSendCommand(0x22);  // Display Update Control 2
    epdSendData(0xFF);     //   Partial update sequence
    epdSendCommand(0x20);  // Activate Display Update Sequence
    epdWaitBusy();
#endif
}

// ── EPD sleep ───────────────────────────────────────────────

void epdSleep() {
#if defined(EPD_PANEL_42_DKE_RY683) || defined(EPD_PANEL_42_GDEM042F52)
    epdSendCommand(0x07);
    epdSendData(0xA5);
    delay(200);
#else
    epdSendCommand(0x10);  // Deep Sleep Mode
    epdSendData(0x01);     //   Enter deep sleep
    delay(200);
#endif
}

#elif defined(EPD_PANEL_42_WFT)
// ── WFT042 4.2" Panel (Waveshare-compatible, supports BW and tricolor via EPD_BPP) ──

#include "epd_wft.h"

// ── GPIO initialization ─────────────────────────────────────

void gpioInit() {
    pinMode(PIN_CFG_BTN, INPUT_PULLUP);
    EPD_initSPI();
}

// ── EPD full init ──

void epdInit() {
#if EPD_BPP >= 2
    EPD_dispIndex = 1; // tricolor mode
#else
    EPD_dispIndex = 0; // BW mode
#endif
    EPD_dispInit();
}

// ── EPD fast init ──

void epdInitFast() { epdInit(); }

// ── BW full refresh ──

void epdDisplay(const uint8_t *image) {
    int w = EPD_WIDTH / 8;
    epdInit();

    EPD_SendCommand(0x10);
    delay(2);
    for (int i = 0; i < w * EPD_HEIGHT; i++) {
        EPD_SendData(0xFF);
    }

    EPD_SendCommand(0x13);
    delay(2);
    for (int i = 0; i < w * EPD_HEIGHT; i++) {
        EPD_SendData(image[i]);
    }

    EPD_dispMass[EPD_dispIndex].show();
}

// ── Deep clear (multi-cycle flush) ──

void epdDisplayDeepClear(const uint8_t *image) {
    epdDisplay(image);
}

// ── Tricolor display (2bpp packed data) ──

void epdDisplay2bpp(const uint8_t *image2bpp) {
#if EPD_BPP >= 2
    int w = EPD_WIDTH / 8;
    int total = w * EPD_HEIGHT;
    epdInit();

    uint8_t *blackBuf = (uint8_t *)malloc(total);
    uint8_t *redBuf = (uint8_t *)malloc(total);
    if (!blackBuf || !redBuf) {
        Serial.println("[EPD] 2bpp buffer alloc failed");
        free(blackBuf);
        free(redBuf);
        return;
    }
    memset(blackBuf, 0xFF, total);
    memset(redBuf, 0x00, total);

    for (int y = 0; y < EPD_HEIGHT; y++) {
        for (int x = 0; x < EPD_WIDTH; x++) {
            int index = (y * EPD_WIDTH + x) / 4;
            int bitOffset = ((y * EPD_WIDTH + x) % 4) * 2;
            uint8_t color = (image2bpp[index] >> (6 - bitOffset)) & 0x03;
            if (color == 0x02) color = 0x01; // map yellow to red

            int bufIndex = y * w + (x / 8);
            int bitPos = x % 8;

            if (color == 0x00) { // Black
                blackBuf[bufIndex] &= ~(0x80 >> bitPos);
                redBuf[bufIndex] |= (0x80 >> bitPos);
            } else if (color == 0x01) { // Red
                redBuf[bufIndex] |= (0x80 >> bitPos);
                blackBuf[bufIndex] |= (0x80 >> bitPos);
            } else { // White (0x03)
                blackBuf[bufIndex] |= (0x80 >> bitPos);
                redBuf[bufIndex] &= ~(0x80 >> bitPos);
            }
        }
    }

    EPD_SendCommand(0x10);
    delay(2);
    for (int i = 0; i < total; i++) {
        EPD_SendData(blackBuf[i]);
    }

    EPD_SendCommand(0x13);
    delay(2);
    for (int i = 0; i < total; i++) {
        EPD_SendData(redBuf[i]);
    }

    free(blackBuf);
    free(redBuf);

    EPD_dispMass[EPD_dispIndex].show();
#else
    epdDisplay(image2bpp);
#endif
}

// ── Fast refresh (same as full for WFT panel) ──

void epdDisplayFast(const uint8_t *image) {
    epdDisplay(image);
}

bool epdSupportsPartialRefresh() {
    return false;
}

// ── Partial display (not supported on LG, fallback to full) ──

void epdPartialDisplay(uint8_t *data, int xStart, int yStart, int xEnd, int yEnd) {
    (void)xStart; (void)yStart; (void)xEnd; (void)yEnd;
    epdDisplay(data);
}

// ── Sleep ──

void epdPartialDisplayWithOld(uint8_t *data, const uint8_t *oldData, int xStart, int yStart, int xEnd, int yEnd) {
    (void)oldData;
    epdPartialDisplay(data, xStart, yStart, xEnd, yEnd);
}

void epdSleep() {
    EPD_SendCommand(0x02); // power off
    EPD_WaitUntilIdle();
    EPD_Send_1(0x07, 0xA5); // deep sleep
}

#elif defined(EPD_PANEL_426_GDEQ0426T82)
// ── XTEINK X4 (SSD1677 800x480) Hardware SPI Driver (adapted from Xtelink-screen) ──

#include <SPI.h>

static const SPISettings epdSpiSettings(20000000, MSBFIRST, SPI_MODE0);
static bool _x4_isScreenOn = false;
static bool _x4_needsInitialFull = true;
static bool _x4_initialized = false;

static inline void epdSendCommand(uint8_t cmd) {
    SPI.beginTransaction(epdSpiSettings);
    digitalWrite(PIN_EPD_DC, LOW);
    digitalWrite(PIN_EPD_CS, LOW);
    SPI.transfer(cmd);
    digitalWrite(PIN_EPD_CS, HIGH);
    SPI.endTransaction();
}

static inline void epdSendData(uint8_t data) {
    SPI.beginTransaction(epdSpiSettings);
    digitalWrite(PIN_EPD_DC, HIGH);
    digitalWrite(PIN_EPD_CS, LOW);
    SPI.transfer(data);
    digitalWrite(PIN_EPD_CS, HIGH);
    SPI.endTransaction();
}

static inline void epdSendDataBytes(const uint8_t *data, uint32_t len) {
    if (!data || len == 0) return;
    SPI.beginTransaction(epdSpiSettings);
    digitalWrite(PIN_EPD_DC, HIGH);
    digitalWrite(PIN_EPD_CS, LOW);
    SPI.writeBytes(data, len);
    digitalWrite(PIN_EPD_CS, HIGH);
    SPI.endTransaction();
}

static void epdWaitBusy(unsigned long timeoutMs = 30000) {
    unsigned long t0 = millis();
    while (digitalRead(PIN_EPD_BUSY) == HIGH) {
        delay(1);
        if (millis() - t0 > timeoutMs) {
            Serial.println("[EPD-X4] Busy TIMEOUT!");
            return;
        }
    }
}

static void epdReset() {
    digitalWrite(PIN_EPD_RST, HIGH);
    delay(20);
    digitalWrite(PIN_EPD_RST, LOW);
    delay(2);
    digitalWrite(PIN_EPD_RST, HIGH);
    delay(20);
}

static void epdSetRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t xLo = x;
    uint16_t xHi = x + w - 1;
    // Gates are physically reversed on this panel
    uint16_t y_internal = H - y - h;

    epdSendCommand(0x11);  // Data Entry Mode
    epdSendData(0x01);     // X increment, Y decrement

    epdSendCommand(0x44);  // Set RAM X Range
    epdSendData(xLo % 256);
    epdSendData(xLo / 256);
    epdSendData(xHi % 256);
    epdSendData(xHi / 256);

    epdSendCommand(0x45);  // Set RAM Y Range
    epdSendData((y_internal + h - 1) % 256);
    epdSendData((y_internal + h - 1) / 256);
    epdSendData(y_internal % 256);
    epdSendData(y_internal / 256);

    epdSendCommand(0x4E);  // Set RAM X Counter
    epdSendData(xLo % 256);
    epdSendData(xLo / 256);

    epdSendCommand(0x4F);  // Set RAM Y Counter
    epdSendData((y_internal + h - 1) % 256);
    epdSendData((y_internal + h - 1) / 256);
}

void gpioInit() {
#if PIN_CFG_BTN >= 0
    pinMode(PIN_CFG_BTN, INPUT_PULLUP);
#endif
    pinMode(PIN_EPD_BUSY, INPUT);
    pinMode(PIN_EPD_RST,  OUTPUT);
    pinMode(PIN_EPD_DC,   OUTPUT);
    pinMode(PIN_EPD_CS,   OUTPUT);
    pinMode(PIN_EPD_SCK,  OUTPUT);
    pinMode(PIN_EPD_MOSI, OUTPUT);

    digitalWrite(PIN_EPD_RST, HIGH);
    digitalWrite(PIN_EPD_CS,  HIGH);
    digitalWrite(PIN_EPD_DC,  HIGH);
    digitalWrite(PIN_EPD_SCK, LOW);

    SPI.begin(PIN_EPD_SCK, -1, PIN_EPD_MOSI, PIN_EPD_CS);
}

void epdInit() {
    if (_x4_initialized) return;

    epdReset();

    epdSendCommand(0x12);  // Soft reset
    epdWaitBusy();

    epdSendCommand(0x18);  // Temperature sensor control
    epdSendData(0x80);     // Internal temperature sensor

    epdSendCommand(0x0C);  // Booster soft start
    epdSendData(0xAE);
    epdSendData(0xC7);
    epdSendData(0xC3);
    epdSendData(0xC0);
    epdSendData(0x80);

    epdSendCommand(0x01);  // Driver output control
    epdSendData((H - 1) % 256);  // 0xDF
    epdSendData((H - 1) / 256);  // 0x01
    epdSendData(0x02);           // SM=1 interlaced, TB=0

    epdSendCommand(0x3C);  // Border waveform
    epdSendData(0x80);     // Init border

    epdSetRamArea(0, 0, W, H);

    epdSendCommand(0x46);  // Auto write BW RAM
    epdSendData(0xF7);
    epdWaitBusy();

    epdSendCommand(0x47);  // Auto write RED RAM
    epdSendData(0xF7);
    epdWaitBusy();

    _x4_initialized = true;
    _x4_isScreenOn = false;
    _x4_needsInitialFull = true;
}

void epdInitFast() {
    epdInit();
}

// Full / clean refresh: uses OEM stock single-pass warmed waveform (0xD7, ~1700ms)
void epdDisplay(const uint8_t *image) {
    if (!_x4_initialized) epdInit();

    const uint32_t bufSize = (uint32_t)(W / 8) * H;

    epdSetRamArea(0, 0, W, H);

    // Write BW RAM (0x24)
    epdSendCommand(0x24);
    epdSendDataBytes(image, bufSize);

    // Write RED RAM (0x26)
    epdSendCommand(0x26);
    epdSendDataBytes(image, bufSize);

    // HALF / Warmed Clean Refresh (Stock X4 OEM parity: 0xD7 with temp 0x5A)
    epdSendCommand(0x21);  // Display update control 1
    epdSendData(0x40);     // Bypass RED

    epdSendCommand(0x3C);  // Border waveform
    epdSendData(0xC0);

    epdSendCommand(0x1A);  // Write temperature register
    epdSendData(0x5A);

    epdSendCommand(0x22);  // Display update control 2
    epdSendData(0xD7);     // Warmed clean update sequence

    epdSendCommand(0x20);  // Master activation
    epdWaitBusy();

    _x4_isScreenOn = true;
    _x4_needsInitialFull = false;
}

// Deep clear: OTP full waveform (0xF7, multi-inversion clean, ~1800ms)
void epdDisplayDeepClear(const uint8_t *image) {
    if (!_x4_initialized) epdInit();

    const uint32_t bufSize = (uint32_t)(W / 8) * H;

    epdSetRamArea(0, 0, W, H);

    epdSendCommand(0x24);
    epdSendDataBytes(image, bufSize);

    epdSendCommand(0x26);
    epdSendDataBytes(image, bufSize);

    // Full OTP update sequence
    epdSendCommand(0x21);  // Display update control 1
    epdSendData(0x40);     // Bypass RED

    epdSendCommand(0x3C);  // Border waveform
    epdSendData(0xC0);

    epdSendCommand(0x22);  // Display update control 2
    epdSendData(0xF7);     // OTP full waveform

    epdSendCommand(0x20);  // Master activation
    epdWaitBusy();

    _x4_isScreenOn = true;
    _x4_needsInitialFull = false;
}

void epdDisplay2bpp(const uint8_t *image2bpp) {
    (void)image2bpp;
    epdDisplay(imgBuf);
}

// Fast partial/differential refresh (0xFC, ~500ms)
void epdDisplayFast(const uint8_t *image) {
    if (!_x4_initialized) epdInit();

    // If first paint after boot or deep sleep, promote to clean refresh
    if (_x4_needsInitialFull) {
        epdDisplay(image);
        return;
    }

    const uint32_t bufSize = (uint32_t)(W / 8) * H;

    // Set RAM area to full screen
    epdSetRamArea(0, 0, W, H);

    // In single-buffer mode, RED RAM (0x26) already holds the baseline from the last refresh!
    // Write new frame to BW RAM (0x24) only:
    epdSendCommand(0x24);
    epdSendDataBytes(image, bufSize);

    // Fast partial/differential update sequence
    epdSendCommand(0x21);  // Display update control 1
    epdSendData(0x00);     // Normal (diff BW against RED)

    epdSendCommand(0x3C);  // Border waveform
    epdSendData(0xC0);

    epdSendCommand(0x22);  // Display update control 2
    epdSendData(0xFC);     // Partial / fast update sequence

    epdSendCommand(0x20);  // Master activation
    epdWaitBusy();

    _x4_isScreenOn = true;

    // After refresh finishes: re-sync RED (0x26) and BW (0x24) to image
    // to establish the baseline for the NEXT fast refresh!
    epdSetRamArea(0, 0, W, H);
    epdSendCommand(0x24);
    epdSendDataBytes(image, bufSize);
    epdSendCommand(0x26);
    epdSendDataBytes(image, bufSize);
}

bool epdSupportsPartialRefresh() {
    return true;
}

void epdPartialDisplay(uint8_t *data, int xStart, int yStart, int xEnd, int yEnd) {
    epdPartialDisplayWithOld(data, nullptr, xStart, yStart, xEnd, yEnd);
}

void epdPartialDisplayWithOld(uint8_t *data, const uint8_t *oldData, int xStart, int yStart, int xEnd, int yEnd) {
    if (!_x4_initialized) epdInit();

    if (_x4_needsInitialFull) {
        epdDisplay(imgBuf);
        return;
    }

    if (xStart < 0) xStart = 0;
    if (yStart < 0) yStart = 0;
    if (xEnd > W) xEnd = W;
    if (yEnd > H) yEnd = H;
    if (xEnd <= xStart || yEnd <= yStart) return;

    // Byte align X
    xStart -= (xStart % 8);
    int w = xEnd - xStart;
    if (w % 8 != 0) w += (8 - (w % 8));
    if (xStart + w > W) w = W - xStart;
    int h = yEnd - yStart;

    const uint16_t wBytes = w / 8;
    const uint32_t windowSize = (uint32_t)wBytes * h;

    epdSetRamArea(xStart, yStart, w, h);

    // Write new window to BW RAM (0x24)
    epdSendCommand(0x24);
    epdSendDataBytes(data, windowSize);

    // If oldData is provided, write it to RED RAM (0x26) for diffing
    if (oldData != nullptr) {
        epdSendCommand(0x26);
        epdSendDataBytes(oldData, windowSize);
    }

    // Fast partial update sequence
    epdSendCommand(0x21);
    epdSendData(0x00);

    epdSendCommand(0x3C);
    epdSendData(0xC0);

    epdSendCommand(0x22);
    epdSendData(0xFC);

    epdSendCommand(0x20);
    epdWaitBusy();

    _x4_isScreenOn = true;

    // If oldData was not provided, re-sync that window in RED (0x26) and BW (0x24)
    if (oldData == nullptr) {
        epdSetRamArea(xStart, yStart, w, h);
        epdSendCommand(0x24);
        epdSendDataBytes(data, windowSize);
        epdSendCommand(0x26);
        epdSendDataBytes(data, windowSize);
    }
}

void epdSleep() {
    if (_x4_isScreenOn) {
        epdSendCommand(0x3C);
        epdSendData(0x80);

        epdSendCommand(0x22);
        epdSendData(0x03);  // ANALOG_OFF_PHASE | CLOCK_OFF
        epdSendCommand(0x20);
        epdWaitBusy();
        _x4_isScreenOn = false;
    }

    // Deep sleep mode 2 (discards RAM, stock X4 parity)
    epdSendCommand(0x10);
    epdSendData(0x03);

    _x4_initialized = false;
    _x4_needsInitialFull = true;
}

#else
// ── Other panel sizes: GxEPD2 (hardware SPI) ─────────────────

#include <SPI.h>
#include <GxEPD2_BW.h>

#ifndef EPD_GXEPD2_SPI_HZ
#define EPD_GXEPD2_SPI_HZ 4000000
#endif

#if defined(EPD_PANEL_42_GXEPD2_T81)
  #include <gdey/GxEPD2_420_GDEY042T81.h>
  GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT / 4> display(
      GxEPD2_420_GDEY042T81(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
#elif defined(EPD_PANEL_42_GXEPD2_GYE042A87)
  #include <other/GxEPD2_420_GYE042A87.h>
  GxEPD2_BW<GxEPD2_420_GYE042A87, GxEPD2_420_GYE042A87::HEIGHT> display(
      GxEPD2_420_GYE042A87(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
#elif defined(EPD_PANEL_42_GXEPD2_420)
  #include <epd/GxEPD2_420.h>
  GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT / 4> display(
      GxEPD2_420(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
#elif defined(EPD_PANEL_42_GXEPD2_M01)
  #include <epd/GxEPD2_420_M01.h>
  GxEPD2_BW<GxEPD2_420_M01, GxEPD2_420_M01::HEIGHT / 4> display(
      GxEPD2_420_M01(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
#elif defined(EPD_PANEL_29)
  #include <gdey/GxEPD2_290_GDEY029T94.h>
  GxEPD2_BW<GxEPD2_290_GDEY029T94, GxEPD2_290_GDEY029T94::HEIGHT> display(
      GxEPD2_290_GDEY029T94(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
  static uint8_t rotated_buffer[IMG_BUF_LEN];

  static bool is_black_pixel(const uint8_t* buffer, int width, int x, int y) {
      int row_bytes = (width + 7) / 8;
      return (buffer[y * row_bytes + x / 8] & (0x80 >> (x % 8))) == 0;
  }

  static void set_black_pixel(uint8_t* buffer, int width, int x, int y) {
      int row_bytes = (width + 7) / 8;
      buffer[y * row_bytes + x / 8] &= ~(0x80 >> (x % 8));
  }

  static void rotate_landscape_to_panel(const uint8_t* source) {
      memset(rotated_buffer, 0xFF, sizeof(rotated_buffer));
      for (int src_y = 0; src_y < H; src_y++) {
          for (int src_x = 0; src_x < W; src_x++) {
              if (!is_black_pixel(source, W, src_x, src_y)) continue;
              int dst_x = src_y;
              int dst_y = W - 1 - src_x;
              set_black_pixel(rotated_buffer, GxEPD2_290_GDEY029T94::WIDTH, dst_x, dst_y);
          }
      }
  }
#elif defined(EPD_PANEL_583_UC8179)
  #include <gdeq/GxEPD2_583_GDEQ0583T31.h>
  GxEPD2_BW<GxEPD2_583_GDEQ0583T31, GxEPD2_583_GDEQ0583T31::HEIGHT / 4> display(
      GxEPD2_583_GDEQ0583T31(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
#elif defined(EPD_PANEL_583)
  #include <gdeq/GxEPD2_583_GDEQ0583T31.h>
  GxEPD2_BW<GxEPD2_583_GDEQ0583T31, GxEPD2_583_GDEQ0583T31::HEIGHT / 4> display(
      GxEPD2_583_GDEQ0583T31(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
#elif defined(EPD_PANEL_75)
  #include <epd/GxEPD2_750_T7.h>
  GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT / 4> display(
      GxEPD2_750_T7(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
#else
  #error "No EPD panel type defined. Use -DEPD_PANEL_42_SSD1683_BW, -DEPD_PANEL_42_DKE_RY683, -DEPD_PANEL_42_GDEM042F52, -DEPD_PANEL_42_GXEPD2_T81, -DEPD_PANEL_42_GXEPD2_GYE042A87, -DEPD_PANEL_42_GXEPD2_420, -DEPD_PANEL_42_GXEPD2_M01, -DEPD_PANEL_29, -DEPD_PANEL_583_UC8179, -DEPD_PANEL_583, or -DEPD_PANEL_75"
#endif

static bool _initialized = false;
#if defined(EPD_PANEL_42_GXEPD2_GYE042A87)
static bool _needs_full_refresh_write = true;
#endif
static const uint8_t DISPLAY_ROTATION =
#if defined(EPD_PANEL_42_GXEPD2_T81) || defined(EPD_PANEL_42_GXEPD2_GYE042A87) || defined(EPD_PANEL_42_GXEPD2_420) || defined(EPD_PANEL_42_GXEPD2_M01)
    0;
#else
    1;
#endif

void gpioInit() {
#if PIN_CFG_BTN >= 0
    pinMode(PIN_CFG_BTN, INPUT_PULLUP);
#endif
    SPI.begin(PIN_EPD_SCK, -1, PIN_EPD_MOSI, PIN_EPD_CS);
}

void epdInit() {
    if (!_initialized) {
        display.epd2.selectSPI(SPI, SPISettings(EPD_GXEPD2_SPI_HZ, MSBFIRST, SPI_MODE0));
        display.init(0);
        display.setRotation(DISPLAY_ROTATION);
        _initialized = true;
    }
}

void epdInitFast() {
    epdInit();
}

void epdDisplay(const uint8_t *image) {
    epdInit();
#if defined(EPD_PANEL_29)
    rotate_landscape_to_panel(image);
    display.writeImage(
        rotated_buffer,
        0,
        0,
        GxEPD2_290_GDEY029T94::WIDTH,
        GxEPD2_290_GDEY029T94::HEIGHT,
        false,
        false,
        false
    );
#elif defined(EPD_PANEL_42_GXEPD2_GYE042A87)
    if (_needs_full_refresh_write) {
        display.epd2.writeImageForFullRefresh(image, 0, 0, W, H, false, false, false);
        _needs_full_refresh_write = false;
    } else {
        display.writeImage(image, 0, 0, W, H, false, false, false);
    }
#else
    display.writeImage(image, 0, 0, W, H, false, false, false);
#endif
    display.refresh(false);
    display.powerOff();
}

void epdDisplayFast(const uint8_t *image) {
#if defined(EPD_PANEL_583_UC8179)
    // 583 UC8179: always full refresh (GxEPD2 refresh(false)); avoids partial LUT ghosting.
    epdDisplay(image);
    return;
#endif
#if defined(EPD_PANEL_42_GXEPD2_GYE042A87)
    epdDisplay(image);
    return;
#endif
    epdInit();
#if defined(EPD_PANEL_29)
    rotate_landscape_to_panel(image);
    display.writeImage(
        rotated_buffer,
        0,
        0,
        GxEPD2_290_GDEY029T94::WIDTH,
        GxEPD2_290_GDEY029T94::HEIGHT,
        false,
        false,
        false
    );
#else
    display.writeImage(image, 0, 0, W, H, false, false, true);
#endif
    display.refresh(true);
    display.powerOff();
}

void epdDisplayDeepClear(const uint8_t *image) {
    epdInit();

    uint8_t *clearBuf = (uint8_t *)malloc(IMG_BUF_LEN);
    if (clearBuf) {
        for (int pass = 0; pass < 4; pass++) {
            memset(clearBuf, (pass % 2 == 0) ? 0x00 : 0xFF, IMG_BUF_LEN);
            display.writeImage(clearBuf, 0, 0, W, H, false, false, true);
            display.refresh(false);
        }
        free(clearBuf);
        display.powerOff();
    }

    epdDisplay(image);
}

void epdPartialDisplay(uint8_t *data, int xStart, int yStart, int xEnd, int yEnd) {
    epdPartialDisplayWithOld(data, nullptr, xStart, yStart, xEnd, yEnd);
}

bool epdSupportsPartialRefresh() {
#if defined(EPD_PANEL_29)
    return false;
#else
    return true;
#endif
}

void epdPartialDisplayWithOld(uint8_t *data, const uint8_t *oldData, int xStart, int yStart, int xEnd, int yEnd) {
    epdInit();
#if defined(EPD_PANEL_29)
    (void)data;
    (void)oldData;
    (void)xStart;
    (void)yStart;
    (void)xEnd;
    (void)yEnd;
    rotate_landscape_to_panel(imgBuf);
    display.writeImage(
        rotated_buffer,
        0,
        0,
        GxEPD2_290_GDEY029T94::WIDTH,
        GxEPD2_290_GDEY029T94::HEIGHT,
        false,
        false,
        false
    );
    display.refresh(true);
#else
    int w = xEnd - xStart;
    int h = yEnd - yStart;
    if (oldData) {
        display.epd2.writeImageAgain(oldData, xStart, yStart, w, h, false, false, true);
        display.writeImage(data, xStart, yStart, w, h, false, false, true);
    } else {
        display.writeImage(data, xStart, yStart, w, h, false, false, true);
        display.epd2.writeImageAgain(data, xStart, yStart, w, h, false, false, true);
    }
    display.refresh(xStart, yStart, w, h);
#endif
    display.powerOff();
}

void epdSleep() {
    display.hibernate();
    _initialized = false;
#if defined(EPD_PANEL_42_GXEPD2_GYE042A87)
    _needs_full_refresh_write = true;
#endif
}
#endif // EPD_PANEL_42_SSD1683_BW
