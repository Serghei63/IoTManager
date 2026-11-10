#pragma once

#include "../../PanelCommon.h"
#include "Global.h"
#include "../../ValueBinder.h"
#include "lvgl.h"
#include <vector>

// Global variables for Panel_slider
extern std::vector<lv_obj_t*> ui_Slaider;
extern std::vector<String> Slaider_id;
extern int sla;

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
void ui_event_Slaider(lv_event_t * e);

// Widget class
class Panel_slider : public IoTItem {
private:
    int nomer = 1;
    String  name = "";
    int _x = 0, _y = 0, _x1 = 0, _y1 =0;
    int _x2 = 0, _y2 = 100, _x3 = 120, _y3 = 60;
    int color = 15792634, color_indicator = 13504009, color_knob = 4142987;
    int valy = 0, scal = 0, scale_division = 11;
    lv_obj_t * scale = NULL;
    String _id2show = "";
    int _round = 1;
    String _valueSource = "unknown";
    bool _pendingMqttUpdate = false;
    bool _generateOrderForTracking = false;
    unsigned long _lastOrderTime = 0;
    unsigned long _lastExternalUpdateTime = 0;
    String _lastExternalUpdateSource = "";
    int sla1 = 0;
    String valrt = "";

    // ValueBinder для синхронизации с _id2show
    ValueBinder* binder = nullptr;
    bool _ignoreNextSync = false;
    String _lastSentValue = "";

public:
    Panel_slider(String parameters);
    ~Panel_slider();

    void onRegEvent(IoTItem* eventItem) override;
    void onModuleOrder(String& key, String& value) override;
    void setValue(const IoTValue& Value, bool genEvent = true) override;
    String getValue() override;
    IoTValue execute(String command, std::vector<IoTValue> &param) override;
    void onMqttRecive(String& topic, String& msg) override;

private:
    String formatValue() const;
    bool isCurrentOrderForMe();
    void drawItem(IoTItem *item);
    void updateUI();
};
