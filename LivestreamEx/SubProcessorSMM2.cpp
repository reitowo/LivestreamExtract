#include "SubProcessorSMM2.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <chrono>
#include <vector>
#include <k1ee/file.h>
#include <k1ee/encoding.h>

#include "../common.h"
#include "OCR.h"
#include "Utils.h"

void imwriteFlush(const std::filesystem::path& path, const cv::Mat& image)
{
	vector<uint8_t> buf;
	cv::imencode(path.extension().string(), image, buf);
	ofstream fout(path, ios::binary | ios::ate);
	fout.write(reinterpret_cast<char*>(buf.data()), buf.size());
	fout.flush();
	fout.close();
}

void SubProcessorSMM2::initialize(LivestreamExtractor* extractor, LivestreamRecorder* recorder)
{
	ocrInit();
	this->recorder = recorder;

	extractor->addMatch("smm2_multi_start", cv::Rect(686, 155, 528, 73), [=](cv::Mat& frame, MatchTask* task)
	{
		smm2MatchStart(frame, extractor->getFrameId());
	});

	extractor->addMatch("smm2_multi_course", cv::Rect(71, 57, 58, 298), [=](cv::Mat& frame, MatchTask* task)
	{
		smm2MatchCourse(frame, extractor->getFrameId());
	});

	extractor->addMatch("smm2_you_lose", cv::Rect(664, 467, 629, 148), [=](cv::Mat& frame, MatchTask* task)
	{
		smm2YouLose(frame, extractor->getFrameId());
	});

	extractor->addMatch("smm2_you_win", cv::Rect(642, 456, 635, 168), [=](cv::Mat& frame, MatchTask* task)
	{
		smm2YouWin(frame, extractor->getFrameId());
	});

	extractor->addMatch("smm2_tie1", cv::Rect(660, 471, 586, 139), [=](cv::Mat& frame, MatchTask* task)
	{
		smm2Tie(frame, extractor->getFrameId());
	});

	extractor->addMatch("smm2_match_result", cv::Rect(60, 143, 234, 45), [=](cv::Mat& frame, MatchTask* task)
	{
		smm2MatchResultFrom(frame, extractor->getFrameId());
		extractor->addDelay("smm2_match_result_end", 50, [=](cv::Mat& frame, DelayedTask* task)
		{
			smm2MatchResultTo(frame, extractor->getFrameId());
		});
	});

	extractor->addMatch("smm2_multi_end", cv::Rect(544, 891, 187, 133), [=](cv::Mat& frame, MatchTask* task)
	{
		smm2MatchEnd(frame, extractor->getFrameId());
	});

	extractor->addMatch("smm2_giveup", cv::Rect(779, 494, 364, 93), [=](cv::Mat& frame, MatchTask* task)
	{
		smm2Giveup(frame, extractor->getFrameId());
	});

	ocrThread = std::thread([this]
	{
		while (true)
		{
			std::optional<SMM2OCRTask> task;
			ocrTaskQueueMutex.lock();
			if (!ocrTaskQueue.empty())
			{
				task = ocrTaskQueue.front();
			}
			ocrTaskQueueMutex.unlock();

			if (task.has_value())
			{
				using namespace cv;
				Mat image = imread(task->path.string());
				Rect roi(900, 560, 120, 40);
				Mat rating(image, roi);
				cvtColor(rating, rating, COLOR_BGR2GRAY);
				threshold(rating, rating, 127, 255, THRESH_BINARY_INV);
				char* out = ocrSmm2Rating(rating);
				int ret = std::stoi(out);
				delete[] out;
				task->callback(ret);

				ocrTaskQueueMutex.lock();
				ocrTaskQueue.pop();
				ocrTaskQueueMutex.unlock();
			}
		}
	});

	uploadThread = std::thread([this]
	{
		while (true)
		{
			std::optional<std::filesystem::path> task;
			uploadQueueMutex.lock();
			if (!uploadQueue.empty())
			{
				task = uploadQueue.front();
			}
			uploadQueueMutex.unlock();

			if (task.has_value())
			{
				auto video = uploader->uploadVideo(task.value());
				uploadTask->videos.push_back(video);

				if (uploadedAvId == 0)
				{
					auto uploadResult = uploader->upload(*uploadTask);
					if (uploadResult.succeed)
					{
						uploadedAvId = uploadResult.av;
					}
				}
				else
				{
					if (uploader->isPassedReview(uploadedAvId))
						uploader->edit(uploadedAvId, *uploadTask);
				}

				uploadQueueMutex.lock();
				uploadQueue.pop();
				uploadQueueMutex.unlock();
			}
		}
	});
}

