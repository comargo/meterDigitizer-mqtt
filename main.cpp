#include <system_error>

#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <wordexp.h>
#include <poll.h>
#include "main.h"

#include <vector>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <sstream>

#include <libconfig.h++>
#include <tinytemplate.hpp>

#include "helper.h"
#include "string_split_join.hpp"

#include <json/json.h>


Application::Application(int argc, char *argv[])
    : argc(argc)
    , argv(argv)
    , fdDevice(-1)
    , fdSignal(-1)
    , mqttClient(nullptr, &mosquitto_destroy)
    , curConnectionState(connection::off)
    , connectionError(0)
{
    mosquitto_lib_init();
}

Application::~Application()
{
    closeSignal();
    closeDevice();
    mqttClient.reset();
    mosquitto_lib_cleanup();
}

void Application::Run()
{
    bool reload = false;
    do {
        std::cout << "Loading configuration..." << std::endl;
        parseArguments();

        std::cout << "Connecting..." << std::endl;
        openDevice();
        openSignal();
        openMQTT();

        std::cout << "Start processing" << std::endl;

        reload = pollingLoop();

        std::cout << "Closing..." << std::endl;

        closeMQTT();
        closeSignal();
        closeDevice();
    } while(reload);
    std::cout << "Quit";
}

void Application::parseArguments()
{
    options = {
        {"device",""},
        {"device-topic","/home/meterDigitizer"},
        {"sensor-topic","{{sensorId}}"},
        {"host", "localhost"},
        {"keep-alive", "60"}
    };

    const std::vector<std::string> defaultConfigPaths = {"/etc/meterDigitizer-mqtt.conf", "~/.config/meterDigitizer-mqtt.conf", "~/.meterDigitizer-mqtt"};
    std::map<std::string, std::string> arguments;

    const struct option long_options[] = {
        {"device", required_argument, nullptr, 'd'},
        {"device-topic", required_argument, nullptr, 't'},
        {"sensor-topic", required_argument, nullptr, 's'},
        {"config", required_argument, nullptr, 'c'},
        {nullptr, 0, NULL, 0}
    };

    std::vector<std::string> configPaths = defaultConfigPaths;

    int option_index;
    int c;
    while((c = getopt_long(argc, argv, "d:t:s:c:", long_options, &option_index)) != -1) {
        switch (c) {
        case 'd':
            arguments["device"] = optarg;
            break;
        case 't':
            arguments["device-topic"] = optarg;
            break;
        case 's':
            arguments["sensor-topic"] = optarg;
            break;
        case 'c':
            configPaths.push_back(optarg);
            break;
        case '?':
            /* getopt_long already printed an error message. */
            break;
        default:
            throw std::runtime_error("arguments error");
        }
    }

    for(auto path : configPaths) {
        try {
            wordexp_t we = {0,nullptr,0};
            wordexp(path.c_str(), &we, WRDE_SHOWERR);
            for(size_t i = 0; i < we.we_wordc; ++i) {
                libconfig::Config cfg;
                cfg.readFile(we.we_wordv[i]);
                for(auto &option : options) {
                    cfg.lookupValue(option.first, option.second);
                }
            }
            wordfree(&we);
        }
        catch(const libconfig::FileIOException &ex) {
            if(std::none_of(defaultConfigPaths.begin(), defaultConfigPaths.end(), [&path](const std::string &defaultPath){return path == defaultPath;} )) {
                throw std::runtime_error(tinytemplate::render("Can't open config file \"{{path}}\"",{{"path",path}}));
            }
        }
        catch(const libconfig::ParseException &pex)
        {
            throw std::runtime_error(tinytemplate::render("{{file}}:{{line}}: Parse error: {{error}}",
                                                            {{"file", pex.getFile()},
                                                              {"line", std::to_string(pex.getLine())},
                                                              {"error", pex.getError()}}
                                                          ));
        }
    }

    for(auto args : arguments) {
        options[args.first] = args.second;
    }

    if(options["device"].empty()) {
        throw std::runtime_error("No device specified");
    }
    if(options["device-topic"].empty()) {
        throw std::runtime_error("No device topic specified");
    }
    if(options["sensor-topic"].empty()) {
        throw std::runtime_error("No sensor topic specified");
    }
}

