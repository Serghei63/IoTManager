#include "Global.h"
#include "classes/IoTItem.h"

#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_GC9A01A.h"
//#include "Fonts/Tiny3x3a2pt7b.h"

#if defined(ESP32) // Feather Huzzah32
  #define TFT_CS         14
  #define TFT_RST        15
  #define TFT_DC         32

#elif defined(ESP8266)
#define TFT_DC D3
#define TFT_CS D4

#endif

// Hardware SPI on Feather or other boards
Adafruit_GC9A01A tft(TFT_CS, TFT_DC);

   #define BLACK      0x0000                                                               // some extra colors
   #define BLUE       0x001F
   #define RED        0xF800
   #define GREEN      0x07E0
   #define CYAN       0x07FF
   #define MAGENTA    0xF81F
   #define YELLOW     0xFFE0
   #define WHITE      0xFFFF
   #define ORANGE     0xFBE0
   #define GREY       0x84B5
   #define BORDEAUX   0xA000
   #define AFRICA     0xAB21   

//====================================================================
/*
   #define DEG2RAD 0.0174532925 

   int    frametime = 1000;                                                       // время кадра

  */ 

    #define DEG2RAD 0.0174532925  
    bool initial = 1;

   int    x_pos;
   int    y_pos; 
   int    center_x = 120;                                                         // центр x циферблата на 240 * 240 TFT-дисплее
   int    center_y = 120;                                                         // центр y циферблата на TFT-дисплее 240 * 240
   float  pivot_x, pivot_y,pivot_x_old, pivot_y_old;
   float  p1_x,p1_y,p2_x,p2_y,p3_x, p3_y, p4_x, p4_y, p5_x, p5_y; 
   float  p1_x_old,p1_y_old, p2_x_old, p2_y_old, p3_x_old, p3_y_old;
   float  p4_x_old, p4_y_old, p5_x_old, p5_y_old;
   float  angleOffset = 3.14;
   float  arc_x;
   float  arc_y;
   int    radius = 120;                                                            // центр y круговой шкалы                                                   
   float  angle_circle = 0;
   float  needleAngle = 0;
   int    iteration = 0;
   int    j;                                                            
   float  volt = 220;
   int    needle_multiplier = 1;
   float  needle_setter;             
   int    correction = 67;                                                         // калибровочный коэффициент, полученный опытным путем с помощью мультиметра


   // voltage wave analysis stuff 
   double sensorValue1 = 0;
   double sensorValue2 = 0;
   int crosscount = 0;
   int climb_flag = 0;
   int val[100];
   int max_v = 0;
   double VmaxD = 0;
   double VeffD = 0;
   double Veff = 0;

   // voltage rolling averaging stuff 
   const byte nvalues = 10;                                                                // скользящий средний размер окна
   static byte current = 0;                                                                // индекс для текущего значения
   static byte cvalues = 0;                                                                // количество прочитанных значений (<= nvalues)
   static float sum = 0;                                                                   // скользящая сумма
   static float values[nvalues];
   float averagedVoltage = 235; 
//===================================================================


   float sx = 0, sy = 1, mx = 1, my = 0, hx = -1, hy = 0;                              // saved H, M, S x & y multipliers
   float sdeg = 0, mdeg= 0, hdeg = 0;
   uint16_t osx = 120, osy = 120, omx = 120, omy = 120, ohx = 120, ohy = 120;          // saved H, M, S x & y coords
   uint16_t x0=0, x1=0, yy0=0, yy1=0;
   uint32_t targetTime = 0;                                                            // for next 1 second timeout

   int hh = 0;                                                                         // hours variable
   int mm = 0;                                                                         // minutes variable
   int ss = 0;    
   
                                                               // seconds variable

#include <map>


class Gc9 : public IoTItem {
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
    int _colour1 = WHITE;
    int _fon1 = BLACK ;
    int _shrift1 = 2;
    int _shrift;
    int _prevStrSize = 0;
    int _prevStrSize1;
    int _rotation;
    int _rotation1 = 0;
    int _size;
    String _fon_screen;
    int _fon_screen1 = BLACK;



