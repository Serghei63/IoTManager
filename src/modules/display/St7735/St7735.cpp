/******************************************************************
  Library for Adafruit ST7735s displays
  Support for ST7735s 1.44, 1.8

  https://github.com/adafruit/Adafruit-ST7735-Library
  
  adapted for version 4dev @Sergei Yakovlev
 ******************************************************************/
#include "Global.h"
#include "classes/IoTItem.h"

#include <Arduino.h>
#include "Adafruit_GFX.h"   // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include "FontsRus/Bahamas16.h"
#include "FontsRus/Bahamas8.h"
#include "FontsRus/Bahamas12.h"
#include "FontsRus/Bahamas18.h"
#include <SPI.h>

#if defined(ESP32) // Feather Huzzah32
  #define TFT_CS         5//14
  #define TFT_RST        26//15
  #define TFT_DC         17//32

#elif defined(ESP8266)   
  #define TFT_CS         4
  #define TFT_RST        16                                            
  #define TFT_DC         5
  #define TFT_MOSI       13  // Data out
  #define TFT_SCLK       14  // Clock out 

//#else
  // For the breakout board, you can use any 2 or 3 pins.
  // These pins will also work for the 1.8" TFT shield.
//  #define TFT_CS        10
 // #define TFT_RST        9 // Or set to -1 and connect to Arduino RESET pin
//  #define TFT_DC         8
#endif


Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);




int _size = 0;

#include <map>

// преоброзование цвета из String в int
int colour(String colour7){
     int colour5;
            if (colour7 == "YELLOW")  colour5 = ST77XX_YELLOW;
            else if (colour7 == "RED") colour5 = ST77XX_RED;
            else if (colour7 == "GREEN") colour5 = ST77XX_GREEN;
            else if (colour7 == "BLUE") colour5 = ST77XX_BLUE;
            else if (colour7 == "WHITE") colour5 = ST77XX_WHITE;
            else if (colour7 == "CYAN") colour5 = ST77XX_CYAN;
            else if (colour7 == "MAGENTA") colour5 = ST77XX_MAGENTA;
            else if (colour7 == "ORANGE") colour5 = ST77XX_ORANGE;
            else  colour5 = ST77XX_BLACK;
    return colour5;
}

// выбор шрифта    
int shrift(int _shrift5){
        

            if (_shrift5 == 2) {tft.setFont(&Bahamas12pt8b);
                                tft.setTextSize(1);}
            else if (_shrift5 == 3) {tft.setFont(&Bahamas16pt8b);
                                tft.setTextSize(1);}
            else if (_shrift5 == 4) {tft.setFont(&Bahamas18pt8b);
                                tft.setTextSize(1); }
            else if (_shrift5 == 5) {tft.setFont(&Bahamas12pt8b);
                                tft.setTextSize(2);}
            else if (_shrift5 == 6) {tft.setFont(&Bahamas16pt8b);
                                tft.setTextSize(2);}   
            else  {tft.setFont(&Bahamas8pt8b);
                                tft.setTextSize(1);} 
        return{};                                
    }




class St7735 : public IoTItem {
   private:
    unsigned int _x;
    unsigned int _y ;
    unsigned int _x1 = 0;
    unsigned int _y1 = 0;
    String _id2show ;
    String _descr ;
    String _descr1 ;
    String _colour;
    String _fon ;
    int _colour1 = ST77XX_WHITE;
    int _fon1 = ST77XX_BLACK ;
    int _shrift1 = 2;
    int _shrift = 2;
    int _prevStrSize = 0;
    int _prevStrSize1 = 0;
    int _rotation = 0;
    int _rotation1 = 0;
   // int _size;
    String _fon_screen ;
    int _fon_screen1 = ST77XX_BLACK;
    String _tmpStr = "";
    String  tmpStr = "";


    bool _isShow = true;    // экран показывает




