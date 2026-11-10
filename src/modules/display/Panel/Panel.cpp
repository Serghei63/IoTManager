#include "Global.h"

#include "lvgl.h"
#include <esp32_smartdisplay.h>

#include "panel.h"
#include "SD.h"
#include "ff.h"

#include "PanelCommon.h"
#include "widgets/Panel_swith/Panel_swith.h"
#include "widgets/Panel_label/Panel_label.h"
#include "widgets/Panel_button/Panel_button.h"
#include "widgets/Panel_tabview/Panel_tabview.h"
#include "widgets/Panel_Container/Panel_Container.h"
#include "widgets/Panel_arc/Panel_arc.h"
#include "widgets/Panel_arc_text/Panel_arc_text.h"
#include "widgets/Panel_TextArea/Panel_TextArea.h"
#include "widgets/Panel_bar/Panel_bar.h"
#include "widgets/Panel_slider/Panel_slider.h"
#include "widgets/Panel_img/Panel_img.h"
#include "widgets/Panel_img_icon/Panel_img_icon.h"
#include "widgets/Panel_AnalogClock/Panel_AnalogClock.h"
#include "widgets/Panel_graph/Panel_graph.h"
//extern const lv_font_t serg45;
//extern "C" const lv_font_t serg45;
// ============================================================================
// Global Variables
// ============================================================================

int  color1 = 0;
ulong next_millis = 0;
unsigned long lv_last_tick = millis();
ulong sla_millis = 0;

lv_obj_t * ui_Screen1 = NULL;
String scrin = "";


// ============================================================================
// Main Panel Class
// ============================================================================

class Panel : public IoTItem {
  String pere = "";
  int mq = 0, color = 0, rotat = 0;
  int zbg = 0;
  private:
  float _brightness = 100;
  bool wasMqttConnected = false;  // Отслеживаем предыдущее состояние подключения
  //bool isSubscribed = false; // Флаг для отслеживания подписки
   public:
    Panel(String parameters) : IoTItem(parameters) {
     
   
    jsonRead(parameters, "color", color1);
    jsonRead(parameters, "rotation", rotat);
    jsonRead(parameters, "brightness", _brightness);
    jsonRead(parameters, "id", scrin);
      
#ifdef ARDUINO_USB_CDC_ON_BOOT
    delay(1000);
#endif
    Serial.begin(115200);
    Serial.setDebugOutput(false);
    log_i("Board: %s", BOARD_NAME);
    log_i("CPU: %s rev%d, CPU Freq: %d Mhz, %d core(s)", ESP.getChipModel(), ESP.getChipRevision(), getCpuFrequencyMhz(), ESP.getChipCores());
    log_i("Free heap: %d bytes", ESP.getFreeHeap());
    log_i("Free PSRAM: %d bytes", ESP.getPsramSize());
    log_i("SDK version: %s", ESP.getSdkVersion());

    smartdisplay_init();

     __attribute__((unused)) auto disp = lv_disp_get_default();
     if (rotat == 90){
    lv_disp_set_rotation(disp, LV_DISP_ROTATION_90);
     }
     if (rotat == 180){
     lv_disp_set_rotation(disp, LV_DISP_ROTATION_180);
     }
     if (rotat == 270){
     lv_disp_set_rotation(disp, LV_DISP_ROTATION_270);
     }
   

    lv_disp_t * dispp = lv_display_get_default();
   lv_theme_t * theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
                                              false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    ui_Screen1_screen_init();

    lv_disp_load_scr(ui_Screen1);

    float n = _brightness/100;
   
    smartdisplay_lcd_set_backlight(n);

    lv_obj_add_event_cb(ui_Screen1, ui_event_Scrin, LV_EVENT_PRESSED, (void*)1);
    lv_obj_add_event_cb(ui_Screen1, ui_event_Scrin, LV_EVENT_LONG_PRESSED_REPEAT, (void*)1);

     value.valD = 1;

     setInterval(30);
     wasMqttConnected = mqttIsConnect();  // Инициализируем предыдущее состояние MQTT при старте
    }

    
void loop()
{ 
   
    IoTItem::loop();  // Вызов базового метода loop для обработки событий и интервалов
  
    static auto last_call = millis();
   // static bool wasMqttConnected = false;  // Отслеживаем предыдущее состояние подключения
    auto now = millis();

    // Обновляем тики LVGL каждые 10-15 мс (оптимизация: было 2 мс)
    // LV_DEF_REFR_PERIOD = 33 мс, поэтому более частое обновление избыточно
    // Увеличение до 10-15 мс экономит ~80% CPU без влияния на качество
    const uint16_t LVGL_TICK_PERIOD_MS = 15;  // Оптимальный период
    
    if (now - last_call >= LVGL_TICK_PERIOD_MS) {
        lv_tick_inc(now - lv_last_tick);
        lv_last_tick = now;
        lv_timer_handler();  // Обработчик дисплея с оптимизацией только измененных регионов
        last_call = now;
        
    }
    /*
if (mqttIsConnect() && !isSubscribed) {

    // Подписываемся на топики со статусами напрямую
String statusTopic = mqttPrefix + "/+/+/status/#";
if (mqtt.subscribe(statusTopic.c_str())) {
    SerialPrint("i", F("Panel"), "Subscribed to status topic: " + statusTopic);
    isSubscribed = true;
} else {
    SerialPrint("e", F("Panel"), "Failed to subscribe to status topic: " + statusTopic);
}
}
*/

}

void doByInterval() override{

   

    
    // Проверяем подключение к MQTT брокеру
    bool isMqttConnected = mqttIsConnect();
    if  (isMqttConnected && !wasMqttConnected) {
        // Подписываемся на топики со статусами напрямую
String statusTopic = mqttPrefix + "/+/+/status/#";
if (mqtt.subscribe(statusTopic.c_str())) {
    SerialPrint("i", F("Panel"), "Subscribed to status topic: " + statusTopic);
   // isSubscribed = true;
}
        // Подключение восстановлено - отправляем HELLO
       // publishStatusMqtt("HELLO", "connected");
      // String path = mqttPrefix;
      // mqtt.publish(path.c_str(), "HELLO" , false);
      if (publish(mqttPrefix, "HELLO")) {
        SerialPrint("i", F("Panel"), "MQTT connected - sent HELLO");
    } else {
        SerialPrint("i", F("Panel"), "Failed to send HELLO on MQTT connection");
    }
}
    wasMqttConnected = isMqttConnected;
 
    
}


      
 IoTValue execute(String command, std::vector<IoTValue> &param){

            if (command == "brightness"){
                float r = param[0].valD;
                r = r/100;
                smartdisplay_lcd_set_backlight(r);
            };

        return {};

     }
     
