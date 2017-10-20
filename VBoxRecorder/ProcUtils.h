#pragma once
#include <Windows.h>
#include <string>

std::wstring GetProcessCommandLine(const DWORD PID, bool& success);
LPVOID GetRemoteLibraryAddress(const HANDLE procHandle, const LPWSTR libraryName);

BOOL EnableDebugPrivileges(HANDLE* mainToken);

BOOL IsElevated();
bool ElevateWithArgs(const std::wstring& args);