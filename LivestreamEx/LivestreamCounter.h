#pragma once
#include <cstdint>
#include "timercpp.h"
#include <iostream>
#include "Utils.h"
#include <atomic>

class LivestreamCounter
{
	Timer printTimer;
	std::atomic<uint64_t> frameCount;
	std::atomic<uint64_t> lastSecondFrameCount;
	std::atomic<uint64_t> bytesDownloaded;
	std::atomic<uint64_t> lastSecondBytesDownloaded;
	time_t startTimestamp;

	const time_t frameCountDelay = 10000;
	const int printInterval = 10;
	
	void print()
	{ 
		using namespace std;
		time_t t = getTimestampMillisecond();
		time_t delta = t - startTimestamp;
		cout << "\r" << string(80, ' ') << "\r";
		cout << "已下载 " << getPrettyByteSize(bytesDownloaded) << 
			" @ " << getPrettyByteSize(lastSecondBytesDownloaded / printInterval) <<
			"/s FPS(Avg) " << (delta > frameCountDelay ? toStringPrecision(frameCount * 1000.0 / delta, 2) : "-") << 
			" FPS " << lastSecondFrameCount / printInterval << " " << endl;
		lastSecondBytesDownloaded = 0;
		lastSecondFrameCount = 0;
	}
public:
	LivestreamCounter()
	{
		printTimer.setInterval([&]()
		{
			print();	
		}, 1000 * printInterval);
		startTimestamp = getTimestampMillisecond();
		frameCount = 0;
		bytesDownloaded = 0;
	}
	~LivestreamCounter()
	{
		printTimer.stop();
	}
	void onPacketArrive(int size)
	{
		bytesDownloaded += size;
		lastSecondBytesDownloaded += size;
	}
	void onFrameArrive()
	{ 
		++frameCount;
		++lastSecondFrameCount;
	}
	void end()
	{
		std::cout << std::endl;
		std::cout << "共下载 " << getPrettyByteSize(bytesDownloaded);
		std::cout << std::endl;
	}
};