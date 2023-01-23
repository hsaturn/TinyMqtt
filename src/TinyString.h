#pragma once
#include <Arduino.h>
#include <assert.h>
#include <cstdint>

#pragma pack(push, 1)

class TinyString
{
  public:
    using value_type = char;
    static constexpr uint16_t npos = std::numeric_limits<uint16_t>::max();

    TinyString() = default;
    TinyString(int, int base=10);
    TinyString(const TinyString&);
    TinyString(const char*, uint16_t size);
    TinyString(const char* s) : TinyString(s, strlen(s)){};
    TinyString& operator= (const TinyString&);
    ~TinyString() { clear(); }

    int compare(const char* buf, uint16_t len) const;
    int compare(const char* buf) const { return compare(buf, strlen(buf)); }

    friend bool operator == (const TinyString& l, const TinyString& r) { return l.compare(r) == 0; }

    friend bool operator < (const TinyString& l, const TinyString& r) { return l.compare(r) <0; }

    const char* c_str() const { return str; }
    uint16_t length() const { return size_; }
    uint16_t size() const { return length(); }
    void concat(const char* buf, uint16_t len);

    bool starts_with(const char* buf, uint16_t len) const;
    bool starts_with(const char* buf) const { return starts_with(buf, strlen(buf)); }

    TinyString substr(uint16_t pos, uint16_t len = npos);

    char& operator[](uint16_t index) const { assert(index < size_); return str[index]; }
    TinyString& operator = (const char c);
    TinyString& operator +=(const char c);
    TinyString& operator +=(const char* buf) { concat(buf, strlen(buf)); return *this; }
    TinyString& operator +=(const TinyString& s) { concat(s.str, s.size_); return *this; }
    TinyString& operator +=(int32_t);

    operator const char*() const { return str; }

    void reserve(uint16_t size) { reserve(size, 0); }

    void erase(uint16_t pos, uint16_t size = npos);

    void dup(const char* buffer, uint16_t size, uint8_t extent = 4);

    void push_back(const char c);
    void clear();

    using const_iterator = const char*;
    using iterator = char*;
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }
    iterator begin() const { return str; }
    iterator end() const { return str + size_; }

    uint16_t capacity() const { return size_ + free_; }
    void collect(); // Save memory

  private:
    void reserve(uint16_t new_size, uint8_t extent);
    void copy(const TinyString& t) { dup(t.str, t.size_); };

    char* str = const_cast<char *>(emptyString);
    uint16_t size_ = 0;   // if size_ == 0 no allocation, but str = emptyString
    uint8_t free_ = 0;    // malloc(str) = size_ + free_

    static const char* emptyString;
    const uint8_t extent = 8;
};

#pragma pack(pop)
