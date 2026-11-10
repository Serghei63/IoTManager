#include "Panel_bar.h"
#include "classes/IoTItem.h"

// Global variables definition
std::vector<lv_obj_t*> ui_Bar(0, NULL);
std::vector<String> bar_id(0, "");
int bar = 0;

// Panel_bar implementation
Panel_bar::Panel_bar(String parameters) : IoTItem(parameters) {

    jsonRead(parameters, "NAME", name);
    jsonRead(parameters, "Nomer", nomer);

    String rt56 = "";
    jsonRead(parameters, "id2show", _id2show);

    bar_id.push_back(_id);

    if (name == "TabPage" && tab2 >= nomer ) 
    {
            ui_Bar.push_back( lv_bar_create( ui_TabPage[nomer - 1]));
    }
    else if (name == "Button" && isf1 >= nomer ) 
    {   
            ui_Bar.push_back( lv_bar_create( ui_Button[nomer - 1]));
    }
    else if (name == "Container" && cont >= nomer ) {
         
            ui_Bar.push_back( lv_bar_create( ui_Container[nomer - 1]));}
    else 
    {
            ui_Bar.push_back( lv_bar_create( ui_Screen1)); 
    } 

    jsonRead(parameters, "x", _x);
    jsonRead(parameters, "y", _y);
    jsonRead(parameters, "width", _x1);
    jsonRead(parameters, "height", _y1);
    jsonRead(parameters, "color", color);
    jsonRead(parameters, "range_sart", _x2);
    jsonRead(parameters, "range_end", _y2);
     
    lv_obj_set_width( ui_Bar[bar], _x1);
    lv_obj_set_height( ui_Bar[bar], _y1);
    lv_obj_set_x( ui_Bar[bar], _x);
    lv_obj_set_y( ui_Bar[bar], _y);
    lv_bar_set_range( ui_Bar[bar], _x2, _y2);
    lv_bar_set_value(ui_Bar[bar], value.valD , LV_ANIM_OFF);
    lv_bar_set_start_value(ui_Bar[bar], 40, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(ui_Bar[bar], lv_color_hex(color), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Bar[bar], 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        
    bar1 = bar;
    bar = bar + 1;

    // Инициализация ValueBinder
    if (_id2show != _id && !_id2show.isEmpty()) {
        binder = new ValueBinder(_id, _id2show);
    }
}

Panel_bar::~Panel_bar() {
    if (binder) {
        delete binder;
        binder = nullptr;
    }
}

void Panel_bar::onRegEvent(IoTItem* eventItem) {
    // Стандартная привязка к локальному элементу
    if (binder && !binder->isLocalTarget && eventItem && eventItem->getID() == binder->targetId) {
        binder->checkAndBind();
    }

    // Реакция на локальное изменение _id2show
    binder && binder->handleTargetLocalEvent(eventItem, true);

    IoTItem::onRegEvent(eventItem);
}

void Panel_bar::drawItem(IoTItem *item) {
    lv_bar_set_value(ui_Bar[bar1], value.valD , LV_ANIM_OFF);
updateUI();
}

void Panel_bar::setValue(const IoTValue& Value, bool genEvent) {
    value = Value;


    drawItem(this);
    regEvent((String)(int)value.valD, F("Panel_bar"), false, genEvent);
}

String Panel_bar::getValue() {
    return (String)(int)value.valD;
}

void Panel_bar::onModuleOrder(String& key, String& value) {
    if (binder) {
        if (key == binder->id) {
            binder->setValueLocally(value);
        } else if (key == binder->targetId) {
            binder->setRemoteValue(Value::parse(value), true);
        }
    }
    IoTItem::onModuleOrder(key, value);
}


IoTValue Panel_bar::execute(String command, std::vector<IoTValue> &param){
    if (command == "hide"){
        lv_obj_add_flag(ui_Bar[bar - 1], LV_OBJ_FLAG_HIDDEN);
    }
    
    if (command == "show"){
         lv_obj_clear_flag(ui_Bar[bar - 1], LV_OBJ_FLAG_HIDDEN);
    }

     if (command == "color"){
         color = param[0].valD;
         lv_obj_set_style_arc_color(ui_Bar[bar - 1], lv_color_hex(color), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
    if (command == "pos"){
        _x = param[0].valD;
        _y = param[1].valD;
        lv_obj_set_pos(ui_Bar[bar - 1], _x, _y);
    }
    return {};
}

void Panel_bar::onMqttRecive(String& topic, String& msg) {
    if (binder && binder->handleMqttMessage(topic, msg)) {
        String valueStr = msg;
        jsonRead(msg, F("status"), valueStr, false);
    }

    IoTItem::onMqttRecive(topic, msg);
}

void Panel_bar::updateUI() {
    // Обновить визуальное состояние
    lv_bar_set_value(ui_Bar[bar1], value.valD, LV_ANIM_OFF);

    if (_ignoreNextSync) return;

    // Если есть привязка и значение изменилось
    if (binder && _id != _id2show && !_id2show.isEmpty()) {
        String newValue = String((int)value.valD);
        if (_lastSentValue != newValue) {
            _lastSentValue = newValue;
            binder->setValueLocally(newValue);
        }
    }
}
