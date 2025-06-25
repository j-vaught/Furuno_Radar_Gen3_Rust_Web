#ifndef ECHOSERVER_H
#define ECHOSERVER_H

#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>

class EchoServer {
public:
    EchoServer(unsigned short port);
    ~EchoServer();

    void start();
    void stop();

    void setImageGetter(std::function<std::string()> getter);
    void setCommandHandler(std::function<void(const std::string&, const std::string&)> handler);
    void setStatusGetter(std::function<std::pair<bool, bool>()> getter); // Added for button states

private:
    void run();

    unsigned short port_;
    std::atomic<bool> running_;
    std::thread serverThread_;
    std::function<std::string()> getImage_;
    std::function<void(const std::string&, const std::string&)> handleCommand_;
    std::function<std::pair<bool, bool>()> getStatus_; // Added for button states
    std::mutex dataMutex_;
};

#endif // ECHOSERVER_H