void SubProcessorSMM2::ocrResult(std::filesystem::path path, std::function<void(int)> callback)
{
	ocrTaskQueueMutex.lock();
	ocrTaskQueue.push({path, callback});
	ocrTaskQueueMutex.unlock();
}

void SubProcessorSMM2::uploadVideoAsync(std::filesystem::path path)
{
	uploadQueueMutex.lock();
	uploadQueue.push(path);
	uploadQueueMutex.unlock();
}

void SubProcessorSMM2::smm2MatchStart(cv::Mat& frame, uint64_t frameId)
{
	if (!working)
		return;
	delete currentSMM2Match;
	currentSMM2Match = new SMM2Match();
	currentSMM2Match->matchStartTimestamp = getTimestampMillisecond();
	currentSMM2Match->matchStartFrameId = frameId;
	imwriteFlush(smm2ImagePath + "/" + std::to_string(frameId) + ".png", frame);
	cout << "SMM2: 比赛开始" << endl;
	recorder->startOutputSlice();
}

void SubProcessorSMM2::smm2MatchEnd(cv::Mat& frame, uint64_t frameId)
{
	if (!working)
		return;
	if (currentSMM2Match == nullptr)
		return;

	if (currentSMM2Match->matchEndFrameId != 0)
		return;

	currentSMM2Match->matchEndFrameId = frameId;
	imwriteFlush(smm2ImagePath + "/" + std::to_string(frameId) + ".png", frame);

	auto t = thread([this]
	{
		while (!ocrTaskEmpty() || currentSMM2Match->ratingFromFrameId == 0 || currentSMM2Match->ratingToFrameId == 0)
		{
			this_thread::sleep_for(chrono::milliseconds(500));
		}

		const auto video_path = recorder->endOutputSlice(currentSMM2Match->ratingFrom, currentSMM2Match->ratingTo);

		smm2Json.push_back(currentSMM2Match->serialize());
		delete currentSMM2Match;
		currentSMM2Match = nullptr;
		cout << "SMM2: 比赛结束" << endl;

		std::ofstream smm2(smm2FilePath);
		smm2 << smm2Json;
		smm2.close();

#define UPLOAD_EVERYTIME 0
#if !UPLOAD_EVERYTIME
		uploadVideoAsync(video_path);
#else
		
		auto ratingFrom = currentSMM2Match->ratingFrom;
		auto ratingTo = currentSMM2Match->ratingTo;

		auto coverPathIn = smm2ImagePath + "/" + std::to_string(currentSMM2Match->matchResultFrameId) + ".png";
		auto coverPathOut = smm2ImagePath + "/" + std::to_string(currentSMM2Match->matchResultFrameId) + ".jpg";
		
		bup::Upload up;
		up.copyright = false;
		up.source = "来源：主播直播流";
		std::time_t t = std::time(nullptr); // get time now
		std::tm now;
		localtime_s(&now, &t);
		up.tag = "超级马里奥制造2,超级小桀,直播,切片";
		up.description =
			R"(SMM2自动切片上传，使用自行开发库：
https://github.com/cnSchwarzer/BilibiliUp
https://github.com/cnSchwarzer/LivestreamExtract)";
		up.title = "[直播切片] ("
			+ std::to_string(ratingFrom) + "->"
			+ std::to_string(ratingTo) + ") ["
			+ std::to_string(now.tm_year + 1900) + "."
			+ std::to_string(now.tm_mon + 1) + "."
			+ std::to_string(now.tm_mday) + " "
			+ std::to_string(now.tm_hour) + ":"
			+ std::to_string(now.tm_min) + ":"
			+ std::to_string(now.tm_sec) + "]";

		auto cover = cv::imread(coverPathIn);
		int width = cover.rows * 1.6;
		cv::Mat coverRoi(cover, cv::Rect(cover.cols / 2 - width / 2, 0, width, cover.rows));
		cv::imwrite(coverPathOut, coverRoi);

		up.videos.push_back(uploader->uploadVideo(video_path));
		up.cover = uploader->uploadCover(coverPathOut);
		auto uploadResult = uploader->upload(up);
		cout << "稿件投递：" << uploadResult.succeed << " " << uploadResult.bv << endl;
#endif
	});
	t.detach();
}

