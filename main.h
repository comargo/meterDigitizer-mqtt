#ifndef APPLICATION_H
#define APPLICATION_H

#include <map>
#include <memory>
#include <vector>
#include <condition_variable>

#include <mosquitto.h>

class Application
{
public:
    Application(int argc, char *argv[]);
    ~Application();

    void Run();

protected:
    enum class signal_action {
        ignore,
        reload,
        quit
    };

protected:
    void parseArguments();

    void openDevice();
    void openSignal();
    void openMQTT();

    void closeMQTT();
    void closeDevice();
    void closeSignal();

    bool pollingLoop();

    void processSerialData(const std::string &data);
    signal_action processSignal();
protected:
    void onMqttConnect(int rc);
    void onMqttDisconnect(int rc);
    void onMqttPublish(int mid) { }
    void onMqttMessage(const struct mosquitto_message *message);
    void onMqttSubscribe(int mid, const std::vector<int> &granted_qos) { }
    void onMqttUnSubscribe(int mid) { }
    void onMqttLog(int level, const std::string &str);
private:
    static void onMqttConnect(struct mosquitto *mqtt, void *pParam, int rc);
    static void onMqttDisconnect(struct mosquitto *mqtt, void *pParam, int rc);
    static void onMqttPublish(struct mosquitto *mqtt, void *pParam, int mid);
    static void onMqttMessage(struct mosquitto *mqtt, void *pParam, const struct mosquitto_message *message);
    static void onMqttSubscribe(struct mosquitto *mqtt, void *pParam, int mid, int qos_count, const int *granted_qos);
    static void onMqttUnSubscribe(struct mosquitto *mqtt, void *pParam, int mid);
    static void onMqttLog(struct mosquitto *mqtt, void *pParam, int level, const char *str);

private:
    int argc;
    char **argv;
    int fdDevice;
    int fdSignal;
    std::map<std::string, std::string> options;
    std::unique_ptr<mosquitto, decltype(&mosquitto_destroy)> mqttClient;

    enum class connection {
        off,
        server,
        host,
        connected,
        mqtt_error,
        error
    };

    connection curConnectionState;
    int connectionError;

    std::mutex mtxCurConnectionState;
    std::condition_variable cvCurConnectionState;
};


#endif//APPLICATION_H