    bool _isShow = true;    // экран показывает



   public:
    Gc9(String parameters) : IoTItem(parameters) {

        String  xy;

        tft.begin(); 

        jsonRead(parameters, "rotation", _rotation);
        if (_rotation == 2) _rotation1 = 2;
        else if (_rotation == 3) _rotation1 = 3;
        else if (_rotation == 1) _rotation1 = 1;
        else  _rotation1 = 0;
        tft.setRotation(_rotation1);    //Landscape

        jsonRead(parameters, "fon_screen", _fon_screen);

        if (_fon_screen == "YELLOW") _fon_screen1 = YELLOW;
              else if (_fon_screen == "RED") _fon_screen1 = RED;
              else if (_fon_screen == "GREEN") _fon_screen1 = GREEN;
              else if (_fon_screen == "BLUE") _fon_screen1 = BLUE;
              else if (_fon_screen == "WHITE") _fon_screen1 = WHITE;
              else if (_fon_screen == "CYAN") _fon_screen1 = CYAN;
              else if (_fon_screen == "MAGENTA") _fon_screen1 = MAGENTA;
              else if (_fon_screen == "GREY") _fon_screen1 = GREY;
              else if (_fon_screen == "BORDEAUX") _fon_screen1 = BORDEAUX;
              else if (_fon_screen == "ORANGE") _fon_screen1 = ORANGE;//AFRICA
              else if (_fon_screen == "AFRICA") _fon_screen1 = AFRICA;//AFRICA
              else  _fon_screen1 = BLACK;

                 #define BLACK      0x0000                                                               // some extra colors


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

//======================================================================================
 
           tft.fillScreen (BLACK);
   tft.drawCircle (center_x, center_y,120, BLACK);             
   pivot_x = center_x;                               // Центр стрелки 120 по Х
   pivot_y = center_y+50;                            // Центр стрелки 120 + 50 == 170 по Y    

   p1_x_old = center_x; p1_y_old = center_y+50;      // Вершины стрелок
   p2_x_old = center_x; p2_y_old = center_y+50;
   p3_x_old = center_x; p3_y_old = center_y+50;
   p4_x_old = center_x; p4_y_old = center_y+50;
   p5_x_old = center_x; p5_y_old = center_y+50;

   volt = 240;                                                                             // начальное значение установки иглы
   create_dial ();
   needle_setter = volt;
   needleAngle = (((needle_setter)*DEG2RAD*1.8)-3.14);
   needle();  
   draw_pivot ();

   //========================================================================
   

   //hh = timeClient.getHours ();
  // hh = _time_local.hour;
  // mm = timeClient.getMinutes ();
  // mm = _time_local.minute;
  // ss = timeClient.getSeconds ();

  // ss = _time_local.second;

     iteration++;
   Serial.println ();      
   Serial.print ("iteration ");
   Serial.println (iteration); 
   read_the_zmtp101 ();  
   Serial.print ("volt out of smpt01B: ");
   Serial.println (volt);  

  averagedVoltage = movingAverage(volt);


   Serial.print ("averaged volt =      ");
   Serial.println (averagedVoltage);
   Serial.println ();

    
   Serial.println ();   

   displayNumerical ();
   needle_setter = averagedVoltage;     
   needle();
   draw_pivot (); 

    }

