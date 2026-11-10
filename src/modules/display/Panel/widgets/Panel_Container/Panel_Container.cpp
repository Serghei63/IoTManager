#include "Panel_Container.h"
#include "classes/IoTItem.h"

// Global variables definition
std::vector<lv_obj_t*> ui_Container(0, NULL);
int cont = 0;

// Panel_Container implementation
Panel_Container::Panel_Container(String parameters) : IoTItem(parameters) {

    jsonRead(parameters, "NAME", name);
    jsonRead(parameters, "Nomer", nomer);

    if (name == "TabPage" && tab2 >= nomer )
    {
            ui_Cont = lv_obj_create( ui_TabPage[nomer - 1]);
    }
    else if (name == "Button" && isf1 >= nomer ) 
    {   
            ui_Cont = lv_obj_create( ui_Button[nomer - 1]);
    }
    else 
    {
            ui_Cont = lv_obj_create( ui_Screen1); 
    } 
    
    jsonRead(parameters, "x", _x);
    jsonRead(parameters, "y", _y);
    jsonRead(parameters, "width", _x1);
    jsonRead(parameters, "height", _y1);
    jsonRead(parameters, "color", color);
    jsonRead(parameters, "color_border", color_border);
    jsonRead(parameters, "size_border", size_border);   

    lv_obj_remove_style_all(ui_Cont);
    lv_obj_set_x(ui_Cont, _x);
    lv_obj_set_y(ui_Cont, _y);
    lv_obj_set_width(ui_Cont, _x1);
    lv_obj_set_height(ui_Cont, _y1);
    lv_obj_set_align(ui_Cont, LV_ALIGN_DEFAULT);
    lv_obj_add_flag(ui_Cont,  LV_OBJ_FLAG_SCROLLABLE); 
    lv_obj_set_style_radius(ui_Cont, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Cont, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Cont, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Cont, lv_color_hex(color_border), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Cont, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Cont, size_border, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_scrollbar_mode(ui_Cont, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_add_event_cb(ui_Cont, ui_event_Scrin, LV_EVENT_PRESSED, (void*)1);
    
    ui_Container.push_back(ui_Cont);
    cont = cont + 1;
}

IoTValue Panel_Container::execute(String command, std::vector<IoTValue> &param){
    if (command == "hide"){
        lv_obj_add_flag(ui_Cont, LV_OBJ_FLAG_HIDDEN);
    }
    
    if (command == "show"){
         lv_obj_clear_flag(ui_Cont, LV_OBJ_FLAG_HIDDEN);
    }

     if (command == "color"){
         color = param[0].valD;
         lv_obj_set_style_bg_color(ui_Cont, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (command == "pos"){
        _x = param[0].valD;
        _y = param[1].valD;
        lv_obj_set_pos(ui_Cont, _x, _y);
    }
    return {};
}


Panel_Container::~Panel_Container(){}
