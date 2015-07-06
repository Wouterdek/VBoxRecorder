#pragma once
#include <Windows.h>
#include <string>

std::wstring GetProcessCommandLine(DWORD PID, bool& success);
LPVOID GetRemoteLibraryAddress(HANDLE procHandle, LPWSTR libraryName);

BOOL EnableDebugPrivileges(HANDLE* mainToken);

BOOL IsElevated();
bool ElevateWithArgs(LPCWSTR args);