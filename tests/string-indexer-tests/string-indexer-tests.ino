// vim: ts=2 sw=2 expandtab
#include <Arduino.h>
#include <AUnit.h>
#include <StringIndexer.h>
#include <map>

/**
  * TinyMqtt / StringIndexer unit tests.
  *
  **/

using string = TinyConsole::string;

test(indexer_empty)
{
  assertEqual(StringIndexer::count(), 0);
}

test(indexer_strings_deleted_should_empty_indexer)
{
  assertTrue(StringIndexer::count()==0);
  {
    IndexedString one("one");
    assertEqual(StringIndexer::count(), 1);
    IndexedString two("two");
    assertEqual(StringIndexer::count(), 2);
    IndexedString three("three");
    assertEqual(StringIndexer::count(), 3);
    IndexedString four("four");
    assertEqual(StringIndexer::count(), 4);
  }
  assertEqual(StringIndexer::count(), 0);
}

test(indexer_same_strings_count_as_one)
{
  IndexedString one  ("one");
  IndexedString two  ("one");
  IndexedString three("one");
  IndexedString fourt("one");

  assertEqual(StringIndexer::count(), 1);
}

test(indexer_size_of_indexed_string)
{
  assertEqual(sizeof(IndexedString), (size_t)1);
}

test(indexer_different_strings_are_different)
{
  IndexedString one("one");
  IndexedString two("two");

  assertFalse(one == two);
}

test(indexer_same_strings_should_equal)
{
  IndexedString one("one");
  IndexedString two("one");

  assertTrue(one == two);
}

test(indexer_compare_strings_with_same_beginning)
{
  IndexedString two("one_two");
  IndexedString one("one");

  assertNotEqual(one.getIndex(), two.getIndex());
}

test(indexer_indexed_operator_eq)
{
  IndexedString one("one");
  {
    IndexedString same = one;
    assertTrue(one == same);
    assertEqual(StringIndexer::count(), 1);
  }
  assertEqual(StringIndexer::count(), 1);
}

test(indexer_get_string)
{
  string sone("one");
  IndexedString one(sone);

  assertTrue(sone==one.str());
}

test(indexer_get_index)
{
  IndexedString one1("one");
  IndexedString one2("one");
  IndexedString two1("two");
  IndexedString two2("two");

  assertTrue(one1.getIndex() == one2.getIndex());
  assertTrue(two1.getIndex() == two2.getIndex());
  assertTrue(one1.getIndex() != two1.getIndex());
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
