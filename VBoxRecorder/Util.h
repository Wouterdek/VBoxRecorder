#pragma once

#include <string>
#include <sstream>
#include <vector>

#include "VirtualBox.h"

typedef unsigned int uint;

std::string &ltrim(std::string &s);
std::string &rtrim(std::string &s);
std::string &trim(std::string &s);

std::vector<std::string> split(const std::string &s, char delim, bool removeEmpty);
std::wstring strtowstr(const std::string& str);
std::string wstrtostr(const std::wstring& wstr);

std::wstring getMachineIDFromName(IVirtualBox* vbox, std::string targetNameStr);
std::wstring getMachineIDFromIndex(IVirtualBox* vbox, uint i);