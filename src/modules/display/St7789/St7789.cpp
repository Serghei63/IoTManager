/******************************************************************
  Library for Adafruit ST7735s displays
  Support for ST7735s 1.44 

  https://github.com/adafruit/Adafruit-ST7735-Library
  
  adapted for version 4dev @Serghei63
 ******************************************************************/
#include "Global.h"
#include "classes/IoTItem.h"

#include <Arduino.h>
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

 #include <map>


class St7789 : public IoTItem {
   private:
    unsigned int _x;
    unsigned int _y;
    unsigned int _x1;
    unsigned int _y1;
    String _id2show;
    String _descr;
    String _descr1;
    String _colour;
    String _fon;
    int _colour1 = TFT_WHITE;
    int _fon1 = TFT_BLACK ;
    int _shrift1 = 2;
    int _shrift = 2;
    int _prevStrSize = 0;
    int _prevStrSize1 = 0;
    int _rotation = 0;
    int _rotation1 = 0;
    int _size;
    String _fon_screen;
    int _fon_screen1 = TFT_BLACK;
    


    bool _isShow = true;    // экран показывает

   public:
    St7789(String parameters) : IoTItem(parameters) {

        String  xy;

    if (_size == 0){
            
         
             jsonRead(parameters, "size", _size);
             if (_size == 144 ) tft.init();   // initialize a ST7735S chip
  

        jsonRead(parameters, "rotation", _rotation);
        if (_rotation == 2) _rotation1 = 2;
        else if (_rotation == 3) _rotation1 = 3;
        else if (_rotation == 1) _rotation1 = 1;
        else  _rotation1 = 0;
        tft.setRotation(_rotation1);    //Landscape
         }
        jsonRead(parameters, "fon_screen", _fon_screen);

        if (_fon_screen == "YELLOW") _fon_screen1 = TFT_YELLOW;
              else if (_fon_screen == "RED") _fon_screen1 = TFT_RED;
              else if (_fon_screen == "GREEN") _fon_screen1 = TFT_GREEN;
              else if (_fon_screen == "BLUE") _fon_screen1 = TFT_BLUE;
              else if (_fon_screen == "WHITE") _fon_screen1 = TFT_WHITE;
              else if (_fon_screen == "CYAN") _fon_screen1 = TFT_CYAN;
              else if (_fon_screen == "MAGENTA") _fon_screen1 = TFT_MAGENTA;
              else if (_fon_screen == "ORANGE") _fon_screen1 = TFT_ORANGE;
              else  _fon_screen1 = TFT_BLACK;

        tft.fillScreen(_fon_screen1);
 

        Serial.println(F("Initialized"));
    
               

        jsonRead(parameters, "coord", xy);
        _x = selectFromMarkerToMarker(xy, ",", 0).toInt();
        _y = selectFromMarkerToMarker(xy, ",", 1).toInt();

        jsonRead(parameters, "descr", _descr);
        jsonRead(parameters, "id2show", _id2show);
        jsonRead(parameters, "descr1", _descr1);
        jsonRead(parameters, "shrift", _shrift);
        jsonRead(parameters, "colour", _colour);
        jsonRead(parameters, "fon", _fon);
    }

