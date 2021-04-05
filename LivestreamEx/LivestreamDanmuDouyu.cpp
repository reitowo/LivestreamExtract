#include <random>
#include <iostream>
#include <vector>
#include "Utils.h"
#include "LivestreamDanmuDouyu.h"

#pragma comment (lib, "crypt32")

void LivestreamDanmuDouyu::login()
{
	json obj;
	obj["type"] = "loginreq";
	obj["roomid"] = room;
	send(obj);
}

void LivestreamDanmuDouyu::joinGroup()
{
	json obj;
	obj["type"] = "joingroup";
	obj["rid"] = room;
	obj["gid"] = -9999;
	send(obj);
}

void LivestreamDanmuDouyu::heartbeat()
{
	heartbeatTimer.setInterval([this]()
	{
		json obj;
		obj["type"] = "mrkl";
		send(obj);
	}, 45000);
}

void LivestreamDanmuDouyu::send(json obj)
{
	ws.send(encode(serialize(obj)), true);
}

std::string LivestreamDanmuDouyu::escape(std::string str)
{
	str = str_replace(str, "@", "@A");
	str = str_replace(str, "/", "@S");
	return str;
}

std::string LivestreamDanmuDouyu::unescape(std::string str)
{
	str = str_replace(str, "@S", "/");
	str = str_replace(str, "@A", "@");
	return str;
}

std::string LivestreamDanmuDouyu::serialize(json obj)
{
	using nlohmann::detail::value_t;
	std::string ret;
	switch (obj.type())
	{
	case value_t::object:
		for (auto& element : obj.items())
		{
			ret += element.key() + "@=" + escape(serialize(element.value())) + "/";
		}
		break;
	case value_t::array:
		for (auto& element : obj)
		{
			ret += escape(serialize(element)) + "/";
		}
		break;
	case value_t::string:
		return obj.get<std::string>();
	default:
		return obj.dump();
	}
	return ret;
}

json LivestreamDanmuDouyu::deserialize(std::string str)
{
	if (str_contains(str, "//"))
	{
		json jarr = json::array();
		std::vector<std::string> splits = str_split(str, "//");
		for (auto& s : splits)
		{
			if (!s.empty())
			{
				jarr.push_back(deserialize(s));
			}
		}
		return jarr;
	}

	if (str_contains(str, "@="))
	{
		json jobj = json::object();
		std::vector<std::string> splits = str_split(str, "/");
		for (auto& s : splits)
		{
			if (!s.empty())
			{
				std::vector<std::string> kv = str_split(s, "@=");
				if (kv.size() == 2)
					jobj[kv[0]] = deserialize(unescape(kv[1]));
			}
		}
		return jobj;
	}

	if (str_contains(str, "@A="))
	{
		return deserialize(unescape(str));
	}

	return str;
}

std::string LivestreamDanmuDouyu::encode(std::string str)
{
	int len = str.size() + 9;
	DouyuWebsocketHeader header(len);
	std::string ret(reinterpret_cast<const char*>(&header), sizeof(DouyuWebsocketHeader));
	ret.append(str);
	ret.append(1, '\0');
	return ret;
}

void LivestreamDanmuDouyu::decode(std::string str)
{
	json obj = deserialize(str);
	std::string type = obj["type"];
	if (type == "chatmsg")
	{
		static time_t lastMessageTime = 0;
		ChatMessageDouyu dmsg;
		//dmsg.username = str_cvtcode(obj["nn"]);
		//dmsg.content = str_cvtcode(obj["txt"]);
		dmsg.username = obj["nn"];
		dmsg.content = obj["txt"];
		dmsg.time = obj.contains("cst") ? std::stoull(obj["cst"].get<std::string>()) : lastMessageTime;
		dmsg.level = std::stoi(obj["level"].get<std::string>());
		if (obj.contains("col"))
			dmsg.color = static_cast<ChatMessageColorDouyu>(std::stoi(obj["col"].get<std::string>()));
		lastMessageTime = dmsg.time;
		onChatMessageCallback(&dmsg);
	}
	else if(type == "rss")
	{
		std::cout << "来自弹幕系统的开播状态 " << obj["ss"] << obj["code"] << std::endl;
	}
}

void LivestreamDanmuDouyu::onOpen(const ix::WebSocketMessagePtr& msg)
{
	std::cout << "弹幕连接成功" << std::endl;
	login();
	joinGroup();
	heartbeat();
}

void LivestreamDanmuDouyu::onClose(const ix::WebSocketMessagePtr& msg)
{
	std::cout << "弹幕连接关闭" << std::endl;
}

void LivestreamDanmuDouyu::onError(const ix::WebSocketMessagePtr& msg)
{
	std::cout << "弹幕连接错误" << std::endl;
}

void LivestreamDanmuDouyu::onMessage(const ix::WebSocketMessagePtr& msg)
{
	rxBuffer = rxBuffer.append(msg->str);
	while (!rxBuffer.empty())
	{
		if (rxLen == 0)
		{
			if (rxBuffer.size() < 4)
				return;
			rxLen = *reinterpret_cast<int*>(const_cast<char*>(rxBuffer.c_str()));
			rxBuffer = rxBuffer.substr(4);
		}

		if (rxBuffer.size() < rxLen)
			return;

		string message = rxBuffer.substr(8, rxLen - 9);
		rxBuffer = rxBuffer.substr(rxLen);
		decode(message);
		rxLen = 0;
	}
}

LivestreamDanmuDouyu::LivestreamDanmuDouyu()
{
	rxLen = 0;
	ix::initNetSystem();
	std::random_device rd;
	std::mt19937_64 gen(rd());
	std::uniform_int_distribution<> distribution(8501, 8506);
	std::string url = "wss://danmuproxy.douyu.com:" + std::to_string(distribution(gen));

	ws.setUrl(url);
	ws.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg)
	{
		switch (msg->type)
		{
		case ix::WebSocketMessageType::Message:
			onMessage(msg);
			break;
		case ix::WebSocketMessageType::Open:
			onOpen(msg);
			break;
		case ix::WebSocketMessageType::Close:
			onClose(msg);
			break;
		case ix::WebSocketMessageType::Error:
			onError(msg);
			break;
		}
	});
}

void LivestreamDanmuDouyu::connect(int room)
{
	LivestreamDanmu::connect(room);
	ws.start();
}

void LivestreamDanmuDouyu::disconnect()
{
	ws.stop();
}
