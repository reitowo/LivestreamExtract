#include "SubProcessorDanmuDouyu.h"

#include <filesystem>
#include <fstream>  

#include "../common.h"

void SubProcessorDanmuDouyu::initialize(LivestreamDanmuDouyu* dm)
{
	dm->onChatMessage([=](ChatMessage* msg)
	{
		danmu(reinterpret_cast<ChatMessageDouyu*>(msg));
	});
}

void SubProcessorDanmuDouyu::danmu(ChatMessageDouyu* msg)
{
	if (!working)
		return;

	json::object_t m = json::object_t();
	m["username"] = msg->username;
	m["content"] = msg->content;
	m["color"] = static_cast<int>(msg->color);
	m["level"] = msg->level;
	m["timestamp"] = msg->time;
	danmuJson.push_back(m);
	danmuCount++;

	//Backup every 50 danmu
	if (danmuCount % 50 == 0)
	{   
		std::ofstream danmu(danmuFilePath);
		danmu << danmuJson.dump(4, ' ', false, nlohmann::detail::error_handler_t::ignore);
		danmu.close();
	}
}

void SubProcessorDanmuDouyu::onSerialize(json& root)
{
	root["danmu_file"] = std::filesystem::absolute(danmuFilePath).string();
}

void SubProcessorDanmuDouyu::start(time_t timestamp)
{
	danmuFilePath = outputFolder + "/output_danmu/" + std::to_string(timestamp) + ".json";
	std::filesystem::create_directories(outputFolder + "/output_danmu");
	working = true;
}

void SubProcessorDanmuDouyu::stop()
{
	std::ofstream danmu(danmuFilePath);
	danmu << danmuJson.dump(4, ' ', false, nlohmann::detail::error_handler_t::ignore);
	danmu.close();

	danmuCount = 0;
	danmuJson.clear();
	working = false;
}
