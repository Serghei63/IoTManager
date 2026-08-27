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

    const GFXfont* getFontByIdx(uint8_t fontIdx) {
        switch (fontIdx) {
            case 1: return &FreeSansBold12pt7b;
            case 2: return &FreeSansBold24pt7b;
            case 3: return &Open_Sans_ExtraBold_60;
            case 4: return &Open_Sans_ExtraBold_90;
            case 5: return &Open_Sans_ExtraBold_120;
            default: return nullptr;
        }
    }

String formatString(String text, uint8_t formatType, uint8_t decimals) {
    if (text.length() == 0) return text;

    char* endptr;
    float val = strtof(text.c_str(), &endptr);

    // Если входящая строка не число (например, ":"), возвращаем как есть
    if (endptr == text.c_str()) {
        return text;
    }

    String suffix = String(endptr);
    char buf[32];

    if (decimals == 0) {
        // Форматирование для ЦЕЛЫХ чисел (часы, минуты)
        int intVal = (int)val;
        if (formatType == 2) {
            // Ведущий ноль для 2 знаков (например: 5 -> "05")
            snprintf(buf, sizeof(buf), "%02d", intVal);
        } else if (formatType == 1) {
            // Пробел спереди
            snprintf(buf, sizeof(buf), "%2d", intVal);
        } else {
            snprintf(buf, sizeof(buf), "%d", intVal);
        }
    } else {
        // Форматирование для чисел С ЗАПЯТОЙ (дробных)
        if (formatType == 1) {
            snprintf(buf, sizeof(buf), "%*.*f", 6, decimals, val);
        } else if (formatType == 2) {
            snprintf(buf, sizeof(buf), "%0*.*f", 7, decimals, val);
        } else {
            snprintf(buf, sizeof(buf), "%.*f", decimals, val);
        }
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

    void drawBmp(String filename, int16_t x, int16_t y, uint16_t w, uint16_t h) {
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

        if (read16(bmpFile) != 0x4D42) {
            if (_debug) Serial.println(F("[E-Ink Error] Not a valid BMP file!"));
            bmpFile.close();
            return;
        }

        read32(bmpFile); 
        read32(bmpFile); 
        uint32_t imageOffset = read32(bmpFile); 
        read32(bmpFile); 
        int32_t bmpWidth = read32(bmpFile);
        int32_t bmpHeight = read32(bmpFile);
        uint16_t planes = read16(bmpFile);
        uint16_t depth = read16(bmpFile);

        if (planes != 1 || depth != 1) {
            if (_debug) Serial.printf("[E-Ink Error] Unsupported BMP format (depth: %d, expected 1-bit)\n", depth);
            bmpFile.close();
            return;
        }

        uint32_t rowSize = ((bmpWidth + 31) / 32) * 4; 
        bool flip = true; 
        if (bmpHeight < 0) {
            bmpHeight = -bmpHeight;
            flip = false;
        }

        uint16_t winW = (w > 0) ? w : bmpWidth;
        uint16_t winH = (h > 0) ? h : bmpHeight;

        display->fillRect(x, y, winW, winH, GxEPD_WHITE);

        display->setPartialWindow(x, y, winW, winH);
        display->firstPage();
        do {
            display->fillRect(x, y, winW, winH, GxEPD_WHITE); 

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

                    if ((b & (1 << bitIdx)) == 0) {
                        display->drawPixel(x + col, y + row, GxEPD_BLACK);
                    }
                }
            }
        } while (display->nextPage());

        bmpFile.close();
        if (_debug) Serial.printf("[E-Ink] BMP rendered: %s (%dx%d) at (%d,%d)\n", filename.c_str(), bmpWidth, bmpHeight, x, y);
    }

    void updateData(String rawText, int x, int y, int w, int h, uint8_t fontIdx, uint8_t align, uint8_t formatType, uint8_t decimals) {
        if (!display) return;

        String text = formatString(rawText, formatType, decimals);
        const GFXfont* f = getFontByIdx(fontIdx);
        display->setFont(f);

        int16_t x1, y1;
        uint16_t textW, textH;
        display->getTextBounds(text.c_str(), 0, 0, &x1, &y1, &textW, &textH);

        int cursorX = x;
        if (align == 1) { 
            cursorX = x + (w / 2) - (textW / 2) - x1;
        } else if (align == 2) { 
            cursorX = x + w - textW - x1;
        } else { 
            cursorX = x - x1;
        }

        int cursorY = y + h - (h - textH) / 2;

        display->fillRect(x, y, w, h, GxEPD_WHITE);

        display->setPartialWindow(x, y, w, h);
        display->firstPage();
        do {
            display->fillRect(x, y, w, h, GxEPD_WHITE);
            display->setTextColor(GxEPD_BLACK);
            display->setCursor(cursorX, cursorY);
            display->print(text.c_str());

            yield();
            
        } while (display->nextPage());

        if (_debug) Serial.printf("[E-Ink] Partial update box (%d,%d,%d,%d) Align:%d Font:%d: %s\n", x, y, w, h, align, fontIdx, text.c_str());
    }

