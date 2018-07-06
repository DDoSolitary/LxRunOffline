#include "stdafx.h"
#include "error.h"

void write(crwstr output, uint16_t color) {
	auto hout = GetStdHandle(STD_ERROR_HANDLE);
	if (hout == INVALID_HANDLE_VALUE) hout = 0;
	CONSOLE_SCREEN_BUFFER_INFO info;
	bool ok = hout ? GetConsoleScreenBufferInfo(hout, &info) : false;
	if (hout) SetConsoleTextAttribute(hout, color);
	std::wcerr << output << std::endl;
	if (hout && ok) SetConsoleTextAttribute(hout, info.wAttributes);
}

void log_warning(crwstr msg) {
	write(L"[WARNING] " + msg, FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN);
}

void log_error(crwstr msg) {
	write(L"[ERROR] " + msg, FOREGROUND_INTENSITY | FOREGROUND_RED);
}
