#include "stdafx.h"
#include "error.h"

auto hcon = GetStdHandle(STD_ERROR_HANDLE);
bool progress_printed;

void write(crwstr output, uint16_t color) {
	CONSOLE_SCREEN_BUFFER_INFO ci;
	bool ok = hcon != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hcon, &ci);
	if (ok) SetConsoleTextAttribute(hcon, color);
	std::wcerr << output << std::endl;
	if (ok) SetConsoleTextAttribute(hcon, ci.wAttributes);
	progress_printed = false;
}

void log_warning(crwstr msg) {
	write(L"[WARNING] " + msg, FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN);
}

void log_error(crwstr msg) {
	write(L"[ERROR] " + msg, FOREGROUND_INTENSITY | FOREGROUND_RED);
}

void print_progress(double progress) {
	static int lc;
	if (hcon == INVALID_HANDLE_VALUE) return;
	CONSOLE_SCREEN_BUFFER_INFO ci;
	if (!GetConsoleScreenBufferInfo(hcon, &ci)) return;
	auto tot = ci.dwSize.X - 2;
	auto cnt = (int)round(tot * progress);
	if (cnt == lc) return;
	lc = cnt;
	if (progress_printed && !SetConsoleCursorPosition(hcon, { 0,ci.dwCursorPosition.Y - 1 })) return;
	std::wcerr << L'[';
	for (int i = 0; i < tot; i++) {
		if (i < cnt) std::wcerr << L'=';
		else std::wcerr << L'-';
	}
	std::wcerr << L']' << std::endl;
	progress_printed = true;
}
