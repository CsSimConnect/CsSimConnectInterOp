#pragma once
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

#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <cwchar>
#include <functional>

#include <filesystem>


namespace nl {
namespace rakis {
namespace logging {

	enum LogLevel {
		LOGLVL_INIT = 0,
		LOGLVL_TRACE,
		LOGLVL_DEBUG,
		LOGLVL_INFO,
		LOGLVL_WARN,
		LOGLVL_ERROR,
		LOGLVL_FATAL
	};

	constexpr const char* CFG_SEPARATOR{ "." };
	constexpr const char* CFG_ROOTLOGGER{ "rootLogger" };
	constexpr const char* CFG_FILENAME{ "filename" };
	constexpr const char* CFG_LOGGER{ "logger" };

	constexpr const char* CFG_STDOUT{ "STDOUT" };
	constexpr const char* CFG_STDERR{ "STDERR" };

	constexpr const char* CFG_LEVEL_TRACE{ "TRACE" };
	constexpr const char* CFG_LEVEL_DEBUG{ "DEBUG" };
	constexpr const char* CFG_LEVEL_INFO{ "INFO" };
	constexpr const char* CFG_LEVEL_WARN{ "WARN" };
	constexpr const char* CFG_LEVEL_ERROR{ "ERROR" };
	constexpr const char* CFG_LEVEL_FATAL{ "FATAL" };
	constexpr const char* CFG_LEVEL_INIT{ "INIT" };

	constexpr const char* LOGLVL_NAME[] = {
		CFG_LEVEL_INIT, CFG_LEVEL_TRACE, CFG_LEVEL_DEBUG, CFG_LEVEL_INFO, CFG_LEVEL_WARN, CFG_LEVEL_ERROR, CFG_LEVEL_FATAL
	};


	class Configurer {
	public:

		using StringLogger = std::function<void(LogLevel level, const std::string&)>;

		struct LoggerNode {
			std::string name_;
			std::string fullName_;
			std::string filename_;
			StringLogger logger_;
			LogLevel level_;
			std::map<std::string, LoggerNode> children_;

			LoggerNode()
				: name_(""), fullName_(""), filename_(""),
				logger_([](LogLevel level, const std::string& msg) {}),
				level_(LOGLVL_INFO) {}
			LoggerNode(const std::string& name, const std::string& fullName)
				: name_(name), fullName_(fullName), filename_(""),
				logger_([](LogLevel level, const std::string& msg) {}),
				level_(LOGLVL_INFO) {}
			LoggerNode(const std::string& name, const std::string& fullName, const std::string& filename, StringLogger logger, LogLevel level)
				: name_(name), fullName_(fullName), filename_(filename),
				logger_(logger),
				level_(level) {}
		};

		static Configurer& instance() {
			static Configurer theConfigurer;

			return theConfigurer;
		}

	private:
		Configurer() = default;
		Configurer(const Configurer&) = delete;
		Configurer(Configurer&&) = delete;
		~Configurer() = default;
		Configurer& operator=(const Configurer&) = delete;
		Configurer& operator=(Configurer&&) = delete;

		static bool& configDone() {
			static bool configDone_{ false };

			return configDone_;
		}

		static void configure(std::string const& configFile);
		static void logRoot(LogLevel level, const std::string& msg);


		inline static std::string strip(const std::string& s) {
			auto first = s.find_first_not_of(" \t"); \
				if (first == std::string::npos) {
					return std::string();
				}
			auto last = s.find_last_not_of(" \t");
			return s.substr(first, (last - first + 1));
		}

		inline static std::string strip(const std::string_view& s) {
			auto first = s.find_first_not_of(" \t"); \
				if (first == std::string::npos) {
					return std::string();
				}
			auto last = s.find_last_not_of(" \t");
			return std::string(s[first], s[(last - first + 1)]);
		}

		inline static std::tuple<std::string, std::string> split(const std::string& s, const std::string& sep)
		{
			auto pos = s.find(sep);
			if (pos == std::string::npos) {
				return std::make_tuple(s, std::string());
			}
			return std::make_tuple(strip(s.substr(0, pos)), strip(s.substr(pos + sep.length())));
		}

