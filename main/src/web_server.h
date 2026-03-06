// web_server.h

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#ifdef USE_BOOST_WEB_INTERFACE
// Declare only the functions you want accessible from other files.
void StartWebServerThread();
#endif

#endif // WEB_SERVER_H
