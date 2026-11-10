// ValueBinder.h
#pragma once

#include "classes/IoTItem.h"
#include "utils/JsonUtils.h"
#include "utils/SerialPrint.h"
#include <functional>
#include <math.h>

// Универсальный тип для поддержки int, float, String
struct Value {
    enum Type { INT, FLOAT, STRING } type;
    int valI;
    float valF;
    String valS;

    Value() : type(INT), valI(0), valF(0.0f), valS("") {}
    Value(int v) : type(INT), valI(v), valF((float)v), valS(String(v)) {}
    Value(long v) : type(INT), valI((int)v), valF((float)v), valS(String(v)) {}
    Value(float v) : type(FLOAT), valI((int)v), valF(v), valS(String(v, 2)) {}
    Value(const String& v) : type(STRING), valI(v.toInt()), valF(v.toFloat()), valS(v) {}
    Value(const char* v) : type(STRING), valI(String(v).toInt()), valF(String(v).toFloat()), valS(v) {}

    static bool isNumericString(const String& raw) {
        if (raw.isEmpty()) return false;

        bool hasDigit = false;
        bool hasPoint = false;
        int start = 0;

        if (raw[0] == '-' || raw[0] == '+') {
            start = 1;
        }

        for (int i = start; i < raw.length(); i++) {
            char c = raw[i];
            if (c >= '0' && c <= '9') {
                hasDigit = true;
                continue;
            }
            if (c == '.' || c == ',') {
                if (hasPoint) return false;
                hasPoint = true;
                continue;
            }
            return false;
        }

        return hasDigit;
    }

    static Value parse(const String& raw) {
        String normalized = raw;
        normalized.trim();
        normalized.replace(',', '.');

        if (!isNumericString(normalized)) {
            return Value(normalized);
        }

        if (normalized.indexOf('.') != -1) {
            return Value(normalized.toFloat());
        }

        return Value(normalized.toInt());
    }

    static Value fromIoTValue(const IoTValue& iotValue) {
        if (iotValue.isDecimal) {
            float roundedInt = roundf(iotValue.valD);
            if (fabsf(iotValue.valD - roundedInt) < 0.001f) {
                return Value((int)roundedInt);
            }
            return Value(iotValue.valD);
        }
        return Value(iotValue.valS);
    }

    String toString() const {
        switch (type) {
            case INT:    return String(valI);
            case FLOAT:  return String(valF, 2);
            case STRING: return valS;
            default:     return "";
        }
    }

    float toFloat() const {
        switch (type) {
            case INT:    return (float)valI;
            case FLOAT:  return valF;
            case STRING: return valS.toFloat();
            default:     return 0.0f;
        }
    }

    int toInt() const {
        switch (type) {
            case INT:    return valI;
            case FLOAT:  return (int)roundf(valF);
            case STRING: return valS.toInt();
            default:     return 0;
        }
    }

    bool operator!=(const Value& other) const {
        if (type != other.type) {
            // Сравниваем по числу, если обе стороны выглядят числовыми
            if ((type == INT || type == FLOAT) && (other.type == INT || other.type == FLOAT)) {
                return fabsf(toFloat() - other.toFloat()) > 0.01f;
            }
            if (isNumericString(toString()) && isNumericString(other.toString())) {
                return fabsf(toFloat() - other.toFloat()) > 0.01f;
            }
            return toString() != other.toString();
        }

        switch (type) {
            case INT:    return valI != other.valI;
            case FLOAT:  return fabsf(valF - other.valF) > 0.01f;
            case STRING: return valS != other.valS;
            default:     return false;
        }
    }
};

using OnLocalChange = std::function<void(const Value&)>;
using OnRemoteChange = std::function<void(const Value&, bool)>;
using OnSendOrder = std::function<void(const String&, const String&)>;
using OnSendRemoteValue = std::function<void(const String&, const String&)>;

// === ValueBinder — модуль взаимодействия _id и _id2show ===
// 1. _id == _id2show (или _id2show пустой) -> работаем только по _id
// 2. _id != _id2show и target найден локально -> связь внутри модуля через generateOrder()
// 3. _id != _id2show и target удаленный -> отправка через otpravkaidvalue(),
//    MQTT status -> тихое обновление UI, MQTT order/control -> обновление UI + событие _id
class ValueBinder {
public:
    enum class TargetMode {
        SELF_ID,
        LOCAL_ID2SHOW,
        REMOTE_ID2SHOW
    };

    enum class RemoteMessageType {
        NONE,
        STATUS,
        ORDER
    };

    String id;
    String targetId;
    bool isLocalTarget = false;
    IoTItem* targetItem = nullptr;
    IoTValue* trackingValue = nullptr;