   public:
    St7735(String parameters) : IoTItem(parameters) {

        String  xy;

        if (_size == 0){
            
         
            jsonRead(parameters, "size", _size);
            if (_size == 144 ) tft.initR(INITR_144GREENTAB);
            else tft.initR(INITR_BLACKTAB); // Init ST7735R chip, green tab

             // поворот экрана      
            jsonRead(parameters, "rotation", _rotation);
            if (_rotation == 2) _rotation1 = 2;
            else if (_rotation == 3) _rotation1 = 3;
            else if (_rotation == 1) _rotation1 = 1;
            else  _rotation1 = 0;
            tft.setRotation(_rotation1);    //Landscape
            
            // устоновка фона
            jsonRead(parameters, "fon_screen", _fon_screen);
            _fon_screen1 = colour(_fon_screen);
            tft.fillScreen(_fon_screen1);
        
           Serial.println(F("Initialized"));
        }


        jsonRead(parameters, "fon_screen", _fon_screen);
           
        _fon_screen1 = colour(_fon_screen);

        jsonRead(parameters, "id2show", _id2show);
        jsonRead(parameters, "descr1", _descr1);
        jsonRead(parameters, "shrift", _shrift);
        jsonRead(parameters, "colour", _colour);
        jsonRead(parameters, "fon", _fon);
        jsonRead(parameters, "coord", xy);
        _x = selectFromMarkerToMarker(xy, ",", 0).toInt();
        _y = selectFromMarkerToMarker(xy, ",", 1).toInt();

       
    }
    

    void doByInterval() {
            
          
        if (_descr != "none") tmpStr = _descr + " " + getItemValue(_id2show) + " " + _descr1;
        else tmpStr = getItemValue(_id2show);

        tft.setTextWrap(false);

        _fon1 = colour(_fon); // фон
                 
        _colour1 = colour(_colour);  // цвет шрифта
         
        // если что-нибудь поменялось сначала печатаем старую строку цветом фона,
        //  затем новую строку. 
        if ( _shrift1 != _shrift || _x1 != _x || _y1 != _y || _tmpStr != tmpStr){
                tft.setTextColor(_fon_screen1, _fon1); 
                tft.setCursor(_x1, _y1);

                shrift(_shrift1);

                tft.print(_tmpStr);
                   
                
                
                tft.setTextColor(_colour1, _fon1);
           
                shrift(_shrift);                   
                tft.setCursor(_x, _y);

                tft.print(tmpStr);

                
                _tmpStr = tmpStr;
                _shrift1 = _shrift;
                _x1 = _x;
                _y1 = _y;
        } 
    }
 
    IoTValue execute(String command, std::vector<IoTValue> &param) {  // будет возможным использовать, когда сценарии запустятся
       
      /*  if (command == "noBacklight")
            display->noBacklight();
        else if (command == "backlight")
            display->backlight();
        else */
        if (command == "noDisplay") {
            tft.enableDisplay(false);
            _isShow = false;
            
        }
         else if (command == "display") {
             tft.enableDisplay(true);
            _isShow = true;
            
        } else if (command == "toggle") {
            if (_isShow) {
                tft.enableDisplay(false);
                _isShow = false;
            } else { 
                tft.enableDisplay(true);
                _isShow = true;
            }
        } else 
        
        
        if (command == "x") {
            if (param.size()) {
                _x = param[0].valD;
            }
        } else if (command == "y") {
            if (param.size()) {
                _y = param[0].valD;
            }
        } else if (command == "descr") {
            if (param.size()) {
                _descr = param[0].valS;
            }
        } else if (command == "id2show") {
            if (param.size()) {
                _id2show = param[0].valS;
            }
        } else if (command == "shrift") {
                if (param.size()) {
                _shrift = param[0].valD;
            }
        } else if (command == "xy") {
            if (param.size()) {
                _x = param[0].valD;
                _y = param[1].valD;
            }
        }   else if (command == "colour") {
                if (param.size()) {
                _colour = param[0].valS;
                }
        } else if (command == "xycs") {     //меняем координаты, цвет и шрифт
            if (param.size()) {             //пример
                _x = param[0].valD;         //St64.xycs(0,60,"YELLOW",4);
                _y = param[1].valD;
                _colour = param[2].valS;
                _shrift = param[3].valD;
            }
        } else if (command == "descr1") {
            if (param.size()) {
                _descr1 = param[0].valS;
            }
        } 
        /*else if (command == "fon") {
                if (param.size()) {
                _fon = param[0].valS;
                }
        } */
        else if (command == "line") {
            if (param.size()) {
                int a = param[0].valD;
                int b = param[1].valD;
                int c = param[2].valD;
                int d = param[3].valD;
                String _colour2 = param[4].valS;
                int _colour21;
              
               _colour21 = colour(_colour2);
               
               tft.drawLine(a, b, c, d, _colour21);
            }
        }


        doByInterval();
        return {};


    }

   


    ~St7735(){};
};

void *getAPI_St7735(String subtype, String param) {
    if (subtype == F("St7735")) {
        return new St7735(param);
    } else {
        return nullptr;
    }
}