	public:

		static LoggerNode& targets() {
			static LoggerNode theTargets(
				CFG_ROOTLOGGER,
				"",
				"",
				logRoot,
				LOGLVL_INFO
			);

			return theTargets;
		}

		static LoggerNode& getTarget(Configurer::LoggerNode& root, const std::string& name, bool create = false) {
			auto [head, tail] = split(name, CFG_SEPARATOR);
			if (head.empty()) {
				return root;
			}
			else if (root.children_.find(head) == root.children_.end()) {
				if (create) {
					root.children_[head] = Configurer::LoggerNode(head, root.fullName_ + CFG_SEPARATOR + head);
				}
				else {
					return root;
				}
			}
			return getTarget(root.children_[head], tail);
		}

		inline static LoggerNode& getTarget(std::string const& name, bool create = false) { return getTarget(targets(), name); }

		inline LogLevel getLevel(const std::string& name) { return getTarget(name).level_; }

		inline static void rootLogger(LogLevel level, const std::string& msg) {
			targets().logger_(level, msg);
		}

	};

	class Logger {
	public:

		inline static Logger getLogger(const std::string& name) { return Logger(name); }

	private:
		std::string name_;
		LogLevel level_;

		Logger(const std::string& name);

		inline void stream(std::ostream& str) { str << std::endl << std::flush; }

		template <class T, class... Types>
		void stream(std::ostream& str, const T& arg, const Types&... args) {
			str << arg;
			stream(str, args...);
		}
		template <class... Types>
		void stream(std::ostream& str, const std::wstring& arg, const Types&... args) {
			for (auto i = arg.begin(); i != arg.end(); i++) {
				str << char(wctob(*i));
			}
			stream(str, args...);
		}

		inline void log(LogLevel level, const std::string& msg) {
			auto& target{ Configurer::getTarget(name_) };
			if (level < target.level_) {
				return;
			}
			else {
				if (target.logger_) {
					target.logger_(level, msg);
				}
			}
		}

		static std::string formatLine(LogLevel level, const std::string name, const std::string msg);

	public:
		Logger() = delete;
		Logger(Logger const& log) = default;
		Logger(Logger&& log) = default;
		~Logger() = default;

		Logger& operator=(Logger const& log) = default;
		Logger& operator=(Logger&& log) = default;

		inline std::string const& getName() const { return name_; }
		inline const char* getNameC() const { return name_.c_str(); }

		LogLevel getLevel();
		inline void setLevel(LogLevel level) { level_ = level; }

		inline bool isTraceEnabled() { return getLevel() <= LOGLVL_TRACE; }
		inline bool isDebugEnabled() { return getLevel() <= LOGLVL_DEBUG; }
		inline bool isInfoEnabled() { return getLevel() <= LOGLVL_INFO; }
		inline bool isWarnEnabled() { return getLevel() <= LOGLVL_WARN; }
		inline bool isErrorEnabled() { return getLevel() <= LOGLVL_ERROR; }
		inline bool isFatalEnabled() { return getLevel() <= LOGLVL_FATAL; }

		inline void trace(const std::string& txt) {
			if (isTraceEnabled()) {
				log(LOGLVL_TRACE, txt);
			}
		}
		inline void debug(const std::string& txt) {
			if (isDebugEnabled()) {
				log(LOGLVL_DEBUG, txt);
			}
		}
		inline void info(const std::string& txt) {
			if (isInfoEnabled()) {
				log(LOGLVL_INFO, txt);
			}
		}
		inline void warn(const std::string& txt) {
			if (isWarnEnabled()) {
				log(LOGLVL_WARN, txt);
			}
		}
		inline void error(const std::string& txt) {
			if (isErrorEnabled()) {
				log(LOGLVL_ERROR, txt);
			}
		}
		inline void fatal(const std::string& txt) {
			if (isFatalEnabled()) {
				log(LOGLVL_FATAL, txt);
			}
		}

		friend class Configurer;

		inline static Configurer& configuration() { return Configurer::instance(); }
	};

}
}
}