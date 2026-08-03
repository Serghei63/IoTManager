#include "Global.h"
#include "classes/IoTUart.h"

#include "classes/IoTItem.h"

extern IoTGpio IoTgpio;

// Переключение страниц

 char x0 [10] ={0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, 0x00};
 char x1 [10] ={0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, 0x01};

 char x2 [10] ={0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, 0x02};
 char x3 [10] ={0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, 0x03};
 char x4 [10] ={0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, 0x04};
 char x5 [10] ={0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, 0x05};
 char x6 [10] ={0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, 0x06};
 char x7 [10] ={0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, 0x07};

 char x8 [10] ={0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, 0x08};
 char x9 [10] ={0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, 0x09};



 char x5000m [7] ={0x5A,0xA5,0x05,0x82,0x50,0x00,0x00};
 int x5000w = 0;

  char x0m [13] ={0x5A, 0xA5, 0x0B, 0x82, 0x03, 0x10, 0x5A, 0xA5, 0x01, 0x00, 0x00, 0x01, 0x00};
 int x0w = 0;

  char x5001m [7] ={0x5A,0xA5,0x05,0x82,0x50,0x01,0x00};
 int x5001w = 0;

  char x1m [13] ={0x5A, 0xA5, 0x0B, 0x82, 0x03, 0x10, 0x5A, 0xA5, 0x01, 0x00, 0x01, 0x01, 0x00};
 int x1w = 0;

  char x5002m [7] ={0x5A,0xA5,0x05,0x82,0x50,0x02,0x00};
 int x5002w = 0;

  char x2m [13] ={0x5A, 0xA5, 0x0B, 0x82, 0x03, 0x10, 0x5A, 0xA5, 0x01, 0x00, 0x02, 0x01, 0x00};
 int x2w = 0;

  char x5003m [7] ={0x5A,0xA5,0x05,0x82,0x50,0x03,0x00};
 int x5003w = 0;

  char x3m [13] ={0x5A, 0xA5, 0x0B, 0x82, 0x03, 0x10, 0x5A, 0xA5, 0x01, 0x00, 0x03, 0x01, 0x00};
 int x3w = 0;

  char x5004m [7] ={0x5A,0xA5,0x05,0x82,0x50,0x04,0x00};
 int x5004w = 0;

  char x4m [13] ={0x5A, 0xA5, 0x0B, 0x82, 0x03, 0x10, 0x5A, 0xA5, 0x01, 0x00, 0x04, 0x01, 0x00};
 int x4w = 0;


int x = 3;

//SoftwareSerial mySerial(2,3);
#define ERROR_UPTIME 0
#define INDATA_SIZE 82
#define PCDATA_SIZE 20

    char inData[INDATA_SIZE];
    int PCdata[PCDATA_SIZE];
    int pcdata_count = 0;  // Количество валидных значений в PCdata
    byte PLOTmem[6][16];
    byte blocks, halfs;
    byte lvalue = 0;
    String string_convert;
    unsigned long timeout = 0, uptime_timer = 0, plot_timer = 0;
    boolean lightState, reDraw_flag = 1, updateDisplay_flag, updateTemp_flag, timeOut_flag = 1;
    byte plotLines[] = {0, 1, 4, 5, 6, 7};
    
    void parsing() {
      while (Serial.available() > 0) {
        char aChar = Serial.read();
        if (aChar != 'E') {
          // Защита от переполнения буфера
          if (lvalue < INDATA_SIZE - 1) {
            inData[lvalue] = aChar;
            lvalue++;
            inData[lvalue] = '\0';
          } else {
            // Переполнение буфера - сброс
            SerialPrint("E", "DwinI", "inData buffer overflow, resetting");
            lvalue = 0;
            inData[0] = '\0';
          }
        } else {
          // Парсинг накопленной строки
          char inDataCopy[INDATA_SIZE];
          strncpy(inDataCopy, inData, INDATA_SIZE - 1);
          inDataCopy[INDATA_SIZE - 1] = '\0';
          
          char *saveptr = nullptr;
          char *str = strtok_r(inDataCopy, ";", &saveptr);
          pcdata_count = 0;
          
          while (str != NULL && pcdata_count < PCDATA_SIZE) {
            PCdata[pcdata_count] = atoi(str);
            pcdata_count++;
            str = strtok_r(nullptr, ";", &saveptr);
          }
          
          lvalue = 0;
          inData[0] = '\0';
          updateDisplay_flag = 1;
          updateTemp_flag = 1;
        }
        if (!timeOut_flag) {
          if (ERROR_UPTIME) uptime_timer = millis();
        }
        timeout = millis();
        timeOut_flag = 1;
      }
    }
    void updatePlot() {
      // Проверка валидности pcdata_count и индекса 17
      if (pcdata_count <= 17) return;
      
      uint32_t interval = (uint32_t)PCdata[17] * 1000;
      if ((millis() - plot_timer) > interval) {
        for (int i = 0; i < 6; i++) {
          for (int j = 0; j < 15; j++) {
            PLOTmem[i][j] = PLOTmem[i][j + 1];
          }
        }
        for (int i = 0; i < 6; i++) {
          byte idx = plotLines[i];
          if (idx < pcdata_count) {
            PLOTmem[i][15] = (byte)ceil((float)PCdata[idx] / 3.0f);
          }
        }
        plot_timer = millis();
      }
    }


