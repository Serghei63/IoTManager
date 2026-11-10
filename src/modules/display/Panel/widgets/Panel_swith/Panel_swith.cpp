#include "Panel_swith.h"
#include "classes/IoTItem.h"

// Global variables definition
std::vector<lv_obj_t*> ui_Switc(0, NULL);
std::vector<String> switch_id(0, "");

int isf = 0;

// Event handler implementation
void ui_event_Switch2(lv_event_t * e) {
    int i = (int)lv_event_get_user_data(e);
    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_VALUE_CHANGED) {
        bool v = lv_obj_has_state(ui_Switc[i], LV_STATE_CHECKED);
        
        if (v) {
            generateOrder(switch_id[i], "1");
        } else {
            generateOrder(switch_id[i], "0");
        }
    }
}

// Panel_swith implementation
Panel_swith::Panel_swith(String parameters) : IoTItem(parameters), binder(nullptr) {
    jsonRead(parameters, "NAME", name);
    jsonRead(parameters, "Nomer", nomer);

    if (name == "TabPage" && tab2 >= nomer) {
        ui_Switc.push_back(lv_switch_create(ui_TabPage[nomer - 1]));
    } else if (name == "Button" && isf1 >= nomer) {
        ui_Switc.push_back(lv_switch_create(ui_Button[nomer - 1]));
    } else if (name == "Container" && cont >= nomer) {
        ui_Switc.push_back(lv_switch_create(ui_Container[nomer - 1]));
    } else {
        ui_Switc.push_back(lv_switch_create(ui_Screen1));
    }

    lv_obj_add_event_cb(ui_Switc[isf], ui_event_Switch2, LV_EVENT_ALL, (void*)isf);

    jsonRead(parameters, "x", _x);
    jsonRead(parameters, "y", _y);
    jsonRead(parameters, "id2show", _id2show);
    jsonRead(parameters, "width", _x1);
    jsonRead(parameters, "height", _y1);
    jsonRead(parameters, "color_of", color_of);
    jsonRead(parameters, "color_on", color_on);


    switch_id.push_back(_id);

    lv_obj_set_width(ui_Switc[isf], _x1);
    lv_obj_set_height(ui_Switc[isf], _y1);
    lv_obj_set_align(ui_Switc[isf], LV_ALIGN_DEFAULT);
    lv_obj_set_x(ui_Switc[isf], _x);
    lv_obj_set_y(ui_Switc[isf], _y);

    lv_obj_set_style_bg_color(ui_Switc[isf], lv_color_hex(color_of), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Switc[isf], lv_color_hex(color_on), LV_PART_INDICATOR | LV_STATE_CHECKED);
    isf12 = isf;
    isf = isf + 1;

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
            iotVal.isDecimal = (val.type != Value::STRING);
            iotVal.valD = (val.type == Value::INT) ? val.valI :
                          (val.type == Value::FLOAT) ? val.valF :
                          val.valS.toFloat();
            iotVal.valS = val.toString();

            this->_ignoreNextSync = true;
            this->setValue(iotVal, genEvent);
        };

        // Обработчик локального изменения
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

Panel_swith::~Panel_swith() {
    if (binder) delete binder;
}

void Panel_swith::onRegEvent(IoTItem* eventItem) {
    // Стандартная привязка к локальному элементу
    if (binder && !binder->isLocalTarget && eventItem && eventItem->getID() == binder->targetId) {
        binder->checkAndBind();
    }

    // Реакция на локальное изменение _id2show
    binder && binder->handleTargetLocalEvent(eventItem, true);

    IoTItem::onRegEvent(eventItem);
}


void Panel_swith::updateUI() {
    // Обновить визуальное состояние
    if ((int)value.valD == 0) {
        lv_obj_clear_state(ui_Switc[isf12], LV_STATE_CHECKED);
    } else {
        lv_obj_add_state(ui_Switc[isf12], LV_STATE_CHECKED);
    }

    if (_ignoreNextSync) return;

    // Если есть привязка и значение изменилось
    if (binder && _id != _id2show && !_id2show.isEmpty()) {
        String newValue = String((int)value.valD);
        if (_lastSentValue != newValue) {
            _lastSentValue = newValue;
            binder->setValueLocally((int)value.valD);
        }
    }
}

void Panel_swith::onModuleOrder(String& key, String& value) {
    if (binder) {
        if (key == binder->id) {
            binder->setValueLocally((int)value.toInt());
        } else if (key == binder->targetId) {
            binder->setRemoteValue(Value::parse(value), true);
        }
    }
    IoTItem::onModuleOrder(key, value);
}

void Panel_swith::setValue(const IoTValue& Value, bool genEvent) {
    if (_ignoreNextSync) {
        value = Value;
        _lastSentValue = String((int)value.valD);
        updateUI();
        _ignoreNextSync = false;
        if (genEvent) {
            regEvent(String((int)value.valD), F("Panel_swith"), false, true);
        }
        return;
    }

    value = Value;
    updateUI();
    if (genEvent) {
        regEvent(String((int)value.valD), F("Panel_swith"), false, true);
    }
}

String Panel_swith::getValue() {
    return (String)(int)value.valD;
}

bool Panel_swith::isCurrentOrderForMe() {
    if (orderBuf.length() == 0) return false;
    String currentOrder = selectToMarker(orderBuf, ",");
    if (currentOrder.length() == 0) return false;
    String currentKey = selectToMarker(currentOrder, " ");
    return currentKey == _id;
}

IoTValue Panel_swith::execute(String command, std::vector<IoTValue> &param) {
    if (command == "hide") {
        lv_obj_add_flag(ui_Switc[isf - 1], LV_OBJ_FLAG_HIDDEN);
    } else if (command == "show") {
        lv_obj_clear_flag(ui_Switc[isf - 1], LV_OBJ_FLAG_HIDDEN);
    } else if (command == "pos") {
        _x = param[0].valD;
        _y = param[1].valD;
        lv_obj_set_pos(ui_Switc[isf - 1], _x, _y);
    }
    return {};
}

void Panel_swith::onMqttRecive(String& topic, String& msg) {
    if (binder && binder->handleMqttMessage(topic, msg)) {
        String valueStr = msg;
        jsonRead(msg, F("status"), valueStr, false);
    }

    IoTItem::onMqttRecive(topic, msg);
}
