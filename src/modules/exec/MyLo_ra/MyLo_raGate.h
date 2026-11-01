#pragma once
#include "Const.h"
//#define MYSENSORS

//#ifdef MYSENSORS

/*
 * DESCRIPTION
 * ВНИМАНИЕ ! Этот модуль работает ТОЛЬКО С ПОДКЛЮЧЕННЫМ РАДИО ! RFM95 (Ra-01)
 * The ESP32 gateway sends data received from sensors to the WiFi link.
 * The gateway also accepts input on ethernet interface, which is then sent out to the radio network.

 *  Работаем через LoRa (Ra-01 / RFM95)
 * ----------- PINOUT --------------
 * | IO   | RFM95 (Ra-01) |
 * |------|---------------|
 * | MOSI | 23            |
 * | MISO | 19            |
 * | SCK  | 18            |
 * | CSN  | 5   (NSS)     |
 * | RST  | 17            |
 * | IRQ  | 16  (DIO0)    |
*/

// Enable debug prints to serial monitor
#define MY_DEBUG

// Use a bit lower baudrate for serial prints on ESP8266 than default in MyConfig.h
#define MY_BAUD_RATE 115200

// Enables and select radio type (if attached)

#define MY_RADIO_RFM95

// --- НАСТРОЙКА ЧАСТОТЫ ДЛЯ Ra-01 ---
#define MY_RFM95_FREQUENCY (RFM95_434MHZ) // Базовая частота для Ra-01

// --- ЯВНОЕ УКАЗАНИЕ ПИНОВ ДЛЯ ESP32 ---
#define MY_RFM95_CS_PIN  5    // Пин NSS модуля Ra-01
#define MY_RFM95_RST_PIN 17   // Пин RST модуля Ra-01
#define MY_RFM95_IRQ_PIN 16   // Пин DIO0 модуля Ra-01

// Порог мощности (для тестов на столе можно оставить MAX или понизить)
#define MY_RFM95_TX_POWER (13)

// How many clients should be able to connect to this gateway (default 1)
#define MY_GATEWAY_MAX_CLIENTS 2


// используем гейт в режиме serial хотя нам этот режим не нужен, поэтому в библиотеки отключаем MY_SERIALDEVICE.print
// в файле MyGatewayTransportSerial.cpp в строчке 35
#define MY_GATEWAY_SERIAL

#include <MySensors.h>

extern String parseToString(const MyMessage& message);

//#endif