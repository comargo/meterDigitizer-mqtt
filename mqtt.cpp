#include "mqtt.h"


MQTT::MQTT()
    : mosqpp::mosquittopp(nullptr)
{
}

void MQTT::on_connect(int rc)
{
    if(fnOnConnect)
        fnOnConnect(rc);
}

void MQTT::on_disconnect(int rc)
{
    if(fnOnDisconnect)
        fnOnDisconnect(rc);
}

void MQTT::on_publish(int mid)
{
    if(fnOnPublish)
        fnOnPublish(mid);
}

void MQTT::on_message(const mosquitto_message *message)
{
    if(fnOnMessage)
        fnOnMessage(message);
}

void MQTT::on_subscribe(int mid, int qos_count, const int *granted_qos)
{
    if(fnOnSubscribe)
        fnOnSubscribe(mid, qos_count, granted_qos);
}

void MQTT::on_unsubscribe(int mid)
{
    if(fnOnUnsubscribe)
        fnOnUnsubscribe(mid);
}

void MQTT::on_log(int level, const char *str)
{
    if(fnOnLog)
        fnOnLog(level, str);
}

void MQTT::on_error()
{
    if(fnOnError)
        fnOnError();
}
