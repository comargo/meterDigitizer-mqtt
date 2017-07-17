#ifndef MQTT_H
#define MQTT_H

#include <mosquittopp.h>
#include <functional>

class MQTT : public mosqpp::mosquittopp
{
public:
    MQTT();

    void setOnConnect(std::function<void(int)> fn) {fnOnConnect = fn;}
    void setOnDisconnect(std::function<void(int)> fn) {fnOnDisconnect = fn;}
    void setOnPublish(std::function<void(int)> fn) {fnOnPublish = fn;}
    void setOnMessage(std::function<void(const struct mosquitto_message *message)> fn) {fnOnMessage = fn;}
    void setOnSubscribe(std::function<void(int mid, int qos_count, const int *granted_qos)> fn) {fnOnSubscribe = fn;}
    void setOnUnsubscribe(std::function<void(int)> fn) {fnOnUnsubscribe = fn;}
    void setOnLog(std::function<void(int level, const char *str)> fn) {fnOnLog  = fn;}
    void setOnError(std::function<void()> fn) {fnOnError = fn;}

protected:
    void on_connect(int rc) override;
    void on_disconnect(int rc) override;
    void on_publish(int mid) override;
    void on_message(const struct mosquitto_message *message) override;
    void on_subscribe(int mid, int qos_count, const int *granted_qos) override;
    void on_unsubscribe(int mid) override;
    void on_log(int level, const char *str) override;
    void on_error() override;

private:
    std::function<void(int)> fnOnConnect;
    std::function<void(int)> fnOnDisconnect;
    std::function<void(int)> fnOnPublish;
    std::function<void(const struct mosquitto_message *message)> fnOnMessage;
    std::function<void(int mid, int qos_count, const int *granted_qos)> fnOnSubscribe;
    std::function<void(int)> fnOnUnsubscribe;
    std::function<void(int level, const char *str)> fnOnLog;
    std::function<void()> fnOnError;

};


#endif//MQTT_H
