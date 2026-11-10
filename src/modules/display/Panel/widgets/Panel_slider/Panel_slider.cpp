#include "Panel_slider.h"
#include "classes/IoTItem.h"

// Global variables definition
std::vector<lv_obj_t*> ui_Slaider(0, NULL);
std::vector<String> Slaider_id(0, "");
int sla = 0;

// Event handler implementation
void ui_event_Slaider(lv_event_t * e)
{
   int i = (int)lv_event_get_user_data(e);
  
    lv_event_code_t event_code = lv_event_get_code(e);

   float vert = 0;      

    if (event_code == LV_EVENT_RELEASED){ 
        vert = lv_slider_get_value(ui_Slaider[i]);
        generateOrder( Slaider_id[i], String(vert/10));
    }    
}

// Panel_slider implementation
Panel_slider::Panel_slider(String parameters) : IoTItem(parameters), binder(nullptr) {

    jsonRead(parameters, "NAME", name);
    jsonRead(parameters, "Nomer", nomer);

    String rt56 = "";
    jsonRead(parameters, "id2show", _id2show);

    Slaider_id.push_back(_id);
    _round = 1;
    jsonRead(parameters, "round", _round);

    jsonRead(parameters, "scale", scal);
    jsonRead(parameters, "scale_division", scale_division);

    if (name == "TabPage" && tab2 >= nomer )
    {
            ui_Slaider.push_back( lv_slider_create( ui_TabPage[nomer - 1]));
            if (scal == 1)  scale = lv_scale_create( ui_TabPage[nomer - 1]);
    }
    else if (name == "Button" && isf1 >= nomer )
    {
            ui_Slaider.push_back( lv_slider_create( ui_Button[nomer - 1]));
            if (scal == 1)   scale = lv_scale_create(ui_Button[nomer - 1]);
    }
    else if (name == "Container" && cont >= nomer ) {

            ui_Slaider.push_back( lv_slider_create( ui_Container[nomer - 1]));
            if (scal == 1)  scale = lv_scale_create(ui_Container[nomer - 1]);
    }
    else
    {
            ui_Slaider.push_back( lv_slider_create( ui_Screen1));
            if (scal == 1)   scale = lv_scale_create(ui_Screen1);
    }

    jsonRead(parameters, "x", _x);
    jsonRead(parameters, "y", _y);
    jsonRead(parameters, "width", _x1);
    jsonRead(parameters, "height", _y1);
    jsonRead(parameters, "color_indicator", color_indicator);
    jsonRead(parameters, "color", color);
    jsonRead(parameters, "color_knob", color_knob);
    jsonRead(parameters, "range_sart", _x2);
    jsonRead(parameters, "range_end", _y2);

    if (scal == 1) {
        lv_obj_set_size(scale, _x1, _y1);
        if (_x1 > _y1){
            lv_scale_set_mode(scale, LV_SCALE_MODE_HORIZONTAL_BOTTOM);
            lv_obj_set_x(scale, _x);
            lv_obj_set_y(scale, _y + _y1);
        }
        else {
            lv_scale_set_mode(scale, LV_SCALE_MODE_VERTICAL_RIGHT);
            lv_obj_set_x(scale, _x + _x1);
            lv_obj_set_y(scale, _y);
        }
        lv_scale_set_label_show(scale, true);
        lv_scale_set_total_tick_count(scale, scale_division);
        lv_scale_set_major_tick_every(scale, 1);
        lv_scale_set_range(scale, _x2, _y2);
        lv_obj_set_style_text_color(scale, lv_color_hex(color_knob), LV_PART_INDICATOR |  LV_STATE_DEFAULT);
        lv_obj_set_style_line_color(scale, lv_color_hex(color), LV_PART_INDICATOR |  LV_STATE_DEFAULT);
    }

    if(lv_slider_get_mode(ui_Slaider[sla]) == LV_SLIDER_MODE_RANGE) lv_slider_set_left_value(ui_Slaider[sla], 0, LV_ANIM_OFF);
    lv_obj_set_width( ui_Slaider[sla], _x1);
    lv_obj_set_height(ui_Slaider[sla], _y1);
    lv_obj_set_x(ui_Slaider[sla], _x);
    lv_obj_set_y(ui_Slaider[sla], _y);
    lv_slider_set_range(ui_Slaider[sla], _x2 * 10, _y2 * 10);
    lv_obj_set_align(ui_Slaider[sla], LV_ALIGN_DEFAULT);
    lv_obj_set_style_bg_color(ui_Slaider[sla], lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Slaider[sla], 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_Slaider[sla], lv_color_hex(color_indicator), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Slaider[sla], 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_Slaider[sla], lv_color_hex(color_knob), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Slaider[sla], 255, LV_PART_KNOB | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_Slaider[sla], ui_event_Slaider, LV_EVENT_ALL, (void*)sla);
    lv_slider_set_value(ui_Slaider[sla], value.valD * 10 , LV_ANIM_OFF);
    lv_obj_add_event_cb(ui_Slaider[sla], ui_event_Scrin, LV_EVENT_PRESSED, (void*)1);

    sla1 = sla;
    sla = sla + 1;

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
            iotVal.isDecimal = true; // Slider всегда работает с float
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
            iotVal.isDecimal = true;
            iotVal.valD = (val.type == Value::INT) ? val.valI :
                          (val.type == Value::FLOAT) ? val.valF :
                          val.valS.toFloat();
            iotVal.valS = val.toString();

            this->_ignoreNextSync = true;
            this->setValue(iotVal, false);
        };
    }
}

