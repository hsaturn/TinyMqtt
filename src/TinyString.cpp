#include "TinyString.h"
#include <cstdlib>

const char* TinyString::emptyString = "";

TinyString::TinyString(const char* buffer, uint16_t s)
{
  dup(buffer, s);
}

TinyString::TinyString(int i, int base)
{
  reserve(sizeof(int)*8+1);
  itoa(i, str, base);
  size_ = strlen(str);
  free_ -= size_;
}

TinyString::TinyString(const TinyString& m)
{
  copy(m);
}

TinyString& TinyString::operator=(const TinyString& m)
{
  copy(m);
  return *this;
}

TinyString& TinyString::operator+=(const char c)
{
  push_back(c);
  return *this;
}

TinyString& TinyString::operator +=(int i)
{
  reserve(size_ + sizeof(int)*3+1);
  itoa(i, str + size_, 10);
  int8_t sz = strlen(str+size_);
  size_ += sz;
  free_ -= sz;
  return *this;
}

void TinyString::concat(const char* buf, uint16_t len)
{
  reserve(size_ + len + 1);
  strcpy(str + size_, buf);
  size_ += len;
  free_ -= len;
}

void TinyString::push_back(const char c)
{
  reserve(size_+1, extent);
  str[size_++] = c;
  str[size_] = 0;
  free_--;
}

void TinyString::erase(uint16_t pos, uint16_t size)
{
  if (size == npos) size = size_;
  if (pos > size_) return;
  if (pos + size > size_) size = size_ - pos;
  memmove(str+pos, str+pos+size, size_ - pos + 1);
  if (size_ == size)
  {
    clear();
  }
  else
  {
    size_ -= size;
    free_ += size;
  }
}

void TinyString::clear()
{
  if (size_)
    free(str);
  str = const_cast<char*>(emptyString);  // Dangerous str must be left untouched when size_ == 0
  size_ = 0;
}

TinyString& TinyString::operator = (const char c)
{
  dup(&c, 1);
  return *this;
}

TinyString TinyString::substr(uint16_t pos, uint16_t size)
{
  if (size == npos) size = size_;
  if (pos > size_) return TinyString();
  if (pos + size > size_) size = size_ - pos;
  return TinyString(str+pos, size);
}

bool TinyString::starts_with(const char* buf, uint16_t size) const
{
  const_iterator it(str);
  while(size and it != end() and (*it == *buf))
  {
    it++;
    buf++;
    size--;
  }
  return size == 0;
}

int TinyString::compare(const char* s, uint16_t len) const
{
  if (len > size_)
    return memcmp(str, s, size_ + 1);
  else
    return memcmp(str, s, len + 1);
}

void TinyString::reserve(uint16_t sz, uint8_t extent)
{
  if (sz == 0)
  {
    clear();
    return;
  }
  if (size_ == 0)
  {
    free_ = sz + extent;
    str = static_cast<char*>(malloc(sz + extent));
    return;
  }
  if ((sz > size_ + free_) or (extent > size_ + free_ - sz))
  {
    free_ = sz + extent - size_;
    str = static_cast<char*>(realloc(str, sz + extent));
  }
}

void TinyString::collect()
{
  if (size_ > 0 and free_ > 1)
  {
    str = static_cast<char*>(realloc(str, size_ + 1));
    free_ = 1;
  }
}

void TinyString::dup(const char* buffer, uint16_t sz, uint8_t extent)
{
  reserve(sz + 1, extent);
  memcpy(str, buffer, sz);
  str[sz] = 0;
  if (size_ > sz)
    free_ += size_ - sz;
  else
    free_ -= sz - size_;
  size_ = sz;
}