    OnLocalChange onLocalChange;
    OnRemoteChange onRemoteChange;
    OnSendOrder onSendOrder;
    OnSendRemoteValue onSendRemoteValue;

    ValueBinder(String _id, String _id2show)
        : id(_id), targetId(_id2show) {
        refreshBinding();
    }

    bool hasTarget() const {
        return !targetId.isEmpty() && targetId != id;
    }

    bool isSelfTarget() const {
        return !hasTarget();
    }

    bool isRemoteTarget() const {
        return hasTarget() && !isLocalTarget;
    }

    TargetMode getMode() const {
        if (!hasTarget()) return TargetMode::SELF_ID;
        return isLocalTarget ? TargetMode::LOCAL_ID2SHOW : TargetMode::REMOTE_ID2SHOW;
    }

    bool refreshBinding() {
        targetItem = nullptr;
        trackingValue = nullptr;
        isLocalTarget = false;

        if (!hasTarget()) {
            return false;
        }

        IoTItem* foundItem = findIoTItem(targetId);
        if (foundItem && foundItem->iAmLocal) {
            targetItem = foundItem;
            trackingValue = &(targetItem->value);
            isLocalTarget = true;
            return true;
        }

        return false;
    }

    bool checkAndBind() {
        return refreshBinding();
    }

    void setValueLocally(int newValue) {
        handleOwnerValueChanged(Value(newValue));
    }

    void setValueLocally(float newValue) {
        handleOwnerValueChanged(Value(newValue));
    }

    void setValueLocally(const String& newValue) {
        handleOwnerValueChanged(Value::parse(newValue));
    }

    void setValueLocally(const Value& val) {
        handleOwnerValueChanged(val);
    }

    void setRemoteValue(const Value& newValue, bool genEvent = false) {
        if (onRemoteChange) {
            onRemoteChange(newValue, genEvent);
        }
    }

    void handleOwnerValueChanged(const Value& newValue) {
        if (onLocalChange) {
            onLocalChange(newValue);
        }

        if (!hasTarget()) {
            return;
        }

        if (refreshBinding()) {
            if (onSendOrder) {
                onSendOrder(targetId, newValue.toString());
            }
        } else {
            if (onSendRemoteValue) {
                onSendRemoteValue(targetId, newValue.toString());
            }
        }
    }

    bool handleTargetLocalEvent(IoTItem* eventItem, bool genEvent = false) {
        if (!eventItem || !hasTarget()) return false;
        if (eventItem->getID() != targetId) return false;

        SerialPrint("i", "ValueBinder", "Updating " + id + " from " + eventItem->getID());

        refreshBinding();
        if (!isLocalTarget) return false;

        setRemoteValue(Value::fromIoTValue(eventItem->value), genEvent);
        return true;
    }

    RemoteMessageType classifyMqttMessage(const String& topic) const {
        if (!hasTarget() || isLocalTarget) {
            return RemoteMessageType::NONE;
        }

        String topicId = extractTopicId(topic);
        if (topicId != targetId) {
            return RemoteMessageType::NONE;
        }

        if (topic.indexOf("/status") != -1) {
            return RemoteMessageType::STATUS;
        }

        if (topic.indexOf("/order") != -1 || topic.indexOf("/control") != -1) {
            return RemoteMessageType::ORDER;
        }

        return RemoteMessageType::NONE;
    }

    bool handleMqttMessage(const String& topic, const String& msg) {
        RemoteMessageType type = classifyMqttMessage(topic);
        if (type == RemoteMessageType::NONE) {
            return false;
        }

        Value val = Value::parse(extractMessageValue(msg));
        bool genEvent = (type == RemoteMessageType::ORDER);
        setRemoteValue(val, genEvent);
        return true;
    }

private:
    static String extractTopicId(const String& topic) {
        int partIndex = 0;
        int start = 0;

        while (start >= 0 && start < topic.length()) {
            int slashPos = topic.indexOf('/', start);
            String part = (slashPos == -1) ? topic.substring(start) : topic.substring(start, slashPos);

            if (partIndex == 3) {
                return part;
            }

            if (slashPos == -1) {
                break;
            }

            start = slashPos + 1;
            partIndex++;
        }

        return "";
    }

    static String extractMessageValue(const String& msg) {
        String value = msg;
        String parsed = "";

        if (jsonRead(msg, F("status"), parsed, false) || jsonRead(msg, F("Status"), parsed, false)) {
            return parsed;
        }

        if (jsonRead(msg, F("value"), parsed, false) || jsonRead(msg, F("val"), parsed, false)) {
            return parsed;
        }

        return value;
    }
};