    ~Panel(){};
};


// ============================================================================
// Screen Event Handler
// ============================================================================

void ui_event_Scrin(lv_event_t * e) {   
      generateOrder(scrin, "1");
}


// ============================================================================
// Font Conversion Function
// ============================================================================

void conshrift(lv_font_t &shri, int &sh) {
    if (sh == 20) { shri = serg20; }
    else if (sh == 40) { shri = serg40; }
    else if (sh == 43) { shri = serg43; }
   // else if (sh == 45) { shri = serg45; }
    else if (sh == 60) { shri = serg60; }
    else if (sh == 80) { shri = serg80; }
    else if (sh == 100) { shri = serg100; }
    else if (sh == 120) { shri = serg120; }
    else { shri = serg20; }
}


// ============================================================================
// MQTT Value Send Function
// ============================================================================




    bool isNumeric(const String& str) {
    return Value::isNumericString(str.c_str());
}





// ============================================================================
// Screen Initialization Function
// ============================================================================

void ui_Screen1_screen_init(void)
{
    ui_Screen1 = lv_obj_create(NULL);
    lv_obj_add_flag(ui_Screen1, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Screen1, lv_color_hex(color1), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_scrollbar_mode(ui_Screen1,LV_SCROLLBAR_MODE_AUTO);
}


// ============================================================================
// Factory Function - Widget Creation
// ============================================================================

void *getAPI_Panel(String subtype, String param) {
    if (subtype == F("Panel")) {
        return new Panel(param);
    } else if (subtype == F("Panel_tabview")) {
        return new Panel_tabview(param);
    } else if (subtype == F("Panel_button")) {
        return new Panel_button(param);
    } else if (subtype == F("Panel_label")) {
        return new Panel_label(param);
    } else  if (subtype == F("Panel_swith")) {
        return new Panel_swith(param);
    } else  if (subtype == F("Panel_Container")) {
        return new Panel_Container(param); 
    } else  if (subtype == F("Panel_arc")) {
        return new Panel_arc(param);
    } else  if (subtype == F("Panel_arc_text")) {
        return new Panel_arc_text(param);     
    } else  if (subtype == F("Panel_TextArea")) {
        return new Panel_TextArea(param); 
    } else  if (subtype == F("Panel_bar")) {
        return new Panel_bar(param);  
    } else  if (subtype == F("Panel_slider")) {
        return new Panel_slider(param);
    } else  if (subtype == F("Panel_img")) {
        return new Panel_img(param);
    } else if (subtype == F("Panel_img_icon")) {
        return new Panel_img_icon(param);
    } else  if (subtype == F("Panel_AnalogClock")) {
        return new Panel_AnalogClock(param);
    } else  if (subtype == F("Panel_graph")) {
        return new Panel_graph(param);
    } else     {
        return nullptr;
    }
}
