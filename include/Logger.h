#ifndef LOGGER_H
#define LOGGER_H

#include "SourceCode.h"
#include <string>
#include <sstream>
#include <ostream>

class Logger {
    public:
        bool hasError, hasFatal;

        static Logger& getInstance() {
            static Logger instance;
            return instance;
        }
        Logger(Logger const&) = delete;
        void operator=(Logger const&) = delete;

        template<class T>
        void warn(const T &pos, const std::string &prompt) {
            echo("Warning", pos, prompt);
        }
        template<class T>
        void note(const T &pos, const std::string &prompt) {
            echo("Note", pos, prompt);
        }
        template<class T>
        void error(const T &pos, const std::string &prompt) {
            hasError = true;
            echo("Error", pos, prompt);
        }
        template<class T>
        void fatal(const T &pos, const std::string &prompt) {
            hasFatal = true;
            echo("Fatal", pos, prompt);
        }

        void output(std::ostream &stream) {
            stream << ss.rdbuf();
            ss.clear();
        }
    private:
        std::stringstream ss;

        Logger(): hasError(false), hasFatal(false) {}

        template<class T>
        void echo(const std::string &prefix, const T &pos, const std::string &prompt);
};

#endif // LOGGER_H
