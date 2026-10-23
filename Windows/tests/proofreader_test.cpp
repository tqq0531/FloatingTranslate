#include "../src/EnglishProofreader.h"
#include <cassert>
#include <iostream>
int main(int argc,char** argv){
 assert(argc==2);fuyi::EnglishProofreader p;assert(p.load(std::filesystem::u8path(argv[1])));
 assert(p.check(L"I wnat go to teh gym now ").corrected==L"I want to go to the gym now ");
 assert(p.check(L"I want go to gym now").corrected==L"I want to go to the gym now");
 assert(p.check(L"I want to go to the gym now").corrected==L"I want to go to the gym now");
 assert(p.check(L"I work at OpenAI now").corrected==L"I work at OpenAI now");
 assert(p.check(L"https://example.com/teh").corrected==L"https://example.com/teh");
 assert(p.check(L"I am using foobar_123").corrected==L"I am using foobar_123");
 assert(p.check(L"recieve ").corrected==L"receive ");
 assert(p.check(L"wnat").corrected==L"wnat"); // Last unfinished word: no spell edit.
 std::cout<<"Dictionary-backed spelling, grammar, URL/code/name skips and incomplete-word checks passed\n";
}
