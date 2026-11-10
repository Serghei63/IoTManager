// Panel_button.cpp
#include "Panel_button.h"
#include "classes/IoTItem.h"

std::vector<lv_obj_t*> ui_Button;
std::vector<String> button_value;
int isf1 = 0;

extern std::vector<lv_obj_t*> ui_TabPage;
extern int tab2;
extern std::vector<lv_obj_t*> ui_Container;
extern int cont;
extern lv_obj_t* ui_Screen1;

void ui_event_Button(lv_event_t *e) {
    int i = (int)lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        bool isChecked = lv_obj_has_state(ui_Button[i], LV_STATE_CHECKED);
        
        // Определяем НОВОЕ состояние после клика
        String valueToSend = isChecked ? "0" : "1";

        // Отправляем команду
        generateOrder(button_value[i], valueToSend);

        // Меняем состояние кнопки
        if (isChecked) {
            lv_obj_clear_state(ui_Button[i], LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(ui_Button[i], LV_STATE_CHECKED);
        }
    }
}

void ui_event_Button1(lv_event_t *e) {
    int i = (int)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        lv_obj_add_state(ui_Button[i], LV_STATE_CHECKED);
        generateOrder(button_value[i], "1");
    } else if (code == LV_EVENT_RELEASED) {
        lv_obj_clear_state(ui_Button[i], LV_STATE_CHECKED);
        generateOrder(button_value[i], "0");
    }
}

