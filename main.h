#ifndef APPLICATION_H
#define APPLICATION_H

#include <map>
#include <memory>

class MQTT;

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
    void closeDevice();
    void closeSignal();
    bool pollingLoop();

    void processSerialCommand(const std::string &data);
    signal_action processSignal();

private:
    int argc;
    char **argv;
    int fdDevice;
    int fdSignal;
    std::map<std::string, std::string> options;
    std::unique_ptr<MQTT> mqttClient;
};


#endif//APPLICATION_H