// ОБНОВЛЕННАЯ ФУНКЦИЯ: updateSplitValue с выравниванием ПО ЦЕНТРУ прямоугольника (W)
    void updateSplitValue(String rawText, int x, int y, int w, int h, uint8_t fontMain, uint8_t fontSub, uint8_t decimals) {
        if (!display) return;

        // В строке может быть запятая вместо точки ("23,5")
        String cleanText = rawText;
        cleanText.replace(',', '.');

        char* endptr;
        float val = strtof(cleanText.c_str(), &endptr);

        // Если в строке не число — выводим стандартно через updateData
        if (endptr == cleanText.c_str()) {
            updateData(rawText, x, y, w, h, fontMain, 0, 0, decimals);
            return;
        }

        String suffix = String(endptr);
        
        long intPart = (long)val;
        float fracPart = fabs(val - (float)intPart);

String mainStr = String(intPart);
        String subStr = "";

        if (decimals > 0) {
            char fracBuf[16];
            snprintf(fracBuf, sizeof(fracBuf), "%.*f", decimals, fracPart);
            subStr = String(fracBuf);

            // Отрезаем ведущий ноль (оставляем ".3", ".5" и т.д.)
            if (subStr.startsWith("0.")) {
                subStr = subStr.substring(1);
            }
        }

        // Пририсовываем суффикс (" C", " %" и т.д.)
        subStr += suffix;

        const GFXfont* fMain = getFontByIdx(fontMain);
        const GFXfont* fSub  = getFontByIdx(fontSub);

        // Расчет размеров элементов
        display->setFont(fMain);
        int16_t x1_m, y1_m;
        uint16_t w_m, h_m;
        display->getTextBounds(mainStr.c_str(), 0, 0, &x1_m, &y1_m, &w_m, &h_m);

        display->setFont(fSub);
        int16_t x1_s, y1_s;
        uint16_t w_s, h_s;
        display->getTextBounds(subStr.c_str(), 0, 0, &x1_s, &y1_s, &w_s, &h_s);

        // Общая ширина всей составной надписи (целая часть + отступ 4px + дробный хвост)
        uint16_t totalWidth = w_m + 4 + w_s;

        // Смещение по X для выравнивания всей конструкции СТРОГО ПО ЦЕНТРУ прямоугольника W
        int cursorX = x + (w / 2) - (totalWidth / 2) - x1_m;
        int cursorY = y + h - (h - max(h_m, h_s)) / 2;

        // Предварительная очистка области W x H
        display->fillRect(x, y, w, h, GxEPD_WHITE);

        display->setPartialWindow(x, y, w, h);
        display->firstPage();
        do {
            display->fillRect(x, y, w, h, GxEPD_WHITE);
            display->setTextColor(GxEPD_BLACK);

            // Рисуем целую часть по центру
            display->setFont(fMain);
            display->setCursor(cursorX, cursorY);
            display->print(mainStr.c_str());

            // Рисуем дробную часть мелким шрифтом сразу за целой
            display->setFont(fSub);
            display->setCursor(cursorX + w_m + 4, cursorY); 
            display->print(subStr.c_str());

        } while (display->nextPage());

        if (_debug) Serial.printf("[E-Ink] Split update box (%d,%d,%d,%d): Main '%s', Sub '%s'\n", x, y, w, h, mainStr.c_str(), subStr.c_str());
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
            if (param.size() >= 5) {
                String text = param[0].valS;
                int x = (int)param[1].valD;
                int y = (int)param[2].valD;
                int w = (int)param[3].valD;
                int h = (int)param[4].valD;

                uint8_t fontIdx = (param.size() >= 6) ? (uint8_t)param[5].valD : 2;
                uint8_t align   = (param.size() >= 7) ? (uint8_t)param[6].valD : 0;
                uint8_t format  = (param.size() >= 8) ? (uint8_t)param[7].valD : 0;
                uint8_t dec     = (param.size() >= 9) ? (uint8_t)param[8].valD : 2;

                updateData(text, x, y, w, h, fontIdx, align, format, dec);

                value.valS = text;
                regEvent(value.valS, "EInkUpdate");
            }
        }
        // ВЫЗОВ updateSplitValue С УЧЕТОМ W И H
        else if (command == "updateSplitValue") {
            if (param.size() >= 5) {
                String text = param[0].valS;
                int x = (int)param[1].valD;
                int y = (int)param[2].valD;
                int w = (int)param[3].valD;
                int h = (int)param[4].valD;

                uint8_t fontMain = (param.size() >= 6) ? (uint8_t)param[5].valD : 4;
                uint8_t fontSub  = (param.size() >= 7) ? (uint8_t)param[6].valD : 2;
                uint8_t dec      = (param.size() >= 8) ? (uint8_t)param[7].valD : 1;

                updateSplitValue(text, x, y, w, h, fontMain, fontSub, dec);

                value.valS = text;
                regEvent(value.valS, "EInkSplitUpdate");
            }
        }
        else if (command == "showImage") {
            if (param.size() >= 1) {
                String imgPath = param[0].valS;
                int x = (param.size() >= 2) ? (int)param[1].valD : 0;
                int y = (param.size() >= 3) ? (int)param[2].valD : 0;
                int w = (param.size() >= 4) ? (int)param[3].valD : 0;
                int h = (param.size() >= 5) ? (int)param[4].valD : 0;

                drawBmp(imgPath, x, y, w, h);

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