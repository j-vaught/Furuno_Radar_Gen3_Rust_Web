#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <mutex>
#include <wx/wx.h>
#include <wx/event.h>

class WebServer {
public:
    WebServer(unsigned short port, wxEvtHandler* evtHandler);
    ~WebServer();

    void start();
    void stop();

    // Callback to get current output from the GUI
    void setOutputGetter(std::function<std::string()> getter);

private:
    void run();

    unsigned short port_;
    std::thread serverThread_;
    std::atomic<bool> running_;

    // Callback functions
    std::function<std::string()> getOutput_;

    std::mutex outputMutex_;

    wxEvtHandler* evtHandler_;
};

#endif // WEBSERVER_H
