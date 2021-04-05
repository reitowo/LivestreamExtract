#pragma once
#include <string>
#include <functional>
#include <vector>
#include <opencv2\opencv.hpp>
#include <filesystem>

class MatchTask;
class DelayedTask;

typedef std::function<void(cv::Mat&, MatchTask*)> MatchCallback;
typedef std::function<void(cv::Mat&, DelayedTask*)> DelayedCallback;

class MatchTask
{
public:
	std::string id;
	cv::Mat matchImage;
	cv::Rect matchRect;
	MatchCallback callback;
	double maximumResult;

	bool continuousCallback;

	MatchTask(std::string id, cv::Mat img, cv::Rect r, MatchCallback cb)
		: id(id), matchImage(img), matchRect(r), callback(cb)
	{
		continuousCallback = false;
		cv::Mat black(img.rows, img.cols, img.type(), cv::Scalar(0, 0, 0));
		cv::Mat white(img.rows, img.cols, img.type(), cv::Scalar(255, 255, 255));
		maximumResult = abs(cv::norm(black, white, cv::NORM_L1));
	}

	MatchTask* setContinuousCallback(bool enable)
	{
		continuousCallback = enable;
		return this;
	}
};

class DelayedTask
{
public:
	std::string id;
	uint32_t frameLast;
	DelayedCallback callback;

	DelayedTask(std::string id, uint32_t frameLast, DelayedCallback callback)
		: id(id), frameLast(frameLast), callback(callback)
	{
	}
};

class LivestreamExtractor
{
	std::string lastCallbackTask;
	std::string matchImageFolder;

	std::vector<MatchTask*> matches;
	std::vector<DelayedTask*> delays;

	uint64_t frameId = 0; 

public:
	void setMatchImageFolder(std::string path, int roomId)
	{
		std::string check = path + "_" + std::to_string(roomId);
		if (std::filesystem::exists(check))
			matchImageFolder = check;
		else
			matchImageFolder = path + "_74751";
	}

	MatchTask* addMatch(std::string id, cv::Rect matchRect, MatchCallback callback);
	DelayedTask* addDelay(std::string id, uint32_t delay, DelayedCallback callback);
	void process(cv::Mat& frame);

	uint64_t getFrameId()
	{
		return frameId;
	}
};
