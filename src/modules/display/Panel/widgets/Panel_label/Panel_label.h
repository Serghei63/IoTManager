#pragma once

#include "../../PanelCommon.h"
#include "Global.h"
#include "../../ValueBinder.h"
#include "lvgl.h"
#include <vector>

//extern lv_obj_t* ui_Label;
//extern int nb;

extern std::vector<lv_obj_t*> ui_TabPage;
extern int tab2;
extern std::vector<lv_obj_t*> ui_Button;
extern int isf1;
extern std::vector<lv_obj_t*> ui_Container;
extern int cont;
extern lv_obj_t* ui_Screen1;

class IoTItem;

class Panel_label : public IoTItem {
private:
    int nomer = 1;
    String name = "";
    int _x = 0, _y = 0, shr = 40;
    int color = 0x1A7D35;
    lv_font_t shrit = serg20;
    String _id2show = "";
    String descr = "";
    String descr1 = "";
    // int nb1 = 0;
    int _round = 0;
    String valrtS = "";
    float _valrtD = 0;

    lv_obj_t* ui_Label = nullptr;
  //  bool Tracking = false;

    // ValueBinder для синхронизации с _id2show
    ValueBinder* binder = nullptr;
    bool _ignoreNextSync = false;
    String _lastSentValue = "";

public:
    Panel_label(String parameters);
    ~Panel_label();

    void onRegEvent(IoTItem* eventItem) override;
    void onModuleOrder(String& key, String& value) override;
    void setValue(const IoTValue& Value, bool genEvent = true) override;
    String getValue() override;
    IoTValue execute(String command, std::vector<IoTValue> &param) override;
    void onMqttRecive(String& topic, String& msg) override;
    

private:
    void drawItem(IoTItem *item);
    void updateUI();
    String formatValue() const;
    //void onTrackingValue(IoTItem* item) override;


};
