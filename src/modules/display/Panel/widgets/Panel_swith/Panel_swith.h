#pragma once

#include "../../PanelCommon.h"
#include "Global.h"
#include "../../ValueBinder.h"
#include "lvgl.h"
#include <vector>

// Global variables for Panel_swith
extern std::vector<lv_obj_t*> ui_Switc;
extern std::vector<String> switch_id;

extern int isf;

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
void ui_event_Switch2(lv_event_t * e);

// Widget class
class Panel_swith : public IoTItem {
private:
    int _x = 0, _y = 0;
    int nomer = 1;
    String name = "";
    int _x1 = 60, _y1 = 30;
    int color_on = 578, color_of = 57645;
    String _id2show = "";
    String _valueSource = "unknown";
    bool _pendingMqttUpdate = false;
    bool _generateOrderForTracking = false;
    unsigned long _lastOrderTime = 0;
    unsigned long _lastExternalUpdateTime = 0;
    String _lastExternalUpdateSource = "";
    int isf12 = 0;

    // ValueBinder для синхронизации с _id2show
    ValueBinder* binder = nullptr;
    bool _ignoreNextSync = false;
    String _lastSentValue = "";

public:
    Panel_swith(String parameters);
    ~Panel_swith();

    void onRegEvent(IoTItem* eventItem) override;
    void onModuleOrder(String& key, String& value) override;
    void setValue(const IoTValue& Value, bool genEvent = true) override;
    String getValue() override;
    IoTValue execute(String command, std::vector<IoTValue> &param) override;
    void onMqttRecive(String& topic, String& msg) override;

private:
    bool isCurrentOrderForMe();
    void drawItem(IoTItem *item);
    void updateUI();
    String valrt = "";
};