void SubProcessorSMM2::smm2YouWin(cv::Mat& frame, uint64_t frameId)
{
	if (!working)
		return;
	if (currentSMM2Match == nullptr || currentSMM2Match->hasResult())
		return;

	if (currentSMM2Match->matchResultFrameId != 0)
		return;

	currentSMM2Match->matchResult = 1;
	currentSMM2Match->matchResultFrameId = frameId;
	imwriteFlush(smm2ImagePath + "/" + std::to_string(frameId) + ".png", frame);
	cout << "SMM2: 胜利" << endl;
}

void SubProcessorSMM2::smm2YouLose(cv::Mat& frame, uint64_t frameId)
{
	if (!working)
		return;
	if (currentSMM2Match == nullptr || currentSMM2Match->hasResult())
		return;

	if (currentSMM2Match->matchResultFrameId != 0)
		return;

	currentSMM2Match->matchResult = 2;
	currentSMM2Match->matchResultFrameId = frameId;
	imwriteFlush(smm2ImagePath + "/" + std::to_string(frameId) + ".png", frame);
	cout << "SMM2: 战败" << endl;
}

void SubProcessorSMM2::smm2Tie(cv::Mat& frame, uint64_t frameId)
{
	if (!working)
		return;
	if (currentSMM2Match == nullptr || currentSMM2Match->hasResult())
		return;

	if (currentSMM2Match->matchResultFrameId != 0)
		return;

	currentSMM2Match->matchResult = 3;
	currentSMM2Match->matchResultFrameId = frameId;
	imwriteFlush(smm2ImagePath + "/" + std::to_string(frameId) + ".png", frame);
	cout << "SMM2: 平局" << endl;
}

void SubProcessorSMM2::smm2Giveup(cv::Mat& frame, uint64_t frameId)
{
	if (!working)
		return;
	if (currentSMM2Match == nullptr || currentSMM2Match->hasResult())
		return;

	if (currentSMM2Match->matchResultFrameId != 0)
		return;

	currentSMM2Match->matchResult = 2;
	currentSMM2Match->matchResultFrameId = frameId;
	imwriteFlush(smm2ImagePath + "/" + std::to_string(frameId) + ".png", frame);
	cout << "SMM2: 放弃" << endl;
}

void SubProcessorSMM2::smm2MatchResultFrom(cv::Mat& frame, uint64_t frameId)
{
	if (!working)
		return;
	if (currentSMM2Match == nullptr)
		return;

	if (currentSMM2Match->ratingFromFrameId != 0)
		return;
	
	currentSMM2Match->ratingFromFrameId = frameId;
	auto path = smm2ImagePath + "/" + std::to_string(frameId) + ".png";
	imwriteFlush(path, frame);
	cout << "SMM2: 积分（变化前）" << endl;
	ocrResult(path, [this](int val)
	{
		cout << "OCR：" << val << endl;
		currentSMM2Match->ratingFrom = val;
	});
}

