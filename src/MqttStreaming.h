/* MqttStreaming.h - Fork of Streaming.h adding std::string and with some minor fixes
 * (I have to speek to the author in order to include my changes to his library if possible)
 **/

/*
  Streaming.h - Arduino library for supporting the << streaming operator
  Copyright (c) 2010-2012 Mikal Hart.  All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
  Version 6 library changes
    Copyright (c) 2019 Gazoodle. All rights reserved.

  1.  _BASED moved to template to remove type conversion to long and
      sign changes which break int8_t and int16_t negative numbers. 
      The print implementation still upscales to long for it's internal
      print routine.

  2.  _PAD added to allow padding & filling of characters to the stream

  3.  _WIDTH & _WIDTHZ added to allow width printing with space padding
      and zero padding for numerics

  4.  Simple _FMT mechanism ala printf, but without the typeunsafetyness 
      and no internal buffers for replaceable stream printing
*/

#ifndef ARDUINO_STREAMING
#define ARDUINO_STREAMING

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#ifndef STREAMING_CONSOLE
#include "WProgram.h"
#endif
#endif

#include <string>

#if defined(ARDUINO_ARCH_AVR) || defined(ARDUINO_ARCH_MEGAAVR)
// No stl library, so need trivial version of std::is_signed ...
namespace std {
template<typename T>
  struct is_signed { static const bool value = false; };
  template<>
  struct is_signed<int8_t> { static const bool value = true; };
  template<>
  struct is_signed<int16_t> { static const bool value = true; };
  template<>
  struct is_signed<int32_t> { static const bool value = true; };
};
#else
#include <type_traits>
#endif

#define STREAMING_LIBRARY_VERSION 6

#if !defined(typeof)
#define typeof(x) __typeof__(x)
#endif

// PrintBuffer implementation of Print, a small buffer to print in
// see its use with pad_float()
template <size_t N>
class PrintBuffer : public Print
{
  size_t  pos = 0;
  char    str[N] {};
public:
  inline const char *operator() () 
  { return str; };
  
  // inline void clear() 
  // { pos = 0; str[0] = '\0'; };
  
  inline size_t write(uint8_t c) 
  { return write(&c, 1); };
  
  inline size_t write(const uint8_t *buffer, size_t size)
  {
    size_t s = std::min(size, N-1 - pos); // need a /0 left
    if (s)
    {
      memcpy(&str[pos], buffer, s);
      pos += s;
    }
    return s;
  };
};

// Generic template
template<class T>
inline Print &operator <<(Print &stream, const T &arg)
{ stream.print(arg); return stream; }

// TODO sfinae maybe could do the trick ?
inline Print &operator <<(Print &stream, const std::string &str)
{ stream.print(str.c_str()); return stream; }

template<typename T>
struct _BASED
{
  T val;
  int base;
  _BASED(T v, int b): val(v), base(b)
  {}
};

#if ARDUINO >= 100

struct _BYTE_CODE
{
  byte val;
  _BYTE_CODE(byte v) : val(v)
  {}
};
#define _BYTE(a)    _BYTE_CODE(a)

inline Print &operator <<(Print &obj, const _BYTE_CODE &arg)
{ obj.write(arg.val); return obj; }

#else

#define _BYTE(a)    _BASED<typeof(a)>(a, BYTE)

#endif

#define _HEX(a)     _BASED<typeof(a)>(a, HEX)
#define _DEC(a)     _BASED<typeof(a)>(a, DEC)
#define _OCT(a)     _BASED<typeof(a)>(a, OCT)
#define _BIN(a)     _BASED<typeof(a)>(a, BIN)

// Specialization for class _BASED
// Thanks to Arduino forum user Ben Combee who suggested this
// clever technique to allow for expressions like
//   Serial << _HEX(a);

template<typename T>
inline Print &operator <<(Print &obj, const _BASED<T> &arg)
{ obj.print(arg.val, arg.base); return obj; }

#if ARDUINO >= 18
// Specialization for class _FLOAT
// Thanks to Michael Margolis for suggesting a way
// to accommodate Arduino 0018's floating point precision
// feature like this:
//   Serial << _FLOAT(gps_latitude, 6); // 6 digits of precision

