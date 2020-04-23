#include "OrbitService.h"

#include <iostream>

#include "Capture.h"
#include "ConnectionManager.h"
#include "Core.h"
#include "CoreApp.h"
#include "TcpServer.h"
#include "TimerManager.h"

OrbitService::OrbitService() {
  // TODO: these should be a private fields.
  GTimerManager = std::make_unique<TimerManager>();
  GTcpServer = std::make_unique<TcpServer>();
  Capture::Init();

  GTcpServer->Start(Capture::GCapturePort);
  ConnectionManager::Get().InitAsService();

  core_app_ = std::make_unique<CoreApp>();
  GCoreApp = core_app_.get();
  GCoreApp->InitializeManagers();
}

void OrbitService::Run() {
  while (!exit_requested_) {
    GTcpServer->ProcessMainThreadCallbacks();
    Capture::Update();
    Sleep(16);
  }

  GCoreApp = nullptr;
}
