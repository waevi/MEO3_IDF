#ifndef MEO3_TYPE_H
#define MEO3_TYPE_H

#include <string>
#include <functional>
#include <map>
#include <vector>
#include <cstdint>

// Connection type mirrors org.thingai.meo.define.MConnectionType
enum class MeoConnectionType : int {
    LAN  = 0,
    UART = 1
};

// Device info – maps conceptually to MDevice fields
struct MeoDeviceInfo {
    std::string model;
    std::string manufacturer;
    MeoConnectionType connectionType;

    MeoDeviceInfo()
        : model(""), manufacturer(""), connectionType(MeoConnectionType::LAN) {}
};

// Simple key-value payload type for events/feature params
// Using std::string instead of Arduino String
using MeoEventPayload = std::map<std::string, std::string>;  

// Represent a feature invocation from the gateway
struct MeoFeatureCall {
    std::string deviceId;
    std::string featureName;
    MeoEventPayload params;   // raw string values
};

// Callback type for feature handlers
using MeoFeatureCallback = std::function<void(const MeoFeatureCall&)>;

// Registry of supported features
struct MeoFeatureRegistry {
    std::vector<std::string> eventNames;
    std::map<std::string, MeoFeatureCallback> methodHandlers;
};

// Logging hook
using MeoLogFunction = std::function<void(const char* level, const char* message)>;

#endif // MEO3_TYPE_H