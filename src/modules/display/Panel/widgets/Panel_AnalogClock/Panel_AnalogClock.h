#pragma once

#include "../../PanelCommon.h"
#include "Global.h"
#include "lvgl.h"
#include <vector>
#include "../../ValueBinder.h"

extern std::vector<lv_obj_t*> ui_AnalogClock;
extern int analogClockCount;

extern std::vector<lv_obj_t*> ui_TabPage;
extern int tab2;
extern std::vector<lv_obj_t*> ui_Button;
extern int isf1;
extern std::vector<lv_obj_t*> ui_Container;
extern int cont;
extern lv_obj_t* ui_Screen1;

class IoTItem; 

class Panel_AnalogClock : public IoTItem {
private:
    int nomer = 1;
    String name = "";
    int _x = 0, _y = 0, _width = 200, _height = 200;
    int color = 0x000000; // цвет стрелок
    int bgColor = 0xFFFFFF; // цвет фона
    int hourColor = 0x000000;
    int minuteColor = 0x000000;
    int secondColor = 0xFF0000;
    int markColor = 0x000000; // цвет меток - черный для контраста
    String _id2show = "";
    int _round = 0;
    int markOn = 1; // 1 - показывать метки, 0 - скрыть метки
    int hourWidth = 6; // ширина часовой стрелки
    int minuteWidth = 4; // ширина минутной стрелки
    int secondWidth = 2; // ширина секундной стрелки
    
    lv_obj_t* container = nullptr;
    lv_obj_t* hourHand = nullptr;
    lv_obj_t* minuteHand = nullptr;
    lv_obj_t* secondHand = nullptr;
    std::vector<lv_obj_t*> numberLabels;
    lv_obj_t* centerDot;
    int timezone = 3; // часовой пояс в часах от UTC
    
lv_point_precise_t hourPoints[2];
lv_point_precise_t minutePoints[2];
lv_point_precise_t secondPoints[2];
std::vector<lv_point_precise_t> markPoints;


public:
    Panel_AnalogClock(String parameters);
    ~Panel_AnalogClock();
//
 //   void onRegEvent(IoTItem* eventItem) override;
  // void onModuleOrder(String& key, String& value) override;
 // void setValue(const IoTValue& Value, bool genEvent = true) override;
//   String getValue() override;
    IoTValue execute(String command, std::vector<IoTValue> &param) override;
 // void onMqttRecive(String& topic, String& msg) override;
    void doByInterval() override;

private:
  //  void drawItem(IoTItem *item);
  //  void updateUI();
 //   String formatValue() const;
    void createClock();
    void updateHands();
    void drawHand(lv_obj_t* hand, lv_point_precise_t* pts, float angle, float length, int cx, int cy);
};