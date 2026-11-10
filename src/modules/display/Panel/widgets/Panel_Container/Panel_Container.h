#pragma once

#include "../../PanelCommon.h"
#include "Global.h"
#include "lvgl.h"
#include <vector>

// Global variables for Panel_Container
extern std::vector<lv_obj_t*> ui_Container;
extern int cont;

extern std::vector<lv_obj_t*> ui_TabPage;
extern int tab2;
extern std::vector<lv_obj_t*> ui_Button;
extern int isf1;
extern lv_obj_t* ui_Screen1;

// Forward declarations
class IoTItem;

// Widget class
class Panel_Container : public IoTItem {
private:
    int nomer = 1;
    String  name = "";
    int _x = 0, _y = 0, _x1 = 0, _y1 =0;
    int color = 0x1A7D35;
    int color_border = 35435, size_border = 5;
    lv_obj_t * ui_Cont = NULL;

public:
    Panel_Container(String parameters);
    ~Panel_Container();

    IoTValue execute(String command, std::vector<IoTValue> &param) override;
 //   void onMqttRecive(String& topic, String& msg) override;
};
