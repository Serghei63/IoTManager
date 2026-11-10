#include "Panel_arc.h"
#include "classes/IoTItem.h"

// Global variables definition
std::vector<lv_obj_t*> ui_Arc(0, NULL);
std::vector<String> arc_id(0, "");

int arc = 0;
float valrt = 0;
/**
// Event handler implementation
void ui_event_Arc(lv_event_t * e) 
{
   int i = (int)lv_event_get_user_data(e);
  
    lv_event_code_t event_code = lv_event_get_code(e);

    if(event_code == LV_EVENT_VALUE_CHANGED) {
       valrt = lv_arc_get_value(ui_Arc[i]);   
    }
    if (event_code == LV_EVENT_RELEASED){ 
        generateOrder( arc_id[i], String(valrt / 10));
    }    
}
*/

void Panel_arc::eventHandler(lv_event_t * e)
{
    int i = (int)lv_event_get_user_data(e);

    lv_event_code_t event_code = lv_event_get_code(e);

    if(event_code == LV_EVENT_VALUE_CHANGED) {
        valrt = lv_arc_get_value(ui_Arc[i]);
    }

    if (event_code == LV_EVENT_RELEASED){
        generateOrder(arc_id[i], String(valrt / 10));
    }
}
// Вспомогательная функция для округления с плавающей точкой
float roundToStep(float value, float step) {
    if (step <= 0) return value;
    return roundf(value / step) * step;
}

