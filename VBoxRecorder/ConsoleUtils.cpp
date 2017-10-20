#include "ConsoleUtils.h"
#include <iostream>
#include <windows.h>

void SetConsoleColor(const WORD color) {
	const HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	if(consoleHandle != nullptr) {
		SetConsoleTextAttribute(consoleHandle, color);
	}
}

void PrintColoredText(const WORD color, const char* text) {
	SetConsoleColor(color);
	std::cout << text;
	SetConsoleColor(FOREGROUND_NORMAL);
}