Panel_slider::~Panel_slider() {
    if (binder) delete binder;
}

void Panel_slider::onRegEvent(IoTItem* eventItem) {
    // Стандартная привязка к локальному элементу
    if (binder && !binder->isLocalTarget && eventItem && eventItem->getID() == binder->targetId) {
        binder->checkAndBind();
    }

    // Реакция на локальное изменение _id2show
    binder && binder->handleTargetLocalEvent(eventItem, true);

    IoTItem::onRegEvent(eventItem);
}



String Panel_slider::formatValue() const {
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

void Panel_slider::updateUI() {
    // Обновить визуальное состояние слайдера
    lv_slider_set_value(ui_Slaider[sla1], value.valD * 10, LV_ANIM_OFF);

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

void Panel_slider::setValue(const IoTValue& Value, bool genEvent) {
    String formattedValue;
    if (_ignoreNextSync) {
        value = Value;
        formattedValue = formatValue();
        _lastSentValue = formattedValue;
        updateUI();
        _ignoreNextSync = false;
        if (genEvent) {
            regEvent(formattedValue, F("Panel_slider"), false, true);
        }
        return;
    }

    value = Value;
    formattedValue = formatValue();
    updateUI();
    if (genEvent) {
        regEvent(formattedValue, F("Panel_slider"), false, true);
    }
}

String Panel_slider::getValue() {
    return formatValue();
}

void Panel_slider::onModuleOrder(String& key, String& value) {
    if (binder) {
        if (key == binder->id) {
            binder->setValueLocally(value.toFloat());
        } else if (key == binder->targetId) {
            binder->setRemoteValue(Value::parse(value), true);
        }
    }
    IoTItem::onModuleOrder(key, value);
}

bool Panel_slider::isCurrentOrderForMe() {
    if (orderBuf.length() == 0) return false;
    String currentOrder = selectToMarker(orderBuf, ",");
    if (currentOrder.length() == 0) return false;
    String currentKey = selectToMarker(currentOrder, " ");
    return currentKey == _id;
}

IoTValue Panel_slider::execute(String command, std::vector<IoTValue> &param){
    if (command == "hide"){
        lv_obj_add_flag(ui_Slaider[sla - 1], LV_OBJ_FLAG_HIDDEN);
    }
    
    if (command == "show"){
         lv_obj_clear_flag(ui_Slaider[sla - 1], LV_OBJ_FLAG_HIDDEN);
    }

     if (command == "color"){
         color = param[0].valD;
          lv_obj_set_style_bg_color(ui_Slaider[sla - 1], lv_color_hex(color), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    }
    if (command == "pos"){
        _x = param[0].valD;
        _y = param[1].valD;
        lv_obj_set_pos(ui_Slaider[sla - 1], _x, _y);
    }
    return {};
}

void Panel_slider::onMqttRecive(String& topic, String& msg) {
    if (binder && binder->handleMqttMessage(topic, msg)) {
        String valueStr = msg;
        jsonRead(msg, F("status"), valueStr, false);
    }

    IoTItem::onMqttRecive(topic, msg);
}
