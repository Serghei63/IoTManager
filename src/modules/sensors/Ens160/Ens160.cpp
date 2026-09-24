/*
#include "Global.h"
#include "classes/IoTItem.h"

#include "Wire.h"

#include "SparkFun_ENS160.h"

SparkFun_ENS160 myENS; 

int ensStatus; 

class Ens160tvoc : public IoTItem {
   public:
   
   
    Ens160tvoc(String parameters) : IoTItem(parameters) {

        if( !myENS.begin() )
	{
		Serial.println("Could not communicate with the ENS160, check wiring.");
	}

      Serial.println("Example 1 Basic Example.");

	// Reset the indoor air quality sensor's settings.
	if( myENS.setOperatingMode(SFE_ENS160_RESET) )
		Serial.println("Ready.");

	delay(100);

	// Device needs to be set to idle to apply any settings.
	// myENS.setOperatingMode(SFE_ENS160_IDLE);

	// Set to standard operation
	// Others include SFE_ENS160_DEEP_SLEEP and SFE_ENS160_IDLE
	myENS.setOperatingMode(SFE_ENS160_STANDARD);

	// There are four values here: 
	// 0 - Operating ok: Standard Operation
	// 1 - Warm-up: occurs for 3 minutes after power-on.
	// 2 - Initial Start-up: Occurs for the first hour of operation.
    //		                 and only once in sensor's lifetime.
	// 3 - No Valid Output
	ensStatus = myENS.getFlags();
	Serial.print("Gas Sensor Status Flag (0 - Standard, 1 - Warm up, 2 - Initial Start Up): ");
	Serial.println(ensStatus);

    }

    void doByInterval() {

        	if( myENS.checkDataStatus() )
	  {
		Serial.print("Air Quality Index (1-5) : ");
		Serial.println(myENS.getAQI());

		Serial.print("Total Volatile Organic Compounds: ");
		Serial.print(myENS.getTVOC());
		Serial.println("ppb");

		Serial.print("CO2 concentration: ");
		Serial.print(myENS.getECO2());
		Serial.println("ppm");

	    Serial.print("Gas Sensor Status Flag (0 - Standard, 1 - Warm up, 2 - Initial Start Up): ");
        Serial.println(myENS.getFlags());

		Serial.println();

         
        value.valD = myENS.getTVOC();
        if (value.valD > -1)
            regEvent(value.valD, "Ens160tvoc");
        else
            SerialPrint("E", "Sensor Ens160tvoc", "Error", _id);
    
      }
 }
    ~Ens160tvoc(){};
};

class Ens160eco2 : public IoTItem {
   public:
    Ens160eco2(String parameters) : IoTItem(parameters) {

                	if( !myENS.begin() )
	{
		Serial.println("Could not communicate with the ENS160, check wiring.");
		while(1);
	}

  Serial.println("Example 1 Basic Example.");

	// Reset the indoor air quality sensor's settings.
	if( myENS.setOperatingMode(SFE_ENS160_RESET) )
		Serial.println("Ready.");

	delay(100);

	// Device needs to be set to idle to apply any settings.
	// myENS.setOperatingMode(SFE_ENS160_IDLE);

	// Set to standard operation
	// Others include SFE_ENS160_DEEP_SLEEP and SFE_ENS160_IDLE
	myENS.setOperatingMode(SFE_ENS160_STANDARD);

	// There are four values here: 
	// 0 - Operating ok: Standard Operation
	// 1 - Warm-up: occurs for 3 minutes after power-on.
	// 2 - Initial Start-up: Occurs for the first hour of operation.
    //		                 and only once in sensor's lifetime.
	// 3 - No Valid Output
	ensStatus = myENS.getFlags();
	Serial.print("Gas Sensor Status Flag (0 - Standard, 1 - Warm up, 2 - Initial Start Up): ");
	Serial.println(ensStatus);
    }

    void doByInterval() {

	{

		Serial.print("CO2 concentration: ");
		Serial.print(myENS.getECO2());
		Serial.println("ppm");

	Serial.print("Gas Sensor Status Flag (0 - Standard, 1 - Warm up, 2 - Initial Start Up): ");
    Serial.println(myENS.getFlags());

		Serial.println();

        value.valD = myENS.getECO2();
        if (value.valD > -1)
            regEvent(value.valD, "Ens160eco2");
        else
            SerialPrint("E", "Sensor Ens160eco2", "Error", _id);
      }
    }
    ~Ens160eco2(){};
};

class Ens160aqi : public IoTItem {
   public:
   
    Ens160aqi(String parameters) : IoTItem(parameters) {
					if( !myENS.begin() )
	{
		Serial.println("Could not communicate with the ENS160, check wiring.");
		while(1);
	}

  Serial.println("Example 1 Basic Example.");

	// Reset the indoor air quality sensor's settings.
	if( myENS.setOperatingMode(SFE_ENS160_RESET) )
		Serial.println("Ready.");

	delay(100);

	// Device needs to be set to idle to apply any settings.
	// myENS.setOperatingMode(SFE_ENS160_IDLE);

	// Set to standard operation
	// Others include SFE_ENS160_DEEP_SLEEP and SFE_ENS160_IDLE
	myENS.setOperatingMode(SFE_ENS160_STANDARD);

	// There are four values here: 
	// 0 - Operating ok: Standard Operation
	// 1 - Warm-up: occurs for 3 minutes after power-on.
	// 2 - Initial Start-up: Occurs for the first hour of operation.
    //		                 and only once in sensor's lifetime.
	// 3 - No Valid Output
	ensStatus = myENS.getFlags();
	Serial.print("Gas Sensor Status Flag (0 - Standard, 1 - Warm up, 2 - Initial Start Up): ");
	Serial.println(ensStatus);
    }
	 void doByInterval() {

		
		Serial.print("Air Quality Index (1-5) : ");
		Serial.println(myENS.getAQI());


	Serial.print("Gas Sensor Status Flag (0 - Standard, 1 - Warm up, 2 - Initial Start Up): ");
    Serial.println(myENS.getFlags());

		Serial.println();

        value.valD = myENS.getAQI();
        if (value.valD > -1)
            regEvent(value.valD, "Ens160aqi");
        else
            SerialPrint("E", "Sensor Ens160aqi", "Error", _id);
	 }

   ~Ens160aqi(){};
};

void* getAPI_Ens160(String subtype, String param) {

       if (subtype == F("Ens160tvoc")) {
           return new Ens160tvoc(param);
       } else if (subtype == F("Ens160eco2")) {
           return new Ens160eco2(param);
	   } else if (subtype == F("Ens160aqi")) {
		   return new Ens160aqi(param);
       } else {
    return nullptr;
    }
}
*/
#include "Global.h"
#include "classes/IoTItem.h"
#include "Wire.h"
#include "SparkFun_ENS160.h"

