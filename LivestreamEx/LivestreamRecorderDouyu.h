#pragma once

#include <v8.h>

#include "LivestreamRecorder.h"
#include <cpr/cpr.h>
#include <libplatform/libplatform.h>


#include "../common.h"

class LivestreamRecorderDouyu : public LivestreamRecorder
{
	std::string cookie;
	std::string didToken;

public: 
	void setCookie(std::string cookie);
	std::string requestLivestreamUrl() override;
};
