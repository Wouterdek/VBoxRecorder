#pragma once

#include <SDKDDKVer.h>
#include <Windows.h>
#include <Psapi.h>
#include <stdio.h>
#include <tchar.h>

#include <iostream>
#include <vector>

#include "VirtualBox.h"

#include "Util.h"
#include "ProcUtils.h"
#include "ConsoleUtils.h"
#include "VideoEncoder.h"

bool recordVideo();
bool findProcess();

void startRecording(IVirtualBox* vbox, ISession* session);
void selectMachine(IVirtualBox* vbox, const std::vector<std::string>& args);
void printMachineNames(IVirtualBox* vbox);
void printHelp();
void cmdLoop(IVirtualBoxClient* client);