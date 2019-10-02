#ifndef SOURCECODE_H
#define SOURCECODE_H

#include <cassert>
#include <vector>
#include <string>
#include <istream>
#include <algorithm>

class SourceCode {
protected:
    std::vector<std::string> lines;
    std::string code;

public:
    class const_iterator {
    protected:
        const SourceCode &src;
        size_t pos, lineNumber, columnNumber;
    public:
        size_t getLineNumber() const {
            return lineNumber;
        }
        size_t getColumnNumber() const {
            return columnNumber;
        }
        std::string getCurrentLine() const {
            return src.lines[lineNumber];
        }
        std::string getSlice(size_t stride) const {
            return src.code.substr(pos, std::min(stride, src.code.size() - pos));
        }
        const_iterator(const SourceCode &s, size_t p):
                src(s), pos(p), lineNumber(0), columnNumber(0) {
            assert(p == 0 || p == src.code.size());
        }
        const_iterator& operator++() {
            assert(pos < src.code.size());
            if(**this == '\n') {
                ++lineNumber;
                columnNumber = 0;
                ++pos;
            } else {
                ++columnNumber;
                ++pos;
            }
            return *this;
        }
        const_iterator& operator=(const const_iterator &iter) {
            assert(&iter.src == &src);
            pos = iter.pos;
            lineNumber = iter.lineNumber;
            columnNumber = iter.columnNumber;
            return *this;
        }
        bool operator==(const const_iterator &other) const {
            return pos == other.pos;
        }
        bool operator!=(const const_iterator &other) const {
            return pos != other.pos;
        }
        const char &operator*() const {
            assert(pos < src.code.size());
            return src.code[pos];
        }
        bool good() const {
            return pos < src.code.size();
        }
    };

    SourceCode(std::istream &stream);

    const const_iterator begin() const {
        return const_iterator(*this, 0);
    }
    const const_iterator end() const {
        return const_iterator(*this, code.size());
    }

    std::string getLine(size_t i) const {
        assert(i < lines.size());
        return lines[i];
    }
};

#endif // SOURCECODE_H
