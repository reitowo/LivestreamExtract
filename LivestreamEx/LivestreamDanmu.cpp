#include "LivestreamDanmu.h"

void LivestreamDanmu::connect(int room)
{
	this->room = room;
}

void LivestreamDanmu::onChatMessage(std::function<void(ChatMessage*)> callback)
{
	this->onChatMessageCallback = std::move(callback);
}
