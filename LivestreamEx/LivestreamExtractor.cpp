#include "LivestreamExtractor.h"
#include <cmath>
#include "Utils.h" 

MatchTask* LivestreamExtractor::addMatch(std::string id, cv::Rect matchRect, MatchCallback callback)
{
	cv::Mat image = cv::imread(matchImageFolder + "/" + id + ".png");
	cv::Mat matchImage(image, matchRect); 
	MatchTask* task = new MatchTask(id, matchImage, matchRect, callback);
	matches.push_back(task);
	return task;
}

DelayedTask* LivestreamExtractor::addDelay(std::string id, uint32_t delay, DelayedCallback callback)
{
	DelayedTask* task = new DelayedTask(id, delay, callback);
	delays.push_back(task);
	return task;
}

void LivestreamExtractor::process(cv::Mat& frame)
{
	using namespace std;
#define LIVESTREAM_EXTRACTOR_PROCESS_DEBUG 0

#if LIVESTREAM_EXTRACTOR_PROCESS_DEBUG
	cv::Mat preview;
	cv::resize(frame, preview, cv::Size(1280, 720));
	cv::imshow("", preview);
	cv::waitKey(1);
#endif

	frameId++;

	//Process delayed tasks
	for (int i = 0; i < delays.size(); ++i)
	{
		DelayedTask* task = delays[i];
		task->frameLast--;
		if (task->frameLast == 0)
		{
			task->callback(frame, task);
		}
	}

	delays.erase(std::remove_if(delays.begin(), delays.end(), [](DelayedTask* item)
	{
		bool shouldErase = item->frameLast == 0;
		if (shouldErase)
			delete item;
		return shouldErase;
	}), delays.end());

//#pragma omp parallel for num_threads(matches.size() > 8 ? 8 : matches.size()) if(!LIVESTREAM_EXTRACTOR_PROCESS_DEBUG)
	for (int i = 0; i < matches.size(); ++i)
	{
		MatchTask* task = matches[i];
		cv::Mat& match = task->matchImage; 
		cv::Mat frameRoi(frame, task->matchRect); 
		double result = abs(cv::norm(frameRoi, match, cv::NORM_L1)) / task->maximumResult;
		if (result < 0.03)
		{
			if (lastCallbackTask != task->id || task->continuousCallback)
			{
				task->callback(frame, task);
				lastCallbackTask = task->id;
			}
		}
#if LIVESTREAM_EXTRACTOR_PROCESS_DEBUG
		cout << task->id << " " << toStringPrecision(result, 5) << " ";
#endif
	}
#if LIVESTREAM_EXTRACTOR_PROCESS_DEBUG
	cout << endl;
#endif
}