void Application::openDevice()
{
    fdDevice = open(options["device"].c_str(), O_RDWR|O_NOCTTY);
    if(fdDevice == -1) {
        throw std::system_error(errno, std::system_category(), tinytemplate::render("Can't open device {{device}}", {{"device", options["device"]}}));
    }
}

void Application::openSignal()
{
    sigset_t sigset;
    sigemptyset(&sigset);
//    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGHUP);
    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGUSR1);
    sigprocmask(SIG_BLOCK, &sigset, nullptr);

    fdSignal = signalfd(-1, &sigset, 0);
    if(fdSignal == -1) {
        throw std::system_error(errno, std::system_category(), "Can't open signal file");
    }
}

void Application::openMQTT()
{
    mqttClient.reset(mosquitto_new(nullptr, true, this));
    mosquitto_reinitialise(mqttClient.get(), nullptr, true, this);

    mosquitto_connect_callback_set(mqttClient.get(), &Application::onMqttConnect);
    mosquitto_disconnect_callback_set(mqttClient.get(), &Application::onMqttDisconnect);
    mosquitto_publish_callback_set(mqttClient.get(), &Application::onMqttPublish);
    mosquitto_message_callback_set(mqttClient.get(), &Application::onMqttMessage);
    mosquitto_subscribe_callback_set(mqttClient.get(), &Application::onMqttSubscribe);
    mosquitto_unsubscribe_callback_set(mqttClient.get(), &Application::onMqttUnSubscribe);
    mosquitto_log_callback_set(mqttClient.get(), &Application::onMqttLog);



    if(!options["username"].empty()) {
        mosquitto_username_pw_set(mqttClient.get(), options["username"].c_str(), options["passwd"].c_str());
    }
    if(options["port"].empty()) {
        curConnectionState = connection::server;
        mosquitto_connect_srv(mqttClient.get(), options["host"].c_str(), std::stoi(options["keep-alive"]), nullptr);
    }
    else {
        curConnectionState = connection::host;
        mosquitto_connect_async(mqttClient.get(), options["host"].c_str(), std::stoi(options["port"]), std::stoi(options["keep-alive"]));
    }
    mosquitto_loop_start(mqttClient.get());


    {
        std::unique_lock<std::mutex> lock(mtxCurConnectionState);
        cvCurConnectionState.wait(lock, [this](){
            return curConnectionState == connection::connected || curConnectionState == connection::error || curConnectionState == connection::mqtt_error;});
        if(curConnectionState == connection::error || curConnectionState == connection::mqtt_error) {
            std::string message = tinytemplate::render("Error connecting to {{host}}:{{port}}: ", options);
            if(curConnectionState == connection::error) {
                message += "host unresolvable or connection failed";
            }
            else if(curConnectionState == connection::mqtt_error) {
                switch(connectionError) {
                case 1:
                    message += "connection refused (unacceptable protocol version)";
                    break;
                case 2:
                    message += "connection refused (identifier rejected)";
                    break;
                case 3:
                    message += "connection refused (broker unavailable)";
                    break;
                default:
                    message += "reserved error (" + std::to_string(connectionError) + ")";
                    break;
                }
            }
            throw std::runtime_error(message);
        }
    }

    mosquitto_subscribe(mqttClient.get(), nullptr, (options["device-topic"]+"/+/control").c_str(),0);
    mosquitto_subscribe(mqttClient.get(), nullptr, (options["device-topic"]+"/control").c_str(),0);
}

void Application::closeMQTT()
{
    mosquitto_disconnect(mqttClient.get());
    mosquitto_loop_stop(mqttClient.get(), false);
    mqttClient.reset();
}

void Application::closeDevice()
{
    if(fdDevice != -1) {
        close(fdDevice);
        fdDevice = -1;
    }
}

void Application::closeSignal()
{
    if(fdSignal != -1) {
        close(fdSignal);
        fdSignal = -1;
    }
}

