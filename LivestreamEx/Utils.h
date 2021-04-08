#pragma once

#include <vector>
#include <string>
#include <regex>
#include <functional>
#include <nlohmann/json.hpp>

using std::string;
using std::vector;

#define CP_UTF8 65001
#define CP_GB18030 54936
 
template <typename T>
std::string toStringPrecision(const T a_value, const int n = 6);
std::string getPrettyByteSize(uint64_t bytes);
void setupClosingEvent(std::function<int(int)> callback);
time_t getTimestampMillisecond();
std::string readAllTexts(std::string path);
std::string str_cvtcode(std::string bytes, uint32_t from = CP_UTF8, uint32_t to = CP_GB18030); 
std::wstring str_towstr(std::string bytes, uint32_t encoding = CP_UTF8); 
std::string str_replace(string str, string from, string to); 
bool str_contains(string str, string value); 
vector<string> str_split(string str, string separator);
 
std::string jsonToString(nlohmann::json json);
void notifyMessage(std::string message);