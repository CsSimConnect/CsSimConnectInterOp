#include "pch.h"
/*
 * Copyright (c) 2021. Bert Laverman
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <ctime>
#include <cctype>
#include <cstring>

#include <format>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>

#include <ranges>
#include <tuple>
#include <algorithm>

#include "Log.h"

using namespace nl::rakis;

namespace fs = std::filesystem;


constexpr const char* CFG_SEPARATOR{"."};
constexpr const char* CFG_ROOTLOGGER{"rootLogger"};
constexpr const char* CFG_FILENAME{"filename"};
constexpr const char* CFG_LOGGER{"logger"};

constexpr const char* CFG_STDOUT{"STDOUT"};
constexpr const char* CFG_STDERR{"STDERR"};

constexpr const char* CFG_LEVEL_TRACE{"TRACE"};
constexpr const char* CFG_LEVEL_DEBUG{"DEBUG"};
constexpr const char* CFG_LEVEL_INFO{"INFO"};
constexpr const char* CFG_LEVEL_WARN{"WARN"};
constexpr const char* CFG_LEVEL_ERROR{"ERROR"};
constexpr const char* CFG_LEVEL_FATAL{"FATAL"};
constexpr const char* CFG_LEVEL_INIT{"INIT"};


/*static*/ Logger::LoggerNode Logger::targets_(
	CFG_ROOTLOGGER,
	"",
	"",
	std::make_unique<std::ostream>(std::cerr.rdbuf()),
	LOGLVL_INFO
);

/*static*/ bool Logger::configDone_{ false };


inline std::string strip(const std::string& s) {
	auto first = s.find_first_not_of(" \t");\
	if (first == std::string::npos) {
		return std::string();
	}
	auto last = s.find_last_not_of(" \t");
	return s.substr(first, (last - first + 1));
}

inline std::string strip(const std::string_view& s) {
	auto first = s.find_first_not_of(" \t");\
	if (first == std::string::npos) {
		return std::string();
	}
	auto last = s.find_last_not_of(" \t");
	return std::string(s[first], s[(last - first + 1)]);
}

inline std::tuple<std::string, std::string> split(const std::string& s, const std::string& sep)
{
	auto pos = s.find(sep);
	if (pos == std::string::npos) {
		return std::make_tuple(s, std::string());
	}
	return std::make_tuple(strip(s.substr(0, pos)), strip(s.substr(pos + sep.length())));
}

static std::string join(std::vector<std::string>::const_iterator begin, std::vector<std::string>::const_iterator end, std::string sep)
{
	std::string result;

	while (begin != end) {
		if (!result.empty()) {
			result.append(sep);
		}
		result.append(*begin);
		begin++;
	}
	return result;
}

Logger::Logger(const std::string& name)
	: name_(name)
{
	level_ = getLevel(name);
}

Logger::~Logger()
{

}

Logger::Level nl::rakis::Logger::getLevel()
{
	if (level_ == LOGLVL_INIT) {
		// Check if we're still uninitialized
		if (configDone_) {
			level_ = getLevel(name_);
		}
	}
	return level_;
}


Logger::LoggerNode& getTarget(Logger::LoggerNode& root, std::string const& name, bool create = false) {
	auto [head, tail] = split(name, CFG_SEPARATOR);
	if (head.empty()) {
		return root;
	} else if (root.children.find(head) == root.children.end()) {
		if (create) {
			root.children[head] = Logger::LoggerNode(head, root.fullName + CFG_SEPARATOR + head);
		} else {
			return root;
		}
	}
	return getTarget(root.children[head], tail);
}

/*static*/ Logger::LoggerNode& Logger::getTarget(std::string const& name, bool create)
{
	return ::getTarget(targets_, name);
}


constexpr const char* LOGLVL_NAME[] = {
	CFG_LEVEL_INIT, CFG_LEVEL_TRACE, CFG_LEVEL_DEBUG, CFG_LEVEL_INFO, CFG_LEVEL_WARN, CFG_LEVEL_ERROR, CFG_LEVEL_FATAL
};

Logger::Level valueOf(const std::string& name)
{
	if (name == CFG_LEVEL_TRACE) {
		return Logger::LOGLVL_TRACE;
	}
	else if (name == CFG_LEVEL_DEBUG) {
		return Logger::LOGLVL_DEBUG;
	}
	else if (name == CFG_LEVEL_INFO) {
		return Logger::LOGLVL_INFO;
	}
	else if (name == CFG_LEVEL_WARN) {
		return Logger::LOGLVL_WARN;
	}
	else if (name == CFG_LEVEL_ERROR) {
		return Logger::LOGLVL_ERROR;
	}
	else if (name == CFG_LEVEL_FATAL) {
		return Logger::LOGLVL_FATAL;
	}
	return Logger::LOGLVL_INIT;
}

/*static*/ void Logger::configure(std::string const& configFile)
{
	std::filesystem::path file(configFile);
	if (std::filesystem::exists(file)) {
		std::ifstream cfg(file);

		if (!cfg) {
			root() << std::format("Cannot open configuration file '{}'\n", configFile);
			return;
		}

		for (auto line : std::ranges::istream_view<std::string>(cfg) | std::views::transform([](std::string s) { return strip(s); }) | std::views::filter([](std::string s) { return !s.empty(); })) {
			if ((line[0] == '#') || (line[0] == ';')) {
				continue;
			}

			auto [name, value] = split(line, "=");

			if (name.empty() || value.empty()) {
				root() << std::format("Ignoring line '{}' in '{}'\n", line, configFile);
				continue;
			}

			if (name == CFG_ROOTLOGGER) {
				if (value.empty()) {
					root() << "Ignoring empty value for 'rootLogger'\n";
					continue;
				}
				auto [level, target] = split(value, ",");
				targets_.level = valueOf(level);
				if (!target.empty()) {
					targets_.filename = target;
					targets_.stream = std::make_unique<std::ofstream>(target, std::ios_base::out | std::ios_base::app);
				}
				continue;
			}
			auto& node = getTarget(name, true);
			auto [level, target] = split(value, ",");
			node.level = valueOf(level);
			if (!target.empty()) {
				node.filename = target;
				node.stream = std::make_unique<std::ofstream>(target, std::ios_base::out | std::ios_base::app);
			}
		}
		configDone_ = true;
		root() << std::format("Logging initialized. Root log threshold '{}'\n", LOGLVL_NAME [targets_.level]) << std::flush;
		Logger logger{ getLogger("CsSimConnectInterOp") };
		root() << std::format("getLevel(): '{}', trace is {}\n", LOGLVL_NAME[logger.getLevel()], logger.isTraceEnabled() ? "enabled" : "disabled") << std::flush;
	}
}

static void num(std::ostream& s, int i)
{
	if (i >= 100) {
		s << char('0' + ((i / 1000) % 10)) << char('0' + ((i / 100) % 10)) << char('0' + ((i / 10) % 10)) << char('0' + (i % 10));
	}
	else {
		s << char('0' + ((i / 10) % 10)) << char('0' + (i % 10));
	}
}

void Logger::startLine(std::ostream& s, Level level)
{
	time_t tt(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
	tm ti;

	localtime_s(&ti, &tt);

	num(s, ti.tm_year + 1900); s << '-'; num(s, ti.tm_mon + 1); s << '-'; num(s, ti.tm_mday); s << " ";
	num(s, ti.tm_hour); s << ':'; num(s, ti.tm_min); s << ':'; num(s, ti.tm_sec);
	s << std::format(" [{:<5}] {} ", LOGLVL_NAME [level], name_);
}
