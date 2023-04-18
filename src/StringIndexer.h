// vim: ts=2 sw=2 expandtab
#pragma once
#include <assert.h>
#include <map>
#include <unordered_map>
#include "TinyConsole.h"
#include <string>
#include <string.h>

using string = TinyConsole::string;

/***
 * Allows to store up to 255 different strings with one byte class
 * very memory efficient when one string is used many times.
 */
class StringIndexer
{
  private:

  class StringCounter
  {
    string str;
    uint8_t used=0;
    friend class StringIndexer;

    #if EPOXY_DUINO
    public:
    // Workaround to avoid coredump in Indexer::release
    // when destroying a Topic after the deletion of
    // StringIndexer::strings map (which can occurs only with AUnit,
    // never in the ESP itself, because ESP never ends)
    // (I hate static vars)
    ~StringCounter() { used=255; }
    #endif
  };
  public:
    using index_t = uint8_t;

   static const string& str(const index_t& index)
   {
     static string dummy;
     const auto& it=strings.find(index);
     if (it == strings.end()) return dummy;
     return it->second.str;
   }

    static void use(const index_t& index)
    {
      auto it=strings.find(index);
      if (it != strings.end()) it->second.used++;
    }

    static void release(const index_t& index)
    {
      auto it=strings.find(index);
      if (it != strings.end())
      {
        it->second.used--;
        if (it->second.used == 0)
        {
          strings.erase(it);
          // Serial << "Removing string(" << it->second.str.c_str() << ") size=" << strings.size() << endl;
        }
      }
    }

    static uint16_t count() { return strings.size(); }

  private:
    friend class IndexedString;

    // increment use of str or create a new index
    static index_t strToIndex(const char* str, uint8_t len)
    {
      for(auto it=strings.begin(); it!=strings.end(); it++)
      {
        if (it->second.str.length() == len && strcmp(it->second.str.c_str(), str)==0)
        {
          it->second.used++;
          return it->first;
        }
      }
      for(index_t index=1; index; index++)
      {
        if (strings.find(index)==strings.end())
        {
          strings[index].str = string(str, len);
          strings[index].used++;
          // Serial << "Creating index " << index << " for (" << strings[index].str.c_str() << ") len=" << len << endl;
          return index;
        }
      }
      return 0;  // TODO out of indexes
    }

    using Strings = std::unordered_map<index_t, StringCounter>;

    static Strings strings;
};

class IndexedString
{
  public:
    IndexedString(const IndexedString& source)
    {
      StringIndexer::use(source.index);
      index = source.index;
    }

    IndexedString(IndexedString&& i) : index(i.index) {}

    IndexedString(const char* str, uint8_t len)
    {
      index=StringIndexer::strToIndex(str, len);
    }

    IndexedString(const string& str) : IndexedString(str.c_str(), str.length()) {};

    ~IndexedString() { StringIndexer::release(index); }

    IndexedString& operator=(const IndexedString& source)
    {
      StringIndexer::use(source.index);
      index = source.index;
      return *this;
    }

    friend bool operator<(const IndexedString& i1, const IndexedString& i2)
    {
      return i1.index < i2.index;
    }

    friend bool operator==(const IndexedString& i1, const IndexedString& i2)
    {
      return i1.index == i2.index;
    }

    const string& str() const { return StringIndexer::str(index); }

    const StringIndexer::index_t& getIndex() const { return index; }

  private:
    StringIndexer::index_t index;
};
