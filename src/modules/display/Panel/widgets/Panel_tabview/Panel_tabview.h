#pragma once

#include "../../PanelCommon.h"
#include "Global.h"
#include "lvgl.h"
#include <vector>

// Global variables for Panel_tabview
extern std::vector<lv_obj_t*> ui_TabView;
extern int tab;
extern std::vector<lv_obj_t*> ui_TabPage;
extern int tab2;
extern int tab3;
extern std::vector<lv_obj_t*> ui_Button;
extern int isf1;
extern std::vector<lv_obj_t*> ui_Container;
extern int cont;
extern lv_obj_t* ui_Screen1;

// Forward declarations
class IoTItem;

// Widget class
class Panel_tabview : public IoTItem {
private:
    int _x = 0, _y = 0,  _x1 = 20, _y1 = 20,  tab1 = 50,  shrift = 20;  
    int color = 0;
    lv_font_t ffffgg;
    int nomer = 1;
    String  name = "";
    String position = "";

public:
    Panel_tabview(String parameters);
    ~Panel_tabview();

    IoTValue execute(String command, std::vector<IoTValue> &param) override;
    void onMqttRecive(String& topic, String& msg) override;
};
