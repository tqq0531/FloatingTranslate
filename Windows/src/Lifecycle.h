#pragma once
#include <atomic>
#include <windows.h>
namespace fuyi {
extern std::atomic<LONG> objects;
struct ObjectLifetime {
    ObjectLifetime() {
        ++objects;
    }
    ~ObjectLifetime() {
        --objects;
    }
    ObjectLifetime(const ObjectLifetime &) = delete;
    ObjectLifetime &operator=(const ObjectLifetime &) = delete;
};
} // namespace fuyi
