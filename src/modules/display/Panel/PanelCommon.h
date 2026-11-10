#pragma once

#include "Global.h"
#include "lvgl.h"
#include "panel.h"
#include <vector>

// Common LVGL objects
extern lv_obj_t* ui_Screen1;
extern String scrin;

// Common display settings
extern int color1;
extern unsigned long lv_last_tick;
extern ulong next_millis;
extern ulong sla_millis;

// Forward declarations of all widget classes
class Panel;
class Panel_swith;
class Panel_label;
class Panel_button; 
class Panel_tabview;
class Panel_Container;
class Panel_arc;
class Panel_arc_text;
class Panel_TextArea;
class Panel_bar;
class Panel_slider;
class Panel_img;
class Panel_AnalogClock;

// Event handler for screen
void ui_event_Scrin(lv_event_t * e);

// Helper functions
//void otpravkaidvalue(String id1, String zn);
void conshrift(lv_font_t &shri, int &sh);
bool isNumeric(const String& str);
// Factory functionф/
void* getAPI_Panel(String subtype, String param);
