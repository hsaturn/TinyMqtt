// vim: ts=2 sw=2 expandtab
#include <Arduino.h>
#include <AUnit.h>
#include <TinyMqtt.h>
#include <map>
#include <iostream>

/**
  * TinyMqtt / StringIndexer unit tests.
  *
  **/

bool testTopicMatch(const char* a, const char* b, bool expected)
{
  Topic ta(a);
  Topic tb(b);
  bool match(ta.matches(tb));
  std::cout << "  " << ta.c_str() << ' ';
  if (match != expected)
    std::cout << (expected ? " should match " : " should not match ");
  else
    std::cout << (expected ? " matches " : " unmatches ");
  std::cout << tb.c_str() << std::endl;
  return expected == match;
}

test(topic_matches)
{
  // matching
  assertTrue(testTopicMatch("a/b/c"    , "a/b/c"     , true));
  assertTrue(testTopicMatch("a/*/c"    , "a/xyz/c"   , true));
  assertTrue(testTopicMatch("a/*/e"    , "a/b/c/d/e" , true));
  assertTrue(testTopicMatch("a/*"      , "a/b/c/d/e" , true));
  assertTrue(testTopicMatch("*/c"      , "a/b/c"     , true));
  assertTrue(testTopicMatch("/*/c"     , "/a/b/c"    , true));
  assertTrue(testTopicMatch("a/*"      , "a/b/c/d"   , true));
  assertTrue(testTopicMatch("a/+/c"    , "a/b/c"     , true));
  assertTrue(testTopicMatch("a/+/c/+/e", "a/b/c/d/e" , true));
  assertTrue(testTopicMatch("a/*/c/+/e", "a/b/c/d/e" , true));
  assertTrue(testTopicMatch("/+/b"     , "/a/b"      , true));
  assertTrue(testTopicMatch("+"        , "a"         , true));
  assertTrue(testTopicMatch("a/b/#"    , "a/b/c/d"   , true));
  assertTrue(testTopicMatch("a/b/#"    , "a/b"       , true));
  assertTrue(testTopicMatch("a/*/c"    , "a/*/c"     , true));

  // not matching
  assertTrue(testTopicMatch("a/b/c"    , "a/b/d"     , false));
  assertTrue(testTopicMatch("a/b/c"    , "a/b/d"     , false));
  assertTrue(testTopicMatch("a/*/e"    , "a/b/c/d/f" , false));
  assertTrue(testTopicMatch("a/+"      , "a"         , false));
  assertTrue(testTopicMatch("a/+"      , "a/b/d"     , false));
  assertTrue(testTopicMatch("a/+/"     , "a/"        , false));

  // $SYS topics
  assertTrue(testTopicMatch("+/any"    , "$SYS/any"  , false));
  assertTrue(testTopicMatch("*/any"    , "$SYS/any"  , false));
  assertTrue(testTopicMatch("$SYS/any" , "$SYS/any"  , true));
  assertTrue(testTopicMatch("$SYS/+/y" , "$SYS/a/y"  , true));
  assertTrue(testTopicMatch("$SYS/#"   , "$SYS/a/y"  , true));

  // not valid
  assertTrue(testTopicMatch("a/#/b"    , "a/x/b"     , false));
  assertTrue(testTopicMatch("a+"      , "a/b/d"     , false));
  assertTrue(testTopicMatch("a/b/#/d"  , "a/b/c/d"   , false));

}

//----------------------------------------------------------------------------
// setup() and loop()
void setup() {
  delay(1000);
  Serial.begin(115200);
  while(!Serial);

  Serial.println("=============[ TinyMqtt StringIndexer TESTS ]========================");
}

void loop() {
  aunit::TestRunner::run();

  // if (Serial.available()) ESP.reset();
}