bool Application::pollingLoop()
{
    std::array<struct pollfd, 2> fds = {{{fdDevice, POLLIN, 0}, {fdSignal, POLLIN, 0}}};
    std::string serialData;
    while(true) {
        int pollRet = poll(fds.data(), fds.size(), -1);
        if(pollRet < 0) {
            if(errno != EINTR) {
                throw std::system_error(errno, std::system_category(), "poll error: ");
            }
        }
        else if(pollRet == 0) {
            continue; // Timeout?!
        }
        else {
            for(auto fd : fds) {
                if(fd.revents & (POLLERR|POLLHUP|POLLNVAL)) {
                    if(fd.fd == fdDevice) {
                        throw std::runtime_error("Device file error");
                    }
                    else if(fd.fd == fdSignal) {
                        throw std::runtime_error("Signal file error");
                    }
                    else {
                        throw std::runtime_error("Unknown file error");
                    }
                }
                else if(fd.revents & POLLIN) {
                    if(fd.fd == fdSignal) {
                        switch(processSignal()) {
                        case signal_action::quit:
                            std::cout << "Quit signal detected" << std::endl;
                            return false;
                        case signal_action::reload:
                            std::cout << "Reload signal detected" << std::endl;
                            return true;
                        case signal_action::ignore:
                            break;
                        }
                    }
                    else if(fd.fd == fdDevice) {
                        char ch;
                        ssize_t ret = read(fdDevice, &ch, 1);
                        if(ret != 1) {
                            if(ch == '\n') {
                                if(!serialData.empty() && serialData.back() == '\r') {
                                    serialData.pop_back();
                                }
                                if(!serialData.empty()) {
                                    processSerialData(serialData);
                                    serialData.clear();
                                }
                            }
                            else {
                                serialData.push_back(ch);
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}

void Application::processSerialData(const std::string &data)
{
    if(data == "OK")
        return;
    if(data == "Error")
        return;
    auto splittedData = split(data, "\t");
    Json::Value jsonMsg(Json::objectValue);
    jsonMsg["timestamp"] = splittedData[0];
    jsonMsg["id"] = splittedData[1];
    jsonMsg["name"] = splittedData[2];
    jsonMsg["value"] = splittedData[3];
    std::map<std::string, std::string> renderVars = options;
    renderVars["sensorId"] = jsonMsg["id"].asString();
    renderVars["sensorName"] = jsonMsg["name"].asString();
    std::string mqttPayload = Json::FastWriter().write(jsonMsg);
    std::string mqttTopicName = tinytemplate::render("{{device-topic}}/{{sensor-topic}}/value",renderVars);
    mqttTopicName = tinytemplate::render(mqttTopicName, renderVars);
    mosquitto_publish(mqttClient.get(), nullptr,
                      mqttTopicName.c_str(),
                      mqttPayload.size(), mqttPayload.data(), 0, true);
}

Application::signal_action Application::processSignal()
{
    struct signalfd_siginfo info;
    if(read(fdSignal, &info, sizeof(info)) != sizeof(info)) {
        throw std::runtime_error("Error reading signal file");
    }
    switch(info.ssi_signo) {
    case SIGINT:
    case SIGTERM:
        return signal_action::quit;
    case SIGHUP:
        return signal_action::reload;
    default:
        return signal_action::ignore;
    }
}

void Application::onMqttConnect(int rc)
{
    std::lock_guard<std::mutex> lock(mtxCurConnectionState);
    curConnectionState = connection::connected;
    connectionError = rc;
    if(connectionError != 0) {
        mosquitto_disconnect(mqttClient.get());
    }
    cvCurConnectionState.notify_all();
}

void Application::onMqttDisconnect(int rc)
{
    std::lock_guard<std::mutex> lock(mtxCurConnectionState);
    switch(curConnectionState) {
    case connection::off: // What?!
    case connection::connected:
        if(rc == 0) {
            curConnectionState = connection::off;
            connectionError = 0;
        }
        else {
            curConnectionState = connection::error;
            connectionError = rc;
        }
        break;
    case connection::error:
    case connection::mqtt_error:
        break;
    case connection::server:
        // Server connection failed, lets try host
        curConnectionState = connection::host;
        mosquitto_connect_async(mqttClient.get(), options["host"].c_str(), 1883, std::stoi(options["keep-alive"]));
        break;
    case connection::host:
        curConnectionState = connection::error;
        connectionError = rc;
        break;
    }
    cvCurConnectionState.notify_all();
}

void Application::onMqttMessage(const mosquitto_message *message)
{
    bool match = false;
    mosquitto_topic_matches_sub((options["device-topic"]+"/control").c_str(), message->topic, &match);
    if(match) {
        if(!message->payload) {
            return;
        }
        std::istringstream dataStream(std::string((char*)message->payload, message->payloadlen));
        Json::Value payload;
        Json::parseFromStream(Json::CharReaderBuilder(), dataStream, &payload, nullptr);
        Json::Value jsonTime = payload.get("time", Json::Value());
        if(!jsonTime.isNull()) {
            std::string setTimeCmd = tinytemplate::render("SET TIME {{time}}\r",{{"time",jsonTime.asString()}});
            ssize_t ret = write(fdDevice, setTimeCmd.data(), setTimeCmd.size());
            if(ret != (ssize_t)setTimeCmd.size()) {
                std::cerr << "Error sending command \"" << setTimeCmd << "\"" << std::endl;
            }
        }
        Json::Value jsonList = payload.get("list", Json::Value());
        if(!jsonList.isNull()){
            std::string listCmd("LIST\r");
            ssize_t ret = write(fdDevice, listCmd.data(), listCmd.size());
            if(ret != (ssize_t)listCmd.size()) {
                std::cerr << "Error sending command \"" << listCmd << "\"" << std::endl;
            }
        }
        return;
    }

    mosquitto_topic_matches_sub((options["device-topic"]+"/+/control").c_str(), message->topic, &match);
    if(match) {
        if(!message->payload) {
            return;
        }
        char **topics;
        int topic_count;
        mosquitto_sub_topic_tokenise(message->topic, &topics, &topic_count);
        int deviceId = std::stoi(topics[topic_count-2]);
        mosquitto_sub_topic_tokens_free(&topics, topic_count);
        std::istringstream dataStream(std::string((char*)message->payload, message->payloadlen));
        Json::Value payload;
        Json::parseFromStream(Json::CharReaderBuilder(), dataStream, &payload, nullptr);
        Json::Value jsonVal = payload.get("value", Json::Value());
        if(!jsonVal.isNull() && jsonVal.isConvertibleTo(Json::realValue)) {
            char cmd[256];
            std::sprintf(cmd, "SET METER %d %.3f\r", deviceId, jsonVal.asFloat());
            ssize_t ret = write(fdDevice, cmd, strlen(cmd));
            if(ret != (ssize_t)strlen(cmd)) {
                std::cerr << "Error sending command \"" << cmd << "\"" << std::endl;
            }
        }
    }
}

void Application::onMqttLog(int level, const std::string &str)
{
}



void Application::onMqttConnect(mosquitto *mqtt, void *pParam, int rc)
{
    Application* pThis = static_cast<Application*>(pParam);
    pThis->onMqttConnect(rc);
}

void Application::onMqttDisconnect(mosquitto *mqtt, void *pParam, int rc)
{
    Application* pThis = static_cast<Application*>(pParam);
    pThis->onMqttDisconnect(rc);
}

void Application::onMqttPublish(mosquitto *mqtt, void *pParam, int mid)
{
    Application* pThis = static_cast<Application*>(pParam);
    pThis->onMqttPublish(mid);
}

void Application::onMqttMessage(mosquitto *mqtt, void *pParam, const mosquitto_message *message)
{
    Application* pThis = static_cast<Application*>(pParam);
    pThis->onMqttMessage(message);
}

void Application::onMqttSubscribe(mosquitto *mqtt, void *pParam, int mid, int qos_count, const int *granted_qos)
{
    Application* pThis = static_cast<Application*>(pParam);
    pThis->onMqttSubscribe(mid, std::vector<int>(granted_qos, granted_qos+qos_count));
}

void Application::onMqttUnSubscribe(mosquitto *mqtt, void *pParam, int mid)
{
    Application* pThis = static_cast<Application*>(pParam);
    pThis->onMqttUnSubscribe(mid);
}

void Application::onMqttLog(mosquitto *mqtt, void *pParam, int level, const char *str)
{
    Application* pThis = static_cast<Application*>(pParam);
    pThis->onMqttLog(level, str);
}

int main(int argc, char *argv[])
{
    try {
        Application a(argc, argv);
        a.Run();
    }
    catch(const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