    void doByInterval() {

        //==============================================================
           displayNumerical ();

       
   needle_setter = averagedVoltage;  // крутит стрелку   
   needle();
   draw_pivot (); 

      iteration++;
   Serial.println ();      
  Serial.print ("iteration ");
   Serial.println (iteration); 
   read_the_zmtp101 ();  
   Serial.print ("volt out of smpt01B: ");
   Serial.println (volt);  

   averagedVoltage = movingAverage(volt);


   //=====================================================


                   String tmpStr = "";
       
            
            if (_descr != "none") tmpStr = _descr + " " + getItemValue(_id2show) + " " + _descr1;
                else tmpStr = getItemValue(_id2show);

            //tft.setFont (&Tiny3x3a2pt7b);

            tft.setTextWrap(false);

            tft.setCursor(_x, _y);


             if (_fon == "YELLOW") _fon1 = YELLOW;
              else if (_fon == "RED") _fon1 = RED;
              else if (_fon == "GREEN") _fon1 = GREEN;
              else if (_fon == "BLUE") _fon1 = BLUE;
              else if (_fon == "WHITE") _fon1 = WHITE;
              else if (_fon == "CYAN") _fon1 = CYAN;
              else if (_fon == "MAGENTA") _fon1 = MAGENTA;
              else if (_fon == "ORANGE") _fon1 = ORANGE;
              else if (_fon == "AFRICA") _fon1 = AFRICA;//AFRICA
              else  _fon1 = BLACK;   
            
                 
              if (_colour == "YELLOW")  _colour1 = YELLOW;
               else if (_colour == "RED") _colour1 = RED ;
               else if (_colour == "GREEN") _colour1 = GREEN;
               else if (_colour == "BLUE") _colour1 = BLUE;
               else if (_colour == "BLACK") _colour1 = BLACK;
               else if (_colour == "CYAN") _colour1 = CYAN;
               else if (_colour == "MAGENTA") _colour1 = MAGENTA;
               else if (_colour == "ORANGE") _colour1 = ORANGE;
               else if (_colour == "AFRICA") _colour1 = AFRICA;
               else _colour1 = WHITE;
               
              
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

             //hh = timeClient.getHours ();
   hh = _time_local.hour;
  // mm = timeClient.getMinutes ();
   mm = _time_local.minute;
  // ss = timeClient.getSeconds ();

   ss = _time_local.second;
    }




