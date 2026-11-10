#pragma once

#include "../../PanelCommon.h"
#include "Global.h"
#include "lvgl.h"
#include <vector>

// Global variables for Panel_img
extern std::vector<lv_obj_t*> ui_Image;
extern int img;

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
class Panel_img : public IoTItem {
private:
    int nomer = 1;
    String  name = "";
    int _x = 0, _y = 0, _x1 = 0, _y1 = 0;
    int _x2 = 0, _y2 = 100, _x3 = 120, _y3 = 60;
    int color = 0x1A7D35;
    int valy = 0;
    int currentImageIndex = -1;  // ВАЖНО: индекс текущего изображения

public:
    Panel_img(String parameters);
    ~Panel_img();

    void setValue(const IoTValue& Value, bool genEvent = true) override;
    String getValue() override;
    
    // Execute method for handling commands
    IoTValue execute(String command, std::vector<IoTValue> &param) override;
};