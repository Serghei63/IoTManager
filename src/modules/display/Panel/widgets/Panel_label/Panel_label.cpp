#include "Panel_label.h"
#include "classes/IoTItem.h"

//std::vector<lv_obj_t*> ui_Label(0, NULL);
extern "C" const lv_font_t serg45;
//int nb = 0;


Panel_label::Panel_label(String parameters) : IoTItem(parameters) {
    jsonRead(parameters, "NAME", name);
    jsonRead(parameters, "Nomer", nomer);

    if (name == "TabPage" && tab2 >= nomer) {
        ui_Label = lv_label_create(ui_TabPage[nomer - 1]);
    } else if (name == "Button" && isf1 >= nomer) {
        ui_Label = lv_label_create(ui_Button[nomer - 1]);
    } else if (name == "Container" && cont >= nomer) {
        ui_Label = lv_label_create(ui_Container[nomer - 1]);
    } else {
        ui_Label = lv_label_create(ui_Screen1);
    }

    jsonRead(parameters, "x", _x);
    jsonRead(parameters, "y", _y);
    jsonRead(parameters, "id2show", _id2show);
    jsonRead(parameters, "color", color);
    jsonRead(parameters, "shrift", shr);
    jsonRead(parameters, "descr1", descr);
    jsonRead(parameters, "descr2", descr1);
    jsonRead(parameters, "round", _round);
    jsonRead(parameters, "text", value.valS);

    conshrift(shrit, shr);

   

    //String valy1 = descr + " " + value.valS + " " + descr1;

    lv_obj_set_width(ui_Label, LV_SIZE_CONTENT);
    lv_obj_set_height(ui_Label, LV_SIZE_CONTENT);
    lv_obj_set_x(ui_Label, _x);
    lv_obj_set_y(ui_Label, _y);
    lv_obj_set_align(ui_Label, LV_ALIGN_DEFAULT);
    lv_obj_add_flag(ui_Label, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_remove_flag(ui_Label, LV_OBJ_FLAG_SCROLLABLE);

    if (shr != 45) { 
         lv_obj_set_style_text_color(ui_Label, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_obj_set_style_text_opa(ui_Label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_arclabel_set_text_fmt(ui_Label,"%s %s %s", descr.c_str(), value.valS.c_str(), descr1.c_str()); //lv_label_set_text_fmt
   if (shr == 45) {
        lv_obj_set_style_text_font(ui_Label, &serg45, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_text_font(ui_Label, &shrit, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
   // lv_obj_set_style_text_font(ui_Label, &shrit, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(ui_Label, ui_event_Scrin, LV_EVENT_PRESSED, (void*)1);

    //nb1 = nb;
    //nb = nb + 1;

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
            iotVal.isDecimal = (val.type == Value::FLOAT || val.type == Value::INT);
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
            iotVal.isDecimal = (val.type == Value::FLOAT || val.type == Value::INT);
            iotVal.valD = (val.type == Value::INT) ? val.valI :
                          (val.type == Value::FLOAT) ? val.valF :
                          val.valS.toFloat();
            iotVal.valS = val.toString();

            this->_ignoreNextSync = true;
            this->setValue(iotVal, false);
        };
    }

   // jsonReadStr(parameters, "round", _round);
}

Panel_label::~Panel_label() {
    if (binder) delete binder;
}

void Panel_label::onRegEvent(IoTItem* eventItem) {
    // Стандартная привязка к локальному элементу
    if (binder && !binder->isLocalTarget && eventItem && eventItem->getID() == binder->targetId) {
        binder->checkAndBind();
    }

    // Реакция на локальное изменение _id2show
    binder && binder->handleTargetLocalEvent(eventItem, true);

    IoTItem::onRegEvent(eventItem);
}

void Panel_label::drawItem(IoTItem *item) {
    updateUI();
}

void Panel_label::onMqttRecive(String& topic, String& msg) {
    if (binder && binder->handleMqttMessage(topic, msg)) {
        String valueStr = msg;
        jsonRead(msg, F("status"), valueStr, false);
    }

    IoTItem::onMqttRecive(topic, msg);
}

void Panel_label::onModuleOrder(String& key, String& value) {
    if (binder) {
        if (key == binder->id) {
            binder->setValueLocally(value);
        } else if (key == binder->targetId) {
            binder->setRemoteValue(Value::parse(value), true);
        }
    }
    IoTItem::onModuleOrder(key, value);
}

String Panel_label::formatValue() const {
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

void Panel_label::setValue(const IoTValue& Value, bool genEvent) {
    if (_ignoreNextSync) {
        value = Value;
        String formatted = formatValue();
        _lastSentValue = formatted;
        updateUI();
        _ignoreNextSync = false;
        if (genEvent) {
            regEvent(formatted, F("Panel_label"), false, true);
        }
        return;
    }

    value = Value;
    updateUI();
    if (genEvent) {
        regEvent(formatValue(), F("Panel_label"), false, true);
    }
}

String Panel_label::getValue() {
    return formatValue();
}



IoTValue Panel_label::execute(String command, std::vector<IoTValue> &param) {
    if (command == "hide") {
        lv_obj_add_flag(ui_Label, LV_OBJ_FLAG_HIDDEN);
    } else if (command == "show") {
        lv_obj_clear_flag(ui_Label, LV_OBJ_FLAG_HIDDEN);
    } else if (command == "color") {
        color = param[0].valD;
        lv_obj_set_style_text_color(ui_Label, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (command == "pos") {
        _x = param[0].valD;
        _y = param[1].valD;
        lv_obj_set_pos(ui_Label, _x, _y);
    }
    return {};
}

/**
void Panel_label::updateUI() {
    // Обновить визуальное со
    char buffer[32];
    dtostrf(value.valD, 1, _round, buffer);
    String currentValue = value.valS.isEmpty() ? String(buffer) : value.valS;
    String tmpStr = descr + " " + currentValue + " " + descr1;
    const char* _text = tmpStr.c_str();
    lv_label_set_text(ui_Label, _text);

    if (_ignoreNextSync) return;

    // Если есть привязка и значение изменилось
    if (binder && _id != _id2show && !_id2show.isEmpty()) {
        String newValue = formatValue();
        if (_lastSentValue != newValue) {
            _lastSentValue = newValue;
            binder->setValueLocally(newValue);
        }
    }
}
*/

void Panel_label::updateUI() {
    // Обновить визуальное состояние
    char buffer[32];
    dtostrf(value.valD, 1, _round, buffer);

    String currentValue;
    if (!value.valS.isEmpty()) {
        // Проверяем, является ли valS числом
        if (isNumeric(value.valS)) {
            float num = value.valS.toFloat();
            char buf[32];
            dtostrf(num, 1, _round, buf);
            currentValue = String(buf);
        } else {
            currentValue = value.valS; // остаётся текстом
        }
    } else {
        currentValue = String(buffer); // используется valD
    }
   // String tmpStr = descr + " " + currentValue + " " + descr1; 
   // lv_arclabel_set_text_fmt(ui_Label, "%s %s %s", descr.c_str(), currentValue.c_str(), descr1.c_str()); //lv_label_set_text_fmt
    lv_label_set_text(ui_Label, (descr + " " + currentValue + " " + descr1).c_str());
    if (_ignoreNextSync) return;

    // Если есть привязка и значение изменилось
    if (binder && _id != _id2show && !_id2show.isEmpty()) {
        if (_lastSentValue != currentValue) {
            _lastSentValue = currentValue;
            binder->setValueLocally(currentValue);
        }
    }
}



