#include "pch.h"
/*
 * Copyright (c) 2021-2024. Bert Laverman
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

using namespace nl::rakis::logging;

namespace fs = std::filesystem;



/*static*/ std::string Logger::formatLine(LogLevel level, const std::string name, const std::string msg)
{
	time_t tt(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
	tm ti;

	localtime_s(&ti, &tt);

	return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02} [{:<5}] {} {}",
		ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
		ti.tm_hour, ti.tm_min, ti.tm_sec,
		LOGLVL_NAME[level],
		name, msg);
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
	level_ = configuration().getLevel(name);
}


LogLevel Logger::getLevel()
{
	if (level_ == LOGLVL_INIT) {
		level_ = configuration().getLevel(name_);
	}
	return level_;
}

static LogLevel valueOf(const std::string& name)
{
	if (name == CFG_LEVEL_TRACE) {
		return LOGLVL_TRACE;
	}
	else if (name == CFG_LEVEL_DEBUG) {
		return LOGLVL_DEBUG;
	}
	else if (name == CFG_LEVEL_INFO) {
		return LOGLVL_INFO;
	}
	else if (name == CFG_LEVEL_WARN) {
		return LOGLVL_WARN;
	}
	else if (name == CFG_LEVEL_ERROR) {
		return LOGLVL_ERROR;
	}
	else if (name == CFG_LEVEL_FATAL) {
		return LOGLVL_FATAL;
	}
	return LOGLVL_INIT;
}


/*static*/ void Configurer::logRoot(LogLevel level, const std::string& msg)
{
	std::cerr << Logger::formatLine(level, CFG_ROOTLOGGER, msg) << std::endl;
}

/*static*/ void Configurer::configure(std::string const& configFile)
{
	std::filesystem::path file(configFile);
	if (std::filesystem::exists(file)) {
		std::ifstream cfg(file);

		if (!cfg) {
			Configurer::rootLogger(LOGLVL_ERROR, std::format("Cannot open configuration file '{}'\n", configFile));
			return;
		}

		for (auto line : std::ranges::istream_view<std::string>(cfg) | std::views::transform([](std::string s) { return strip(s); }) | std::views::filter([](std::string s) { return !s.empty(); })) {
			if ((line[0] == '#') || (line[0] == ';')) {
				continue;
			}

			auto [name, value] = split(line, "=");

			if (name.empty() || value.empty()) {
				Configurer::rootLogger(LOGLVL_ERROR, std::format("Ignoring line '{}' in '{}'\n", line, configFile));
				continue;
			}

			if (name == CFG_ROOTLOGGER) {
				if (value.empty()) {
					Configurer::rootLogger(LOGLVL_ERROR, "Ignoring empty value for 'rootLogger'\n");
					continue;
				}
				auto [level, target] = split(value, ",");
				targets().level_ = valueOf(level);
				if (!target.empty()) {
					targets().filename_ = target;
					targets().logger_ = [target, name](LogLevel level, const std::string& msg) mutable {
						auto f = std::ofstream(target, std::ios_base::out | std::ios_base::app);
						f << Logger::formatLine(level, name, msg) << std::endl;
					};
				}
				continue;
			}
			auto& node = getTarget(name, true);
			auto [level, target] = split(value, ",");
			node.level_ = valueOf(level);
			if (!target.empty()) {
				node.filename_ = target;
				node.logger_ = [target, name](LogLevel level, const std::string& msg) mutable {
					auto f = std::ofstream(target, std::ios_base::out | std::ios_base::app);
					f << Logger::formatLine(level, name, msg) << std::endl;
				};
			}
		}
		configDone() = true;
		rootLogger(LOGLVL_ERROR, std::format("Logging initialized. Root log threshold '{}'\n", LOGLVL_NAME [targets().level_]));
		Logger logger{ Logger::getLogger("CsSimConnectInterOp") };
		rootLogger(LOGLVL_ERROR, std::format("getLevel(): '{}', trace is {}\n", LOGLVL_NAME[logger.getLevel()], logger.isTraceEnabled() ? "enabled" : "disabled"));
	}
}
