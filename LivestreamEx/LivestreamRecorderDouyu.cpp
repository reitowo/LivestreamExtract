#include "LivestreamRecorderDouyu.h"

#include "Utils.h"
#include <boost/regex.hpp>
#include <nlohmann/json.hpp>
#include <libplatform/libplatform.h>
#include <v8.h>
#include <iostream>

#include "../common.h"

using nlohmann::json;

#define LIVESTREAM_RECORDER_DOUYU_USE_PROXY 0

void LivestreamRecorderDouyu::setCookie(std::string cookie)
{
	this->cookie = cookie;
	boost::regex regex(R"(dy_did ?= ?([^,; ]+))", boost::regex_constants::JavaScript);
	boost::smatch match;
	if (!boost::regex_search(cookie, match, regex))
		return;
	didToken = match[1].str();
}

std::string LivestreamRecorderDouyu::requestLivestreamUrl()
{
	cpr::Response resp = cpr::Get(
		cpr::Url("https://www.douyu.com/" + std::to_string(roomId))
#if LIVESTREAM_RECORDER_DOUYU_USE_PROXY
					,
					cpr::Proxies{{"https", "http://127.0.0.1:8888"}, {"http", "http://127.0.0.1:8888"}},
					cpr::VerifySsl{false}
#endif
	);
	std::string roomPage = resp.text;

	boost::regex regex(R"(\$ROOM.show_status ?= ?([0-9]+);)", boost::regex_constants::JavaScript);
	boost::smatch match;
	if (!boost::regex_search(roomPage, match, regex))
	{
		std::cerr << "获取不到房间直播状态" << std::endl;
		return std::string();
	}

	int status = stoi(match[1].str());
	if (status != 1)
	{
		std::cerr << "直播间没有开启：" << status << std::endl;
		return std::string();
	}

	resp = cpr::Get(
		cpr::Url("https://www.douyu.com/wgapi/live/liveweb/getRoomLoopInfo?rid=" + std::to_string(roomId))
#if LIVESTREAM_RECORDER_DOUYU_USE_PROXY
					,
					cpr::Proxies{{"https", "http://127.0.0.1:8888"}, {"http", "http://127.0.0.1:8888"}},
					cpr::VerifySsl{false}
#endif
	);

	json record = json::parse(resp.text);
	string vid = record["data"]["vid"];
	if (!vid.empty())
	{
		std::cerr << "房间正在轮播" << std::endl;
		return std::string();
	}

	size_t scriptStart = roomPage.find("var vdwdae325w_64we");
	size_t scriptEnd = roomPage.find("</script>", scriptStart);
	std::string signScript = roomPage.substr(scriptStart, scriptEnd - scriptStart);
	std::string cryptoScript = readAllTexts(workingFolder + "/crypto-js.min.js");

	using namespace v8;
	std::string sign;

	Isolate::CreateParams create_params;
	create_params.array_buffer_allocator = ArrayBuffer::Allocator::NewDefaultAllocator();
	Isolate* isolate = Isolate::New(create_params);
	{
		Isolate::Scope isolate_scope(isolate);
		HandleScope handle_scope(isolate);
		Local<Context> context = Context::New(isolate);
		Context::Scope context_scope(context);

		Local<String> source = String::NewFromUtf8(isolate, signScript.c_str(), NewStringType::kNormal).
			ToLocalChecked();
		Local<Script> script = Script::Compile(context, source).ToLocalChecked();
		Local<Value> result = script->Run(context).ToLocalChecked();

		source = String::NewFromUtf8(isolate, cryptoScript.c_str(), NewStringType::kNormal).
			ToLocalChecked();
		script = Script::Compile(context, source).ToLocalChecked();
		result = script->Run(context).ToLocalChecked();

		Local<Object> global = context->Global();
		Local<Function> signFunc = global->Get(
			                                   context,
			                                   String::NewFromUtf8(
				                                   isolate, "ub98484234",
				                                   NewStringType::kNormal).ToLocalChecked()).
		                                   ToLocalChecked().As<Function>();
		Local<Value> args[3] = {
			String::NewFromUtf8(isolate, std::to_string(roomId).c_str(), NewStringType::kNormal).
			ToLocalChecked(),
			String::NewFromUtf8(isolate, didToken.c_str(), NewStringType::kNormal).ToLocalChecked(),
			BigInt::New(isolate, getTimestampMillisecond() / 1000)
		};
		Local<String> signResult = signFunc->Call(context, Null(isolate), 3, args).ToLocalChecked().As<
			String>();
		// Convert the result to an UTF8 string and print it.
		v8::String::Utf8Value utf8(isolate, signResult);
		printf("签名 %s\n", *utf8);
		sign = std::string(*utf8);
	}
	// Dispose the isolate and tear down V8.
	isolate->Dispose();
	delete create_params.array_buffer_allocator;

	if (sign.empty())
	{
		std::cerr << "签名失败" << std::endl;
		return std::string();
	}

	sign += "&cdn=&rate=0&ver=Douyu_219052705&iar=0&ive=1";

	resp = cpr::Post(
		cpr::Url{"https://www.douyu.com/lapi/live/getH5Play/" + std::to_string(roomId)},
		cpr::Body{sign},
		cpr::Header{
			{"Content-Type", "application/x-www-form-urlencoded"},
			{"Accept", "application/json, text/plain, */*"},
			{"Accept-Encoding", "gzip, deflate, sdch, br"},
			{"X-Requested-With", "XMLHttpRequest, ShockwaveFlash/28.0.0.137"},
			{"Accept-Language", "zh-CN,zh;q=0.8"},
			{"Origin", "https://www.douyu.com"},
			{"Referer", "https://www.douyu.com/topic/xyb01?rid=" + std::to_string(roomId)},
			{"Cookie", cookie},
			{
				"User-Agent",
				"Mozilla/5.0 (Windows NT 10.0; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36 SE 2.X MetaSr 1.0"
			}
		}
#if LIVESTREAM_RECORDER_DOUYU_USE_PROXY
					,
					cpr::Proxies{{"https", "http://127.0.0.1:8888"}, {"http", "http://127.0.0.1:8888"}},
					cpr::VerifySsl{false}
#endif
	);

	json live = json::parse(resp.text);
	if (live["error"].get<int>() != 0)
	{
		std::cerr << "取流错误" << std::endl;
		return std::string();
	}
	
	std::string liveHost = live["data"]["rtmp_url"];
	std::string livePath = live["data"]["rtmp_live"];
	std::string liveUrl = liveHost + "/" + livePath;
	int rate = live["data"]["rate"];

	std::cout << "获取到清晰度 " << rate << std::endl;
	std::cout << "链接 " << liveUrl << std::endl;

	return liveUrl;
}
