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

#include <libconfig.h++>
#include <tinytemplate.hpp>

#include "helper.h"


Application::Application(int argc, char *argv[])
    : argc(argc)
    , argv(argv)
    , fdDevice(-1)
    , fdSignal(-1)
    , mqttClient(nullptr, &mosquitto_destroy)
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
        parseArguments();
        openDevice();
        openSignal();
        mosquitto_reinitialise(mqttClient.get(), nullptr, true, this);
        if(!options["username"].empty()) {
            mosquitto_username_pw_set(mqttClient.get(), options["username"].c_str(), options["passwd"].c_str());
        }
        mosquitto_loop_start(mqttClient.get());

        int mqttKeepAlive = 60;
        if(!options["keep-alive"].empty()) {
            mqttKeepAlive = std::stoi(options["keep-alive"]);
        }
        if(options["port"].empty()) {
            mosquitto_connect_srv(mqttClient.get(), options["host"].c_str(), mqttKeepAlive, nullptr);
        }
        else {
            mosquitto_connect_async(mqttClient.get(), options["host"].c_str(), std::stoi(options["port"]), mqttKeepAlive);
        }
        int mid;
        mosquitto_subscribe(mqttClient.get(), &mid, (options["device-topic"]+"/+/control").c_str(),0);

        reload = pollingLoop();

        mosquitto_disconnect(mqttClient.get());
        mosquitto_loop_stop(mqttClient.get(), false);

        closeSignal();
        closeDevice();
    } while(!reload);
}

void Application::parseArguments()
{
    options = {
        {"device",""},
        {"device-topic","/home/meterDigitizer"},
        {"sensor-topic","${sensorId"}
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
                throw std::runtime_error(tinytemplate::render("Can't open config file \"{path}\"",{{"path",path}}));
            }
        }
        catch(const libconfig::ParseException &pex)
        {
            throw std::runtime_error(tinytemplate::render("{file}:{line}: Parse error: {error}",
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
        throw std::system_error(errno, std::system_category(), tinytemplate::render("Can't open device {device}", {{"device", options["device"]}}));
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
                            return false;
                        case signal_action::reload:
                            return true;
                        case signal_action::ignore:
                            break;
                        }
                    }
                    else if(fd.fd == fdDevice) {
                        char ch;
                        read(fdDevice, &ch, 1);
                        if(ch == '\n') {
                            if(!serialData.empty() && serialData.back() == '\r') {
                                serialData.pop_back();
                            }
                            if(!serialData.empty()) {
                                processSerialCommand(serialData);
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
    return false;
}

void Application::processSerialCommand(const std::string &data)
{
    std::cout << hexDump(data.data(), data.length());
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
