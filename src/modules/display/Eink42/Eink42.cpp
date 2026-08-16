#include "Global.h"
#include "classes/IoTItem.h"
#include <GxEPD2_BW.h>
#include <LittleFS.h>

// Подключаем шрифты Adafruit GFX
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/Open_Sans_ExtraBold_120.h>
#include <Fonts/Open_Sans_ExtraBold_60.h>
#include <Fonts/Open_Sans_ExtraBold_90.h>

// Единый глобальный указатель на физический дисплей
GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT>* display = nullptr;

class Eink42 : public IoTItem {
   private:
    int _cs, _dc, _rst, _busy, _rotation;
    bool _debug;
    bool _isFirstRun = true;

    // Чтение 16-битных и 32-битных значений из заголовка BMP
    uint16_t read16(File &f) {
        uint16_t result;
        f.read((uint8_t*)&result, sizeof(result));
        return result;
    }

    uint32_t read32(File &f) {
        uint32_t result;
        f.read((uint8_t*)&result, sizeof(result));
        return result;
    }

    // Вспомогательная функция форматирования численных значений
    String formatString(String text, uint8_t formatType, uint8_t decimals) {
        if (text.length() == 0) return text;

        // Пытаемся распарсить число в начале строки (игнорируя суффиксы " %", " C")
        char* endptr;
        float val = strtof(text.c_str(), &endptr);

        // Если в начале строки вообще нет числа (обычный текст), возвращаем как есть
        if (endptr == text.c_str()) {
            return text;
        }

        // Запоминаем суффикс (например, " C" или " %")
        String suffix = String(endptr);

        char buf[32];
        if (formatType == 1) {
            snprintf(buf, sizeof(buf), "%*.*f", 6, decimals, val);
        } else if (formatType == 2) {
            snprintf(buf, sizeof(buf), "%0*.*f", 7, decimals, val);
        } else {
            // Обычный вывод с заданным количеством знаков после запятой (decimals)
            snprintf(buf, sizeof(buf), "%.*f", decimals, val);
        }

        return String(buf) + suffix;
    }

   public:
    Eink42(String parameters) : IoTItem(parameters) {
        jsonRead(parameters, "cs", _cs);
        jsonRead(parameters, "dc", _dc);
        jsonRead(parameters, "rst", _rst);
        jsonRead(parameters, "busy", _busy);
        jsonRead(parameters, "rotation", _rotation);
        jsonRead(parameters, "debug", _debug);
    }

    void setRotationAngle(uint8_t rotation) {
        if (!display) return;
        uint8_t newRot = rotation % 4;
        if (_rotation != newRot) {
            _rotation = newRot;
            display->setRotation(_rotation);
            forceFullRefresh();
            if (_debug) Serial.printf("[E-Ink] Rotation set to: %d\n", _rotation);
        }
    }

    void forceFullRefresh() {
        if (!display) return;
        if (_debug) Serial.println(F("[E-Ink] Performing full refresh..."));

        display->setFullWindow();
        display->firstPage();
        do {
            display->fillScreen(GxEPD_WHITE);
        } while (display->nextPage());
    }

    // Отрисовка 1-bit BMP из LittleFS
    void drawBmp(String filename, int16_t x, int16_t y) {
        if (!display) return;

        if (!filename.startsWith("/")) {
            filename = "/" + filename;
        }

        if (!LittleFS.exists(filename)) {
            if (_debug) Serial.printf("[E-Ink Error] File not found: %s\n", filename.c_str());
            return;
        }

        File bmpFile = LittleFS.open(filename, "r");
        if (!bmpFile) {
            if (_debug) Serial.printf("[E-Ink Error] Failed to open file: %s\n", filename.c_str());
            return;
        }

        // Проверяем сигнатуру BMP ('BM')
        if (read16(bmpFile) != 0x4D42) {
            if (_debug) Serial.println(F("[E-Ink Error] Not a valid BMP file!"));
            bmpFile.close();
            return;
        }

        read32(bmpFile); // Prop Size
        read32(bmpFile); // Reserved
        uint32_t imageOffset = read32(bmpFile); // Смещение массива пикселей
        read32(bmpFile); // Header size
        int32_t bmpWidth = read32(bmpFile);
        int32_t bmpHeight = read32(bmpFile);
        uint16_t planes = read16(bmpFile);
        uint16_t depth = read16(bmpFile);

        // Нам нужен только 1-битный монохромный BMP
        if (planes != 1 || depth != 1) {
            if (_debug) Serial.printf("[E-Ink Error] Unsupported BMP format (depth: %d, expected 1-bit)\n", depth);
            bmpFile.close();
            return;
        }

        uint32_t rowSize = ((bmpWidth + 31) / 32) * 4; // Размер строки в байтах (выравнивание по 4 байтам)
        bool flip = true; // BMP обычно хранятся снизу вверх
        if (bmpHeight < 0) {
            bmpHeight = -bmpHeight;
            flip = false;
        }

        // Частичное обновление под размер изображения
        display->setPartialWindow(x, y, bmpWidth, bmpHeight);
        display->firstPage();
        do {
            display->fillRect(x, y, bmpWidth, bmpHeight, GxEPD_WHITE); // Стираем фоновую область

            for (int32_t row = 0; row < bmpHeight; row++) {
                uint32_t pos;
                if (flip) {
                    pos = imageOffset + (bmpHeight - 1 - row) * rowSize;
                } else {
                    pos = imageOffset + row * rowSize;
                }
                bmpFile.seek(pos);

                uint8_t b = 0;
                int bitIdx = 0;
                for (int32_t col = 0; col < bmpWidth; col++) {
                    if (bitIdx == 0) {
                        b = bmpFile.read();
                        bitIdx = 8;
                    }
                    bitIdx--;

                    // 1 bit in BMP: 1 = White, 0 = Black
                    if ((b & (1 << bitIdx)) == 0) {
                        display->drawPixel(x + col, y + row, GxEPD_BLACK);
                    }
                }
            }
        } while (display->nextPage());

        bmpFile.close();
        if (_debug) Serial.printf("[E-Ink] BMP rendered: %s (%dx%d) at (%d,%d)\n", filename.c_str(), bmpWidth, bmpHeight, x, y);
    }

