#include "../src/Core.h"
#include <cassert>
#include <iostream>
using namespace fuyi;
int main() {
    Sentence s;
    s.begin(100);
    s.append(L"我", 100);
    s.begin(200);
    s.append(L"今天", 200);
    s.begin(300);
    s.append(L"去北京", 300);
    assert(s.text == L"我今天去北京");
    assert(!s.expired(60300));
    assert(s.expired(60301));
    s.append(L"。", 400);
    assert(s.finished);
    assert(s.expired(8401));
    s.begin(500);
    assert(s.text.empty());
    s.append(L"你好", 600);
    s.begin(700, true);
    assert(s.text.empty());
    ShiftTap shift;
    shift.down(0, false);
    assert(shift.up(200, false));
    shift.down(1000, false);
    shift.key();
    assert(!shift.up(1100, false));
    shift.down(2000, true);
    assert(!shift.up(2100, false));
    shift.down(3000, false);
    assert(!shift.up(4000, false));
    auto r = place({1900, 1000, 1901, 1020}, 500, 150, {0, 0, 1920, 1080}, false);
    assert(r && r->left == 1416 && r->top == 847);
    r = place({-1900, 20, -1899, 40}, 500, 150, {-1920, 0, 0, 1080}, true);
    assert(r && r->left == -1900 && r->top == 43);
    assert(!place({0, 0, 0, 0}, 500, 150, {0, 0, 1920, 1080}, false));
    std::cout << "Core sentence, expiry, Shift and multi-monitor placement checks passed\n";
}