Panel_button::Panel_button(String parameters) : IoTItem(parameters), binder(nullptr) {
    jsonRead(parameters, "NAME", name);
    jsonRead(parameters, "Nomer", nomer);

    lv_obj_t* parent = ui_Screen1;
    if (name == "TabPage" && tab2 >= nomer) {
        parent = ui_TabPage[nomer - 1];
    } else if (name == "Button" && isf1 >= nomer) {
        parent = ui_Button[nomer - 1];
    } else if (name == "Container" && cont >= nomer) {
        parent = ui_Container[nomer - 1];
    }

    ui_Button.push_back(lv_button_create(parent));
    button_value.push_back(_id);

    jsonRead(parameters, "diger", dig);
    jsonRead(parameters, "x", _x);
    jsonRead(parameters, "y", _y);
    jsonRead(parameters, "id2show", _id2show);
    jsonRead(parameters, "width", _x1);
    jsonRead(parameters, "height", _y1);
    jsonRead(parameters, "color_of", color_of);
    jsonRead(parameters, "color_on", color_on);
    jsonRead(parameters, "color_border", color_border);
    jsonRead(parameters, "size_border", size_border);
    jsonRead(parameters, "Visibility", Visibility);
    jsonRead(parameters, "radius", radius);

    lv_obj_set_size(ui_Button[isf1], _x1, _y1);
    lv_obj_set_pos(ui_Button[isf1], _x, _y);
    lv_obj_set_align(ui_Button[isf1], LV_ALIGN_DEFAULT);
    lv_obj_add_flag(ui_Button[isf1], LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_clear_flag(ui_Button[isf1], LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(ui_Button[isf1], lv_color_hex(color_of), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Button[isf1], lv_color_hex(color_on), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(ui_Button[isf1], Visibility, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Button[isf1], lv_color_hex(color_border), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Button[isf1], Visibility, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Button[isf1], size_border, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_Button[isf1], radius, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (dig == 0) {
        lv_obj_add_event_cb(ui_Button[isf1], ui_event_Button1, LV_EVENT_ALL, (void*)isf1);
    } else {
        lv_obj_add_event_cb(ui_Button[isf1], ui_event_Button, LV_EVENT_ALL, (void*)isf1);
    }

    lv_obj_add_event_cb(ui_Button[isf1], ui_event_Scrin, LV_EVENT_PRESSED, (void*)1);
    _buttonIndex = isf1++;

    // Инициализация ValueBinder для id2show
    if (!_id2show.isEmpty()) {
        binder = new ValueBinder(_id, _id2show);

        binder->onSendOrder = [this](const String& key, const String& value) {
            generateOrder(key, value);
        };

        binder->onSendRemoteValue = [this](const String& key, const String& value) {
            if (mqttIsConnect() == 1) {
                String json = "{}";
                jsonWriteStr_(json, "id", key);
                jsonWriteStr_(json, "val", value);
                jsonWriteInt_(json, "int", 0);

                SerialPrint("i", F("=>MQTT"), "Generate remote event(no echo): " + json);

                publishEvent(key, json);
            }
        };

        binder->onRemoteChange = [this](const Value& val, bool genEvent) {

            IoTValue iotVal;
            iotVal.isDecimal = (val.type != Value::STRING);
            iotVal.valD = (val.type == Value::INT) ? val.valI :
                          (val.type == Value::FLOAT) ? val.valF :
                          val.valS.toFloat();
            iotVal.valS = val.toString();

            this->_ignoreNextSync = true;
            this->setValue(iotVal, genEvent);
        };

        binder->onLocalChange = [this](const Value& val) {
            IoTValue iotVal;
            iotVal.isDecimal = (val.type != Value::STRING);
            iotVal.valD = (val.type == Value::INT) ? val.valI :
                          (val.type == Value::FLOAT) ? val.valF :
                          val.valS.toFloat();
            iotVal.valS = val.toString();

            this->_ignoreNextSync = true;
            this->setValue(iotVal, false);
        };
    }
}

Panel_button::~Panel_button() {
    delete binder;
}

void Panel_button::onRegEvent(IoTItem* eventItem) {
    // Стандартная привязка к локальному элементу
    if (binder && !binder->isLocalTarget && eventItem && eventItem->getID() == binder->targetId) {
        binder->checkAndBind();
    }

    // Реакция на локальное изменение _id2show
    if (binder && binder->handleTargetLocalEvent(eventItem, true)) {
        String valStr = eventItem->getValue();
        int intValue = valStr.toInt();

        IoTValue iotVal;
        iotVal.isDecimal = true;
        iotVal.valD = intValue;
        iotVal.valS = valStr;
        this->_ignoreNextSync = true;
        this->setValue(iotVal, true);
    }

    IoTItem::onRegEvent(eventItem);
}

void Panel_button::onMqttRecive(String& topic, String& msg) {
    if (binder && binder->handleMqttMessage(topic, msg)) {
        String valueStr = msg;
        if (jsonRead(msg, F("status"), valueStr, false) || jsonRead(msg, F("Status"), valueStr, false) ||
            jsonRead(msg, F("value"), valueStr, false) || jsonRead(msg, F("val"), valueStr, false)) {
        }
        valrt = valueStr;
    }

    IoTItem::onMqttRecive(topic, msg);
}

void Panel_button::onModuleOrder(String& key, String& value) {
    if (binder) {
        if (key == binder->id) {
            binder->setValueLocally((int)value.toInt());
        } else if (key == binder->targetId) {
            binder->setRemoteValue(Value::parse(value), true);
        }
    }
    IoTItem::onModuleOrder(key, value);
}

void Panel_button::setValue(const IoTValue& Value, bool genEvent) {
    if (_ignoreNextSync) {
        value = Value;
        _lastSentValue = String((int)value.valD);
        updateUI();
        _ignoreNextSync = false;
        if (genEvent) {
            regEvent(String((int)value.valD), F("Panel_button"), false, true);
        }
        return;
    }

    value = Value;
    updateUI();
    if (genEvent) {
        regEvent(String((int)value.valD), F("Panel_button"), false, true);
    }
}

String Panel_button::getValue() {
    return String((int)value.valD);
}

IoTValue Panel_button::execute(String command, std::vector<IoTValue> &param) {
    if (command == "hide") {
        lv_obj_add_flag(ui_Button[_buttonIndex], LV_OBJ_FLAG_HIDDEN);
    } else if (command == "show") {
        lv_obj_clear_flag(ui_Button[_buttonIndex], LV_OBJ_FLAG_HIDDEN);
    } else if (command == "color_on" && !param.empty()) {
        color_on = param[0].valD;
        lv_obj_set_style_bg_color(ui_Button[_buttonIndex], lv_color_hex(color_on), LV_PART_MAIN | LV_STATE_CHECKED);
    } else if (command == "color_of" && !param.empty()) {
        color_of = param[0].valD;
        lv_obj_set_style_bg_color(ui_Button[_buttonIndex], lv_color_hex(color_of), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (command == "pos" && param.size() >= 2) {
        _x = param[0].valD;
        _y = param[1].valD;
        lv_obj_set_pos(ui_Button[_buttonIndex], _x, _y);
    }
    return IoTValue();
}

void Panel_button::updateUI() {
    if ((int)value.valD == 0) {
        lv_obj_clear_state(ui_Button[_buttonIndex], LV_STATE_CHECKED);
    } else {
        lv_obj_add_state(ui_Button[_buttonIndex], LV_STATE_CHECKED);
    }

    if (_ignoreNextSync) {
        return; 
    }

    if (binder && _id != _id2show && !_id2show.isEmpty()) {
        String newValue = String((int)value.valD);
        if (_lastSentValue != newValue) {
            _lastSentValue = newValue;
            binder->setValueLocally((int)value.valD);
        }
    }
}

void Panel_button::drawItem(IoTItem *item) {
    updateUI();
}


