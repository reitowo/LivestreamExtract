#pragma once

#include "timercpp.h"
#include "LivestreamDanmu.h"
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

enum ChatMessageColorDouyu
{
	White,
	Red,
	Blue,
	Green,
	Yellow,
	Purple,
	Pink
};

struct ChatMessageDouyu : ChatMessage
{
	ChatMessageColorDouyu color = White;
};

struct DouyuWebsocketHeader
{
	uint32_t len1;
	uint32_t len2;
	uint16_t val1;
	uint16_t val2;

	DouyuWebsocketHeader(uint32_t len)
	{
		len1 = len;
		len2 = len;
		val1 = 689;
		val2 = 0;
	}
};

class LivestreamDanmuDouyu final : public LivestreamDanmu
{
	ix::WebSocket ws;
	Timer heartbeatTimer;

	std::string rxBuffer;
	uint64_t rxLen;

	void login();
	void joinGroup();
	void heartbeat();

	void send(json obj);
	std::string escape(std::string str);
	std::string unescape(std::string str);
	std::string serialize(json obj);
	json deserialize(std::string str);
	std::string encode(std::string str);
	void decode(std::string str);

	void onOpen(const ix::WebSocketMessagePtr& msg);
	void onClose(const ix::WebSocketMessagePtr& msg);
	void onError(const ix::WebSocketMessagePtr& msg);
	void onMessage(const ix::WebSocketMessagePtr& msg);

public:
	LivestreamDanmuDouyu();
	void connect(int room) override;
	void disconnect() override;
};
