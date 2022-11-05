#pragma once

#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <string_view>
#include <string>
#include <vector>

using std::cout;
using std::endl;
using std::string;
using std::string_view;
namespace fs = std::filesystem;
using namespace std::string_literals;
using namespace std::string_view_literals;

#define CPPHTTPLIB_BROTLI_SUPPORT
#define CPPHTTPLIB_ZLIB_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>

#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>
using njson = nlohmann::json;

#include <date/date.h>
#include <date/tz.h>