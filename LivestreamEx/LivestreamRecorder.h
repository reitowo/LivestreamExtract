#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <filesystem>
#include <functional>
#include <optional>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libswscale/swscale.h>
}

#include <opencv2/opencv.hpp>

using std::string;

class LivestreamRecorder
{
protected:
	bool shouldSaveCopy;
	int roomId;
	string file;

	AVFormatContext* in = nullptr;
	AVFormatContext* out = nullptr;

	std::mutex outSliceMutex;
	AVFormatContext* outSlice = nullptr;
	uint64_t outSliceVideoKeyFramePts = 0;
	
	std::atomic<int> downloadCancel;
	std::thread downloadThread;
	time_t timestamp;

	int targetWidth = 1920;
	int targetHeight = 1080;
	std::function<void(cv::Mat&)> onFrameCallback;

	bool bt709Conversion = false;

public:
	LivestreamRecorder() = default;
	virtual ~LivestreamRecorder() = default;
	LivestreamRecorder(const LivestreamRecorder&) = default;
	LivestreamRecorder& operator=(const LivestreamRecorder&) = default;
	LivestreamRecorder(LivestreamRecorder&&) = default;
	LivestreamRecorder& operator=(LivestreamRecorder&&) = default;

	void onFrame(std::function<void(cv::Mat&)> onFrame)
	{
		onFrameCallback = onFrame;
	}

	void setFrameMatSize(int width, int height)
	{
		targetWidth = width;
		targetHeight = height;
	}

	void setBt709Input(bool val)
	{
		bt709Conversion = val;
	}

	bool start(int roomId);
	void start(string file);
	void startUntilSuccess(int roomId, int intervalSeconds);
	void stop();
	void waitForStop();

	std::string getSaveFilePath()
	{
		if (out == nullptr)
			return file;
		return std::string(out->url);
	}
	void startOutputSlice();
	std::string endOutputSlice(int from = 0, int to = 0);

private:
	void download();
	virtual std::string requestLivestreamUrl();
	virtual std::string generateSaveFilePath();
	virtual std::string generateSliceFilePath(int from = 0, int to = 0);

};