    void doByInterval() {
            
            String tmpStr = "";
       
            
            if (_descr != "none") tmpStr = _descr + " " + getItemValue(_id2show) + " " + _descr1;
                else tmpStr = getItemValue(_id2show);

            

            tft.setTextWrap(false);

            tft.setCursor(_x, _y);


             if (_fon == "YELLOW") _fon1 = TFT_YELLOW;
              else if (_fon == "RED") _fon1 = TFT_RED;
              else if (_fon == "GREEN") _fon1 = TFT_GREEN;
              else if (_fon == "BLUE") _fon1 = TFT_BLUE;
              else if (_fon == "WHITE") _fon1 = TFT_WHITE;
              else if (_fon == "CYAN") _fon1 = TFT_CYAN;
              else if (_fon == "MAGENTA") _fon1 = TFT_MAGENTA;
              else if (_fon == "ORANGE") _fon1 = TFT_ORANGE;
              else  _fon1 = TFT_BLACK;   
            
                 
              if (_colour == "YELLOW")  _colour1 = TFT_YELLOW;
               else if (_colour == "RED") _colour1 = TFT_RED ;
               else if (_colour == "GREEN") _colour1 = TFT_GREEN;
               else if (_colour == "BLUE") _colour1 = TFT_BLUE;
               else if (_colour == "BLACK") _colour1 = TFT_BLACK;
               else if (_colour == "CYAN") _colour1 = TFT_CYAN;
               else if (_colour == "MAGENTA") _colour1 = TFT_MAGENTA;
               else if (_colour == "ORANGE") _colour1 = TFT_ORANGE;
               else _colour1 = TFT_WHITE;
               
              
               tft.setTextColor(_colour1, _fon_screen1);
          

          _prevStrSize1 = tmpStr.length();
          if (_prevStrSize > _prevStrSize1){

            String _tmpStr = "";

            int strSize = _prevStrSize - _prevStrSize1;

            for (int i = 0; i < strSize; i++) _tmpStr += " ";

            tft.setTextSize(_shrift);
             
            tft.setCursor (_prevStrSize1 * 6 * _shrift + _x , _y);
            
            tft.print(_tmpStr);
          }
        if ( _shrift1 != _shrift || _x1 != _x || _y1 != _y){
          
                    tft.setCursor(_x1, _y1);
                    tft.setTextSize(_shrift1);
                    String _tmpStr = "";
                    for (int i = 0; i < _prevStrSize; i++) _tmpStr += " ";
                    
                    tft.print(_tmpStr);
                   
                    _shrift1 = _shrift;
                    _x1 = _x;
                    _y1 = _y;
                    }
          
          tft.setTextColor(_colour1, _fon1); 
          tft.setTextSize(_shrift);
          tft.setCursor(_x, _y);

          tft.print(tmpStr);

          _prevStrSize = tmpStr.length();
        
        
    }
 
    IoTValue execute(String command, std::vector<IoTValue> &param) {  // будет возможным использовать, когда сценарии запустятся
       
      /*  if (command == "noBacklight")
            display->noBacklight();
        else if (command == "backlight")
            display->backlight();
        else if (command == "noDisplay") {
            display->noDisplay();
            _isShow = false;
        }
         else if (command == "display") {
           // display.display();
            _isShow = true;
        } else if (command == "toggle") {
            if (_isShow) {
              //  display->noDisplay();
                _isShow = false;
            } else { 
              //  display.display();
                _isShow = true;
            }
        } else */
        
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
        } else if (command == "xycs") {
            if (param.size()) {
                _x = param[0].valD;
                _y = param[1].valD;
                _colour = param[2].valS;
                _shrift = param[3].valD;
            }
        } else if (command == "descr1") {
            if (param.size()) {
                _descr1 = param[0].valS;
            }
        } else if (command == "fon") {
                if (param.size()) {
                _fon = param[0].valS;
                }
        } else if (command == "line") {
            if (param.size()) {
                int a = param[0].valD;
                int b = param[1].valD;
                int c = param[2].valD;
                int d = param[3].valD;
                String _colour2 = param[4].valS;
                int _colour21;
                if (_colour2 == "YELLOW")  _colour21 = TFT_YELLOW;
               else if (_colour2 == "RED") _colour21 = TFT_RED ;
               else if (_colour2 == "GREEN") _colour21 = TFT_GREEN;
               else if (_colour2 == "BLUE") _colour21 = TFT_BLUE;
               else if (_colour2 == "BLACK") _colour21 = TFT_BLACK;
               else if (_colour2 == "CYAN") _colour21 = TFT_CYAN;
               else if (_colour2 == "MAGENTA") _colour21 = TFT_MAGENTA;
               else if (_colour2 == "ORANGE") _colour21 = TFT_ORANGE;
               else _colour21 = TFT_WHITE;
               
               tft.drawLine(a, b, c, d, _colour21);
            }
        }


        doByInterval();
        return {};


    }


    ~St7789(){};
};

void *getAPI_St7789(String subtype, String param) {
    if (subtype == F("St7789")) {
        return new St7789(param);
    } else {
        return nullptr;
    }
}
