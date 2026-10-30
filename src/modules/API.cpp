#include "ESPConfiguration.h"

void* getAPI_Cron(String subtype, String params);
void* getAPI_Loging(String subtype, String params);
void* getAPI_LogingDaily(String subtype, String params);
void* getAPI_LogingHourly(String subtype, String params);
void* getAPI_IoTMath(String subtype, String params);
void* getAPI_Timer(String subtype, String params);
void* getAPI_UpdateServer(String subtype, String params);
void* getAPI_Variable(String subtype, String params);
void* getAPI_VButton(String subtype, String params);
void* getAPI_Impulse(String subtype, String params);
void* getAPI_ButtonIn(String subtype, String params);
void* getAPI_ButtonOut(String subtype, String params);
void* getAPI_Encoder(String subtype, String params);
void* getAPI_Multitouch(String subtype, String params);
void* getAPI_SDcard(String subtype, String params);
void* getAPI_TelegramLT(String subtype, String params);

void* getAPI(String subtype, String params) {
void* tmpAPI; void* foundAPI = nullptr;
if ((tmpAPI = getAPI_Cron(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_Loging(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_LogingDaily(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_LogingHourly(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_IoTMath(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_Timer(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_UpdateServer(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_Variable(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_VButton(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_Impulse(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_ButtonIn(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_ButtonOut(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_Encoder(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_Multitouch(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_SDcard(subtype, params)) != nullptr) foundAPI = tmpAPI;
if ((tmpAPI = getAPI_TelegramLT(subtype, params)) != nullptr) foundAPI = tmpAPI;
return foundAPI;
}