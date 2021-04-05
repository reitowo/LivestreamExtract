#pragma once

#include <string>
#include <functional>
#include <chrono>

struct ChatMessage
{
	time_t time;
	std::string username;
	std::string content;
	int level;
};

class LivestreamDanmu
{
protected:
	int room = 0;
	std::function<void(ChatMessage*)> onChatMessageCallback = nullptr;

public:
	LivestreamDanmu() = default;
	virtual ~LivestreamDanmu() = default;
	LivestreamDanmu(const LivestreamDanmu&) = default;
	LivestreamDanmu& operator=(const LivestreamDanmu&) = default;
	LivestreamDanmu(LivestreamDanmu&&) = default;
	LivestreamDanmu& operator=(LivestreamDanmu&&) = default;

	virtual void connect(int room);
	virtual void disconnect() = 0;
	virtual void onChatMessage(std::function<void(ChatMessage*)> callback);
};
