#include "Timer.h"

Timer::Timer(string name, TIMER_PRINT_FORMAT format)
{
	this->mTimerName = name;
	this->reset();
	this->mPrintFormat = format;
}

void Timer::reset()
{
	mTimeStamp = std::chrono::steady_clock::now();
	this->mElapsedTimeMicrosecondRecords.clear();
	mTotalElapsedTimeMicrosecond = 0;
}

void Timer::addRecord(string name)
{
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	float elapsedTimeMicrosecond = std::chrono::duration_cast<std::chrono::microseconds>(now - mTimeStamp).count();
	mElapsedTimeMicrosecondRecords.push_back(std::make_pair(name, elapsedTimeMicrosecond));
	mTimeStamp = now;
	mTotalElapsedTimeMicrosecond += elapsedTimeMicrosecond;
}

void Timer::addChildTimer(Timer* timer)
{
	mChildTimers.push_back(timer);
}

void Timer::processGUI(bool printTotal)
{
	TIMER_PRINT_FORMAT targetFormat = this->mPrintFormat;

	string formatUnit = "ms";
	float divideUnit = 1e3;
	switch (targetFormat)
	{
	case TIMER_PRINT_FORMAT_SEC: formatUnit = "s"; divideUnit = 1e6; break;
	case TIMER_PRINT_FORMAT_MILLISEC: formatUnit = "ms"; divideUnit = 1e3; break;
	case TIMER_PRINT_FORMAT_MICROSEC: formatUnit = "us"; divideUnit = 1; break;
	default: formatUnit = "ms"; divideUnit = 1e3; break;
	}

	if (ImGui::TreeNode(this->mTimerName.c_str()))
	{
		bool isLeafTimer = mChildTimers.size() == 0;
		if (isLeafTimer) {
			for (auto it = mElapsedTimeMicrosecondRecords.begin(); it != mElapsedTimeMicrosecondRecords.end(); it++)
			{
				ImGui::Text("%s: %.3f %s", it->first.c_str(), it->second / divideUnit, formatUnit.c_str());
			}

			ImGui::Text("Total : %.3f %s", mTotalElapsedTimeMicrosecond / divideUnit, formatUnit.c_str());
			ImGui::TreePop();
		}
		else {
			for (auto it = mElapsedTimeMicrosecondRecords.begin(); it != mElapsedTimeMicrosecondRecords.end(); it++)
			{
				ImGui::Text("%s: %.3f %s", it->first.c_str(), it->second / divideUnit, formatUnit.c_str());
			}

			//float totalElapsedTimeMicrosecond = 0.0f;
			for (auto childTimer : mChildTimers)
			{
				childTimer->processGUI();
				//totalElapsedTimeMicrosecond += childTimer->mTotalElapsedTimeMicrosecond;
			}

			ImGui::Text("Total : %.3f %s", mTotalElapsedTimeMicrosecond / divideUnit, formatUnit.c_str());
			ImGui::TreePop();
		}
		

		//for (auto it = mElapsedTimeMicrosecondRecords.begin(); it != mElapsedTimeMicrosecondRecords.end(); it++)
		//{
		//	ImGui::Text("%s: %.3f %s", it->first.c_str(), it->second / divideUnit, formatUnit.c_str());
		//}
		//ImGui::Text("Total : %.3f %s", mTotalElapsedTimeMicrosecond / divideUnit, formatUnit.c_str());
		//ImGui::TreePop();
	}
}