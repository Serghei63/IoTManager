#pragma once
#include "lvgl.h"
#include <Arduino.h>

// объявления (должны совпадать с твоими .c файлами)
LV_IMG_DECLARE(img_01d);
LV_IMG_DECLARE(img_01n);
LV_IMG_DECLARE(img_02d);
LV_IMG_DECLARE(img_02n);
LV_IMG_DECLARE(img_03d);
LV_IMG_DECLARE(img_03n);
LV_IMG_DECLARE(img_04d);
LV_IMG_DECLARE(img_04n);
LV_IMG_DECLARE(img_09d);
LV_IMG_DECLARE(img_09n);
LV_IMG_DECLARE(img_10d);
LV_IMG_DECLARE(img_10n);
LV_IMG_DECLARE(img_11d);
LV_IMG_DECLARE(img_11n);
LV_IMG_DECLARE(img_13d);
LV_IMG_DECLARE(img_13n);
LV_IMG_DECLARE(img_50d);
LV_IMG_DECLARE(img_50n);

// функция
const lv_img_dsc_t* getWeatherIcon(const String& code);