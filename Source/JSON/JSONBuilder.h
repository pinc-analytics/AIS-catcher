/*
	Copyright(c) 2021-2026 jvde.github@gmail.com

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

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

    // Geometric growth to minimize allocations
    inline void ensure(size_t len) {
        if (cursor + len >= buf.size()) {
            // Double capacity or ensure enough for len, whichever is larger
            size_t new_cap = std::max<size_t>(buf.size() * 2, cursor + len + 32);
            buf.resize(new_cap);
        }
    }

    // Fast unsafe write (caller must ensure space)
    inline void writeRaw(const char* data, size_t len) {
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

    void appendKey(const char* k, size_t len) {
        comma();
        ensure(len + 4); 
        buf[cursor++] = '"';
        std::memcpy(&buf[cursor], k, len);
        cursor += len;
        buf[cursor++] = '"';
        buf[cursor++] = ':';
        need_comma = false;
    }

    // Chunk-based escaping (much faster than char-by-char)
    void escapeString(const char* s, size_t len) {
        ensure(len * 2 + 2); // Reserve worst case
        buf[cursor++] = '"';
        
        size_t start = 0;
        for (size_t i = 0; i < len; ++i) {
            const char* replacement = nullptr;
            size_t repl_len = 0;
            
            switch (s[i]) {
                case '"':  replacement = "\\\""; repl_len = 2; break;
                case '\\': replacement = "\\\\"; repl_len = 2; break;
                case '\b': replacement = "\\b";  repl_len = 2; break;
                case '\f': replacement = "\\f";  repl_len = 2; break;
                case '\n': replacement = "\\n";  repl_len = 2; break;
                case '\r': replacement = "\\r";  repl_len = 2; break;
                case '\t': replacement = "\\t";  repl_len = 2; break;
                default: continue; 
            }

            // Flush safe chunk
            if (i > start) {
                size_t chunk_len = i - start;
                std::memcpy(&buf[cursor], s + start, chunk_len);
                cursor += chunk_len;
            }
            
            // Write replacement
            std::memcpy(&buf[cursor], replacement, repl_len);
            cursor += repl_len;
            start = i + 1;
        }

        // Flush remaining safe chunk
        if (start < len) {
            size_t chunk_len = len - start;
            std::memcpy(&buf[cursor], s + start, chunk_len);
            cursor += chunk_len;
        }
        
        buf[cursor++] = '"';
    }

    // Custom itoa (integer to string)
    template<typename T>
    void writeInt(T value) {
        ensure(24);
        if (value == 0) { buf[cursor++] = '0'; return; }
        
        if (value < 0) {
            buf[cursor++] = '-';
            value = -value; 
        }

        char temp[24];
        char* p = temp + 23;
        typename std::make_unsigned<T>::type uval = value;
        
        while (uval > 0) {
            *--p = '0' + (uval % 10);
            uval /= 10;
        }

        size_t len = (temp + 23) - p;
        std::memcpy(&buf[cursor], p, len);
        cursor += len;
    }

public:
    JSONBuilder() : cursor(0), need_comma(false) { buf.resize(4096); }
    
    // Core structure
    JSONBuilder& start() { ensure(1); buf[cursor++] = '{'; need_comma = false; return *this; }
    JSONBuilder& end() { ensure(1); buf[cursor++] = '}'; need_comma = true; return *this; }
    JSONBuilder& startArray() { ensure(1); buf[cursor++] = '['; need_comma = false; return *this; }
    JSONBuilder& endArray() { ensure(1); buf[cursor++] = ']'; need_comma = true; return *this; }

    // Key handling (Optimized for literals)
    JSONBuilder& key(const char* k) { appendKey(k, std::strlen(k)); return *this; }
    JSONBuilder& key(const std::string& k) { appendKey(k.data(), k.size()); return *this; }

    // Types
    JSONBuilder& add(const char* k, int v) { appendKey(k, std::strlen(k)); writeInt(v); need_comma = true; return *this; }
    JSONBuilder& add(const char* k, long v) { appendKey(k, std::strlen(k)); writeInt(v); need_comma = true; return *this; }
    JSONBuilder& add(const char* k, long long v) { appendKey(k, std::strlen(k)); writeInt(v); need_comma = true; return *this; }
    JSONBuilder& add(const char* k, unsigned int v) { appendKey(k, std::strlen(k)); writeInt(v); need_comma = true; return *this; }
    
    JSONBuilder& add(const char* k, bool v) {
        appendKey(k, std::strlen(k));
        if (v) { ensure(4); writeRaw("true", 4); } 
        else   { ensure(5); writeRaw("false", 5); }
        need_comma = true;
        return *this;
    }

    JSONBuilder& add(const char* k, double v) {
        appendKey(k, std::strlen(k));
        ensure(32);
        int len = std::snprintf(&buf[cursor], 32, "%.6g", v);
        if(len > 0) cursor += len;
        need_comma = true;
        return *this;
    }

    JSONBuilder& addNull(const char* k) {
        appendKey(k, std::strlen(k));
        ensure(4); writeRaw("null", 4);
        need_comma = true;
        return *this;
    }

    // Raw String (already escaped or object)
    JSONBuilder& add(const char* k, const std::string& v) {
        appendKey(k, std::strlen(k));
        ensure(v.size()); writeRaw(v.data(), v.size());
        need_comma = true;
        return *this;
    }

    // Escaped String
    JSONBuilder& addString(const char* k, const char* v) {
        appendKey(k, std::strlen(k));
        escapeString(v, std::strlen(v));
        need_comma = true;
        return *this;
    }
    
    // Escaped String (std::string overload)
    JSONBuilder& addString(const char* k, const std::string& v) {
        appendKey(k, std::strlen(k));
        escapeString(v.data(), v.size());
        need_comma = true;
        return *this;
    }

    // Safe String (no escaping, manual quotes)
    JSONBuilder& addSafeString(const char* k, const char* v) {
        appendKey(k, std::strlen(k));
        size_t len = std::strlen(v);
        ensure(len + 2);
        buf[cursor++] = '"';
        writeRaw(v, len);
        buf[cursor++] = '"';
        need_comma = true;
        return *this;
    }
    
    JSONBuilder& addSafeString(const char* k, const std::string& v) {
        appendKey(k, std::strlen(k));
        ensure(v.size() + 2);
        buf[cursor++] = '"';
        writeRaw(v.data(), v.size());
        buf[cursor++] = '"';
        need_comma = true;
        return *this;
    }

    // Value helpers (for arrays)
    JSONBuilder& value(int v) { comma(); writeInt(v); return *this; }
    JSONBuilder& value(const std::string& v) { comma(); escapeString(v.data(), v.size()); return *this; }
    JSONBuilder& valueRaw(const std::string& v) { comma(); ensure(v.size()); writeRaw(v.data(), v.size()); return *this; }

    // Output
    std::string str() {
        buf.resize(cursor);
        return buf;
    }
    
    void clear() { cursor = 0; need_comma = false; }
};

} // namespace JSON