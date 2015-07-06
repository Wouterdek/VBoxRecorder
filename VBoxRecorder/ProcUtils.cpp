#include "ProcUtils.h"

#include <windows.h>
#include <Psapi.h>
#include <stdio.h>
#include <conio.h>
#include <tchar.h>
#include <iostream>
#include <strsafe.h>
#include <wctype.h>
#include <string>
#include <algorithm>

#include <Wbemidl.h> //Required for GetProcessCommandLine

#pragma comment(lib, "Wbemuuid.lib")

std::wstring GetProcessCommandLine(DWORD PID, bool& success) {
	//Init COM
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED); 
	if(FAILED(hr)) { success = false; return L""; }

	bool COMWasAlreadyOpen = hr == S_FALSE; //CoInitializeEx return S_FALSE if COM was already open
	
	//Set COM security
	if(!COMWasAlreadyOpen) {
		hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
		if(FAILED(hr)) {
			if(!COMWasAlreadyOpen) { CoUninitialize(); }
			success = false; 
			return L""; 
		}
	}

	//Get WMI locator
	IWbemLocator* WbemLocator = NULL;
	hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *)&WbemLocator);
	if(FAILED(hr)) {
		if(!COMWasAlreadyOpen) { CoUninitialize(); }
		success = false;
		return L"";
	}

	//Connect to WMI
	IWbemServices* WbemServices = NULL;
	hr = WbemLocator->ConnectServer(L"ROOT\\CIMV2", NULL, NULL, NULL, 0, NULL, NULL, &WbemServices);
	if(FAILED(hr)) {
		WbemLocator->Release();
		if(!COMWasAlreadyOpen) { CoUninitialize(); }
		success = false;
		return L"";
	}

	//Set security levels on the proxy
	hr = CoSetProxyBlanket(WbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
	if(FAILED(hr)) {
		WbemServices->Release();
		WbemLocator->Release();
		if(!COMWasAlreadyOpen) { CoUninitialize(); }
		success = false;
		return L"";
	}

	//Make WMI request
	std::wstring query(L"SELECT CommandLine FROM Win32_Process WHERE ProcessID=");
	query.append(std::to_wstring(PID));

	BSTR queryBSTR = SysAllocString(query.c_str());
	IEnumWbemClassObject *results = NULL;
	hr = WbemServices->ExecQuery(L"WQL", queryBSTR, WBEM_FLAG_FORWARD_ONLY, NULL, &results);
	SysFreeString(queryBSTR);
	if(FAILED(hr)) {
		WbemServices->Release();
		WbemLocator->Release();
		if(!COMWasAlreadyOpen) { CoUninitialize(); }
		success = false;
		return L"";
	}

	//Parse results
	std::wstring returnValue;
	if(results != NULL) {
		IWbemClassObject *result = NULL;
		ULONG returnedCount = 0;

		while((hr = results->Next(WBEM_INFINITE, 1, &result, &returnedCount)) == S_OK) {
			VARIANT CommandLine;
			hr = result->Get(L"CommandLine", 0, &CommandLine, 0, 0);
			if(FAILED(hr)) {
				WbemServices->Release();
				WbemLocator->Release();
				if(!COMWasAlreadyOpen) { CoUninitialize(); }
				success = false;
				return L"";
			}
			returnValue.assign(CommandLine.bstrVal);
			result->Release();
			break;
		}
	}

	//Cleanup
	results->Release();
	WbemServices->Release();
	WbemLocator->Release();

	if(!COMWasAlreadyOpen) { CoUninitialize(); }
	success = true;
	return returnValue;
}

LPVOID GetRemoteLibraryAddress(HANDLE procHandle, LPWSTR libraryName) {
	HMODULE* modules = new HMODULE[30];
	UINT size = 30*sizeof(HMODULE);
	DWORD requiredBytes;
	if(!EnumProcessModules(procHandle, modules, size, &requiredBytes)) {
		return NULL;
	}
	UINT moduleCount = (requiredBytes/sizeof(HMODULE));
	if(requiredBytes > size) {
		delete[] modules;
		size = requiredBytes;
		modules = new HMODULE[moduleCount];
		if(!EnumProcessModules(procHandle, modules, size, &requiredBytes)) {
			return NULL;
		}
	}

	WCHAR filepath[32767];
	WCHAR filename[_MAX_FNAME+_MAX_EXT];
	WCHAR extension[_MAX_EXT];
	for(UINT i = 0; i<moduleCount; i++) {
		if(!GetModuleFileNameEx(procHandle, modules[i], filepath, MAX_PATH)) {
			delete[] modules;
			return NULL;
		}

		_wsplitpath_s(filepath, NULL, 0, NULL, 0, filename, _MAX_FNAME, extension, _MAX_EXT);
		if(FAILED(StringCchCat(filename, _MAX_FNAME+_MAX_EXT, extension))) {
			delete[] modules;
			return NULL;
		}

		if(lstrcmpiW(libraryName, filename) == 0) {
			MODULEINFO info;
			if(!GetModuleInformation(procHandle, modules[i], &info, sizeof(MODULEINFO))) {
				delete[] modules;
				return NULL;
			}
			delete[] modules;
			return info.lpBaseOfDll;
		}
	}

	delete[] modules;
	return NULL;
}

