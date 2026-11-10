#include "weather_icons.h"

const lv_img_dsc_t* getWeatherIcon(const String& code)
{
    if (code == "01d") return &img_01d;
    if (code == "01n") return &img_01n;
    if (code == "02d") return &img_02d;
    if (code == "02n") return &img_02n;
    if (code == "03d") return &img_03d;
    if (code == "03n") return &img_03n;
    if (code == "04d") return &img_04d;
    if (code == "04n") return &img_04n;
    if (code == "09d") return &img_09d;
    if (code == "09n") return &img_09n;
    if (code == "10d") return &img_10d;
    if (code == "10n") return &img_10n;
    if (code == "11d") return &img_11d;
    if (code == "11n") return &img_11n;
    if (code == "13d") return &img_13d;
    if (code == "13n") return &img_13n;
    if (code == "50d") return &img_50d;
    if (code == "50n") return &img_50n;

    return &img_01d; // fallback
}