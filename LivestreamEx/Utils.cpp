#include "Utils.h"

#include <Windows.h>
#include <stringapiset.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <functional>
#include <cpr/cpr.h>
 
template <typename T>
std::string toStringPrecision(const T a_value, const int n)
{
    std::ostringstream out;
    out.precision(n);
    out << std::fixed << a_value;
    return out.str();
}

std::string getPrettyByteSize(uint64_t bytes)
{
	static std::string suffixs[] = {"B", "KB", "MB", "GB"};
	int suffixIndex = 0;
	double dbytes = bytes;
	while(dbytes >= 1024 && suffixIndex <= 3)
	{
		dbytes /= 1024.0;
		suffixIndex++;
	}
	return toStringPrecision(dbytes, 2) + " " + suffixs[suffixIndex];
}

static std::function<int(int)> closingEventCallback;
static int closingEventHandler(unsigned long e)
{
	if(closingEventCallback)
		return closingEventCallback(e);
	return 0;
}
void setupClosingEvent(std::function<int(int)> callback)
{
	SetConsoleCtrlHandler(closingEventHandler, true);
	closingEventCallback = callback;
}

time_t getTimestampMillisecond()
{
	using namespace std::chrono;
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string readAllTexts(std::string path)
{
	std::ifstream t(path);
	std::stringstream buffer;
	buffer << t.rdbuf();
	return buffer.str();
}

std::string str_cvtcode(std::string bytes, uint32_t from, uint32_t to)
{
	int from_size = MultiByteToWideChar(from, 0, bytes.c_str(), bytes.size(), nullptr, 0);
	wchar_t* from_bytes = new wchar_t[from_size];
	int ret = MultiByteToWideChar(from, 0, bytes.c_str(), bytes.size(), from_bytes, from_size);
	int to_size = WideCharToMultiByte(to, 0, from_bytes, from_size, nullptr, 0, nullptr, nullptr);
	char* to_bytes = new char[to_size];
	ret = WideCharToMultiByte(to, 0, from_bytes, from_size, to_bytes, to_size, nullptr, nullptr);
	string str(to_bytes, to_size);
	delete[] to_bytes;
	delete[] from_bytes;
	return str;
}

std::wstring str_towstr(std::string bytes, uint32_t encoding)
{
	int from_size = MultiByteToWideChar(encoding, 0, bytes.c_str(), bytes.size(), nullptr, 0);
	wchar_t* from_bytes = new wchar_t[from_size];
	int ret = MultiByteToWideChar(encoding, 0, bytes.c_str(), bytes.size(), from_bytes, from_size);
	std::wstring str(from_bytes, from_size);
	delete[] from_bytes;
	return str;
}

std::string str_replace(string str, string from, string to)
{
	return std::regex_replace(str, std::regex("\\" + from), to);
}

bool str_contains(string str, string value)
{
	return str.find(value) != string::npos;
}

vector<string> str_split(string str, string separator)
{
	vector<string> ret;

	size_t pos = 0;
	while (true)
	{
		size_t sep = str.find(separator, pos);
		if (sep == string::npos)
		{
			string sub = str.substr(pos);
			ret.push_back(sub);
			break;
		}

		string sub = str.substr(pos, sep - pos);
		ret.push_back(sub);
		pos = sep + separator.size();
	}

	return ret;
} 

std::string jsonToString(nlohmann::json json)
{
	std::stringstream ss;
	ss << json;
	return ss.str();
}

#define UTILS_PUSH_PROXY 0
void notifyMessage(std::string message)
{
	using nlohmann::json;
	message = str_cvtcode(message, CP_GB18030, CP_UTF8);
	json j;
	j["platform"] = "android";
	json a;
	a["registration_id"] = json::array({"170976fa8a21d4328bc"});
	j["audience"] = a;
	json android;
	android["title"] = "Livestream Extractor";
	android["alert"] = message;
	json n;
	n["android"] = android;
	j["notification"] = n;
	std::string jstr = jsonToString(j);
	cpr::Response resp = cpr::Post(
		cpr::Url("https://api.jpush.cn/v3/push"),
		cpr::Authentication("8843781c633a06f5c8180c47", "fa79cf8a181cfd9e93be7afb"),
		cpr::Header{
			{"Content-Type", "application/json"}
		},
		cpr::Body{jstr}
#if UTILS_PUSH_PROXY
					,
					cpr::Proxies{{"https", "http://127.0.0.1:8888"}, {"http", "http://127.0.0.1:8888"}},
					cpr::VerifySsl{false}
#endif
	);
}
