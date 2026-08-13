#include "Global.h"
#include "classes/IoTItem.h"
#include <GxEPD2_BW.h>

// Конструктор под e-ink 4.2" (400х300)
GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT>* display = nullptr;

class Eink42 : public IoTItem {
   private:
    int _cs, _dc, _rst, _busy, _rotation;
    bool _debug;
    bool _isFirstRun = true;
    int _currentBackground = -1; // Храним номер текущего фона

   public:
    Eink42(String parameters) : IoTItem(parameters) {
        jsonRead(parameters, "cs", _cs);
        jsonRead(parameters, "dc", _dc);
        jsonRead(parameters, "rst", _rst);
        jsonRead(parameters, "busy", _busy);
        jsonRead(parameters, "rotation", _rotation);
        jsonRead(parameters, "debug", _debug);
    }

    // 1. ПОЛНЫЙ РЕФРЕШ (Для ночной очистки по Крону или при смене грани)
    void forceFullRefresh() {
        if (!display) return;
        if (_debug) Serial.println(F("[E-Ink] Performing full refresh (clear)..."));
        
        display->setFullWindow();
        display->firstPage();
        do {
            display->fillScreen(GxEPD_WHITE);
        } while (display->nextPage());
        
        // Если до этого горел какой-то фон, восстановим его после очистки
        if (_currentBackground != -1) {
            drawBackgroundFile(_currentBackground);
        }
    }

    // Вспомогательная функция прорисовки фона из LittleFS
    void drawBackgroundFile(int imgNum) {
        String fileName = "/" + String(imgNum) + ".bin";
        if (!FileFS.exists(fileName)) return;

        display->setFullWindow();
        display->firstPage();
        do {
            display->fillScreen(GxEPD_WHITE);
            File file = FileFS.open(fileName, "r");
            if (file) {
                uint16_t x = 0, y = 0;
                uint8_t byteData;
                while (file.available()) {
                    byteData = file.read();
                    for (int bit = 0; bit < 8; bit++) {
                        bool pixel = (byteData << bit) & 0x80;
                        if (!pixel) display->drawPixel(x, y, GxEPD_BLACK);
                        x++;
                        if (x >= display->width()) {
                            x = 0; y++;
                            if (y >= display->height()) break;
                        }
                    }
                }
                file.close();
            }
        } while (display->nextPage());
        _currentBackground = imgNum;
    }

    // 2. ЧАСТИЧНЫЙ РЕФРЕШ (Для быстрого вывода данных поверх фона)
    void drawPartialText(const String& text, uint16_t x, uint16_t y, uint8_t textSize) {
        if (!display) return;

        display->setFont(nullptr); // Масштабируемый шрифт Adafruit
        display->setTextSize(textSize);
        display->setTextColor(GxEPD_BLACK);

        // Рассчитываем габариты подложки, чтобы стереть старое значение
        uint16_t w = text.length() * 6 * textSize;
        uint16_t h = 8 * textSize;

        // Устанавливаем частичное окно только под наши новые цифры
        display->setPartialWindow(x, y, w, h);
        display->firstPage();
        do {
            display->fillRect(x, y, w, h, GxEPD_WHITE); // Стираем старые цифры белым прямоугольником
            display->setCursor(x, y);
            display->print(text.c_str());               // Пишем новое значение
        } while (display->nextPage());

        if (_debug) Serial.printf("[E-Ink] Partial update at (%d,%d): %s\n", x, y, text.c_str());
    }

    void loop() override {
        if (_isFirstRun) {
            _isFirstRun = false;
            display = new GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT>(GxEPD2_420(_cs, _dc, _rst, _busy));
            if (display) {
                display->init(115200, true, 50, false);
                display->setRotation(_rotation);
                
                // При первом старте просто очищаем экран в красивый белый цвет
                forceFullRefresh();
                value.valS = "Ready";
            }
        }
    }

    IoTValue execute(String command, std::vector<IoTValue>& param) override {
        if (!display) return {};

        // Команда ночной очистки: eink.fullRefresh()
        if (command == "fullRefresh") {
            forceFullRefresh();
        }
        // Команда отрисовки фона: eink.drawBackground(НомерКартинки)
        else if (command == "drawBackground") {
            if (param.size() == 1) {
                int imgNum = param[0].valD;
                drawBackgroundFile(imgNum);
                value.valS = "Bg " + String(imgNum);
                regEvent(value.valS, "EInkBg");
            }
        }
        // Команда быстрого обновления данных: eink.updateData(Текст, X, Y, РазмерШрифта)
        else if (command == "updateData") {
            if (param.size() >= 3) {
                String text = param[0].valS;
                uint16_t x = param[1].valD;
                uint16_t y = param[2].valD;
                uint8_t size = (param.size() == 4) ? (uint8_t)param[3].valD : 2;

                drawPartialText(text, x, y, size);
                
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