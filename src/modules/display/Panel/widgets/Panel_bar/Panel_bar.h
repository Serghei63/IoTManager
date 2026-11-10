#pragma once

#include "../../PanelCommon.h"
#include "Global.h"
#include "lvgl.h"
#include <vector>
#include "../../ValueBinder.h"

// Global variables for Panel_bar
extern std::vector<lv_obj_t*> ui_Bar;
extern std::vector<String> bar_id;
//extern std::vector<String> bar_id2;
extern int bar;

extern std::vector<lv_obj_t*> ui_TabPage;
extern int tab2;
extern std::vector<lv_obj_t*> ui_Button;
extern int isf1;
extern std::vector<lv_obj_t*> ui_Container;
extern int cont;
extern lv_obj_t* ui_Screen1;

// Forward declarations
class IoTItem;

// Widget class
class Panel_bar : public IoTItem {
private:
    int nomer = 1;
    String  name = "";
    int _x = 0, _y = 0, _x1 = 0, _y1 =0;
    int _x2 = 0, _y2 = 100, _x3 = 120, _y3 = 60;
    int color = 0x1A7D35;
    int valy = 0;
    String _id2show = "";
    String _valueSource = "unknown";
  //  bool _pendingMqttUpdate = false;
   // bool _generateOrderForTracking = false;
   // unsigned long _lastOrderTime = 0;
   // unsigned long _lastExternalUpdateTime = 0;
  //  String _lastExternalUpdateSource = "";
    int bar1 = 0;
  //  String valrt = "";
    ValueBinder* binder = nullptr;
    String _lastSentValue = "";
    bool _ignoreNextSync = false;

public:
    Panel_bar(String parameters);
    ~Panel_bar();

    void onRegEvent(IoTItem* eventItem) override;
    void setValue(const IoTValue& Value, bool genEvent = true) override;
    String getValue() override;
    IoTValue execute(String command, std::vector<IoTValue> &param) override;
    void onMqttRecive(String& topic, String& msg) override;
    void updateUI();
    void onModuleOrder(String& key, String& value) override;

private:
  //  bool isCurrentOrderForMe();
    void drawItem(IoTItem *item);
};
