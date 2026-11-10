#pragma once

#include "../../PanelCommon.h"
#include "Global.h"
#include "../../ValueBinder.h"
#include "lvgl.h"
#include <vector>

// Global variables for Panel_TextArea
extern std::vector<lv_obj_t*> ui_TextArea;
extern std::vector<String> TextArea_id;
extern int tex;
extern std::vector<lv_obj_t*> ui_Keyboard1;

extern std::vector<lv_obj_t*> ui_TabPage;
extern int tab2;
extern std::vector<lv_obj_t*> ui_Button;
extern int isf1;
extern std::vector<lv_obj_t*> ui_Container;
extern int cont;
extern lv_obj_t* ui_Screen1;

// Forward declarations
class IoTItem;

// Event handlers
void ui_event_key(lv_event_t * e);
void ui_event_TextArea(lv_event_t * e);

// Widget class
class Panel_TextArea : public IoTItem {
private:
    int nomer = 1;
    String  name = "";
    int _x = 0, _y = 0, _x1 = 100, _y1 = 50, shr = 40;
    int color = 0x1A7D35;
    int color_border = 35435, size_border = 5;
    int color_text = 0xFFFFFF;
    int _round = 0;
    String valy = "";
    lv_font_t shrit = serg40;
    int tex1= 0;
    String _id2show = "";
    String _valueSource = "unknown";
  //  bool _pendingMqttUpdate = false;
   // bool _generateOrderForTracking = false;
  //  unsigned long _lastOrderTime = 0;
   // unsigned long _lastExternalUpdateTime = 0;
   // String _lastExternalUpdateSource = "";
   // String valrt = "";

    // ValueBinder для синхронизации с _id2show
    ValueBinder* binder = nullptr;
    bool _ignoreNextSync = false;
    String _lastSentValue = "";

public:
    Panel_TextArea(String parameters);
    ~Panel_TextArea();

    void onRegEvent(IoTItem* eventItem) override;
    void onModuleOrder(String& key, String& value) override;
    void setValue(const IoTValue& Value, bool genEvent = true) override;
    String getValue() override;
    IoTValue execute(String command, std::vector<IoTValue> &param) override;
    void onMqttRecive(String& topic, String& msg) override;

private:
    void drawItem(IoTItem *item);
    void updateUI();

private:
    bool isCurrentOrderForMe();
};