struct _FLOAT
{
  double val; // only Print::print(double)
  int digits;
  _FLOAT(double v, int d): val(v), digits(d)
  {}
};

inline Print &operator <<(Print &obj, const _FLOAT &arg)
{ obj.print(arg.val, arg.digits); return obj; }
#endif

// Specialization for enum _EndLineCode
// Thanks to Arduino forum user Paul V. who suggested this
// clever technique to allow for expressions like
//   Serial << "Hello!" << endl;

enum _EndLineCode { endl };

inline Print &operator <<(Print &obj, _EndLineCode)
{ obj.println(); return obj; }

// Specialization for padding & filling, mainly utilized
// by the width printers
//
//  Use like 
//    Serial << _PAD(10,' ');   // Will output 10 spaces
//    Serial << _PAD(4, '0');   // Will output 4 zeros
struct _PAD
{
  int8_t width;
  char   chr;
  _PAD(int8_t w, char c) : width(w), chr(c) {}
};

inline Print &operator <<(Print& stm, const _PAD &arg)
{
  for(int8_t i = 0; i < arg.width; i++)
    stm.print(arg.chr);
  return stm;
}

// Specialization for width printing
//
//    Use like                            Result
//    --------                            ------
//    Serial << _WIDTH(1,5)               "    1"
//    Serial << _WIDTH(10,5)              "   10"
//    Serial << _WIDTH(100,5)             "  100"
//    Serial << _WIDTHZ(1,5)              "00001"
//
//  Great for times & dates, or hex dumps
//
//    Serial << _WIDTHZ(hour,2) << ':' << _WIDTHZ(min,2) << ':' << _WIDTHZ(sec,2)
//
//    for(int index=0; index<byte_array_size; index++)
//      Serial << _WIDTHZ(_HEX(byte_array[index]))

template<typename T>
struct __WIDTH
{
  const T val;
  int8_t width;
  char pad;
  __WIDTH(const T& v, int8_t w, char p) : val(v), width(w), pad(p) {}
};

//  Count digits in an integer of specific base
template<typename T>
inline uint8_t digits(T v, int8_t base = 10)
{
  uint8_t digits = 0;
  if ( std::is_signed<T>::value )
  {
    if ( v < 0 )
    {
      digits++;
      v = -v; // v needs to be postive for the digits counter to work
    }
  }
  do
  {
    v /= base;
    digits++;
  } while( v > 0 );
  return digits;
}

// Generic get the width of a value in base 10
template<typename T>
inline uint8_t get_value_width(T val)
{ return digits(val); }

inline uint8_t get_value_width(const char * val)
{ return strlen(val); }

#ifdef ARDUINO
inline uint8_t get_value_width(const __FlashStringHelper * val)
{ return strlen_P(reinterpret_cast<const char *>(val)); }
#endif

// _BASED<T> get the width of a value
template<typename T>
inline uint8_t get_value_width(_BASED<T> b)
{ return digits(b.val, b.base); }

// Constructor wrapper to allow automatic template parameter deduction
template<typename T>
__WIDTH<T> _WIDTH(T val, int8_t width) { return __WIDTH<T>(val, width, ' '); }
template<typename T>
__WIDTH<T> _WIDTHZ(T val, int8_t width) { return __WIDTH<T>(val, width, '0'); }


// Operator overload to handle width printing.
template<typename T>
inline Print &operator <<(Print &stm, const __WIDTH<T> &arg)
{ stm << _PAD(arg.width - get_value_width(arg.val), arg.pad) << arg.val; return stm; }

// explicit Operator overload to handle width printing of _FLOAT, double and float
template<typename T>
inline Print &pad_float(Print &stm, const __WIDTH<T> &arg, const double val, const int digits = 2) // see Print::print(double, int = 2)
{
  PrintBuffer<32> buf; // it's only ~45B on the stack, no allocation, leak or fragmentation
  size_t size = buf.print(val, digits); // print in buf
  return stm << _PAD(arg.width - size, arg.pad) << buf(); // pad and concat what's in buf
}