class DwinI : public IoTUart {
   private:
    uint8_t _headerBuf[260];    // буфер для приема пакета dwin
    int _headerIndex = 0;       // счетчик принятых байт пакета



   public:
    DwinI(String parameters) : IoTUart(parameters) {


        
        
    }

    

    void uartHandle() {
        if (!_myUART) return;
        
        if (_myUART->available()) {
            _headerBuf[_headerIndex] = _myUART->read();

            // ищем валидный заголовок пакета dwin, проверяя каждый следующий байт
            if (_headerIndex == 0 && _headerBuf[_headerIndex] != 0x5A || 
                _headerIndex == 1 && _headerBuf[_headerIndex] != 0xA5 || 
                _headerIndex == 2 && _headerBuf[_headerIndex] == 0 || 
                _headerIndex == 3 && _headerBuf[_headerIndex] == 0x82 ) {
                _headerIndex = 0;
                return;
            }

            if (_headerIndex == _headerBuf[2] + 2) {    // получили все данные из пакета
                 Serial.print("ffffffff");
                  for (int i=0; i<=_headerIndex; i++)
                     Serial.printf("%#x ", _headerBuf[i]);
                 Serial.println("!!!");
                
                String valStr, id = "_";
                if (_headerIndex == 8) {    // предполагаем, что получили int16
                    valStr = (String)((_headerBuf[7] << 8) | _headerBuf[8]);
                }

                char buf[5];
                hex2string(_headerBuf + 4, 2, buf);
                id += (String)buf;

                IoTItem* item = findIoTItemByPartOfName(id);
                if (item) {
                    Serial.printf("received data: %s for VP: %s for ID: %s\n", valStr, buf, item->getID());
                    generateOrder(item->getID(), valStr);
                }
                
                _headerIndex = 0;
                return;
            }

            
                
            _headerIndex++;
        }
    }

    void onRegEvent(IoTItem* eventItem) {
        if (!_myUART || !eventItem) return; 
        int indexOf_;
        String printStr = "";
        
        printStr = eventItem->getID();
        indexOf_ = printStr.indexOf("_");
        uint8_t sizeOfVPPart = printStr.length() - indexOf_ - 1;
        if (indexOf_ == -1 || !_myUART || sizeOfVPPart < 4 || indexOf_ == 0)  return;  // пропускаем событие, если нет признака _ или признак пустой
        
        char typeOfVP = sizeOfVPPart == 5 ? printStr.charAt(indexOf_ + 5) : 0;
        String VP = printStr.substring(indexOf_ + 1, indexOf_ + 5);

        if (typeOfVP == 0) {    // если не указан тип, то додумываем на основании типа данных источника
            if (eventItem->value.isDecimal)
                typeOfVP = 'i';
                
            else
                typeOfVP = 's';
        }

        if (typeOfVP == 'i') {
            _myUART->write(0x5A);
            _myUART->write(0xA5);
            _myUART->write(0x05);   // размер данных отправляемых с учетом целых чисел int
            _myUART->write(0x82);   // требуем запись в память
            uartPrintHex(VP);       // отправляем адрес в памяти VP
            
            if (!eventItem->value.isDecimal) {
                eventItem->value.valD = atoi(eventItem->value.valS.c_str());
            }

            _myUART->write(highByte((int)eventItem->value.valD));
            _myUART->write(lowByte((int)eventItem->value.valD));
        }

        if (typeOfVP == 's') {
            if (eventItem->value.isDecimal) {
                eventItem->value.valS = eventItem->getValue();
            }

            // подсчитываем количество символов отличающихся от ASCII, для понимания сколько символов состоит из дух байт
            int u16counter = 0;
            const char* valSptr = eventItem->value.valS.c_str();
            for (int i=0; i < eventItem->value.valS.length(); i++) {
                if (valSptr[i] > 200) u16counter++;
            }

            _myUART->write(0x5A);
            _myUART->write(0xA5);
            _myUART->write((eventItem->value.valS.length() - u16counter) * 2 + 5);  // подсчитываем и отправляем размер итоговой строки + служебные байты
            _myUART->write(0x82);   // требуем запись в память
            uartPrintHex(VP);   // отправляем адрес в памяти VP
            uartPrintStrInUTF16(eventItem->value.valS.c_str(), eventItem->value.valS.length());     // отправляем строку для записи
            _myUART->write(0xFF);   // терминируем строку, чтоб экран очистил все остальное в элементе своем
            _myUART->write(0xFF);

            //Serial.printf("fffffffff %#x %#x %#x %#x \n", Data[0], Data[1], Data[2], Data[3]);
        }

        if (typeOfVP == 'f') {
            _myUART->write(0x5A);
            _myUART->write(0xA5);
            _myUART->write(0x07);   // размер данных отправляемых с учетом дробных чисел dword
            _myUART->write(0x82);   // требуем запись в память
            uartPrintHex(VP);       // отправляем адрес в памяти VP
            
            byte hex[4] = {0};
            if (!eventItem->value.isDecimal) {
                eventItem->value.valD = atof(eventItem->value.valS.c_str()); 
            }

            byte* f_byte = reinterpret_cast<byte*>(&(eventItem->value.valD));
            memcpy(hex, f_byte, 4);

            _myUART->write(hex[3]);
            _myUART->write(hex[2]);
            _myUART->write(hex[1]);
            _myUART->write(hex[0]);
        }

// ----------------- Переключение страниц -----------------------------

        if (typeOfVP == 'r') {
            _myUART->write(0x5A);
            _myUART->write(0xA5);
            _myUART->write(0x07);   // размер данных отправляемых с учетом целых чисел int
            _myUART->write(0x82);   // требуем запись в память
            _myUART->write(0x0084);;
            uartPrintHex(VP);       // отправляем адрес в памяти VP


          if (!eventItem->value.isDecimal) {
                eventItem->value.valD = atoi(eventItem->value.valS.c_str());
            }

            _myUART->write(highByte((int)eventItem->value.valD));
            _myUART->write(lowByte((int)eventItem->value.valD));
        }
        //--------------------------------------------------------------------
        
    }