// Panel_arc implementation
Panel_arc::Panel_arc(String parameters) : IoTItem(parameters) {

    jsonRead(parameters, "NAME", name);
    jsonRead(parameters, "Nomer", nomer);
    
    jsonRead(parameters, "id2show", _id2show);
    jsonRead(parameters, "round", _round);
    
    // Новый параметр - шаг дуги (поддерживает 0.5, 1, 2, 5)
    String stepStr;
    jsonRead(parameters, "step", stepStr);
    if (stepStr.length() > 0) {
        _step = stepStr.toFloat();
    } else {
        _step = 1.0f; // значение по умолчанию
    }
    if (_step <= 0) _step = 1.0f;
    
    arc_id.push_back(_id);

    if (name == "TabPage" && tab2 >= nomer )
    {
            ui_Arc.push_back( lv_arc_create( ui_TabPage[nomer - 1]));
    }
    else if (name == "Button" && isf1 >= nomer ) 
    {   
            ui_Arc.push_back( lv_arc_create( ui_Button[nomer - 1]));
    }
    else if (name == "Container" && cont >= nomer ) {
         
            ui_Arc.push_back( lv_arc_create( ui_Container[nomer - 1]));}
    else 
    {
            ui_Arc.push_back( lv_arc_create( ui_Screen1)); 
    } 

    jsonRead(parameters, "x", _x);
    jsonRead(parameters, "y", _y);
    jsonRead(parameters, "width", _x1);
    jsonRead(parameters, "height", _y1);
    jsonRead(parameters, "color", color);
    jsonRead(parameters, "size", size);
    jsonRead(parameters, "range_sart", _x2);
    jsonRead(parameters, "range_end", _y2);
    jsonRead(parameters, "angles_start", _x3);
    jsonRead(parameters, "angles_end", _y3);
     
    lv_obj_set_width( ui_Arc[arc], _x1);
    lv_obj_set_height( ui_Arc[arc], _y1);
    lv_obj_set_x( ui_Arc[arc], _x);
    lv_obj_set_y( ui_Arc[arc], _y);
    lv_arc_set_range( ui_Arc[arc], _x2 * 10, _y2 * 10);
    
    // Применяем шаг к начальному значению
    float initialValue = roundToStep(value.valD, _step);
    lv_arc_set_value( ui_Arc[arc], initialValue * 10);
    value.valD = initialValue;
    
    lv_arc_set_bg_angles( ui_Arc[arc], _x3, _y3);

    lv_obj_set_style_arc_color(ui_Arc[arc], lv_color_hex(color), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui_Arc[arc], size, LV_PART_INDICATOR | LV_STATE_DEFAULT);
   
    lv_obj_set_style_bg_color( ui_Arc[arc], lv_color_hex(0xE05151), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa( ui_Arc[arc], 255, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color( ui_Arc[arc], lv_color_hex(0x211515), LV_PART_KNOB | LV_STATE_DEFAULT);

   // lv_obj_add_event_cb(ui_Arc[arc], ui_event_Arc, LV_EVENT_ALL, (void*)arc);
   lv_obj_add_event_cb(ui_Arc[arc], Panel_arc::eventHandler, LV_EVENT_ALL, (void*)arc);
    lv_obj_add_event_cb(ui_Arc[arc], ui_event_Scrin, LV_EVENT_PRESSED, (void*)1);
    
    arc1 = arc;
    arc = arc + 1;

    // Инициализация ValueBinder
    if (_id2show != _id && !_id2show.isEmpty()) {
        binder = new ValueBinder(_id, _id2show);

        // Настройка callbacks для ValueBinder
        binder->onLocalChange = [this](const Value& val) {
            // Локальное изменение значения - обновляем UI с применением шага
            float newValue = val.toFloat();
            newValue = roundToStep(newValue, _step);
            this->value.valD = newValue;
            this->updateUI();
        };

        binder->onRemoteChange = [this](const Value& val, bool genEvent) {
            // Удаленное изменение значения - обновляем UI и генерируем событие если нужно
            float newValue = val.toFloat();
            newValue = roundToStep(newValue, _step);
            IoTValue iotVal;
            iotVal.valD = newValue;
            iotVal.isDecimal = true;
            this->setValue(iotVal, genEvent);
        };

        binder->onSendOrder = [this](const String& target, const String& val) {
            // Отправка приказа локальному элементу
            generateOrder(target, val);
        };

        binder->onSendRemoteValue = [this](const String& target, const String& val) {
            // Отправка значения удаленному элементу через MQTT
            publishStatusMqtt(target, val);
        };
    }
}

Panel_arc::~Panel_arc() {
    if (binder) {
        delete binder;
        binder = nullptr;
    }
}

void Panel_arc::drawItem(IoTItem *item) {
    // Применяем шаг к значению
    float steppedValue = roundToStep(value.valD, _step);
    lv_arc_set_value( ui_Arc[arc1], steppedValue * 10);
    updateUI();
}

String Panel_arc::formatValue() const {
    // Применяем шаг к форматируемому значению
    float steppedValue = roundToStep(value.valD, _step);
    
    if (!value.valS.isEmpty()) {
        if (isNumeric(value.valS)) {
            char buf[32];
            // Используем dtostrf с правильным количеством знаков после запятой
            int decimals = (_step < 1) ? 1 : 0;
            if (_step == 0.5) decimals = 1;
            dtostrf(steppedValue, 1, _round > decimals ? _round : decimals, buf);
            return String(buf);
        }
        return value.valS;
    }
    char buf[32];
    int decimals = (_step < 1) ? 1 : 0;
    if (_step == 0.5) decimals = 1;
    dtostrf(steppedValue, 1, _round > decimals ? _round : decimals, buf);
    return String(buf);
}

void Panel_arc::setValue(const IoTValue& Value, bool genEvent) {
    String formattedValue;
    
    // Применяем шаг к входящему значению
    IoTValue steppedValue = Value;
    steppedValue.valD = roundToStep(Value.valD, _step);
    
    if (_ignoreNextSync) {
        value = steppedValue;
        formattedValue = formatValue();
        _lastSentValue = formattedValue;
        updateUI();
        _ignoreNextSync = false;
        if (genEvent) {
            regEvent(formattedValue, F("Panel_arc"), false, genEvent);
        }
        return;
    }

    value = steppedValue;
    formattedValue = formatValue();
    updateUI();
    if (genEvent) {
         regEvent(formattedValue, F("Panel_arc"), false, genEvent);
    }
}
   

String Panel_arc::getValue() {
    return formatValue();
}

void Panel_arc::onRegEvent(IoTItem* item) {
    Serial.println("Panel_arc::onRegEvent: item=" + (item ? item->getID() : "null") + ", _id2show=" + _id2show);
    // Стандартная привязка к локальному элементу
    if (binder && !binder->isLocalTarget && item && item->getID() == binder->targetId) {
        if (binder->checkAndBind()) {
            Serial.println("Panel_arc: [DYNAMIC BIND] Привязан к локальному: " + binder->targetId);
        }
    }

    // Реакция на локальное изменение _id2show
    if (binder) {
        Serial.println("Panel_arc: checking handleTargetLocalEvent: hasTarget=" + String(binder->hasTarget()) + ", isLocalTarget=" + String(binder->isLocalTarget) + ", item ID=" + (item ? item->getID() : "null") + ", targetId=" + binder->targetId);
        if (binder->handleTargetLocalEvent(item, true)) {
            Serial.println("Panel_arc: [EVENT SYNC] " + _id + " <- " + _id2show + " = " + (item ? item->getValue() : "null"));
        } else {
            Serial.println("Panel_arc: handleTargetLocalEvent returned false");
        }
    }

    IoTItem::onRegEvent(item);
}

void Panel_arc::onModuleOrder(String& key, String& value) {
    Serial.println("Panel_arc::onModuleOrder: key=" + key + ", value=" + value + ", _id=" + _id + ", _id2show=" + _id2show);

    if (binder) {
        if (key == binder->id) {
            binder->setValueLocally(value);
        } else if (key == binder->targetId) {
            binder->setRemoteValue(Value::parse(value), true);
        }
    }

    IoTItem::onModuleOrder(key, value);
}

IoTValue Panel_arc::execute(String command, std::vector<IoTValue> &param){
    if (command == "hide"){
        lv_obj_add_flag(ui_Arc[arc - 1], LV_OBJ_FLAG_HIDDEN);
    }
    
    if (command == "show"){
         lv_obj_clear_flag(ui_Arc[arc - 1], LV_OBJ_FLAG_HIDDEN);
    }

     if (command == "color"){
         color = param[0].valD;
         lv_obj_set_style_arc_color(ui_Arc[arc - 1], lv_color_hex(color), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
    if (command == "pos"){
        _x = param[0].valD;
        _y = param[1].valD;
        lv_obj_set_pos(ui_Arc[arc - 1], _x, _y);
    }
    if (command == "step"){
        _step = param[0].valD;
        if (_step <= 0) _step = 1.0f;
        // Пересчитываем текущее значение с новым шагом
        value.valD = roundToStep(value.valD, _step);
        updateUI();
    }
    return {};
}

void Panel_arc::onMqttRecive(String& topic, String& msg) {
    
    if (binder && binder->handleMqttMessage(topic, msg)) {
        String valueStr = msg;
        jsonRead(msg, F("status"), valueStr, false);
        Serial.println("Panel_arc: [MQTT SYNC] " + _id2show + " = " + valueStr);
    }

    IoTItem::onMqttRecive(topic, msg);
}

void Panel_arc::updateUI() {
    // Применяем шаг к значению перед отображением
    float steppedValue = roundToStep(value.valD, _step);
    // Обновить визуальное состояние
    lv_arc_set_value(ui_Arc[arc1], steppedValue * 10);

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