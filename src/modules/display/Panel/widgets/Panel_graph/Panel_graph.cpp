// Panel_graph.cpp
#include "Panel_graph.h"
#include "classes/IoTItem.h"
#include "lvgl.h"
// Явно подключаем модуль charts и проверяем, включён ли он в конфиг
// #if !defined(LV_USE_CHART)
// #warning "LV_USE_CHART not defined. Please enable LV_USE_CHART in lv_conf.h"
// #define LV_USE_CHART 0
// #endif
// #if LV_USE_CHART
// #include "lv_charts.h"
// #endif

std::vector<lv_obj_t*> ui_Chart;
std::vector<String> chart_id;
int chart_count = 0;

Panel_graph::Panel_graph(String parameters) : IoTItem(parameters) {
    jsonRead(parameters, "NAME", name);
    jsonRead(parameters, "Nomer", nomer);
    jsonRead(parameters, "id2show", _id2show);
    jsonRead(parameters, "x", _x);
    jsonRead(parameters, "y", _y);
    jsonRead(parameters, "width", _x1);
    jsonRead(parameters, "height", _y1);
    jsonRead(parameters, "range_start", _rangeStart);
    jsonRead(parameters, "range_end", _rangeEnd);
    jsonRead(parameters, "point_count", _pointCount);
    jsonRead(parameters, "color_line", color_line);
    jsonRead(parameters, "color_bg", color_bg);
    jsonRead(parameters, "color_grid", color_grid);
    jsonRead(parameters, "color_axis", color_axis);

    lv_obj_t* parent = ui_Screen1;
    if (name == "TabPage" && tab2 >= nomer) {
        parent = ui_TabPage[nomer - 1];
    } else if (name == "Button" && isf1 >= nomer) {
        parent = ui_Button[nomer - 1];
    } else if (name == "Container" && cont >= nomer) {
        parent = ui_Container[nomer - 1];
    }

    ui_Chart.push_back(lv_chart_create(parent));
    chart_id.push_back(_id);
    _chartIndex = chart_count++;

    lv_obj_set_size(ui_Chart[_chartIndex], _x1, _y1);
    lv_obj_set_pos(ui_Chart[_chartIndex], _x, _y);
    lv_chart_set_type(ui_Chart[_chartIndex], LV_CHART_TYPE_BAR);
    //lv_chart_set_point_count(ui_Chart[_chartIndex], _pointCount > 0 ? _pointCount : 40);
    lv_chart_set_point_count(ui_Chart[_chartIndex], _pointCount > 0 ? _pointCount : 400);
    lv_chart_set_range(ui_Chart[_chartIndex], LV_CHART_AXIS_PRIMARY_Y, _rangeStart, _rangeEnd);
    lv_chart_set_update_mode(ui_Chart[_chartIndex], LV_CHART_UPDATE_MODE_SHIFT);
    // В старых/новых версиях LVGL сигнатура tick может отличаться — проверить lv_charts.h
    #ifdef lv_chart_set_axis_tick
        lv_chart_set_axis_tick(ui_Chart[_chartIndex], LV_CHART_AXIS_PRIMARY_Y, 5, 5, 5, 1, true, 50);
        lv_chart_set_axis_tick(ui_Chart[_chartIndex], LV_CHART_AXIS_PRIMARY_X, 5, 5, 5, 1, true, 50);
    #endif

    lv_obj_set_style_bg_color(ui_Chart[_chartIndex], lv_color_hex(color_bg), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Chart[_chartIndex], lv_color_hex(color_axis), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Chart[_chartIndex], 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui_Chart[_chartIndex], lv_color_hex(color_grid), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_width(ui_Chart[_chartIndex], 1, LV_PART_MAIN | LV_STATE_DEFAULT);

    series = lv_chart_add_series(ui_Chart[_chartIndex], lv_color_hex(color_line), LV_CHART_AXIS_PRIMARY_Y);
    if (series) {
        lv_chart_set_series_color(ui_Chart[_chartIndex], series, lv_color_hex(color_line));
        lv_chart_set_all_values(ui_Chart[_chartIndex], series, _rangeStart);
    }

    if (_id2show != _id && !_id2show.isEmpty()) {
        binder = new ValueBinder(_id, _id2show);
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

Panel_graph::~Panel_graph() {
    if (binder) {
        delete binder;
        binder = nullptr;
    }
}

void Panel_graph::onRegEvent(IoTItem* eventItem) {
    if (binder && !binder->isLocalTarget && eventItem && eventItem->getID() == binder->targetId) {
        binder->checkAndBind();
    }

    if (binder && binder->handleTargetLocalEvent(eventItem, true)) {
        String valStr = eventItem->getValue();
        IoTValue iotVal;
        iotVal.isDecimal = true;
        iotVal.valD = valStr.toFloat();
        iotVal.valS = valStr;
        this->_ignoreNextSync = true;
        this->setValue(iotVal, true);
    }

    IoTItem::onRegEvent(eventItem);
}

void Panel_graph::onMqttRecive(String& topic, String& msg) {
    if (binder && binder->handleMqttMessage(topic, msg)) {
        String valueStr = msg;
        jsonRead(msg, F("status"), valueStr, false);
        jsonRead(msg, F("value"), valueStr, false);
        jsonRead(msg, F("val"), valueStr, false);
    }

    IoTItem::onMqttRecive(topic, msg);
}

void Panel_graph::onModuleOrder(String& key, String& value) {
    if (binder) {
        if (key == binder->id) {
            binder->setValueLocally(value);
        } else if (key == binder->targetId) {
            binder->setRemoteValue(Value::parse(value), true);
        }
    }
    IoTItem::onModuleOrder(key, value);
}

void Panel_graph::setValue(const IoTValue& Value, bool genEvent) {
    if (_ignoreNextSync) {
        value = Value;
        _lastSentValue = getValue();
        drawItem(this);
        _ignoreNextSync = false;
        if (genEvent) {
            regEvent(getValue(), F("Panel_graph"), false, true);
        }
        return;
    }

    value = Value;
    drawItem(this);
    if (genEvent) {
        regEvent(getValue(), F("Panel_graph"), false, true);
    }
}

String Panel_graph::getValue() {
    if (value.isDecimal) {
        return String(value.valD, 2);
    }
    return value.valS;
}

IoTValue Panel_graph::execute(String command, std::vector<IoTValue> &param) {
    if (command == "hide") {
        lv_obj_add_flag(ui_Chart[_chartIndex], LV_OBJ_FLAG_HIDDEN);
    } else if (command == "show") {
        lv_obj_clear_flag(ui_Chart[_chartIndex], LV_OBJ_FLAG_HIDDEN);
    } else if (command == "pos" && param.size() >= 2) {
        _x = param[0].valD;
        _y = param[1].valD;
        lv_obj_set_pos(ui_Chart[_chartIndex], _x, _y);
    } else if (command == "color_line" && !param.empty()) {
        color_line = param[0].valD;
        if (series) {
            lv_chart_set_series_color(ui_Chart[_chartIndex], series, lv_color_hex(color_line));
        }
    } else if (command == "color_bg" && !param.empty()) {
        color_bg = param[0].valD;
        lv_obj_set_style_bg_color(ui_Chart[_chartIndex], lv_color_hex(color_bg), LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (command == "clear") {
        if (series) {
            lv_chart_set_all_values(ui_Chart[_chartIndex], series, _rangeStart);
        }
    }
    return IoTValue();
}

void Panel_graph::updateUI() {
    if (!series || !ui_Chart[_chartIndex]) return;

    int nextValue = (int)roundf(value.valD);
    lv_chart_set_next_value(ui_Chart[_chartIndex], series, nextValue);

    if (_ignoreNextSync) return;

    if (binder && _id != _id2show && !_id2show.isEmpty()) {
        String newValue = getValue();
        if (_lastSentValue != newValue) {
            _lastSentValue = newValue;
            binder->setValueLocally(newValue);
        }
    }
}

void Panel_graph::drawItem(IoTItem *item) {
    updateUI();
}
