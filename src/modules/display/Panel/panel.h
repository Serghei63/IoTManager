#include "Global.h"
#include "lvgl.h"


void generateScreen(String id, String stat);
//void otpravka(  String id , String znah );
//void otpravkaOrder(String id1, String zn);
//void otpravkaidvalue(String id1,  String zn);
void conshrift( lv_font_t &shri, int &sh );
void ui_init(void);
extern void ui_event_Scrin(lv_event_t * e);



#ifndef panel_H
#define panel_H

#ifdef __cplusplus
extern "C" {
#endif

// SCREEN: ui_Screen1
extern void ui_Screen1_screen_init(void);
//extern void ui_Screen1_screen_destroy(void);
extern lv_obj_t * ui_Screen1; 
extern void ui_event_Switch2(lv_event_t * e);
extern void ui_event_Button(lv_event_t * e);
extern void ui_event_Button1(lv_event_t * e);

extern void ui_event_Arc(lv_event_t * e);
extern void ui_event_TextArea(lv_event_t * e);
extern void ui_event_Slaider(lv_event_t * e);
extern void ui_event_key(lv_event_t * e);
//extern void load_png_image(const char* filename, lv_obj_t* parent);
//void sd_init();
//void spi_init();
//void load_png_image(const char* , lv_obj_t* );
//void load_jpg_image(const char* , lv_obj_t* );


// CUSTOM VARIABLES

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif