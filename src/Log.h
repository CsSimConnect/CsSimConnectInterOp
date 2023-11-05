#pragma once
/*
 * Copyright (c) 2021-2023. Bert Laverman
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
#include <ostream>
#include <fstream>
#include <cwchar>

#include <filesystem>


namespace nl {
namespace rakis {

	class Logger {
	public:
		enum Level {
			LOGLVL_INIT = 0,
			LOGLVL_TRACE,
			LOGLVL_DEBUG,
			LOGLVL_INFO,
			LOGLVL_WARN,
			LOGLVL_ERROR,
			LOGLVL_FATAL
		};
		struct LoggerNode {
			std::string name;
			std::string fullName;
			std::string filename;
			std::unique_ptr<std::ostream> stream;
			Level level;
			std::map<std::string, LoggerNode> children;

			LoggerNode() = default;
			LoggerNode(const std::string& name, const std::string& fullName)
				: name(name), fullName(fullName) {}
			LoggerNode(const std::string& name, const std::string& fullName, const std::string& filename, std::unique_ptr<std::ostream> stream, Level level)
				: name(name), fullName(fullName), filename(filename), stream(std::move(stream)), level(level) {}
		};

		static void configure(std::string const& configFile);
		inline static Logger getLogger(const std::string& name) { return Logger(name); }

	private:
		std::string name_;
		Level level_;

		Logger(const std::string& name);

		void startLine(std::ostream& s, Level level);
		void stream(std::ostream& str) { str << std::endl << std::flush; }

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

		static LoggerNode targets_;

		static bool configDone_;


		static LoggerNode& getTarget(std::string const& name, bool create = false);
		inline Level getLevel(const std::string& name) { return getTarget(name).level; }

		inline void log(Level level, const std::string& txt) {
			auto& target{ getTarget(name_) };
			if (level < target.level) {
				return;
			} else {
				if (target.stream) {
					startLine(*target.stream, level);
					*target.stream << txt << std::endl << std::flush;
				}
			}
		}

		inline static std::ostream& root() { return *targets_.stream; }

	public:
		Logger() = delete;
		Logger(Logger const& log) = default;
		Logger(Logger&& log) = default;
		~Logger();

		Logger& operator=(Logger const& log) = default;
		Logger& operator=(Logger&& log) = default;

		inline std::string const& getName() const { return name_; }
		inline const char* getNameC() const { return name_.c_str(); }

		Level getLevel();
		inline void setLevel(Level level) { level_ = level; }

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

	};
}
}
