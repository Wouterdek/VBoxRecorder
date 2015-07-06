#include "Util.h"

#include <locale>
#include <codecvt>
#include <string>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <iostream>

// trim from start
std::string &ltrim(std::string &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
	return s;
}

// trim from end
std::string &rtrim(std::string &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	return s;
}

// trim from both ends
std::string &trim(std::string &s) {
	return ltrim(rtrim(s));
}

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems, bool removeEmpty) {
	std::stringstream ss(s);
	std::string item;
	while(std::getline(ss, item, delim)) {
		if(removeEmpty && item.empty()) {
			continue;
		}
		elems.push_back(item);
	}
	return elems;
}


std::vector<std::string> split(const std::string &s, char delim, bool removeEmpty) {
	std::vector<std::string> elems;
	split(s, delim, elems, removeEmpty);
	return elems;
}

std::wstring strtowstr(const std::string& str) {
	size_t converted = 0;
	wchar_t* wchars = new wchar_t[str.length()+1];
	errno_t error = mbstowcs_s(&converted, wchars, str.length()+1, str.c_str(), _TRUNCATE);
	std::wstring wstr(wchars, wchars+converted-1);
	delete[] wchars;

	return wstr;
}
std::string wstrtostr(const std::wstring& wstr) {
	size_t size = wstr.length();
	std::string str(size + 1, 0);

	WideCharToMultiByte(CP_ACP,
						0,
						wstr.c_str(),
						size,
						&str[0],
						size,
						NULL,
						NULL);
	return str;
}

std::wstring getMachineIDFromName(IVirtualBox* vbox, std::string targetNameStr) {
	std::wstring resultID;

	std::wstring targetName = strtowstr(targetNameStr);

	SAFEARRAY* machinesArray = NULL;
	if(vbox->get_Machines(&machinesArray) == S_OK) {
		IMachine** machines = NULL;
		HRESULT result = SafeArrayAccessData(machinesArray, (void **)&machines);
		if(result == S_OK) {
			for(uint i = 0; i<machinesArray->rgsabound->cElements;i++) {
				BSTR name;
				machines[i]->get_Name(&name);
				if(wcscmp(name, targetName.c_str()) == 0){
					BSTR id;
					machines[i]->get_Id(&id);

					resultID = std::wstring(id);
					SysFreeString(id);
				}
				SysFreeString(name);
			}
			SafeArrayUnaccessData(machinesArray);
		}
	}
	return resultID;
}

std::wstring getMachineIDFromIndex(IVirtualBox* vbox, uint i) {
	std::wstring resultID;

	SAFEARRAY* machinesArray = NULL;
	if(vbox->get_Machines(&machinesArray) == S_OK) {
		IMachine** machines = NULL;
		HRESULT result = SafeArrayAccessData(machinesArray, (void **)&machines);
		if(result == S_OK) {
			if(i >= 0 && i < machinesArray->rgsabound->cElements) {
				BSTR id;
				machines[i]->get_Id(&id);

				resultID = std::wstring(id);
				SysFreeString(id);
			}
			SafeArrayUnaccessData(machinesArray);
		}
	}
	return resultID;
}