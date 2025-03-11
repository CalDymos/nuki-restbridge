#pragma once

#include <Preferences.h>
#include <WebServer.h>
#include "NukiWrapper.h"
#include "NukiNetworkLock.h"

extern TaskHandle_t networkTaskHandle;
extern TaskHandle_t nukiTaskHandle;

class WebCfgServer {
public:
  WebCfgServer(NukiWrapper* nuki, NukiNetwork* network, Preferences* preferences, bool allowRestartToPortal);
  ~WebCfgServer() = default;

  void initialize();
  void update();

private:
  NukiWrapper* _nuki = nullptr;
  NukiNetwork* _network = nullptr;
  Preferences* _preferences = nullptr;



  bool _allowRestartToPortal = false;
};