    void updateData(String rawText, int x, int y, uint8_t fontIdx, uint8_t align, uint8_t formatType, uint8_t decimals) {
        if (!display) return;

        String text = formatString(rawText, formatType, decimals);

        switch (fontIdx) {
            case 1: display->setFont(&FreeSansBold12pt7b); break;
            case 2: display->setFont(&FreeSansBold24pt7b); break;
            case 3: display->setFont(&Open_Sans_ExtraBold_60); break;
            case 4: display->setFont(&Open_Sans_ExtraBold_90); break;
            case 5: display->setFont(&Open_Sans_ExtraBold_120); break;
            default: display->setFont(nullptr); break;
        }

        int16_t x1, y1;
        uint16_t w, h;
        display->getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);

        int renderX = x;
        if (align == 1) {
            renderX = x - (w / 2);
        } else if (align == 2) {
            renderX = x - w;
        }

        uint16_t pad = 4;
        int winX = (renderX + x1 > pad) ? (renderX + x1 - pad) : 0;
        int winY = (y + y1 > pad) ? (y + y1 - pad) : 0;
        uint16_t winW = w + (pad * 2);
        uint16_t winH = h + (pad * 2);

        if (winX + winW > display->width())  winW = display->width() - winX;
        if (winY + winH > display->height()) winH = display->height() - winY;

        display->setPartialWindow(winX, winY, winW, winH);
        display->firstPage();
        do {
            display->fillRect(winX, winY, winW, winH, GxEPD_WHITE);
            display->setTextColor(GxEPD_BLACK);
            display->setCursor(renderX, y);
            display->print(text.c_str());
        } while (display->nextPage());

        if (_debug) Serial.printf("[E-Ink] Partial update (%d,%d) Align:%d Font:%d: %s\n", renderX, y, align, fontIdx, text.c_str());
    }

    void loop() override {
        if (_isFirstRun) {
            _isFirstRun = false;

            display = new GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT>(GxEPD2_420(_cs, _dc, _rst, _busy));
            if (display) {
                display->init(115200, true, 50, false);
                display->setRotation(_rotation);
                value.valS = "Ready";
            }
        }
    }

    IoTValue execute(String command, std::vector<IoTValue>& param) override {
        if (!display) return {};

        if (command == "fullRefresh") {
            forceFullRefresh();
        }
        else if (command == "setRotation") {
            if (param.size() >= 1) {
                setRotationAngle((uint8_t)param[0].valD);
            }
        }
        else if (command == "updateData") {
            if (param.size() >= 3) {
                String text = param[0].valS;
                int x = (int)param[1].valD;
                int y = (int)param[2].valD;

                uint8_t fontIdx = (param.size() >= 4) ? (uint8_t)param[3].valD : 2;
                uint8_t align   = (param.size() >= 5) ? (uint8_t)param[4].valD : 0;
                uint8_t format  = (param.size() >= 6) ? (uint8_t)param[5].valD : 0;
                uint8_t dec     = (param.size() >= 7) ? (uint8_t)param[6].valD : 2;

                updateData(text, x, y, fontIdx, align, format, dec);

                value.valS = text;
                regEvent(value.valS, "EInkUpdate");
            }
        }
        else if (command == "showImage") {
            if (param.size() >= 1) {
                String imgPath = param[0].valS;
                int x = (param.size() >= 2) ? (int)param[1].valD : 0;
                int y = (param.size() >= 3) ? (int)param[2].valD : 0;

                drawBmp(imgPath, x, y);

                value.valS = imgPath;
                regEvent(value.valS, "EInkImage");
            }
        }

        return {};
    }

    ~Eink42() {
        if (display) { delete display; display = nullptr; }
    }
};

void* getAPI_Eink42(String subtype, String param) {
    if (subtype == F("Eink42")) return new Eink42(param);
    return nullptr;
}