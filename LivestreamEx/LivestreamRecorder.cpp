#include "LivestreamRecorder.h"

#include <cassert>
#include <utility>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

#include "Utils.h"
#include "LivestreamCounter.h"
#include "../common.h"

//取流
bool LivestreamRecorder::start(int roomId)
{
	//取流因此要保存
	shouldSaveCopy = true;
	this->roomId = roomId;

	//一个working的ffmpeg使用案例，下同
	//TODO: 可以稍微封装一下？
	in = avformat_alloc_context();

	//子类实现的直播流地址获取方法
	std::string url = requestLivestreamUrl();

	if (url.empty())
		return false;

	int ret = avformat_open_input(&in, url.c_str(), nullptr, nullptr);
	if (ret < 0)
		return false;

	ret = avformat_find_stream_info(in, nullptr);
	if (ret < 0)
		return false;

	//本次录制开始的时间戳
	timestamp = getTimestampMillisecond();

	//原汁原味的直播录像保存地点
	std::string save = generateSaveFilePath();
	std::filesystem::create_directories(outputFolder + std::string("/output_video"));
	
	avformat_alloc_output_context2(&out, nullptr, "flv", save.c_str());
	avio_open2(&out->pb, save.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr);
	for (int i = 0; i < in->nb_streams; i++)
	{
		AVStream* stream = avformat_new_stream(out, nullptr);
		ret = avcodec_parameters_copy(stream->codecpar, in->streams[i]->codecpar);
	}
	ret = avformat_write_header(out, nullptr);

	//启动线程开始录制以及处理
	downloadCancel = 0;
	downloadThread = std::thread([&]() { download(); });
	std::cout << "分析开始（直播）" << std::endl;
	return true;
}

//文件测试
void LivestreamRecorder::start(string file)
{
	//不转录，因为是测试
	shouldSaveCopy = false;
	this->file = file;

	in = avformat_alloc_context();
	std::string url = file;
	int ret = avformat_open_input(&in, url.c_str(), nullptr, nullptr);
	ret = avformat_find_stream_info(in, nullptr);

	downloadCancel = 0;
	downloadThread = std::thread([&]() { download(); });
	std::cout << "分析开始（文件）" << std::endl;
}

void LivestreamRecorder::startUntilSuccess(int roomId, int intervalSeconds = 10)
{
	//顾名思义
	int retryCount = 0;
	while (!start(roomId))
	{
		++retryCount;
		std::cout << "启动失败，" << intervalSeconds << "秒后重试 (" << retryCount << ")" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
	}
}

void LivestreamRecorder::stop()
{
	//结束，并等待线程退出
	downloadCancel = 1;
	if (downloadThread.joinable())
	{
		try
		{
			downloadThread.join();
		}
		catch (std::exception)
		{
		}
	}

	std::cout << "分析结束" << std::endl;
}

void LivestreamRecorder::waitForStop()
{
	//阻塞直到线程推出
	if (downloadThread.joinable())
	{
		try
		{
			downloadThread.join();
		}
		catch (std::exception)
		{
		}
	}
	std::cout << std::dec;
}

