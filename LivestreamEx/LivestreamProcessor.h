#pragma once

#include <opencv2/opencv.hpp>
#include "LivestreamDanmuDouyu.h"
#include <atomic>
#include <fstream>
#include <vector>
#include <string> 
#include <nlohmann/json.hpp>

#include "LivestreamSubProcessor.h"

class LivestreamProcessor
{
	std::atomic_bool working;

	std::string roomId;
	std::string liveFilePath;
	std::string recordingFilePath;

	std::vector<LivestreamSubProcessor*> subProcessors;

	json serialize();

public:
	template <class T>
	T* addSubProcessor()
	{
		LivestreamSubProcessor* subProcessor = new T();
		subProcessors.push_back(subProcessor);
		return reinterpret_cast<T*>(subProcessor);
	}

	~LivestreamProcessor();
	void start(std::string recordingFile);
	std::string stop();

	void setRoomId(int roomId)
	{
		this->roomId = std::to_string(roomId);
	}
};
