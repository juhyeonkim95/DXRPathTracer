#pragma once
#include <string>
#include <chrono>
#include <vector>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

using namespace std;

enum TIMER_PRINT_FORMAT : unsigned int {
	TIMER_PRINT_FORMAT_NONE = 0,
	TIMER_PRINT_FORMAT_SEC = 1 << 0,
	TIMER_PRINT_FORMAT_MILLISEC = 1 << 1,
	TIMER_PRINT_FORMAT_MICROSEC = 1 << 2,
};

class Timer
{
public:
	Timer(string name, TIMER_PRINT_FORMAT format = TIMER_PRINT_FORMAT_MILLISEC);
	void addRecord(string name);
	void reset();
	void processGUI(bool printTotal = true);
	void addChildTimer(Timer* child);

private:
	string mTimerName;
	TIMER_PRINT_FORMAT mPrintFormat;
	std::chrono::steady_clock::time_point mTimeStamp;
	vector<pair<string, float>> mElapsedTimeMicrosecondRecords;
	vector<Timer* > mChildTimers;
	float mTotalElapsedTimeMicrosecond;
};