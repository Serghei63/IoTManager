// Panel_graph.h
#pragma once

#include "../../PanelCommon.h"
#include "Global.h"
#include "lvgl.h"
#include <vector>
#include "../../ValueBinder.h"

// Глобальные переменные для графиков
extern std::vector<lv_obj_t*> ui_Chart;
extern std::vector<String> chart_id;
extern int chart_count;

extern std::vector<lv_obj_t*> ui_TabPage;
extern int tab2;
extern std::vector<lv_obj_t*> ui_Button;
extern int isf1;
extern std::vector<lv_obj_t*> ui_Container;
extern int cont;
extern lv_obj_t* ui_Screen1;

class IoTItem;

class Panel_graph : public IoTItem {
public:
    Panel_graph(String parameters);
    ~Panel_graph();

    void onRegEvent(IoTItem* eventItem) override;
    void onMqttRecive(String& topic, String& msg) override;
    void onModuleOrder(String& key, String& value) override;
    void setValue(const IoTValue& Value, bool genEvent = true) override;
    String getValue() override;
    IoTValue execute(String command, std::vector<IoTValue> &param) override;

    void drawItem(IoTItem *item);

private:
    void updateUI();

    int _chartIndex = 0;
    int _x = 0, _y = 0, _x1 = 240, _y1 = 120;
    int _rangeStart = 0, _rangeEnd = 100;
    int _pointCount = 40;
    int color_line = 0x00FF00;
    int color_bg = 0x000000;
    int color_grid = 0x333333;
    int color_axis = 0xAAAAAA;
    String name = "";
    String _id2show = "";
    int nomer = 0;
    bool _ignoreNextSync = false;
    String _lastSentValue = "";

    lv_chart_series_t* series = nullptr;
    ValueBinder* binder = nullptr;
};