// Panel_img_icon.cpp

#include "Panel_img_icon.h"
#include "classes/IoTItem.h"

Panel_img_icon::Panel_img_icon(String parameters) : IoTItem(parameters)
{
    iconCode = "10d";

    jsonRead(parameters, "id2show", _id2show);
    jsonRead(parameters, "x", x);
    jsonRead(parameters, "y", y);
    jsonRead(parameters, "Nomer", nomer);
    jsonRead(parameters, "NAME", name);

   // ValueBinder инициализация для синхронизации с _id2show
    if (!_id2show.isEmpty()) {
        binder = new ValueBinder(_id, _id2show);

        // Обработчик отправки команды
        binder->onSendOrder = [this](const String& key, const String& value) {
            generateOrder(key, value);
        };

        // Обработчик отправки значения на удаленный модуль
        binder->onSendRemoteValue = [this](const String& key, const String& value) {
            if (mqttIsConnect() == 1) {
                String json = "{}";
                jsonWriteStr_(json, "id", key);
                jsonWriteStr_(json, "val", value);
                jsonWriteInt_(json, "int", 0);
                publishEvent(key, json);
            }
        };

        // Обработчик изменения значения из другого модуля
        binder->onRemoteChange = [this](const Value& val, bool genEvent) {

            IoTValue iotVal;
            iotVal.isDecimal = (val.type == Value::FLOAT || val.type == Value::INT);
            iotVal.valD = (val.type == Value::INT) ? val.valI :
                          (val.type == Value::FLOAT) ? val.valF :
                          val.valS.toFloat();
            iotVal.valS = val.toString();

            this->_ignoreNextSync = true;
            this->setValue(iotVal, genEvent);
        };

        // Обработчик локального изменения
        binder->onLocalChange = [this](const Value& val) {
            IoTValue iotVal;
            iotVal.isDecimal = (val.type == Value::FLOAT || val.type == Value::INT);
            iotVal.valD = (val.type == Value::INT) ? val.valI :
                          (val.type == Value::FLOAT) ? val.valF :
                          val.valS.toFloat();
            iotVal.valS = val.toString();

            this->_ignoreNextSync = true;
            this->setValue(iotVal, false);
        };
    }

    createObj();
    update();
}

Panel_img_icon::~Panel_img_icon()
{
    if (binder) {
        delete binder;
        binder = nullptr;
    }

    if (obj) {
        lv_obj_del(obj);
        obj = nullptr;
    }
}

void Panel_img_icon::createObj()
{
    if (obj) return;

     if (name == "TabPage" && tab2 >= nomer) {
        obj =lv_image_create(ui_TabPage[nomer - 1]);
    } else if (name == "Button" && isf1 >= nomer) {
        obj = lv_image_create(ui_Button[nomer - 1]);
    } else if (name == "Container" && cont >= nomer) {
        obj = lv_image_create(ui_Container[nomer - 1]);
    } else {
        obj = lv_image_create(ui_Screen1);
    }

    //obj = lv_image_create(lv_scr_act());

    lv_obj_set_size(obj, 128, 128);
    lv_obj_align(obj, LV_ALIGN_DEFAULT, x, y);

    lv_image_set_src(obj, getWeatherIcon(iconCode));
}

void Panel_img_icon::update()
{
    if (!obj) return;

    String newCode = value.valS;

    if (newCode != iconCode)
    {
        iconCode = newCode;

        const lv_img_dsc_t* icon = getWeatherIcon(iconCode);

        if (icon) {
            lv_image_set_src(obj, icon);
        }
    }
}

String Panel_img_icon::formatValue() const {
    if (!value.valS.isEmpty()) {
        if (isNumeric(value.valS)) {
            float num = value.valS.toFloat();
            char buf[32];
            dtostrf(num, 1, _round, buf);
            return String(buf);
        }
        return value.valS;
    }
    char buf[32];
    dtostrf(value.valD, 1, _round, buf);
    return String(buf);
}

void Panel_img_icon::setValue(const IoTValue& Value, bool genEvent) {
    if (_ignoreNextSync) {
        value = Value;
        String formatted = formatValue();
        _lastSentValue = formatted;
        update();
        _ignoreNextSync = false;
        if (genEvent) {
            regEvent(formatted, F("Panel_label"), false, true);
        }
        return;
    }

    value = Value;
    update();
    if (genEvent) {
        regEvent(formatValue(), F("Panel_label"), false, true);
    }
}

String Panel_img_icon::getValue()
{
    return (String)value.valS;
}

void Panel_img_icon::onRegEvent(IoTItem* eventItem)
{
    if (binder && !binder->isLocalTarget &&
        eventItem &&
        eventItem->getID() == binder->targetId)
    {
        binder->checkAndBind();
    }

    if (binder) {
        binder->handleTargetLocalEvent(eventItem, true);
    }

    IoTItem::onRegEvent(eventItem);
}

void Panel_img_icon::onModuleOrder(String& key, String& value)
{
    if (binder)
    {
        if (key == binder->id) {
            binder->setValueLocally(value);
        }
        else if (key == binder->targetId) {
            binder->setRemoteValue(Value::parse(value), true);
        }
    }

    IoTItem::onModuleOrder(key, value);
}

void Panel_img_icon::onMqttRecive(String& topic, String& msg)
{
    if (binder && binder->handleMqttMessage(topic, msg))
    {
        String valueStr = msg;
        jsonRead(msg, F("status"), valueStr, false);
    }

    IoTItem::onMqttRecive(topic, msg);
}