    void needle (){                                                                        // иголка                                                                     // динамическое управление иглами

   tft.drawLine (pivot_x, pivot_y, p1_x_old, p1_y_old, AFRICA);                            // удалить старую иглу  
   tft.fillTriangle (p1_x_old, p1_y_old, p2_x_old, p2_y_old, p3_x_old, p3_y_old, AFRICA);  // удалить старый наконечник стрелы
   tft.fillTriangle (pivot_x, pivot_y, p4_x_old, p4_y_old, p5_x_old, p5_y_old, AFRICA);    // удалить старый наконечник стрелы
    
   needleAngle = (((needle_setter)*0.01745331*1.8)-3.14);
   p1_x = (pivot_x + ((radius)*cos(needleAngle)));                                         // кончик иглы
   p1_y = (pivot_y + ((radius)*sin(needleAngle))); 

   p2_x = (pivot_x + ((radius-15)*cos(needleAngle-0.05)));                                 // треугольник иглы слева
   p2_y = (pivot_y + ((radius-15)*sin(needleAngle-0.05))); 

   p3_x = (pivot_x + ((radius-15)*cos(needleAngle+0.05)));                                 // треугольник справа
   p3_y = (pivot_y + ((radius-15)*sin(needleAngle+0.05))); 

   p4_x = (pivot_x + ((radius-90)*cos(angleOffset+(needleAngle-0.2))));                    // треугольник иглы слева
   p4_y = (pivot_y + ((radius-90)*sin(angleOffset+(needleAngle-0.2)))); 

   p5_x = (pivot_x + ((radius-90)*cos(angleOffset+(needleAngle+0.2))));                    // треугольник справа
   p5_y = (pivot_y + ((radius-90)*sin(angleOffset+(needleAngle+0.2)))); 
  
   p1_x_old = p1_x; p1_y_old = p1_y;                                                       // запомнить предыдущее положение иглы
   p2_x_old = p2_x; p2_y_old = p2_y;                                                                         
   p3_x_old = p3_x; p3_y_old = p3_y;                                                                      

   p4_x_old = p4_x; p4_y_old = p4_y;                                                       // запомнить предыдущее положение противовеса иглы
   p5_x_old = p5_x; p5_y_old = p5_y;                                                                      


   tft.drawLine (pivot_x, pivot_y, p1_x, p1_y, BLACK);                                     // создать иглу 
   tft.fillTriangle (p1_x, p1_y, p2_x, p2_y, p3_x, p3_y, BLACK);                           // создать указатель кончика иглы
   tft.drawLine (center_x-80, center_y+70, center_x+80,center_y+70, WHITE);                // ремонт пола 
   tft.fillTriangle (pivot_x, pivot_y, p4_x, p4_y, p5_x, p5_y, BLACK);                     // создать противовес иглы
}


void create_dial (){                                                                       // создать циферблат

   tft.fillCircle (center_x, center_y,120, AFRICA);                                        // общее поле набора
   tft.drawCircle (center_x, center_y,118,GREY);  
   tft.drawCircle (center_x, center_y,117,BLACK);
   tft.drawCircle (center_x, center_y,116,BLACK);  
   tft.drawCircle (center_x, center_y,115,GREY);

   for (j= 30; j<75    ; j+=5)
       {
        needleAngle = ((j*DEG2RAD*1.8)-3.14);
        arc_x = (pivot_x + ((radius+15)*cos(needleAngle)));                                // кончик иглы
        arc_y = (pivot_y + ((radius+15)*sin(needleAngle))); 
        tft.drawPixel  (arc_x,arc_y,BLACK);
        tft.fillCircle (arc_x,arc_y,2, BLACK);

        tft.drawPixel  (arc_x,arc_y,RED);                                                    // Точки на циферблате 
        tft.fillCircle (arc_x,arc_y,2,RED);
        }

   tft.setTextColor (BLACK,AFRICA);    
   tft.setTextSize (2);
   tft.setCursor (center_x+15, center_y+40);
   tft.print ("V - AC");                                                                                                                                                                          
   tft.drawLine (center_x-80, center_y+70, center_x+80,center_y+70, WHITE);                // создать пол
   
}


void draw_pivot (){                                                                        // рисовать стержень
 
   tft.fillCircle (pivot_x, pivot_y,8,RED);               
   tft.drawCircle (pivot_x, pivot_y,8,BLACK);            
   tft.drawCircle (pivot_x, pivot_y,3,BLACK);      
}

void read_the_zmtp101 (){
  
    for ( int i = 0; i < 100; i++ )
      {
       sensorValue1 = analogRead (A0);
       if (analogRead (A0) > 511) 
          {
           val[i] = sensorValue1;
          }
       else
          {
           val[i] = 0;
          }
       delay (1);
      }

   max_v = 0;

   for ( int i = 0; i < 100; i++ )
      {
       if ( val[i] > max_v )
         {
          max_v = val[i];
         }
       val[i] = 0;
      }
   if (max_v != 0)
      {
       VmaxD = max_v;
       VeffD = VmaxD / sqrt(2);
       Veff = (((VeffD - 420.76) / -90.24) * -210.2) + 210.2;
      }
   else
      {
       Veff = 0;
      }

   //volt = (Veff-correction);
   volt = analogRead(0);
   VmaxD = 0;
}

void displayNumerical (){

   tft.fillRect (center_x-82, center_y+40, 62,16,AFRICA);
   tft.setTextColor (BLACK);    
   tft.setTextSize (2);
   tft.setCursor (center_x-80, center_y+40);
   tft.print (averagedVoltage,1);   
}

float movingAverage(float value) {       //  скользящее среднее

   sum += value;

  // если окно заполнено, отрегулируйте сумму, удалив самое старое значение
   if (cvalues == nvalues)
     sum -= values[current];

   values[current] = value;          // заменить самый старый на последний

   if (++current >= nvalues)
     current = 0;

   if (cvalues < nvalues)
     cvalues += 1;

   return sum/cvalues;
}

