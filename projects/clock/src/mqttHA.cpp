#include <HaBridge.h>

#include "VM_IIC.h"
#include "config.h"
#include <ArduinoJson.h>
#include <MQTTRemote.h>

#include <entities/HaEntityText.h>
#include <entities/HaEntitySensor.h>
#include <entities/HaDeviceClasses.h>


#define DISCOVERY_PREFIX "homeassistant"

// MQTTClient mqtt(1024);

IJsonDocument _json_this_device_doc;

MQTTRemote mqttRemote(MY_HOSTNAME, BROKER_IP, 1883, MQTT_USER, MQTT_PASS);
HaBridge haBridge(mqttRemote, MY_HOSTNAME, _json_this_device_doc);

// Encoding is CP437, but can't set that in auto discovery right now, need to do it via customize in yaml
HaEntityText haText1(haBridge, "Line 1", "line1");  
HaEntityText haText2(haBridge, "Line 2", "line2");
homeassistantentities::Sensor::DataSize dataSize;
HaEntitySensor haHeap(haBridge, "Free Heap", "heap_free", {
    .device_class = dataSize, 
    .unit_of_measurement = homeassistantentities::Sensor::DataSize::B 
});

const int UPDATE_INTERVAL = 60 * 1000;   // ms
uint32_t lastValueUpdate = 0;

void setupJsonForThisDevice() {
  _json_this_device_doc["identifiers"] = MY_HOSTNAME;
  _json_this_device_doc["name"] = MY_HOSTNAME;
//   _json_this_device_doc["sw_version"] = "1.0.0";
//   _json_this_device_doc["model"] = "my_hardware";
//   _json_this_device_doc["manufacturer"] = "custom inc.";
}

String line[2] = {"", ""};

extern VM_IIC flipdot;  // hacky

void setLine(int lineNum, const char *text) {
    String txtStr = String(text);
    if (lineNum >= 0 && lineNum < 2) {
        line[lineNum] = txtStr;
    }

    String toSet = line[0];
    if (line[1] != NULL && line[1] != "" ) {
        toSet += "\n" + line[1];
    }

    flipdot.startScrollText(68, 6, toSet.c_str());
}

void updateValues() {
    haHeap.updateValue(ESP.getFreeHeap());
}

void mqttInit() {
    setupJsonForThisDevice();

    mqttRemote.start([](bool connected) {
        if (connected) {
            haText1.publishConfiguration();
            haText2.publishConfiguration();
            haHeap.publishConfiguration();
            haText1.setOnText([&](std::string txt) { setLine(0, txt.c_str()); });
            haText2.setOnText([&](std::string txt) { setLine(1, txt.c_str()); });
            updateValues();
        }
    });

    // Hackily set startup text
    haText1.updateText("Code for Heilbronn");
    setLine(0, "Code for Heilbronn");
    haText2.updateText("Maker Space experimenta");
    setLine(1, "Maker Space experimenta");
}

void mqttLoop() {
    if (!mqttRemote.connected()) {
        // TODO??
    }

    if (millis() - lastValueUpdate > UPDATE_INTERVAL) {
        lastValueUpdate = millis();

        updateValues();
    }
}