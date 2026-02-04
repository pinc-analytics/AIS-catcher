#pragma once

#include <string>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <type_traits>

namespace JSON {

class JSONBuilder {
    std::string buf;
    size_t cursor;
    bool need_comma;

    void ensure(size_t len) {
        if (cursor + len >= buf.size()) {
            buf.resize(std::max(buf.size() * 2, cursor + len + 64));
        }
    }

    void writeRaw(const char* data, size_t len) {
        std::memcpy(&buf[cursor], data, len);
        cursor += len;
    }

    void comma() {
        if (need_comma) {
            ensure(1);
            buf[cursor++] = ',';
        }
        need_comma = true;
    }

    void appendKeyImpl(const char* k, size_t len) {
        ensure(len + 4);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, len);
        cursor += len;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        need_comma = false;
    }

    void escapeString(const char* s, size_t len) {
        ensure(len * 2 + 2);
        buf[cursor++] = '"';

        size_t start = 0;
        for (size_t i = 0; i < len; ++i) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            const char* repl = NULL;
            size_t rlen = 2;

            switch (c) {
                case '"':  repl = "\\\""; break;
                case '\\': repl = "\\\\"; break;
                case '\b': repl = "\\b";  break;
                case '\f': repl = "\\f";  break;
                case '\n': repl = "\\n";  break;
                case '\r': repl = "\\r";  break;
                case '\t': repl = "\\t";  break;
                default:
                    if (c < 0x20) {
                        if (i > start) {
                            std::memcpy(&buf[cursor], s + start, i - start);
                            cursor += i - start;
                        }
                        start = i + 1;
                    }
                    continue;
            }

            if (i > start) {
                std::memcpy(&buf[cursor], s + start, i - start);
                cursor += i - start;
            }
            std::memcpy(&buf[cursor], repl, rlen);
            cursor += rlen;
            start = i + 1;
        }

        if (start < len) {
            std::memcpy(&buf[cursor], s + start, len - start);
            cursor += len - start;
        }
        buf[cursor++] = '"';
    }

    template<typename T>
    void writeInt(T value) {
        ensure(24);
        if (value == 0) {
            buf[cursor++] = '0';
            return;
        }

        typename std::make_unsigned<T>::type uval;
        if (value < 0) {
            buf[cursor++] = '-';
            uval = -static_cast<typename std::make_unsigned<T>::type>(value);
        } else {
            uval = static_cast<typename std::make_unsigned<T>::type>(value);
        }

        char temp[24];
        char* p = temp + 23;
        while (uval > 0) {
            *--p = '0' + (uval % 10);
            uval /= 10;
        }

        size_t len = (temp + 23) - p;
        std::memcpy(&buf[cursor], p, len);
        cursor += len;
    }

    void writeUInt(unsigned long long value) {
        ensure(24);
        if (value == 0) {
            buf[cursor++] = '0';
            return;
        }

        char temp[24];
        char* p = temp + 23;
        while (value > 0) {
            *--p = '0' + (value % 10);
            value /= 10;
        }

        size_t len = (temp + 23) - p;
        std::memcpy(&buf[cursor], p, len);
        cursor += len;
    }

    void writeDouble(double v) {
        ensure(32);
        int len = std::snprintf(&buf[cursor], 32, "%.6g", v);
        if (len > 0) cursor += len;
    }

    void writeBool(bool v) {
        if (v) {
            ensure(4);
            writeRaw("true", 4);
        } else {
            ensure(5);
            writeRaw("false", 5);
        }
    }

    void writeNull() {
        ensure(4);
        writeRaw("null", 4);
    }

    // Type-dispatched value writing for templates
    void writeValue(short v)              { writeInt(static_cast<int>(v)); }
    void writeValue(int v)                { writeInt(v); }
    void writeValue(long v)               { writeInt(v); }
    void writeValue(long long v)          { writeInt(v); }
    void writeValue(unsigned short v)     { writeUInt(v); }
    void writeValue(unsigned int v)       { writeUInt(v); }
    void writeValue(unsigned long v)      { writeUInt(v); }
    void writeValue(unsigned long long v) { writeUInt(v); }
    void writeValue(float v)              { writeDouble(static_cast<double>(v)); }
    void writeValue(double v)             { writeDouble(v); }
    void writeValue(bool v)               { writeBool(v); }

