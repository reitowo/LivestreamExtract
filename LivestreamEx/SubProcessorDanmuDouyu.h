#pragma once 

#include "LivestreamDanmuDouyu.h"
#include "LivestreamSubProcessor.h"

class SubProcessorDanmuDouyu : public LivestreamSubProcessor
{
	int danmuCount = 0;
	json danmuJson;
	std::string danmuFilePath;
public:
	void initialize(LivestreamDanmuDouyu* dm);
	void danmu(ChatMessageDouyu* msg);
	void onSerialize(json&) override;
	void start(time_t timestamp) override;
	void stop() override;
};