        void loop() {

      parsing();
      
      updatePlot();
     x = PCdata[18];
      x5000w = map(analogRead(34), 0, 1023, 0, 255);// или 1023 ?
        _myUART->write(x5000m , 7);
        _myUART->write(x5000w);
       // x0w = map(analogRead(34), 0, 1000, 140, 223);
         x0w = map(analogRead(34), 0, 2000, 90, 300);
        _myUART->write(x0m , 13);
        _myUART->write(x0w);
      x = PCdata[18];
      x5001w = map(analogRead(35), 0, 1023, 0, 255);
        _myUART->write(x5001m , 7);
        _myUART->write(x5001w);
        x1w = map(analogRead(35), 0, 1000, 19, 100);
        _myUART->write(x1m , 13);
        _myUART->write(x1w);
      x = PCdata[18];
      x5002w = map(analogRead(36), 0, 1023, 0, 255);
        _myUART->write(x5002m , 7);
        _myUART->write(x5002w);
        x2w = map(analogRead(36), 0, 1000, 140, 223);
        _myUART->write(x2m , 13);
        _myUART->write(x2w);
      x = PCdata[18];
      x5003w = map(analogRead(39), 0, 1023, 0, 255);
        _myUART->write(x5003m , 7);
        _myUART->write(x5003w);
        x3w = map(analogRead(39), 0, 1000, 19, 100);
        _myUART->write(x3m , 13);
        _myUART->write(x3w);
      x = PCdata[18];
  

           IoTItem::loop();
        }
      

    IoTValue execute(String command, std::vector<IoTValue> &param) {  // будет возможным использовать, когда сценарии запустятся

        if (command == "scr0")
            _myUART->write(x0 , 10);

        else if (command == "scr1")
           _myUART->write(x1 , 10);

        else if (command == "scr2")
           _myUART->write(x2 , 10);

        else if (command == "scr3")
           _myUART->write(x3 , 10);

        else if (command == "scr4")
           _myUART->write(x4 , 10);

        else if (command == "scr5")
           _myUART->write(x5 , 10);

        else if (command == "scr6")
           _myUART->write(x6 , 10);

        else if (command == "scr7")
           _myUART->write(x7 , 10);

        else if (command == "scr8")
           _myUART->write(x8 , 10);

        else if (command == "scr9")
           _myUART->write(x9 , 10);


        doByInterval();
        return {};
    }


    void onModuleOrder(String &key, String &value) {
        if (key == "uploadUI") {
            //SerialPrint("i", F("DwinI"), "Устанавливаем UI: " + value);
            
        }
    }
    
    ~DwinI(){
        
    };
};

void *getAPI_DwinI(String subtype, String param) {
    if (subtype == F("DwinI")) {
        return new DwinI(param);
    } else {
        return nullptr;
    }
}

