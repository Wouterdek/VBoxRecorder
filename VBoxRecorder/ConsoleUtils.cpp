#include "ConsoleUtils.h"
#include <iostream>

void SetConsoleColor(WORD color) {
	HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	if(consoleHandle == NULL) {
		return;
	}
	SetConsoleTextAttribute(consoleHandle, color);
}

void PrintColoredText(WORD color, char* text) {
	SetConsoleColor(color);
	std::cout << text;
	SetConsoleColor(FOREGROUND_NORMAL);
}