inline Print &operator <<(Print &stm, const __WIDTH<float>  &arg) 
{ return pad_float(stm, arg, arg.val); }

inline Print &operator <<(Print &stm, const __WIDTH<double> &arg) 
{ return pad_float(stm, arg, arg.val); } 

inline Print &operator <<(Print &stm, const __WIDTH<_FLOAT> &arg) 
{ auto& f = arg.val; return pad_float(stm, arg, f.val, f.digits); }

// a less verbose _FLOATW for _WIDTH(_FLOAT)
#define _FLOATW(val, digits, width) _WIDTH<_FLOAT>(_FLOAT((val), (digits)), (width))

// Specialization for replacement formatting
//
//  Designed to be similar to printf that everyone knows and loves/hates. But without
//  the internal buffers and type agnosticism. This version only has placeholders in
//  the format string, the actual values are supplied using the stream safe operators
//  defined in this library.
//
//  Use like this:
//
//      Serial << FMT(F("Replace % with %"), 1, 2 )
//      Serial << FMT("Time is %:%:%", _WIDTHZ(hours,2), _WIDTHZ(minutes,2), _WIDTHZ(seconds,2))
//      Serial << FMT("Your score is %\\%", score);   // Note the \\ to escape the % sign

// Ok, hold your hats. This is a foray into C++11's variadic template engine ...

inline char get_next_format_char(const char *& format_string)
{
  char format_char = *format_string;
  if ( format_char > 0 ) format_string++;
  return format_char;
}

#ifdef ARDUINO
inline char get_next_format_char(const __FlashStringHelper*& format_string)
{
  char format_char = pgm_read_byte(format_string);
  if ( format_char > 0 ) format_string = reinterpret_cast<const __FlashStringHelper*>(reinterpret_cast<const char *>(format_string)+1);
  return format_char;
}
#endif

template<typename Ft>
inline bool check_backslash(char& format_char, Ft& format_string)
{
  if ( format_char == '\\')
  {
    format_char = get_next_format_char(format_string);
    return true;
  }
  return false;
}

// The template tail printer helper
template<typename Ft, typename... Ts>
struct __FMT
{
  Ft format_string;
  __FMT(Ft f, Ts ... args) : format_string(f) {}
  inline void tstreamf(Print& stm, Ft format) const
  {
    while(char c = get_next_format_char(format))
    {
      check_backslash(c, format);
      if ( c )
        stm.print(c);
    }
  }
};

// The variadic template helper
template<typename Ft, typename T, typename... Ts>
struct __FMT<Ft, T, Ts...> : __FMT<Ft, Ts...>
{
  T val;
  __FMT(Ft f, T t, Ts... ts) : __FMT<Ft, Ts...>(f, ts...), val(t) {}
  inline void tstreamf(Print& stm, Ft format) const
  {
    while(char c = get_next_format_char(format))
    {
      if (!check_backslash(c, format))
      {
        if ( c == '%')
        {
          stm << val;
          // Variadic recursion ... compiler rolls this out during 
          // template argument pack expansion
          __FMT<Ft, Ts...>::tstreamf(stm, format);
          return;
        }
      }
      if (c)
        stm.print(c);
    }
  }
};

// The actual operator should you only instanciate the FMT 
// helper with a format string and no parameters
template<typename Ft, typename... Ts>
inline Print& operator <<(Print &stm, const __FMT<Ft, Ts...> &args)
{
    args.tstreamf(stm, args.format_string);
    return stm;
}

// The variadic stream helper
template<typename Ft, typename T, typename... Ts>
inline Print& operator <<(Print &stm, const __FMT<Ft, T, Ts...> &args)
{
    args.tstreamf(stm, args.format_string);
    return stm;
}

// As we don't have C++17, we can't get a constructor to use
// automatic argument deduction, but ... this little trick gets
// around that ...
template<typename Ft, typename... Ts>
__FMT<Ft, Ts...> _FMT(Ft format, Ts ... args) { return __FMT<Ft, Ts...>(format, args...); }

#endif