void SubProcessorSMM2::smm2MatchResultTo(cv::Mat& frame, uint64_t frameId)
{
	if (!working)
		return;
	if (currentSMM2Match == nullptr)
		return;

	if (currentSMM2Match->ratingToFrameId != 0)
		return;

	currentSMM2Match->ratingToFrameId = frameId;
	auto path = smm2ImagePath + "/" + std::to_string(frameId) + ".png";
	imwriteFlush(path, frame);
	cout << "SMM2: 积分（变化后）" << endl;
	ocrResult(path, [this](int val)
	{
		cout << "OCR：" << val << endl;
		currentSMM2Match->ratingTo = val;
	});
}

void SubProcessorSMM2::smm2MatchCourse(cv::Mat& frame, uint64_t frameId)
{
	if (!working)
		return;
	if (currentSMM2Match == nullptr)
		return;

	if(currentSMM2Match->matchCourseFrameId != 0)
		return;
	
	currentSMM2Match->matchCourseFrameId = frameId;
	imwriteFlush(smm2ImagePath + "/" + std::to_string(frameId) + ".png", frame);
	cout << "SMM2: 关卡" << endl;
}

void SubProcessorSMM2::onSerialize(json& root)
{
	root["smm2_file"] = std::filesystem::absolute(smm2FilePath).string();
	root["smm2_image"] = std::filesystem::absolute(smm2ImagePath).string();
}

void SubProcessorSMM2::start(time_t timestamp)
{
	this->timestamp = timestamp;
	smm2FilePath = outputFolder + "/output_smm2/" + std::to_string(timestamp) + ".json";
	smm2ImagePath = outputFolder + "/output_smm2/" + std::to_string(timestamp);
	std::filesystem::create_directories(outputFolder + "/output_smm2");
	std::filesystem::create_directories(smm2ImagePath);
	uploader = new bup::BUpload(std::atol(k1ee::read_all_texts( workingFolder + "/bili-uid.txt").c_str()),
		k1ee::read_all_texts( workingFolder + "/bili-accesstoken.txt"));
	uploadTask = new bup::Upload();
	uploadedAvId = 0;

	uploadTask->tag = "超级小桀,马里奥制造2,直播切片,剪辑,直播";
	uploadTask->copyright = false;
	uploadTask->source = "主播直播流";
	std::time_t t = std::time(nullptr); // get time now
	std::tm now;
	localtime_s(&now, &t);
	uploadTask->description =
		R"(SMM2自动切片上传，使用自行开发库：
https://github.com/cnSchwarzer/BilibiliUp
https://github.com/cnSchwarzer/LivestreamExtract)";
	uploadTask->title = "【超级小桀】直播自动切片 "
		+ std::to_string(now.tm_year + 1900) + "年"
		+ std::to_string(now.tm_mon + 1) + "月"
		+ std::to_string(now.tm_mday) + "日";
	uploadTask->cover = uploader->uploadCover(workingFolder + "/slice_cover.jpg");

	working = true;
}

void SubProcessorSMM2::stop()
{
	//等待所有OCR任务完成
	while (true)
	{
		ocrTaskQueueMutex.lock();
		if (ocrTaskQueue.empty())
		{
			ocrTaskQueueMutex.unlock();
			break;
		}
		ocrTaskQueueMutex.unlock();
		cout << "等待所有OCR任务完成" << endl;
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	//等待所有上传任务完成
	while (true)
	{
		uploadQueueMutex.lock();
		if (uploadQueue.empty())
		{
			uploadQueueMutex.unlock();
			break;
		}
		uploadQueueMutex.unlock();
		cout << "等待所有上传任务完成" << endl;
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	uploader->edit(uploadedAvId, *uploadTask);
	delete uploader;
	delete uploadTask;
	uploader = nullptr;
	uploadTask = nullptr;

	std::ofstream smm2(smm2FilePath);
	smm2 << smm2Json;
	smm2.close();
	smm2Json.clear();
	working = false;
}
