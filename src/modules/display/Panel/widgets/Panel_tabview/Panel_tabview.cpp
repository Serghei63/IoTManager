#include "Panel_tabview.h"
#include "classes/IoTItem.h"

// Global variables definition
std::vector<lv_obj_t*> ui_TabView(0, NULL);
int tab = 0, tab2 = 0, tab3 = 0;
std::vector<lv_obj_t*> ui_TabPage(0, NULL);

// Panel_tabview implementation
Panel_tabview::Panel_tabview(String parameters) : IoTItem(parameters) {  

    jsonRead(parameters, "x", _x);
    jsonRead(parameters, "y", _y);
    jsonRead(parameters, "size", tab1);
    jsonRead(parameters, "width", _x1);
    jsonRead(parameters, "height", _y1);
    jsonRead(parameters, "color", color);
    jsonRead(parameters, "TabPage", tab2);
    jsonRead(parameters, "shrift", shrift);
    jsonRead(parameters, "position", position);

    ffffgg = serg20;
    conshrift( ffffgg, shrift);

    jsonRead(parameters, "NAME", name);
    jsonRead(parameters, "Nomer", nomer);

    if (name == "TabPage" && tab3 >= nomer){
            ui_TabView.push_back( lv_tabview_create( ui_TabPage[nomer - 1]));    
    }
    else if (name == "Button" && isf1 >= nomer ) {
         
            ui_TabView.push_back( lv_tabview_create( ui_Button[nomer - 1]));}

    else if (name == "Container" && cont >= nomer ) {
         
            ui_TabView.push_back(lv_tabview_create( ui_Container[nomer - 1]));}

    else {ui_TabView.push_back( lv_tabview_create( ui_Screen1)); }

    if (position == "left"){
            lv_tabview_set_tab_bar_position(ui_TabView[tab], LV_DIR_LEFT); }
    else if (position == "right"){
            lv_tabview_set_tab_bar_position(ui_TabView[tab], LV_DIR_RIGHT); }
    else if (position == "bottom"){
            lv_tabview_set_tab_bar_position(ui_TabView[tab], LV_DIR_BOTTOM); }
     else {
            lv_tabview_set_tab_bar_position(ui_TabView[tab], LV_DIR_TOP); }        

    lv_tabview_set_tab_bar_size(ui_TabView[tab], tab1);
    lv_obj_set_x(ui_TabView[tab], _x);
    lv_obj_set_y(ui_TabView[tab], _y);
    lv_obj_set_width(ui_TabView[tab], _x1);
    lv_obj_set_height(ui_TabView[tab], _y1);
    lv_obj_set_align(ui_TabView[tab], LV_ALIGN_DEFAULT);
    lv_obj_add_flag(ui_TabView[tab], LV_OBJ_FLAG_SCROLLABLE); 
    lv_obj_set_style_text_font(lv_tabview_get_tab_bar(ui_TabView[tab]),  &ffffgg,
                           LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(lv_tabview_get_tab_bar(ui_TabView[tab]), lv_color_hex(color),
                          LV_PART_MAIN | LV_STATE_DEFAULT);
                         
     
     int b = tab3;                       

     for (size_t i = b; i < tab2 + b; i++)
     {
       
        int p = i + 1 - b, color10 = 0;
        String _i = String(p);
        String tex = "text" + _i;
        String text = "";
        String col = "color" + _i;

        jsonRead(parameters, tex, text);
        jsonRead(parameters, col, color10);

        const char*  _text = text.c_str();
        ui_TabPage.push_back( lv_tabview_add_tab(ui_TabView[tab], _text));
        lv_obj_set_style_bg_color(ui_TabPage[i], lv_color_hex(color10), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_TabPage[i], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_add_event_cb(ui_TabPage[i], ui_event_Scrin, LV_EVENT_PRESSED, (void*)1);

        tab3 = tab3 + 1;
     }
    
tab = tab + 1;
}

IoTValue Panel_tabview::execute(String command, std::vector<IoTValue> &param){
    if (command == "hide"){
        lv_obj_add_flag(ui_TabView[tab - 1], LV_OBJ_FLAG_HIDDEN);
    }
    
    if (command == "show"){
         lv_obj_clear_flag(ui_TabView[tab - 1], LV_OBJ_FLAG_HIDDEN);
    }

     if (command == "color"){
         color = param[0].valD;
         int r = param[1].valD;
         lv_obj_set_style_bg_color(ui_TabPage[r], lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (command == "pos"){
        _x = param[0].valD;
        _y = param[1].valD;
        lv_obj_set_pos(ui_TabView[tab - 1], _x, _y);
    }
    if (command == "active"){
        int a = param[0].valD;
        lv_tabview_set_act(ui_TabView[tab - 1], a - 1 , LV_ANIM_ON);
    }
    return {};
}

void Panel_tabview::onMqttRecive(String& topic, String& msg) {
    IoTItem::onMqttRecive(topic, msg);
}

Panel_tabview::~Panel_tabview(){}
