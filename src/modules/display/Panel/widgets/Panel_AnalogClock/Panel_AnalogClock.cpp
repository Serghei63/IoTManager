#include "Panel_AnalogClock.h"
#include "classes/IoTItem.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern unsigned long unixTime;

// ---------------------- КОНСТРУКТОР ----------------------

Panel_AnalogClock::Panel_AnalogClock(String parameters) : IoTItem(parameters) {

    jsonRead(parameters, "x", _x);
    jsonRead(parameters, "y", _y);
    jsonRead(parameters, "width", _width);
    jsonRead(parameters, "height", _height);
    jsonRead(parameters, "NAME", name);
    jsonRead(parameters, "Nomer", nomer);


    if (_width < 120) _width = 200;
    if (_height < 120) _height = 200;

    jsonRead(parameters, "hourColor", hourColor);
    jsonRead(parameters, "minuteColor", minuteColor);
    jsonRead(parameters, "secondColor", secondColor);
    jsonRead(parameters, "markColor", markColor);
    jsonRead(parameters, "markOn", markOn);
    jsonRead(parameters, "hour_width", hourWidth);
    jsonRead(parameters, "minute_width", minuteWidth);
    jsonRead(parameters, "second_width", secondWidth);

    hourColor   = hourColor   ? hourColor   : 0xFFFFFF;
    minuteColor = minuteColor ? minuteColor : 0xDDDDDD;
    secondColor = secondColor ? secondColor : 0xFF3B30;
    markColor   = markColor   ? markColor   : 0xAAAAAA;

    timezone = 3;
    jsonRead(parameters, "timezone", timezone);

    if (name == "TabPage" && tab2 >= nomer) {
        container = lv_obj_create(ui_TabPage[nomer - 1]);
    } else if (name == "Button" && isf1 >= nomer) {
         container = lv_obj_create(ui_Button[nomer - 1]);
    } else if (name == "Container" && cont >= nomer) {
         container = lv_obj_create(ui_Container[nomer - 1]);
    } else {
         container = lv_obj_create(ui_Screen1);
    }
   // container = lv_obj_create(ui_Screen1);
    lv_obj_set_pos(container, _x, _y);
    lv_obj_set_size(container, _width + 10, _height + 10);

    // прозрачный фон
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_clip_corner(container, true, 0);

    createClock();
    setInterval(1);
}

Panel_AnalogClock::~Panel_AnalogClock() {}

void Panel_AnalogClock::doByInterval() {
    updateHands();
}

// ---------------------- СОЗДАНИЕ ----------------------

void Panel_AnalogClock::createClock() {

    int cx = _width / 2;
    int cy = _height / 2;
    int radius = (_width < _height ? _width : _height) / 2 - 25;


if (markOn != 0) {

    // 🔵 Деления
    markPoints.resize(60 * 2);

    int outer = radius - 2;

    for (int i = 0; i < 60; i++) {

        lv_obj_t* mark = lv_line_create(container);

        float angle = i * 6.0f * M_PI / 180.0f;
        bool isHour = (i % 5 == 0);

        int len   = isHour ? 16 : 6;
        int width = isHour ? 4  : 1;

        int inner = outer - len;
        int idx = i * 2;

        markPoints[idx].x = cx + (int)(inner * sin(angle));
        markPoints[idx].y = cy - (int)(inner * cos(angle));

        markPoints[idx + 1].x = cx + (int)(outer * sin(angle));
        markPoints[idx + 1].y = cy - (int)(outer * cos(angle));

        lv_line_set_points(mark, &markPoints[idx], 2);

        lv_obj_set_style_line_color(mark, lv_color_hex(isHour ? 0xFFFFFF : 0x555555), 0);
        lv_obj_set_style_line_width(mark, width, 0);
    }

    // 🔢 ЦИФРЫ (исправленные)
    for (int i = 1; i <= 12; i++) {

        if (i == 12 || i == 3 || i == 6 || i == 9) {

            lv_obj_t* label = lv_label_create(container);

            float angle = i * 30.0f * M_PI / 180.0f;

            int textRadius = radius - 30;

            int x = cx + (int)(textRadius * sin(angle));
            int y = cy - (int)(textRadius * cos(angle));

            lv_label_set_text(label, String(i).c_str());
            lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

            lv_obj_set_pos(label, x - lv_obj_get_width(label) / 2 - 5, y - lv_obj_get_height(label) / 2 - 8 );
            //);
        }
    }
}

    // 🕒 СТРЕЛКИ
    hourHand = lv_line_create(container);
    minuteHand = lv_line_create(container);
    secondHand = lv_line_create(container);

    lv_obj_set_style_line_color(hourHand, lv_color_hex(hourColor), 0);
    lv_obj_set_style_line_width(hourHand, 6, 0);

    lv_obj_set_style_line_color(minuteHand, lv_color_hex(minuteColor), 0);
    lv_obj_set_style_line_width(minuteHand, 4, 0);

    lv_obj_set_style_line_color(secondHand, lv_color_hex(secondColor), 0);
    lv_obj_set_style_line_width(secondHand, 2, 0);

    // 🔴 ЦЕНТР
    centerDot = lv_obj_create(container);
    int size = 8;

    lv_obj_set_size(centerDot, size, size);
    lv_obj_set_pos(centerDot, cx - size / 2, cy - size / 2);

    lv_obj_set_style_radius(centerDot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(centerDot, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(centerDot, 0, 0);

    lv_obj_move_foreground(centerDot);
}

// ---------------------- ОБНОВЛЕНИЕ ----------------------

void Panel_AnalogClock::updateHands() {

    unsigned long localTime = unixTime + (timezone * 3600);

    int s = localTime % 60;
    int m = (localTime / 60) % 60;
    int h = (localTime / 3600) % 24;

    int cx = _width / 2;
    int cy = _height / 2;
    int radius = (_width < _height ? _width : _height) / 2 - 25;

    float hourAngle = (h % 12) * 30.0f + m * 0.5f;
    float minuteAngle = m * 6.0f;
    float secondAngle = (s + (millis() % 1000) / 1000.0f) * 6.0f;

    drawHand(hourHand, hourPoints, hourAngle, radius * 0.5f, cx, cy);
    drawHand(minuteHand, minutePoints, minuteAngle, radius * 0.75f, cx, cy);
    drawHand(secondHand, secondPoints, secondAngle, radius * 0.9f, cx, cy);
}

// ---------------------- РИСОВАНИЕ ----------------------

void Panel_AnalogClock::drawHand(
    lv_obj_t* hand,
    lv_point_precise_t* pts,
    float angle,
    float len,
    int cx,
    int cy
) {
    float rad = angle * M_PI / 180.0f;

    int x2 = cx + (int)(len * sin(rad));
    int y2 = cy - (int)(len * cos(rad));

    int tail = len * 0.2f;
    int x1 = cx - (int)(tail * sin(rad));
    int y1 = cy + (int)(tail * cos(rad));

    pts[0].x = x1;
    pts[0].y = y1;

    pts[1].x = x2;
    pts[1].y = y2;

    lv_line_set_points(hand, pts, 2);
}

// ---------------------- КОМАНДЫ ----------------------

IoTValue Panel_AnalogClock::execute(String command, std::vector<IoTValue> &param) {
    if (command == "hide") {
        lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
    } else if (command == "show") {
        lv_obj_clear_flag(container, LV_OBJ_FLAG_HIDDEN);
    }
    return {};
}