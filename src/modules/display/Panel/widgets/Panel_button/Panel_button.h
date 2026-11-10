// Panel_button.h
#pragma once

#include "../../PanelCommon.h"
#include "Global.h"
#include "lvgl.h"
#include <vector>
#include "../../ValueBinder.h"  // 🔽 Подключаем ValueBinder

// Глобальные переменные
extern std::vector<lv_obj_t*> ui_Button;
extern std::vector<String> button_value;
extern int isf1;

extern std::vector<lv_obj_t*> ui_TabPage;
extern int tab2;
extern std::vector<lv_obj_t*> ui_Container;
extern int cont;
extern lv_obj_t* ui_Screen1;

// Предварительное объявление
class IoTItem;

// Обработчики событий LVGL
void ui_event_Button(lv_event_t * e);
void ui_event_Button1(lv_event_t * e);

// === Класс Panel_button ===
class Panel_button : public IoTItem {
public:
    Panel_button(String parameters);
    ~Panel_button();  // теперь реализован

    void onRegEvent(IoTItem* eventItem) override;
    //void onTrackingValue(IoTItem* item) override;
    void onMqttRecive(String& topic, String& msg) override;
    void onModuleOrder(String& key, String& value) override;
    void setValue(const IoTValue& Value, bool genEvent = true) override;
    String getValue() override;
    IoTValue execute(String command, std::vector<IoTValue> &param) override;

    void drawItem(IoTItem *item);

private:
    // UI и параметры
    int _buttonIndex = 0;
    int _x = 0, _y = 0, _x1 = 0, _y1 = 0;
    int color_of = 0xFFFFFF, color_on = 0x00FF00;
    int color_border = 0xAAAAAA, size_border = 1;
    int Visibility = 255, radius = 0;
    int dig = 1;
    String name = "", _id2show = "";
    int nomer = 0;

    // Состояния
    String _lastSentValue = "";
    String valrt = "";
    bool _ignoreNextSync = false;

    // 🔽 ValueBinder для связи _id и _id2show
    ValueBinder* binder = nullptr;

    // Внутреннее обновление UI
    void updateUI();
};