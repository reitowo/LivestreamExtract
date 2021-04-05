#pragma once
 
#include <opencv2/opencv.hpp>

#include "LivestreamExtractor.h"
#include "LivestreamRecorder.h"
#include "LivestreamSubProcessor.h"

#include <bup/bup.h>

struct SMM2Match
{
	time_t matchStartTimestamp = 0;

	int matchResult = 0;
	uint64_t matchStartFrameId = 0;
	uint64_t matchEndFrameId = 0;
	uint64_t ratingFromFrameId = 0;
	uint64_t ratingToFrameId = 0;
	uint64_t matchResultFrameId = 0;
	uint64_t matchCourseFrameId = 0;
	uint32_t ratingFrom = 0;
	uint32_t ratingTo = 0;

	bool hasResult()
	{
		return matchResultFrameId != 0;
	}

	json serialize()
	{
		json s;
		s["timestamp"] = matchStartTimestamp;
		s["result"] = matchResult;

		s["matchStartFrame"] = matchStartFrameId;
		s["matchEndFrame"] = matchEndFrameId;
		s["ratingFromFrame"] = ratingFromFrameId;
		s["ratingToFrame"] = ratingToFrameId;
		s["matchResultFrame"] = matchResultFrameId;
		s["matchCourseFrame"] = matchCourseFrameId;

		s["ratingFrom"] = ratingFrom;
		s["ratingTo"] = ratingTo;
		return s;
	}
};

class SubProcessorSMM2 : public LivestreamSubProcessor
{
	time_t timestamp;
	json::array_t smm2Json;
	SMM2Match* currentSMM2Match = nullptr;
	std::string smm2FilePath;
	std::string smm2ImagePath;

	LivestreamRecorder* recorder = nullptr;
	bup::BUpload* uploader = nullptr; 
	bup::Upload* uploadTask = nullptr;
	uint64_t uploadedAvId = 0;
	std::thread uploadThread;
	std::queue<std::filesystem::path> uploadQueue;
	std::mutex uploadQueueMutex;
	
	struct SMM2OCRTask
	{
		std::filesystem::path path;
		std::function<void(int)> callback;
	};

	std::thread ocrThread;
	std::queue<SMM2OCRTask> ocrTaskQueue;
	std::mutex ocrTaskQueueMutex;

public:
	void initialize(LivestreamExtractor* extractor, LivestreamRecorder* recorder);

	void ocrResult(std::filesystem::path path, std::function<void(int)> callback); 
	bool ocrTaskEmpty()
	{
		ocrTaskQueueMutex.lock();
		if (ocrTaskQueue.empty())
		{
			ocrTaskQueueMutex.unlock();
			return true;
		}
		ocrTaskQueueMutex.unlock();
		return false;
	}

	void uploadVideoAsync(std::filesystem::path path);
	
	void smm2MatchStart(cv::Mat& frame, uint64_t frameId);
	void smm2MatchEnd(cv::Mat& frame, uint64_t frameId);
	void smm2YouWin(cv::Mat& frame, uint64_t frameId);
	void smm2YouLose(cv::Mat& frame, uint64_t frameId);
	void smm2Tie(cv::Mat& frame, uint64_t frameId);
	void smm2Giveup(cv::Mat& frame, uint64_t frameId);
	void smm2MatchResultFrom(cv::Mat& frame, uint64_t frameId);
	void smm2MatchResultTo(cv::Mat& frame, uint64_t frameId);
	void smm2MatchCourse(cv::Mat& frame, uint64_t frameId);

	void onSerialize(json&) override;
	void start(time_t timestamp) override;
	void stop() override;
};
