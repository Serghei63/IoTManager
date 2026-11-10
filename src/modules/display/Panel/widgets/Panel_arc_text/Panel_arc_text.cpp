#include "Panel_arc_text.h"
#include "classes/IoTItem.h"

//extern "C" const lv_font_t serg45;

std::vector<lv_obj_t*> ui_Arc_text(0, NULL);

int nar = 0;

// ===== Размер символа =====
lv_point_t getCharSize(const lv_font_t * font, uint32_t letter) {
    lv_point_t size;

    lv_font_glyph_dsc_t g;
    bool found = lv_font_get_glyph_dsc(font, &g, letter, '\0');

    if (!found) {
        lv_font_get_glyph_dsc(font, &g, ' ', '\0');
    }

    size.x = g.adv_w;
    size.y = font->line_height;

    return size;
}

// ===== Рисование по дуге =====
void Panel_arc_text::drawTextOnArc(lv_obj_t* parent,
                                   const char* text,
                                   lv_point_t center,
                                   int radius,
                                   int angle_start,
                                   int angle_end,
                                   const lv_font_t* font,
                                   lv_color_t color)
{
    if (!text || strlen(text) == 0) return;

    lv_obj_clean(parent);

    int total_width = 0;
    for (int i = 0; text[i] != '\0'; i++) {
        total_width += getCharSize(font, text[i]).x;
    }

    if (total_width == 0) return;

    float start_rad = angle_start * 3.14159f / 180.0f;
    float end_rad   = angle_end   * 3.14159f / 180.0f;

    float arc_rad = end_rad - start_rad;
    if (arc_rad <= 0) arc_rad += 2 * 3.14159f;

    float rad_per_px = arc_rad / total_width;

    int current_width = 0;

    for (int i = 0; text[i] != '\0'; i++) {

        lv_point_t char_size = getCharSize(font, text[i]);

        float char_rad = start_rad + (current_width + char_size.x / 2) * rad_per_px;
        float char_angle = char_rad * 180.0f / 3.14159f;

        int x = center.x + radius * cosf(char_rad - 3.14159f / 2);
        int y = center.y + radius * sinf(char_rad - 3.14159f / 2);

        lv_obj_t* label = lv_label_create(parent);
        lv_label_set_text_fmt(label, "%c", text[i]);

        // стиль как у label
        lv_obj_set_style_text_color(label, color, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

        if (shr == 45) {
           // lv_obj_set_style_text_font(label, &serg45, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_text_font(label, (lv_font_t*)font, LV_PART_MAIN | LV_STATE_DEFAULT);
        }

        lv_obj_set_pos(label,
            x - char_size.x / 2,
            y - char_size.y / 2
        );

        lv_obj_set_style_transform_angle(label, (char_angle + 90) * 10, 0);

        // кликабельность
        lv_obj_add_event_cb(label, ui_event_Scrin, LV_EVENT_PRESSED, (void*)1);

        current_width += char_size.x;
    }
}

// ===== Конструктор =====
Panel_arc_text::Panel_arc_text(String parameters) : IoTItem(parameters) {

    jsonRead(parameters, "NAME", name);
    jsonRead(parameters, "Nomer", nomer);

    if (name == "TabPage" && tab2 >= nomer) {
        ui_Arc_text.push_back(lv_obj_create(ui_TabPage[nomer - 1]));
    } else if (name == "Button" && isf1 >= nomer) {
        ui_Arc_text.push_back(lv_obj_create(ui_Button[nomer - 1]));
    } else if (name == "Container" && cont >= nomer) {
        ui_Arc_text.push_back(lv_obj_create(ui_Container[nomer - 1]));
    } else {
        ui_Arc_text.push_back(lv_obj_create(ui_Screen1));
    }

    jsonRead(parameters, "x", _x);
    jsonRead(parameters, "y", _y);
    jsonRead(parameters, "id2show", _id2show);
    jsonRead(parameters, "color", color);
    jsonRead(parameters, "shrift", shr);
    jsonRead(parameters, "descr1", descr);
    jsonRead(parameters, "descr2", descr1);
    jsonRead(parameters, "round", _round);
    jsonRead(parameters, "val", value.valS);

    jsonRead(parameters, "angle_start", _angle_start);
    jsonRead(parameters, "angle_end", _angle_end);

    if (!jsonRead(parameters, "radius", _radius)) {
        _radius = 50;
    }

    if (_angle_start == _angle_end) {
        _angle_start = 0;
        _angle_end = 180;
    }

    conshrift(shrit, shr);

    lv_obj_set_size(ui_Arc_text[nar], LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(ui_Arc_text[nar], 0, 0);
    lv_obj_set_style_bg_opa(ui_Arc_text[nar], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_Arc_text[nar], 0, 0);
    lv_obj_clear_flag(ui_Arc_text[nar], LV_OBJ_FLAG_SCROLLABLE);

    nar1 = nar;
    nar++;

    // ===== Binder =====
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
                publishEvent(key, json);
            }
        };

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

    updateUI();
}

// ===== formatValue =====
String Panel_arc_text::formatValue() const {
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

// ===== update =====
void Panel_arc_text::updateUI() {

    String currentValue = formatValue();
    String text = descr + " " + currentValue + " " + descr1;

    lv_area_t coords;
    lv_obj_get_coords(ui_Arc_text[nar1], &coords);

    lv_point_t center;
    center.x = _x - coords.x1;
    center.y = _y - coords.y1;

    int width  = coords.x2 - coords.x1;
    int height = coords.y2 - coords.y1;

    int radius = _radius;
    int max_radius = LV_MIN(width, height) / 2 - 5;

    if (radius > max_radius) radius = max_radius;
    if (radius < 5) radius = 5;

    drawTextOnArc(
        ui_Arc_text[nar1],
        text.c_str(),
        center,
        radius,
        _angle_start,
        _angle_end,
        &shrit,
        lv_color_hex(color)
    );

    if (_ignoreNextSync) return;

    if (binder && _id != _id2show && !_id2show.isEmpty()) {
        if (_lastSentValue != currentValue) {
            _lastSentValue = currentValue;
            binder->setValueLocally(currentValue);
        }
    }
}

// ===== setValue =====
void Panel_arc_text::setValue(const IoTValue& Value, bool genEvent) {

    if (_ignoreNextSync) {
        value = Value;
        String formatted = formatValue();
        _lastSentValue = formatted;
        updateUI();
        _ignoreNextSync = false;

        if (genEvent) {
            regEvent(formatted, F("Panel_arc_text"), false, true);
        }
        return;
    }

    value = Value;
    updateUI();

    if (genEvent) {
        regEvent(formatValue(), F("Panel_arc_text"), false, true);
    }
}

// ===== getValue =====
String Panel_arc_text::getValue() {
    return formatValue();
}

// ===== execute =====
IoTValue Panel_arc_text::execute(String command, std::vector<IoTValue> &param) {

    if (command == "hide") {
        lv_obj_add_flag(ui_Arc_text[nar1], LV_OBJ_FLAG_HIDDEN);

    } else if (command == "show") {
        lv_obj_clear_flag(ui_Arc_text[nar1], LV_OBJ_FLAG_HIDDEN);

    } else if (command == "color") {
        color = param[0].valD;
        updateUI();

    } else if (command == "pos") {
        _x = param[0].valD;
        _y = param[1].valD;
        lv_obj_set_pos(ui_Arc_text[nar1], _x, _y);
    }

    return {};
}