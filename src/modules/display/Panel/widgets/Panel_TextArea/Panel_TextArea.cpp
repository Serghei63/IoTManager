#include "Panel_TextArea.h"
#include "classes/IoTItem.h"

// Global variables definition
std::vector<lv_obj_t*> ui_TextArea(0, NULL);
std::vector<String> TextArea_id(0, "");
int tex = 0;
std::vector<lv_obj_t*> ui_Keyboard1(0, NULL);

// Event handler implementations
void ui_event_key(lv_event_t * e)
{
   int i = (int)lv_event_get_user_data(e);
  
    lv_event_code_t event_code = lv_event_get_code(e);

    String vert = "";

    if (event_code == LV_EVENT_READY){ 
        vert = lv_textarea_get_text(ui_TextArea[i]);
        generateOrder( TextArea_id[i], vert);
    }       
}

void ui_event_TextArea(lv_event_t * e)
{
   int i = (int)lv_event_get_user_data(e);
  
    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_CLICKED){ 
        if (lv_obj_has_flag(ui_Keyboard1[i],LV_OBJ_FLAG_HIDDEN)){
            lv_obj_clear_flag(ui_Keyboard1[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(ui_Keyboard1[i]);
        }
        else {
            lv_obj_add_flag(ui_Keyboard1[i], LV_OBJ_FLAG_HIDDEN);
        }
    }    
}

// Panel_TextArea implementation
Panel_TextArea::Panel_TextArea(String parameters) : IoTItem(parameters) {

    jsonRead(parameters, "NAME", name);
    jsonRead(parameters, "Nomer", nomer);

    ui_Keyboard1.push_back( lv_keyboard_create(ui_Screen1));

    if (name == "TabPage" && tab2 >= nomer ) 
    {
            ui_TextArea.push_back( lv_textarea_create( ui_TabPage[nomer - 1]));
    }
    else if (name == "Button" && isf1 >= nomer ) 
    {   
            ui_TextArea.push_back( lv_textarea_create( ui_Button[nomer - 1]));
    }
    else if (name == "Container" && cont >= nomer ) {
         
            ui_TextArea.push_back( lv_textarea_create( ui_Container[nomer - 1]));
    }
    else 
    {
            ui_TextArea.push_back( lv_textarea_create( ui_Screen1)); 
    } 
    
    jsonRead(parameters, "id2show", _id2show);
    jsonRead(parameters, "shrift", shr);
    jsonRead(parameters, "x", _x);
    jsonRead(parameters, "y", _y);
    jsonRead(parameters, "width", _x1);
    jsonRead(parameters, "height", _y1);
    jsonRead(parameters, "color", color);
    jsonRead(parameters, "color_text", color_text);
    jsonRead(parameters, "round", _round);
    jsonRead(parameters, "shrift", shr);
   

     TextArea_id.push_back(_id);
      

    lv_keyboard_set_mode(ui_Keyboard1[tex], LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_width(ui_Keyboard1[tex], 370);
    lv_obj_set_height(ui_Keyboard1[tex], 180);
    lv_obj_set_x(ui_Keyboard1[tex], 30);
    lv_obj_set_y(ui_Keyboard1[tex], _y + 60 + _y1);
    lv_obj_set_align(ui_Keyboard1[tex], LV_ALIGN_DEFAULT);
    lv_obj_add_flag(ui_Keyboard1[tex], LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_width(ui_TextArea[tex], _x1);
    lv_obj_set_height(ui_TextArea[tex], _y1);
    lv_obj_set_x(ui_TextArea[tex], _x);
    lv_obj_set_y(ui_TextArea[tex], _y);
    lv_obj_set_align(ui_TextArea[tex], LV_ALIGN_DEFAULT);
    lv_textarea_set_text(ui_TextArea[tex], value.valS.c_str());
    lv_textarea_set_placeholder_text(ui_TextArea[tex], "0");
    lv_obj_set_style_text_color(ui_TextArea[tex], lv_color_hex(color_text), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_TextArea[tex], lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_keyboard_set_textarea(ui_Keyboard1[tex],ui_TextArea[tex]);

    shrit = serg20;
    conshrift( shrit , shr );
      
    lv_obj_set_style_text_font(ui_TextArea[tex], &shrit, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_TextArea[tex], ui_event_TextArea, LV_EVENT_ALL, (void*)tex);
    lv_obj_add_event_cb(ui_Keyboard1[tex], ui_event_key, LV_EVENT_ALL, (void*)tex);
    lv_obj_add_event_cb(ui_Keyboard1[tex], ui_event_Scrin, LV_EVENT_PRESSED, (void*)1);

    tex1 = tex;
    tex = tex + 1;

    // ValueBinder инициализация для синхронизации с _id2show
    if (!_id2show.isEmpty()) {
        binder = new ValueBinder(_id, _id2show);

        // Обработчик отправки команды
        binder->onSendOrder = [this](const String& key, const String& value) {
            generateOrder(key, value);
        };

        // Обработчик отправки значения на удаленный модуль
        binder->onSendRemoteValue = [this](const String& key, const String& value) {
            if (mqttIsConnect() == 1) {
                String json = "{}";
                jsonWriteStr_(json, "id", key);
                jsonWriteStr_(json, "val", value);
                jsonWriteInt_(json, "int", 0);
                publishEvent(key, json);
            }
        };

        // Обработчик изменения значения из другого модуля
        binder->onRemoteChange = [this](const Value& val, bool genEvent) {

            IoTValue iotVal;
            iotVal.valS = val.toString();
            iotVal.valD = val.valS.toFloat();
            iotVal.isDecimal = false; // TextArea работает со строками

            this->_ignoreNextSync = true;
            this->setValue(iotVal, genEvent);
        };

        // Обработчик локального изменения
        binder->onLocalChange = [this](const Value& val) {
            IoTValue iotVal;
            iotVal.valS = val.toString();
            iotVal.valD = val.valS.toFloat();
            iotVal.isDecimal = false;

            this->_ignoreNextSync = true;
            this->setValue(iotVal, false);
        };
    }
}

Panel_TextArea::~Panel_TextArea() {
    if (binder) delete binder;
}

void Panel_TextArea::onRegEvent(IoTItem* eventItem) {
    // Стандартная привязка к локальному элементу
    if (binder && !binder->isLocalTarget && eventItem && eventItem->getID() == binder->targetId) {
        binder->checkAndBind();
    }

    // Реакция на локальное изменение _id2show
    binder && binder->handleTargetLocalEvent(eventItem, true);

    IoTItem::onRegEvent(eventItem);
}

void Panel_TextArea::setValue(const IoTValue& Value, bool genEvent) {
    String formattedValue;
    if (_ignoreNextSync) {
        value = Value;
        formattedValue = getValue();
        _lastSentValue = formattedValue;
        updateUI();
        _ignoreNextSync = false;
        if (genEvent) {
            regEvent(formattedValue, F("Panel_TextArea"), false, true);
        }
        return;
    }

    value = Value;
    formattedValue = getValue();
    updateUI();
    if (genEvent) {
        regEvent(formattedValue, F("Panel_TextArea"), false, true);
    }
}

String Panel_TextArea::getValue() {
    if (!value.valS.isEmpty()) {
        if (isNumeric(value.valS)) {
            float num = value.valS.toFloat();
            char buf[32];
            dtostrf(num, 1, _round, buf);
            return String(buf);
        }
        return value.valS;
    }
    char buf[32];
    dtostrf(value.valD, 1, _round, buf);
    return String(buf);
}

void Panel_TextArea::onModuleOrder(String& key, String& value) {
    if (binder) {
        if (key == binder->id) {
            binder->setValueLocally(value);
        } else if (key == binder->targetId) {
            binder->setRemoteValue(Value::parse(value), true);
        }
    }
    IoTItem::onModuleOrder(key, value);
}

IoTValue Panel_TextArea::execute(String command, std::vector<IoTValue> &param){
    if (command == "hide"){
        lv_obj_add_flag(ui_TextArea[tex - 1], LV_OBJ_FLAG_HIDDEN);
    }
    
    if (command == "show"){
         lv_obj_clear_flag(ui_TextArea[tex - 1], LV_OBJ_FLAG_HIDDEN);
    }

     if (command == "color"){
         color = param[0].valD;
         lv_obj_set_style_bg_color(ui_TextArea[tex - 1], lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (command == "pos"){
        _x = param[0].valD;
        _y = param[1].valD;
        lv_obj_set_pos(ui_TextArea[tex - 1], _x, _y);
    }
    return {};
}

void Panel_TextArea::onMqttRecive(String& topic, String& msg) {
    if (binder && binder->handleMqttMessage(topic, msg)) {
        String valueStr = msg;
        jsonRead(msg, F("status"), valueStr, false);
    }

    IoTItem::onMqttRecive(topic, msg);
}



void Panel_TextArea::updateUI() {
    // Обновить визуальное состояние
    String currentValue;
    if (!value.valS.isEmpty()) {
        if (isNumeric(value.valS)) {
            float num = value.valS.toFloat();
            char buf[32];
            dtostrf(num, 1, _round, buf);
            currentValue = String(buf);
        } else {
            currentValue = value.valS;
        }
    } else {
        char buf[32];
        dtostrf(value.valD, 1, _round, buf);
        currentValue = String(buf);
    }

    lv_textarea_set_text(ui_TextArea[tex1], currentValue.c_str());

    if (_ignoreNextSync) return;

    // Если есть привязка и значение изменилось
    if (binder && _id != _id2show && !_id2show.isEmpty()) {
        if (_lastSentValue != currentValue) {
            _lastSentValue = currentValue;
            binder->setValueLocally(currentValue);
        }
    }
}