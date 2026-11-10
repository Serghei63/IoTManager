// Panel_img_icon.h

#pragma once

#include "../../PanelCommon.h"
#include "Global.h"

#include "Arduino.h"
#include "lvgl.h"
#include "weather_icons/weather_icons.h"
#include "../../ValueBinder.h"

extern std::vector<lv_obj_t*> ui_TabPage;
extern int tab2;
extern std::vector<lv_obj_t*> ui_Button;
extern int isf1;
extern std::vector<lv_obj_t*> ui_Container;
extern int cont;
extern lv_obj_t* ui_Screen1;


class IoTItem;

class Panel_img_icon : public IoTItem
{
public:
    Panel_img_icon(String param);
    ~Panel_img_icon();

    virtual void update();

    void setValue(const IoTValue& Value, bool genEvent = true) override;
    String getValue() override;

    void onRegEvent(IoTItem* eventItem) override;
    void onModuleOrder(String& key, String& value) override;
    void onMqttRecive(String& topic, String& msg) override;

private:
    void createObj();

    lv_obj_t* obj = nullptr;
    String iconCode;
    bool _ignoreNextSync = false;
     String _lastSentValue = "";
      String formatValue() const;
    String _id2show = "";
    int x = 0;
    int y = 0;
    ValueBinder* binder = nullptr;
    int nomer = 1;
    String name = "";
};