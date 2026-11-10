#pragma once

#include "../../PanelCommon.h"
#include "Global.h"
#include "../../ValueBinder.h"
#include "lvgl.h"
#include <vector>

extern std::vector<lv_obj_t*> ui_Arc_text;
extern int nar;

extern std::vector<lv_obj_t*> ui_TabPage;
extern int tab2;
extern std::vector<lv_obj_t*> ui_Button;
extern int isf1;
extern std::vector<lv_obj_t*> ui_Container;
extern int cont;
extern lv_obj_t* ui_Screen1;

class IoTItem;

class Panel_arc_text : public IoTItem {
private:
    int nomer = 1;
    String name = "";
    int _x = 0, _y = 0, shr = 40;
    int color = 0x1A7D35;
    lv_font_t shrit = serg20;
    String _id2show = "";
    String descr = "";
    String descr1 = "";
    int nar1 = 0;
    int _round = 0;
    String valrtS = "";
    float _valrtD = 0;
  //  bool Tracking = false;

    // Параметры для отображения текста по дуге
    int _radius = 100;      // радиус дуги
    int _angle_start = 0;   // начальный угол
    int _angle_end = 360;   // конечный угол
    bool _text_on_arc = true; // флаг использования текста по дуге

    // ValueBinder для синхронизации с _id2show
    ValueBinder* binder = nullptr;
    bool _ignoreNextSync = false;
    String _lastSentValue = "";

public:
    Panel_arc_text(String parameters);
  //  ~Panel_arc_text();

  //  void onRegEvent(IoTItem* eventItem) override;
  //  void onModuleOrder(String& key, String& value) override;
    void setValue(const IoTValue& Value, bool genEvent = true) override;
    String getValue() override;
    IoTValue execute(String command, std::vector<IoTValue> &param) override; // Добавлены команды hide/show/pos/color/text
 //    void onMqttRecive(String& topic, String& msg) override;
    

private:
 //  void drawItem(IoTItem *item);
    void updateUI();
    String formatValue() const;
  //  void onTrackingValue(IoTItem* item) override;

    // Вспомогательные функции для рисования текста по дуге
   // void drawTextOnArc(lv_obj_t* parent, const char* text, lv_point_t center, int radius, int angle_start, int angle_end, const lv_font_t* font, lv_color_t color);
void drawTextOnArc(lv_obj_t* parent,
                   const char* text,
                   lv_point_t center,
                   int radius,
                   int angle_start,
                   int angle_end,
                   const lv_font_t* font,
                   lv_color_t color);

};
