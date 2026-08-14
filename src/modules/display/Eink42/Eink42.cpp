#include "Global.h"
#include "classes/IoTItem.h"
#include <GxEPD2_BW.h>

// Подключаем шрифты Adafruit GFX
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/Open_Sans_ExtraBold_60.h>
#include <Fonts/Open_Sans_ExtraBold_90.h>
#include <Fonts/Open_Sans_ExtraBold_120.h>

// Единый глобальный указатель на физический дисплей
GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT>* display = nullptr;

class Eink42 : public IoTItem {
   private:
    int _cs, _dc, _rst, _busy, _rotation;
    bool _debug;
    bool _isFirstRun = true;

    // Вспомогательная функция форматирования численных значений (DWIN-style)
    String formatString(String text, uint8_t formatType, uint8_t decimals) {
        // Если пришел не float (например, обычный текст или время "10:20"), возвращаем как есть
        if (formatType == 0 || text.length() == 0) return text;

        char* endptr;
        float val = strtof(text.c_str(), &endptr);
        if (*endptr != '\0' && endptr == text.c_str()) {
            return text; // Строку не удалось распарсить как число
        }

        char buf[32];
        if (formatType == 1) { 
            // Формат 1: Выравнивание пробелами спереди (для датчиков, чтоб запятая не прыгала)
            snprintf(buf, sizeof(buf), "%*.*f", 6, decimals, val);
        } else if (formatType == 2) { 
            // Формат 2: Ведущие нули (для счетчиков газа/воды, например: 0003.40)
            snprintf(buf, sizeof(buf), "%0*.*f", 7, decimals, val);
        } else {
            snprintf(buf, sizeof(buf), "%.*f", decimals, val);
        }
        return String(buf);
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

    // Изменение ориентации дисплея (0..3)
    void setRotationAngle(uint8_t rotation) {
        if (!display) return;
        uint8_t newRot = rotation % 4;
        if (_rotation != newRot) {
            _rotation = newRot;
            display->setRotation(_rotation);
            forceFullRefresh(); // Полный рефреш обязателен при смене сетки координат!
            if (_debug) Serial.printf("[E-Ink] Rotation set to: %d\n", _rotation);
        }
    }

    // Полный рефреш (ночная очистка матрицы)
    void forceFullRefresh() {
        if (!display) return;
        if (_debug) Serial.println(F("[E-Ink] Performing full refresh..."));
        
        display->setFullWindow();
        display->firstPage();
        do {
            display->fillScreen(GxEPD_WHITE);
        } while (display->nextPage());
    }

    // Основная функция вывода данных с выравниванием и выбором шрифта
    void updateData(String rawText, int x, int y, uint8_t fontIdx, uint8_t align, uint8_t formatType, uint8_t decimals) {
        if (!display) return;

        // 1. Форматируем входную строку
        String text = formatString(rawText, formatType, decimals);

        // 2. Выбираем шрифт
        switch (fontIdx) {
            case 1:
                display->setFont(&FreeSansBold12pt7b); // Средний шрифт
                break;
            case 2:
                display->setFont(&FreeSansBold24pt7b); // Крупный шрифт
                break;
            case 3:
                display->setFont(&Open_Sans_ExtraBold_60); // Огромный шрифт
                break;
            case 4:
                display->setFont(&Open_Sans_ExtraBold_90); // Огромный шрифт
                break;
            case 5:
                display->setFont(&Open_Sans_ExtraBold_120); // Огромный шрифт
                break;
            default:
                display->setFont(nullptr);            // Стандартный системный monospaced
                break;
        }

        // 3. Авторасчет точных габаритов текста под выбранный шрифт
        int16_t x1, y1;
        uint16_t w, h;
        display->getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);

        // 4. Расчет X с учетом выравнивания (0 - Left, 1 - Center, 2 - Right)
        int renderX = x;
        if (align == 1) {
            renderX = x - (w / 2);
        } else if (align == 2) {
            renderX = x - w;
        }

        // 5. Вычисляем безопасные границы частичной очистки (с запасом в 4px)
        uint16_t pad = 4;
        int winX = (renderX + x1 > pad) ? (renderX + x1 - pad) : 0;
        int winY = (y + y1 > pad) ? (y + y1 - pad) : 0;
        uint16_t winW = w + (pad * 2);
        uint16_t winH = h + (pad * 2);

        // Ограничиваем окно рамками экрана
        if (winX + winW > display->width())  winW = display->width() - winX;
        if (winY + winH > display->height()) winH = display->height() - winY;

        // 6. Быстрый частичный рефреш
        display->setPartialWindow(winX, winY, winW, winH);
        display->firstPage();
        do {
            display->fillRect(winX, winY, winW, winH, GxEPD_WHITE); // Стираем старое значение
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
                
                // Считываем опциональные параметры (с дефолтными значениями)
                uint8_t fontIdx = (param.size() >= 4) ? (uint8_t)param[3].valD : 2;
                uint8_t align   = (param.size() >= 5) ? (uint8_t)param[4].valD : 0;
                uint8_t format  = (param.size() >= 6) ? (uint8_t)param[5].valD : 0;
                uint8_t dec     = (param.size() >= 7) ? (uint8_t)param[6].valD : 2;

                updateData(text, x, y, fontIdx, align, format, dec);
                
                value.valS = text;
                regEvent(value.valS, "EInkUpdate");
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