#pragma once

#include "../../PanelCommon.h"
#include "Global.h"
#include "lvgl.h"
#include <vector>
#include "../../ValueBinder.h"

// Global variables for Panel_arc
extern std::vector<lv_obj_t*> ui_Arc;
extern std::vector<String> arc_id;
extern int arc;
extern float valrt;

extern std::vector<lv_obj_t*> ui_TabPage;
extern int tab2;
extern std::vector<lv_obj_t*> ui_Button;
extern int isf1;
extern std::vector<lv_obj_t*> ui_Container;
extern int cont;
extern lv_obj_t* ui_Screen1;

// Forward declarations
class IoTItem;

// Event handler
//void ui_event_Arc(lv_event_t * e);

// Widget class
class Panel_arc : public IoTItem {
private:
    int nomer = 1;
    String  name = "";
    int _x = 0, _y = 0, _x1 = 0, _y1 =0;
    int _x2 = 0, _y2 = 100, _x3 = 120, _y3 = 60;
    int color = 0x1A7D35;
    int  size = 15;
    String _id2show = "";
    float _step = 1.0f;  //шаг дуги
  //  String _valueSource = "unknown";
  //  bool _pendingMqttUpdate = false;
  //  bool _generateOrderForTracking = false;
   // unsigned long _lastOrderTime = 0;
   // unsigned long _lastExternalUpdateTime = 0;
   // String _lastExternalUpdateSource = "";
    int arc1 = 0;
  //  String valrt_member = "";
  //  int j = 0;
    ValueBinder* binder = nullptr;
    String _lastSentValue = "";
    bool _ignoreNextSync = false;
    int _round = 0;

public:
    Panel_arc(String parameters);
    ~Panel_arc();

    void onRegEvent(IoTItem* item) override;
    void setValue(const IoTValue& Value, bool genEvent = true) override;
    String getValue() override;
    IoTValue execute(String command, std::vector<IoTValue> &param) override;
    void onMqttRecive(String& topic, String& msg) override;
    void updateUI();
    void onModuleOrder(String& key, String& value) override;

private:
    String formatValue() const;
  //  bool isCurrentOrderForMe();
  static void eventHandler(lv_event_t * e);
    void drawItem(IoTItem *item);
};