void LivestreamRecorder::download()
{
	//分配一个Packet以及Frame的内存
	AVPacket* packet = av_packet_alloc();
	AVFrame* frame = av_frame_alloc();

	//寻找视频流的解码器，记录视频流的流Idx，不处理音频直接复制
	AVCodecContext* codecContext = nullptr;
	int videoStreamIndex = -1;
	for (int i = 0; i < in->nb_streams; i++)
	{
		auto param = in->streams[i]->codecpar;
		if (param->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			videoStreamIndex = i;
			AVCodec* codec = avcodec_find_decoder(param->codec_id);
			//AVCodec* codec = avcodec_find_decoder_by_name("h264_cuvid");
			codecContext = avcodec_alloc_context3(codec);
			avcodec_parameters_to_context(codecContext, param);
			avcodec_open2(codecContext, codec, nullptr);
			break;
		}
	}

	//所有视频输入都需要经过sws模块进行像素格式转换，缩放其实都是1080P
	AVFrame* resizedFrame = av_frame_alloc();
	resizedFrame->format = AV_PIX_FMT_BGR24;
	resizedFrame->width = targetWidth;
	resizedFrame->height = targetHeight;
	av_frame_get_buffer(resizedFrame, 0);
	SwsContext* swsContext = sws_getContext(
		codecContext->width, codecContext->height, codecContext->pix_fmt,
		targetWidth, targetHeight, AV_PIX_FMT_BGR24,
		SWS_BICUBIC, nullptr, nullptr, nullptr);
	int *srcCoef, *destCoef;
	int srcRange, destRange;
	int brightness, contrast, saturation;
	sws_getColorspaceDetails(swsContext, &srcCoef, &srcRange, &destCoef, &destRange, &brightness, &contrast,
	                         &saturation);

	//BT709的系数和BT601不一样
	if (bt709Conversion)
		srcCoef = const_cast<int*>(sws_getCoefficients(AVCOL_SPC_BT709));
	
	sws_setColorspaceDetails(swsContext, srcCoef, srcRange, destCoef, destRange, brightness, contrast, saturation);

	//一个简单的命令行状态统计
	LivestreamCounter counter;

	//下载并处理
	int ret = 0;
	while (downloadCancel == 0)
	{
		//ffmpeg阻塞式下载
		ret = av_read_frame(in, packet);
		if (ret != 0)
		{
			break;
		}

		//统计下载速度
		counter.onPacketArrive(packet->size);

		//只处理视频流
		if (packet->stream_index == videoStreamIndex)
		{
			//一个working的ffmpeg解码
			avcodec_send_packet(codecContext, packet);
			while (avcodec_receive_frame(codecContext, frame) == 0)
			{
				//统计帧率
				counter.onFrameArrive();

				//缩放、改变格式YUV420到BGR24
				sws_scale(swsContext, frame->data, frame->linesize, 0, frame->height, resizedFrame->data,
				          resizedFrame->linesize);

				//用OpenCV的Mat回调给注册的回调函数
				cv::Mat mat{targetHeight, targetWidth, CV_8UC3, resizedFrame->data[0]};
				if (onFrameCallback)
					onFrameCallback(mat);
			}
		}

		//输出到切片流outSlice
		outSliceMutex.lock();
		if (outSlice != nullptr)
		{
			AVPacket* slicePacket = av_packet_clone(packet);
			
			//首先要拿到关键帧，其次记录关键帧的PTS，对于在其之前的音频视频流进行丢弃防止音画不同步
			if (((slicePacket->flags & AV_PKT_FLAG_KEY) && (slicePacket->stream_index == videoStreamIndex)) ||
				outSliceVideoKeyFramePts != 0)
			{
				AVStream* inStream = in->streams[slicePacket->stream_index];
				AVStream* outStream = outSlice->streams[slicePacket->stream_index];

				slicePacket->dts = av_rescale_q(slicePacket->dts, inStream->time_base, outStream->time_base);
				slicePacket->pts = av_rescale_q(slicePacket->pts, inStream->time_base, outStream->time_base);
				slicePacket->duration = av_rescale_q(slicePacket->duration, inStream->time_base, outStream->time_base);
				slicePacket->pos = -1;

				outSliceVideoKeyFramePts = slicePacket->pts;

				if (slicePacket->pts >= outSliceVideoKeyFramePts)
				{
					ret = av_interleaved_write_frame(outSlice, slicePacket);
				}
			}
			av_packet_unref(slicePacket);
			av_packet_free(&slicePacket);
		}
		outSliceMutex.unlock();

		//转录原生流到本地out
		if (shouldSaveCopy)
		{
			AVStream* inStream = in->streams[packet->stream_index];
			AVStream* outStream = out->streams[packet->stream_index];

			packet->dts = av_rescale_q(packet->dts, inStream->time_base, outStream->time_base);
			packet->pts = av_rescale_q(packet->pts, inStream->time_base, outStream->time_base);
			packet->duration = av_rescale_q(packet->duration, inStream->time_base, outStream->time_base);
			packet->pos = -1;

			ret = av_interleaved_write_frame(out, packet);
		}

		//解引用packet，并不是释放
		av_packet_unref(packet);
	}

	//流断了释放资源
	av_packet_free(&packet);
	av_frame_free(&frame);
	av_frame_free(&resizedFrame);
	sws_freeContext(swsContext);
	avcodec_free_context(&codecContext);

	//关闭统计
	counter.end();

	//关闭流
	if (in != nullptr)
	{
		avformat_close_input(&in);
	}
	if (out != nullptr)
	{
		av_write_trailer(out);
		avio_closep(&out->pb);
		avformat_close_input(&out);
	}

	//停止切片流的输出
	endOutputSlice();

	if (ret == AVERROR_EOF)
	{
		std::cout << "取流结束" << std::endl;
	}
	else if (ret == 0)
	{
		std::cout << "取流中断" << std::endl;
	}
	else
	{
		std::cout << "取流错误: " << ret << " " << std::hex << ret << std::endl;
	}
}

