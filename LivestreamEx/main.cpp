#include <iostream>
#include <v8.h>
#include <CLI/CLI.hpp>
#include <libplatform/libplatform.h>
#include <boost/filesystem.hpp>

#include "LivestreamDanmuDouyu.h"
#include "LivestreamRecorderDouyu.h"
#include "LivestreamExtractor.h"
#include "Utils.h"
#include <opencv2/opencv.hpp>
 
#include "LivestreamProcessor.h"
#include "SubProcessorDanmuDouyu.h"
#include "SubProcessorSMM2.h"
#include "../common.h"
#include <k1ee/encoding.h>

using std::cout;
using std::endl;

int main(int argc, char** argv)
{
	//程序用了UTF8编码，调试不方便的话在字符串变量后面加",s8"，如str,s8就可以解析UTF8内存字符串了
	SetConsoleCP(65001);
	SetConsoleOutputCP(65001);
	
	auto* danmu = new LivestreamDanmuDouyu;
	auto* recorder = new LivestreamRecorderDouyu;
	auto* extractor = new LivestreamExtractor;
	auto* processor = new LivestreamProcessor;
	 
	//V8引擎初始化，只是用来执行斗鱼的Javascript签名代码
	v8::V8::InitializeICUDefaultLocation(argv[0]);
	v8::V8::InitializeExternalStartupData(argv[0]);
	std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
	v8::V8::InitializePlatform(platform.get());
	v8::V8::Initialize(); 

	//捕获Ctrl+C、关闭等事件，实际上在这退出
	setupClosingEvent([=](int val) 
	{
		processor->stop();
		recorder->stop();
		danmu->disconnect();
		v8::V8::Dispose();
		v8::V8::ShutdownPlatform(); 
		exit(0);
		return 0;
	});

	//随便试了试CLI这个库
	CLI::App app{"Livestream Interest Extractor"};

	int roomId = 74751;
	app.add_option("--room", roomId, "Livestreamer Room ID", true);

	int rate = 0;
	app.add_option("--rate", rate, "Livestreamer Video Rate", true);

	CLI11_PARSE(app, argc, argv)

	cout << "房间号 " << roomId << endl;

	//连接弹幕服务器
	danmu->connect(roomId);

	//设置提取器读取匹配图像的文件夹，工作目录请去common.h配置
	extractor->setMatchImageFolder(matchMatFolderPath, roomId);

	//告诉处理总模块房间号
	processor->setRoomId(roomId);

	//添加处理模块
	processor->addSubProcessor<SubProcessorDanmuDouyu>()->initialize(danmu);
	processor->addSubProcessor<SubProcessorSMM2>()->initialize(extractor, recorder);

	//注册录制器每一帧的回调，传给提取器
	recorder->onFrame([&](cv::Mat& mat)
	{
		extractor->process(mat);
	});

	//设置录制器输出图像大小
	recorder->setFrameMatSize(1920, 1080);

	//设置（斗鱼）录制器请求链接的Cookie
	recorder->setCookie(readAllTexts(workingFolder + std::string("/douyu-cookie.txt")));

	//主播的流是否BT.709颜色空间，用处似乎不大？
	recorder->setBt709Input(false);

	//便捷本地测试，本地状态下，不会复制输入流保存
#define LIVESTREAM_LOCALTEST 0
#if LIVESTREAM_LOCALTEST
	std::cout << "本地测试" << std::endl; 
	recorder->start(workingFolder + "/output_video/510541-1616605047675.flv");
	processor->start(recorder->getSaveFilePath());
	recorder->waitForStop(); 
	processor->stop();
	std::this_thread::sleep_until(std::chrono::system_clock::time_point::max());
#endif

	//实际运行
	while (true)
	{
		//直到连接成功
		recorder->startUntilSuccess(roomId, 20);
		notifyMessage(std::to_string(roomId) + " 主播开播");
		//处理模块启动
		processor->start(recorder->getSaveFilePath());
		//直到录制结束
		recorder->waitForStop();
		notifyMessage(std::to_string(roomId) + " 主播下播");
		//处理模块关闭，序列化所有子模块
		std::string live = processor->stop();
		std::cout << "录制结束，进入后处理" << std::endl;
		 
		//生成SMM2分数图
		//system((R"(python ")" + pythonScriptsPath + "/smm2RatingChart.py" + R"(" ")" + live + R"(" "upper left")").c_str());
		//system((R"(python ")" + pythonScriptsPath + "/smm2RatingChart.py" + R"(" ")" + live + R"(" "lower left")").c_str());
		
		std::cout << "处理结束，重新等待" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds::duration(10));
	}
}