// Глобальный объект датчика и флаг успешной инициализации
SparkFun_ENS160 myENS; 
static bool ensInitialized = false;

// Вспомогательная функция безопасной инициализации
bool initENS160Once() {
    if (ensInitialized) return true;

    if (!myENS.begin()) {
        SerialPrint("E", F("ENS160"), F("Could not communicate with ENS160! Check wiring."));
        return false;
    }

    myENS.setOperatingMode(SFE_ENS160_RESET);
    myENS.setOperatingMode(SFE_ENS160_STANDARD);
    ensInitialized = true;
    SerialPrint("I", F("ENS160"), F("Sensor initialized successfully."));
    return true;
}

// ------------------- TVOC -------------------
class Ens160tvoc : public IoTItem {
public:
    Ens160tvoc(String parameters) : IoTItem(parameters) {
        initENS160Once();
    }

    void doByInterval() override {
        if (!ensInitialized && !initENS160Once()) return;

        if (myENS.checkDataStatus()) {
            value.valD = myENS.getTVOC();
            if (value.valD >= 0) {
                regEvent(value.valD, "Ens160tvoc");
            } else {
                SerialPrint("E", F("Ens160tvoc"), F("Read error"), _id);
            }
        }
    }
    ~Ens160tvoc() {}
};

// ------------------- eCO2 -------------------
class Ens160eco2 : public IoTItem {
public:
    Ens160eco2(String parameters) : IoTItem(parameters) {
        initENS160Once();
    }

    void doByInterval() override {
        if (!ensInitialized && !initENS160Once()) return;

        if (myENS.checkDataStatus()) {
            value.valD = myENS.getECO2();
            if (value.valD >= 0) {
                regEvent(value.valD, "Ens160eco2");
            } else {
                SerialPrint("E", F("Ens160eco2"), F("Read error"), _id);
            }
        }
    }
    ~Ens160eco2() {}
};

// ------------------- AQI -------------------
class Ens160aqi : public IoTItem {
public:
    Ens160aqi(String parameters) : IoTItem(parameters) {
        initENS160Once();
    }

    void doByInterval() override {
        if (!ensInitialized && !initENS160Once()) return;

        if (myENS.checkDataStatus()) {
            value.valD = myENS.getAQI();
            if (value.valD >= 0) {
                regEvent(value.valD, "Ens160aqi");
            } else {
                SerialPrint("E", F("Ens160aqi"), F("Read error"), _id);
            }
        }
    }
    ~Ens160aqi() {}
};

void* getAPI_Ens160(String subtype, String param) {
    if (subtype == F("Ens160tvoc")) {
        return new Ens160tvoc(param);
    } else if (subtype == F("Ens160eco2")) {
        return new Ens160eco2(param);
    } else if (subtype == F("Ens160aqi")) {
        return new Ens160aqi(param);
    }
    return nullptr;
}
