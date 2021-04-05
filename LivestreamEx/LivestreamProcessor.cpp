#include "LivestreamProcessor.h"
 
#include <filesystem>
#include "Utils.h" 
#include "../common.h"

json LivestreamProcessor::serialize()
{
	json s;
	s["room_id"] = roomId;
	s["video_path"] = std::filesystem::absolute(recordingFilePath).string();
	return s;
}

LivestreamProcessor::~LivestreamProcessor()
{
	for(auto sub : subProcessors)
	{
		delete sub;
	}
}

void LivestreamProcessor::start(std::string recordingFile)
{ 
	time_t timestamp = getTimestampMillisecond(); 
		 
	std::filesystem::create_directories(outputFolder + "/output_live");
	liveFilePath = outputFolder + "/output_live/" + std::to_string(timestamp) + ".json";
	recordingFilePath = recordingFile;  

	for(auto* sub : subProcessors)
	{
		sub->start(timestamp);
	}
	
	working = true;
}

std::string LivestreamProcessor::stop()
{
	if (!working)
		return std::string(); 
	 
	json root = serialize();
	for(auto* sub : subProcessors)
	{
		sub->onSerialize(root);
	}
	std::ofstream proc(liveFilePath);
	proc << root;
	proc.close(); 
	
	for(auto* sub : subProcessors)
	{
		sub->stop();
	}
	
	working = false;

	return liveFilePath;
}
