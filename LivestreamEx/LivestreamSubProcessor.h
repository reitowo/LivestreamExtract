#pragma once 
#include <nlohmann/json.hpp>

using namespace nlohmann;

class LivestreamSubProcessor
{
protected:
	std::atomic_bool working;
public:
	virtual ~LivestreamSubProcessor() = default;
	virtual void onSerialize(json&) = 0;
	virtual void start(time_t timestamp) = 0;
	virtual void stop() = 0;
};