std::string LivestreamRecorder::requestLivestreamUrl()
{
	//在子类实现
	assert(!file.empty());
	return file;
}

std::string LivestreamRecorder::generateSaveFilePath()
{
	return outputFolder + "/output_video/" + std::to_string(roomId) + "-" + std::to_string(timestamp) + ".flv";
}

std::string LivestreamRecorder::generateSliceFilePath(int from, int to)
{
	//生成一个切片的保存路径
	time_t slice = getTimestampMillisecond();
	std::filesystem::create_directories(outputFolder + "/output_slice/" + std::to_string(timestamp));
	if (from == 0 && to == 0)
	{
		return outputFolder + "/output_slice/" + std::to_string(timestamp) + "/" + std::to_string(slice) + ".flv";
	}
	return outputFolder + "/output_slice/" + std::to_string(timestamp) + "/" +
		std::to_string(from) + "-" + std::to_string(to) + " (" + std::to_string(slice) + ")" ".flv";
}

void LivestreamRecorder::startOutputSlice()
{
	//开始切片，思路是重开一条流直接输出
	endOutputSlice();
	const auto save = generateSliceFilePath();
	cout << "切片开始：" << save << endl;

	outSliceMutex.lock();
	avformat_alloc_output_context2(&outSlice, nullptr, "flv", save.c_str());
	avio_open2(&outSlice->pb, save.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr);
	for (int i = 0; i < in->nb_streams; i++)
	{
		AVStream* stream = avformat_new_stream(outSlice, nullptr);
		avcodec_parameters_copy(stream->codecpar, in->streams[i]->codecpar);
	}
	avformat_write_header(outSlice, nullptr);
	
	outSliceVideoKeyFramePts = 0;
	outSliceMutex.unlock();
}

std::string LivestreamRecorder::endOutputSlice(int from, int to)
{
	//停止切片并返回切片的文件名，讲真实现的很随意
	std::string ret;
	outSliceMutex.lock();
	outSliceVideoKeyFramePts = 0;
	if (outSlice != nullptr)
	{
		auto oldName = std::string(outSlice->url);
		cout << "切片结束：" << oldName << endl;
		av_write_trailer(outSlice);
		avio_closep(&outSlice->pb);
		avformat_close_input(&outSlice);
		outSlice = nullptr;

		if (from != 0 && to != 0)
		{
			auto newName = generateSliceFilePath(from, to);
			std::filesystem::rename(oldName, newName);
			cout << "重命名为：" << newName << endl;
			ret = newName;
		}
	}
	outSliceMutex.unlock();
	return ret;
}
