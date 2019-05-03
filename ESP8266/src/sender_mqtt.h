#ifndef _SENDERMQTT_h
#define _SENDERMQTT_h

#ifdef SEND_MQTT

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "master_i2c.h"
#include "Logging.h"


bool send_mqtt(const Settings &sett, const SlaveData &data, const float &channel0, const float &channel1)
{
    if (strnlen(sett.mqtt_host, MQTT_HOST_LEN) == 0) {
        LOG_NOTICE( "MQT", "No host. SKIP");
        return false;
    }

    WiFiClient wclient;   
    PubSubClient client(wclient);
    client.setServer(sett.mqtt_host, sett.mqtt_port);

    String clientId = "waterius-" + String(ESP.getChipId());

    const char *login = strnlen(sett.mqtt_login, MQTT_LOGIN_LEN) ? sett.mqtt_login : NULL;
    const char *pass = strnlen(sett.mqtt_password, MQTT_PASSWORD_LEN) ? sett.mqtt_password : NULL;
    
    String topic(sett.mqtt_topic);
    if (!topic.endsWith("/"))
        topic += '/';


    unsigned int delta0 = (data.impulses0 - sett.impulses0_previous)*sett.liters_per_impuls; // litres
    unsigned int delta1 = (data.impulses1 - sett.impulses1_previous)*sett.liters_per_impuls; 
    
    if (client.connect(clientId.c_str(), login, pass)) {
        client.publish((topic + "ch0").c_str(), String(channel0).c_str());
        client.publish((topic + "ch1").c_str(), String(channel1).c_str());
        client.publish((topic + "delta0").c_str(), String(delta0).c_str());
        client.publish((topic + "delta1").c_str(), String(delta1).c_str());
        client.publish((topic + "voltage").c_str(), String((float)(data.voltage / 1000.0)).c_str());
        client.publish((topic + "resets").c_str(), String(data.resets).c_str());
        client.publish((topic + "good").c_str(), String(data.diagnostic).c_str());
        client.publish((topic + "boot").c_str(), String(data.version).c_str());
        client.publish((topic + "imp0").c_str(), String(data.impulses0).c_str());
        client.publish((topic + "imp1").c_str(), String(data.impulses1).c_str());
        client.publish((topic + "version").c_str(), String(sett.version).c_str());
        client.publish((topic + "version_esp").c_str(), String(FIRMWARE_VERSION).c_str());

        client.disconnect();
        return true;
    }  else {
        LOG_ERROR("MQT", "connect error");
    } 

    return false;
}        

#endif //#ifdef SEND_MQTT

#endif
