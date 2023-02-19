#pragma once
#include <Arduino.h>
#include <assert.h>
#include <cstdint>

#pragma pack(push, 1)

class TinyString
{
  public:
    class size_t
    {
      public:
        using type = uint16_t;
        size_t (const size_t& s) : s_(s) {}
        size_t (int i) : s_(static_cast<type>) {}

        type operator() const { return s_; }

      private:
        type s_;
    };
    using value_type = char;
    constexpr size_t::type npos = std::numeric_limits<size_t::type>::max();

    TinyString() = default;
    TinyString(int, int base=10);
    TinyString(const TinyString&);
    TinyString(const char*, size_t size);
    TinyString(const char* s) : TinyString(s, strlen(s)){};
    TinyString& operator= (const TinyString&);
    ~TinyString() { clear(); }

    int compare(const char* buf, size_t len) const;
    int compare(const char* buf) const { return compare(buf, strlen(buf)); }

    friend bool operator == (const TinyString& l, const TinyString& r) { return l.compare(r) == 0; }

    friend bool operator < (const TinyString& l, const TinyString& r) { return l.compare(r) <0; }

    const char* c_str() const { return str; }
    size_t length() const { return size_; }
    size_t size() const { return length(); }
    void concat(const char* buf, size_t len);

    bool starts_with(const char* buf, size_t len) const;
    bool starts_with(const char* buf) const { return starts_with(buf, strlen(buf)); }

    size_t find(const char c, const size_t from=0) const;

    TinyString substr(size_t pos, size_t len = npos);

    char& operator[](size_t index) const { assert(index < size_); return str[index]; }
    TinyString& operator = (const char c);
    TinyString& operator +=(const char c);
    TinyString& operator +=(const char* buf) { concat(buf, strlen(buf)); return *this; }
    TinyString& operator +=(const TinyString& s) { concat(s.str, s.size_); return *this; }
    TinyString& operator +=(int32_t);

    operator const char*() const { return str; }

    void reserve(size_t size) { reserve(size, 0); }

    void erase(size_t pos, size_t size = npos);

    void dup(const char* buffer, size_t size, uint8_t extent = 4);

    void push_back(const char c);
    void clear();

    using const_iterator = const char*;
    using iterator = char*;
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }
    iterator begin() const { return str; }
    iterator end() const { return str + size_; }

    size_t capacity() const { return size_ + free_; }
    void collect(); // Save memory

  private:
    void reserve(size_t new_size, uint8_t extent);
    void copy(const TinyString& t) { dup(t.str, t.size_); };

    char* str = const_cast<char *>(emptyString);
    size_t size_ = 0;   // if size_ == 0 no allocation, but str = emptyString
    uint8_t free_ = 0;    // malloc(str) = size_ + free_

    static const char* emptyString;
    const uint8_t extent = 8;
};

#pragma pack(pop)