public:
    JSONBuilder() : cursor(0), need_comma(false) {
        buf.resize(1024);
    }

    void clear() {
        cursor = 0;
        need_comma = false;
    }

    std::string str() const {
        return std::string(buf.data(), cursor);
    }

    std::string take() {
        buf.resize(cursor);
        std::string result;
        result.swap(buf);
        cursor = 0;
        need_comma = false;
        buf.resize(1024);
        return result;
    }

    size_t size() const { return cursor; }

    // ==================== STRUCTURE ====================

    JSONBuilder& start() {
        comma();
        ensure(1);
        buf[cursor++] = '{';
        need_comma = false;
        return *this;
    }

    JSONBuilder& end() {
        ensure(1);
        buf[cursor++] = '}';
        need_comma = true;
        return *this;
    }

    JSONBuilder& startArray() {
        comma();
        ensure(1);
        buf[cursor++] = '[';
        need_comma = false;
        return *this;
    }

    JSONBuilder& endArray() {
        ensure(1);
        buf[cursor++] = ']';
        need_comma = true;
        return *this;
    }

    // ==================== KEY (runtime) ====================

    JSONBuilder& key(const char* k) {
        appendKeyImpl(k, std::strlen(k));
        return *this;
    }

    JSONBuilder& key(const std::string& k) {
        appendKeyImpl(k.data(), k.size());
        return *this;
    }

    // ==================== LITERAL KEY: add ====================

    template<size_t N>
    JSONBuilder& add(const char (&k)[N], short v) {
        ensure(N + 27);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        writeInt(static_cast<int>(v));
        need_comma = true;
        return *this;
    }

    template<size_t N>
    JSONBuilder& add(const char (&k)[N], int v) {
        ensure(N + 27);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        writeInt(v);
        need_comma = true;
        return *this;
    }

    template<size_t N>
    JSONBuilder& add(const char (&k)[N], long v) {
        ensure(N + 27);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        writeInt(v);
        need_comma = true;
        return *this;
    }

    template<size_t N>
    JSONBuilder& add(const char (&k)[N], long long v) {
        ensure(N + 27);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        writeInt(v);
        need_comma = true;
        return *this;
    }

    template<size_t N>
    JSONBuilder& add(const char (&k)[N], unsigned short v) {
        ensure(N + 27);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        writeUInt(v);
        need_comma = true;
        return *this;
    }

    template<size_t N>
    JSONBuilder& add(const char (&k)[N], unsigned int v) {
        ensure(N + 27);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        writeUInt(v);
        need_comma = true;
        return *this;
    }

    template<size_t N>
    JSONBuilder& add(const char (&k)[N], unsigned long v) {
        ensure(N + 27);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        writeUInt(v);
        need_comma = true;
        return *this;
    }

    template<size_t N>
    JSONBuilder& add(const char (&k)[N], unsigned long long v) {
        ensure(N + 27);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        writeUInt(v);
        need_comma = true;
        return *this;
    }

    template<size_t N>
    JSONBuilder& add(const char (&k)[N], float v) {
        ensure(N + 35);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        writeDouble(static_cast<double>(v));
        need_comma = true;
        return *this;
    }

    template<size_t N>
    JSONBuilder& add(const char (&k)[N], double v) {
        ensure(N + 35);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        writeDouble(v);
        need_comma = true;
        return *this;
    }

    template<size_t N>
    JSONBuilder& add(const char (&k)[N], bool v) {
        ensure(N + 8);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        writeBool(v);
        need_comma = true;
        return *this;
    }

    template<size_t N>
    JSONBuilder& addNull(const char (&k)[N]) {
        ensure(N + 7);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        writeNull();
        need_comma = true;
        return *this;
    }

    template<size_t N>
    JSONBuilder& addString(const char (&k)[N], const std::string& v) {
        ensure(N + 3);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        escapeString(v.data(), v.size());
        need_comma = true;
        return *this;
    }

    template<size_t N>
    JSONBuilder& addString(const char (&k)[N], const char* v) {
        ensure(N + 3);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        escapeString(v, std::strlen(v));
        need_comma = true;
        return *this;
    }

    template<size_t N>
    JSONBuilder& addSafe(const char (&k)[N], const std::string& v) {
        ensure(N + v.size() + 5);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], v.data(), v.size());
        cursor += v.size();
        buf[cursor++] = '"';
        need_comma = true;
        return *this;
    }

    template<size_t N, size_t M>
    JSONBuilder& addSafe(const char (&k)[N], const char (&v)[M]) {
        ensure(N + M + 4);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], v, M - 1);
        cursor += M - 1;
        buf[cursor++] = '"';
        need_comma = true;
        return *this;
    }

    template<size_t N>
    JSONBuilder& addRaw(const char (&k)[N], const std::string& v) {
        ensure(N + v.size() + 3);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        std::memcpy(&buf[cursor], v.data(), v.size());
        cursor += v.size();
        need_comma = true;
        return *this;
    }

    template<size_t N>
    JSONBuilder& addStringOrNull(const char (&k)[N], const std::string& val) {
        ensure(N + 3);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        if (val.empty()) {
            writeNull();
        } else {
            escapeString(val.data(), val.size());
        }
        need_comma = true;
        return *this;
    }

    template<size_t N>
    JSONBuilder& addStringOrNull(const char (&k)[N], const char* val) {
        ensure(N + 3);
        if (need_comma) buf[cursor++] = ',';
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        if (val == NULL || val[0] == '\0') {
            writeNull();
        } else {
            escapeString(val, std::strlen(val));
        }
        need_comma = true;
        return *this;
    }

    // ==================== LITERAL KEY: addIf ====================

    template<size_t N, typename T>
    JSONBuilder& addIf(bool condition, const char (&k)[N], T val) {
        if (condition) return add(k, val);
        return *this;
    }

    template<size_t N>
    JSONBuilder& addStringIf(bool condition, const char (&k)[N], const std::string& val) {
        if (condition) return addString(k, val);
        return *this;
    }

    // ==================== RUNTIME KEY ====================

    JSONBuilder& add(const char* k, short v) {
        appendKeyImpl(k, std::strlen(k));
        writeInt(static_cast<int>(v));
        need_comma = true;
        return *this;
    }

    JSONBuilder& add(const char* k, int v) {
        appendKeyImpl(k, std::strlen(k));
        writeInt(v);
        need_comma = true;
        return *this;
    }

    JSONBuilder& add(const char* k, long v) {
        appendKeyImpl(k, std::strlen(k));
        writeInt(v);
        need_comma = true;
        return *this;
    }

    JSONBuilder& add(const char* k, long long v) {
        appendKeyImpl(k, std::strlen(k));
        writeInt(v);
        need_comma = true;
        return *this;
    }

    JSONBuilder& add(const char* k, unsigned short v) {
        appendKeyImpl(k, std::strlen(k));
        writeUInt(v);
        need_comma = true;
        return *this;
    }

    JSONBuilder& add(const char* k, unsigned int v) {
        appendKeyImpl(k, std::strlen(k));
        writeUInt(v);
        need_comma = true;
        return *this;
    }

    JSONBuilder& add(const char* k, unsigned long v) {
        appendKeyImpl(k, std::strlen(k));
        writeUInt(v);
        need_comma = true;
        return *this;
    }

    JSONBuilder& add(const char* k, unsigned long long v) {
        appendKeyImpl(k, std::strlen(k));
        writeUInt(v);
        need_comma = true;
        return *this;
    }

    JSONBuilder& add(const char* k, float v) {
        appendKeyImpl(k, std::strlen(k));
        writeDouble(static_cast<double>(v));
        need_comma = true;
        return *this;
    }

    JSONBuilder& add(const char* k, double v) {
        appendKeyImpl(k, std::strlen(k));
        writeDouble(v);
        need_comma = true;
        return *this;
    }

    JSONBuilder& add(const char* k, bool v) {
        appendKeyImpl(k, std::strlen(k));
        writeBool(v);
        need_comma = true;
        return *this;
    }

    JSONBuilder& addRaw(const char* k, const std::string& v) {
        appendKeyImpl(k, std::strlen(k));
        ensure(v.size());
        writeRaw(v.data(), v.size());
        need_comma = true;
        return *this;
    }

    JSONBuilder& addString(const char* k, const std::string& v) {
        appendKeyImpl(k, std::strlen(k));
        escapeString(v.data(), v.size());
        need_comma = true;
        return *this;
    }

    JSONBuilder& addString(const char* k, const char* v) {
        appendKeyImpl(k, std::strlen(k));
        escapeString(v, std::strlen(v));
        need_comma = true;
        return *this;
    }

    JSONBuilder& addNull(const char* k) {
        appendKeyImpl(k, std::strlen(k));
        writeNull();
        need_comma = true;
        return *this;
    }

    template<typename T>
    JSONBuilder& addOrNull(const char* k, T val, T undefined) {
        appendKeyImpl(k, std::strlen(k));
        if (val == undefined) {
            writeNull();
        } else {
            writeValue(val);
        }
        need_comma = true;
        return *this;
    }

    JSONBuilder& addStringOrNull(const char* k, const std::string& val) {
        appendKeyImpl(k, std::strlen(k));
        if (val.empty()) {
            writeNull();
        } else {
            escapeString(val.data(), val.size());
        }
        need_comma = true;
        return *this;
    }

    // ==================== ARRAY VALUES ====================

    JSONBuilder& value(short v) {
        comma();
        writeInt(static_cast<int>(v));
        return *this;
    }

    JSONBuilder& value(int v) {
        comma();
        writeInt(v);
        return *this;
    }

    JSONBuilder& value(long v) {
        comma();
        writeInt(v);
        return *this;
    }

    JSONBuilder& value(long long v) {
        comma();
        writeInt(v);
        return *this;
    }

    JSONBuilder& value(unsigned short v) {
        comma();
        writeUInt(v);
        return *this;
    }

    JSONBuilder& value(unsigned int v) {
        comma();
        writeUInt(v);
        return *this;
    }

    JSONBuilder& value(unsigned long v) {
        comma();
        writeUInt(v);
        return *this;
    }

    JSONBuilder& value(unsigned long long v) {
        comma();
        writeUInt(v);
        return *this;
    }

    JSONBuilder& value(float v) {
        comma();
        writeDouble(static_cast<double>(v));
        return *this;
    }

    JSONBuilder& value(double v) {
        comma();
        writeDouble(v);
        return *this;
    }

    JSONBuilder& value(bool v) {
        comma();
        writeBool(v);
        return *this;
    }

    JSONBuilder& value(const std::string& v) {
        comma();
        escapeString(v.data(), v.size());
        return *this;
    }

    JSONBuilder& value(const char* v) {
        comma();
        escapeString(v, std::strlen(v));
        return *this;
    }

    JSONBuilder& valueRaw(const std::string& v) {
        comma();
        ensure(v.size());
        writeRaw(v.data(), v.size());
        return *this;
    }

    JSONBuilder& valueNull() {
        comma();
        writeNull();
        return *this;
    }

    template<size_t N>
    JSONBuilder& valueSafe(const char (&v)[N]) {
        comma();
        ensure(N + 1);
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], v, N - 1);
        cursor += N - 1;
        buf[cursor++] = '"';
        return *this;
    }

    JSONBuilder& valueSafe(const std::string& v) {
        comma();
        ensure(v.size() + 2);
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], v.data(), v.size());
        cursor += v.size();
        buf[cursor++] = '"';
        return *this;
    }

    // ==================== ARRAY: valueOrNull ====================

    template<typename T>
    JSONBuilder& valueOrNull(T val, T undefined) {
        comma();
        if (val == undefined) {
            writeNull();
        } else {
            writeValue(val);
        }
        return *this;
    }

    JSONBuilder& valueStringOrNull(const std::string& val) {
        comma();
        if (val.empty()) {
            writeNull();
        } else {
            escapeString(val.data(), val.size());
        }
        return *this;
    }

    JSONBuilder& valueStringOrNull(const char* val) {
        comma();
        if (val == NULL || val[0] == '\0') {
            writeNull();
        } else {
            escapeString(val, std::strlen(val));
        }
        return *this;
    }
};

} // namespace JSON