    IoTValue execute(String command, std::vector<IoTValue> &param) {  // будет возможным использовать, когда сценарии запустятся
       
        
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
        }else if (command == "fon") {
                if (param.size()) {
                _fon = param[0].valS;
                }
        }else if (command == "fon_screen") {
                if (param.size()) {
                _fon_screen = param[0].valS;
                }
        }  else if (command == "line") {
            if (param.size()) {
                int a = param[0].valD;
                int b = param[1].valD;
                int c = param[2].valD;
                int d = param[3].valD;
                String _colour2 = param[4].valS;
                int _colour21;
                if (_colour2 == "YELLOW")  _colour21 = YELLOW;
               else if (_colour2 == "RED") _colour21 = RED ;
               else if (_colour2 == "GREEN") _colour21 = GREEN;
               else if (_colour2 == "BLUE") _colour21 = BLUE;
               else if (_colour2 == "BLACK") _colour21 = BLACK;
               else if (_colour2 == "CYAN") _colour21 = CYAN;
               else if (_colour2 == "MAGENTA") _colour21 = MAGENTA;
               else if (_colour2 == "ORANGE") _colour21 = ORANGE;
               else if (_colour2 == "AFRICA") _colour21 = AFRICA;
               else _colour21 = WHITE;
               
               tft.drawLine(a, b, c, d, _colour21);
            }

        }  else if (command == "watch") {
            if (param.size()) {
               // int a = param[0].valD;
               // int b = param[1].valD;
               // int c = param[2].valD;
              //  int d = param[3].valD;
               // String _colour2 = param[4].valS;
               // int _colour21;

               //======================================================================================

           tft.fillScreen (BLACK);
   tft.drawCircle (center_x, center_y,120, BLACK);             
   pivot_x = center_x;                               // Центр стрелки 120 по Х
   pivot_y = center_y;                            // Центр стрелки 120 + 50 == 170 по Y    

   p1_x_old = center_x; p1_y_old = center_y+50;      // Вершины стрелок
   p2_x_old = center_x; p2_y_old = center_y+50;
   p3_x_old = center_x; p3_y_old = center_y+50;
   p4_x_old = center_x; p4_y_old = center_y+50;
   p5_x_old = center_x; p5_y_old = center_y+50;

   volt = 240;                                                                             // начальное значение установки иглы
   create_dial ();
   needle_setter = volt;
   needleAngle = (((needle_setter)*DEG2RAD*1.8)-3.14);
   needle();  
   draw_pivot ();

   //========================================================================
            //    if (_colour2 == "YELLOW")  _colour21 = YELLOW;
            //   else if (_colour2 == "RED") _colour21 = RED ;
           //    else if (_colour2 == "GREEN") _colour21 = GREEN;
            //   else if (_colour2 == "BLUE") _colour21 = BLUE;
            //   else if (_colour2 == "BLACK") _colour21 = BLACK;
             //  else if (_colour2 == "CYAN") _colour21 = CYAN;
             //  else if (_colour2 == "MAGENTA") _colour21 = MAGENTA;
             //  else if (_colour2 == "ORANGE") _colour21 = ORANGE;
             //  else if (_colour2 == "AFRICA") _colour21 = AFRICA;
             //  else _colour21 = WHITE;
               
            //   tft.drawLine(a, b, c, d, _colour21);


                                                                    // иголка                                                                     // динамическое управление иглами

   tft.drawLine (pivot_x, pivot_y, p1_x_old, p1_y_old, AFRICA);                            // удалить старую иглу  
   tft.fillTriangle (p1_x_old, p1_y_old, p2_x_old, p2_y_old, p3_x_old, p3_y_old, AFRICA);  // удалить старый наконечник стрелы
   tft.fillTriangle (pivot_x, pivot_y, p4_x_old, p4_y_old, p5_x_old, p5_y_old, AFRICA);    // удалить старый наконечник стрелы
    
   needleAngle = (((needle_setter)*0.01745331*1.8)-3.14);
   p1_x = (pivot_x + ((radius)*cos(needleAngle)));                                         // кончик иглы
   p1_y = (pivot_y + ((radius)*sin(needleAngle))); 

   p2_x = (pivot_x + ((radius-15)*cos(needleAngle-0.05)));                                 // треугольник иглы слева
   p2_y = (pivot_y + ((radius-15)*sin(needleAngle-0.05))); 

   p3_x = (pivot_x + ((radius-15)*cos(needleAngle+0.05)));                                 // треугольник справа
   p3_y = (pivot_y + ((radius-15)*sin(needleAngle+0.05))); 

   p4_x = (pivot_x + ((radius-90)*cos(angleOffset+(needleAngle-0.2))));                    // треугольник иглы слева
   p4_y = (pivot_y + ((radius-90)*sin(angleOffset+(needleAngle-0.2)))); 

   p5_x = (pivot_x + ((radius-90)*cos(angleOffset+(needleAngle+0.2))));                    // треугольник справа
   p5_y = (pivot_y + ((radius-90)*sin(angleOffset+(needleAngle+0.2)))); 
  
   p1_x_old = p1_x; p1_y_old = p1_y;                                                       // запомнить предыдущее положение иглы
   p2_x_old = p2_x; p2_y_old = p2_y;                                                                         
   p3_x_old = p3_x; p3_y_old = p3_y;                                                                      

   p4_x_old = p4_x; p4_y_old = p4_y;                                                       // запомнить предыдущее положение противовеса иглы
   p5_x_old = p5_x; p5_y_old = p5_y;                                                                      


   tft.drawLine (pivot_x, pivot_y, p1_x, p1_y, BLACK);                                     // создать иглу 
   tft.fillTriangle (p1_x, p1_y, p2_x, p2_y, p3_x, p3_y, BLACK);                           // создать указатель кончика иглы
   tft.drawLine (center_x-80, center_y+70, center_x+80,center_y+70, WHITE);                // ремонт пола 
   tft.fillTriangle (pivot_x, pivot_y, p4_x, p4_y, p5_x, p5_y, BLACK);                     // создать противовес иглы
}


//void create_dial (){                                                                       // создать циферблат

   tft.fillCircle (center_x, center_y,120, AFRICA);                                        // общее поле набора
   tft.drawCircle (center_x, center_y,118,GREY);  
   tft.drawCircle (center_x, center_y,117,BLACK);
   tft.drawCircle (center_x, center_y,116,BLACK);  
   tft.drawCircle (center_x, center_y,115,GREY);

   for (j= 30; j<75    ; j+=5)
       {
        needleAngle = ((j*DEG2RAD*1.8)-3.14);
        arc_x = (pivot_x + ((radius+15)*cos(needleAngle)));                                // кончик иглы
        arc_y = (pivot_y + ((radius+15)*sin(needleAngle))); 
        tft.drawPixel  (arc_x,arc_y,BLACK);
        tft.fillCircle (arc_x,arc_y,2, BLACK);

        tft.drawPixel  (arc_x,arc_y,RED);                                                    // Точки на циферблате 
        tft.fillCircle (arc_x,arc_y,2,RED);
        }

   tft.setTextColor (BLACK,AFRICA);    
   tft.setTextSize (1);
  // tft.setCursor (center_x+15, center_y+40);
   tft.setCursor (center_x, center_y+40);
   tft.print ("Krasnikoff");                                                                                                                                                                          
   tft.drawLine (center_x-80, center_y+70, center_x+80,center_y+70, WHITE);                // создать пол
   
//}


//void draw_pivot (){                                                                        // рисовать стержень
 
   tft.fillCircle (pivot_x, pivot_y,8,RED);               
   tft.drawCircle (pivot_x, pivot_y,8,BLACK);            
   tft.drawCircle (pivot_x, pivot_y,3,BLACK);      
//}

   tft.fillRect (center_x-82, center_y+40, 62,16,AFRICA);
   tft.setTextColor (BLACK);    
   tft.setTextSize (2);
   tft.setCursor (center_x-80, center_y+40);
   tft.print (averagedVoltage,1);   
            //} 
  
              
       // }

                }  else if (command == "clock") {

            if (param.size()) {
               // int a = param[0].valD;
               // int b = param[1].valD;
               // int c = param[2].valD;
              //  int d = param[3].valD;
               // String _colour2 = param[4].valS;
               // int _colour21;

               //======================================================================================

   if (targetTime < millis())
      {
      targetTime += 1000;
      ss++;                                                                            // advance second
      if (ss==60)
         {
         ss=0;
         mm++;                                                                         // advance minute
         if(mm>59)
            {
            mm=0;
            hh++;                                                                      // advance hour
            if (hh>23) 
               {
               hh=0;
              // timeClient.update ();                                                   // update at midnight
               }
            }
         }
          
      // pre-compute hand degrees, x & y coords for a fast screen update предварительно вычислить градусы рук, координаты x и y для быстрого обновления экрана
      sdeg = ss*6;                                                                     // 0-59 -> 0-354
      mdeg = mm*6+sdeg*0.01666667;                                                     // 0-59 -> 0-360 - includes seconds
      hdeg = hh*30+mdeg*0.0833333;                                                     // 0-11 -> 0-360 - includes minutes and seconds
      hx = cos ((hdeg-90)*DEG2RAD);    
      hy = sin ((hdeg-90)*DEG2RAD);
      mx = cos ((mdeg-90)*DEG2RAD);    
      my = sin ((mdeg-90)*DEG2RAD);
      sx = cos ((sdeg-90)*DEG2RAD);    
      sy = sin ((sdeg-90)*DEG2RAD);

      if (ss==0 || initial) 
         {
         initial = 0;
         tft.drawLine (ohx, ohy, 120, 121, BLACK);                                     // erase hour and minute hand positions every minute
         ohx = hx*62+121;    
         ohy = hy*62+121;
         tft.drawLine (omx, omy, 120, 121, BLACK);
         omx = mx*84+120;    
         omy = my*84+121;
         }
 
      tft.drawLine (osx, osy, 120, 121, BLACK);                                      // redraw new hand positions, hour and minute hands not erased here to avoid flicker
      osx = sx*90+121;    
      osy = sy*90+121;
      tft.drawLine (osx, osy, 120, 121, RED);
      tft.drawLine (ohx, ohy, 120, 121, WHITE);
      tft.drawLine (omx, omy, 120, 121, WHITE);
      tft.drawLine (osx, osy, 120, 121, RED);
      tft.fillCircle(120, 121, 3, RED);
      }
}

//void createDial (){

   tft.setTextColor (WHITE, GREY);  
   tft.fillCircle(120, 120, 118, BORDEAUX);                                           // создает внешнее кольцо
   tft.fillCircle(120, 120, 110, BLACK); //BLUE 
   //tft.fillCircle(120, 120, 110, BLUE); //BLUE 
 
   for (int i = 0; i<360; i+= 30)                                                     // нарисуйте 12 отрезков на внешнем кольце 
      {                                                   
      sx = cos((i-90)*DEG2RAD);
      sy = sin((i-90)*DEG2RAD);
      x0 = sx*114+120;
      yy0 = sy*114+120;
      x1 = sx*100+120;
      yy1 = sy*100+120;
      tft.drawLine(x0, yy0, x1, yy1, GREEN);
      }
                                                             
   for (int i = 0; i<360; i+= 6)                                                      // нарисовать 60 точек - минутные маркеры
      {
      sx = cos((i-90)*DEG2RAD);
      sy = sin((i-90)*DEG2RAD);
      x0 = sx*102+120;
      yy0 = sy*102+120;    
      tft.drawPixel(x0, yy0, WHITE);
    
      if(i==0  || i==180) tft.fillCircle (x0, yy0, 2, WHITE);                         // нарисовать точки главного квадранта
      if(i==90 || i==270) tft.fillCircle (x0, yy0, 2, WHITE);
     //}
  
   //tft.fillCircle(120, 121, 3, WHITE);                                               // центр вращения
   targetTime = millis() + 1000;   
}
            
              
        }


        doByInterval();
        return {};


    }

    ~Gc9(){};
};

void *getAPI_Gc9(String subtype, String param) {
    if (subtype == F("Gc9")) {
        return new Gc9(param);
    } else {
        return nullptr;
    }
}