BOOL SetPrivilege(
		HANDLE hToken,          // token handle
		LPCTSTR Privilege,      // Privilege to enable/disable
		BOOL bEnablePrivilege   // TRUE to enable.  FALSE to disable
	){
	TOKEN_PRIVILEGES tp;
	LUID luid;
	TOKEN_PRIVILEGES tpPrevious;
	DWORD cbPrevious = sizeof(TOKEN_PRIVILEGES);

	if(!LookupPrivilegeValue(NULL, Privilege, &luid)) return FALSE;

	// 
	// first pass.  get current privilege setting
	// 
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = 0;

	AdjustTokenPrivileges(
		hToken,
		FALSE,
		&tp,
		sizeof(TOKEN_PRIVILEGES),
		&tpPrevious,
		&cbPrevious
		);

	if(GetLastError() != ERROR_SUCCESS) return FALSE;

	// 
	// second pass.  set privilege based on previous setting
	// 
	tpPrevious.PrivilegeCount = 1;
	tpPrevious.Privileges[0].Luid = luid;

	if(bEnablePrivilege) {
		tpPrevious.Privileges[0].Attributes |= (SE_PRIVILEGE_ENABLED);
	} else {
		tpPrevious.Privileges[0].Attributes ^= (SE_PRIVILEGE_ENABLED &
												tpPrevious.Privileges[0].Attributes);
	}

	AdjustTokenPrivileges(
		hToken,
		FALSE,
		&tpPrevious,
		cbPrevious,
		NULL,
		NULL
		);

	if(GetLastError() != ERROR_SUCCESS) return FALSE;

	return TRUE;
};

int privileges() {
	HANDLE Token;
	TOKEN_PRIVILEGES tp;
	if(OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &Token)) {
		LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
		tp.PrivilegeCount = 1;
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		if(AdjustTokenPrivileges(Token, 0, &tp, sizeof(tp), NULL, NULL)==0) {
			return 1; //FAIL
		} else {
			return 0; //SUCCESS
		}
	}
	return 1;
}

BOOL EnableDebugPrivileges(HANDLE* mainToken) {
	if(!OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, mainToken)) {
		if(GetLastError() == ERROR_NO_TOKEN) {
			if(!ImpersonateSelf(SecurityImpersonation)) {
				return FALSE;
			}

			if(!OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, mainToken)) {
				return FALSE;
			}
		} else {
			return FALSE;
		}
	}

	if(!SetPrivilege(*mainToken, SE_DEBUG_NAME, TRUE)) {
		CloseHandle(*mainToken);
		return FALSE;
	};
	return TRUE;
}

BOOL IsElevated() {
	BOOL fRet = FALSE;
	HANDLE hToken = NULL;
	if(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
		TOKEN_ELEVATION Elevation;
		DWORD cbSize = sizeof(TOKEN_ELEVATION);
		if(GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) {
			fRet = Elevation.TokenIsElevated;
		}
	}
	if(hToken) {
		CloseHandle(hToken);
	}
	return fRet;
}

bool ElevateWithArgs(LPCWSTR args) {
	wchar_t szPath[MAX_PATH];
	if(GetModuleFileName(NULL, szPath, MAX_PATH)) {
		SHELLEXECUTEINFO sei = {0};
		sei.cbSize = sizeof(SHELLEXECUTEINFO);
		sei.lpVerb = L"runas";
		sei.lpFile = szPath;
		sei.hwnd = NULL;
		sei.nShow = SW_NORMAL;
		sei.lpParameters = args;

		return ShellExecuteEx(&sei) == TRUE;
	}
	return false;
}
