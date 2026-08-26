#pragma region LICENSE

// MIT License
//
// Copyright (c) 2026 Rohan Bharatia
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma endregion LICENSE

#pragma once

#ifndef _PCH_H_
    #define _PCH_H_ (1)

// C++ standard version detection
#ifdef _MSC_VER
    #define CPP_VERSION (_MSVC_LANG)
#else // (NOT) _MSC_VER
    #define CPP_VERSION (__cplusplus)
#endif // _MSC_VER
#define CPP98 (199711L)
#define CPP11 (201103L)
#define CPP14 (201402L)
#define CPP17 (201703L)
#define CPP20 (202002L)
#define CPP23 (202302L)
#define CHECK_CPP_VERSION(version) \
    (CPP_VERSION >= version)

#if !CHECK_CPP_VERSION(CPP20)
    #error "ChromeLab requires C++20 or higher!"
#endif // !CHECK_CPP_VERSION(CPP20)

// C/C++ standard library headers
#ifndef _GLIBCXX_NO_ASSERT
    #include <cassert>
#endif // _GLIBCXX_NO_ASSERT
#include <cctype>
#include <cerrno>
#include <cfloat>
#include <climits>
#include <clocale>
#include <cmath>
#include <csetjmp>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <cwctype>
#if CHECK_CPP_VERSION(CPP11)
    #include <cfenv>
    #include <cinttypes>
    #include <cstdint>
    #include <cuchar>
#endif // CHECK_CPP_VERSION(CPP11)
#include <algorithm>
#include <bitset>
#include <complex>
#include <deque>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <iterator>
#include <limits>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <new>
#include <numeric>
#include <ostream>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <typeinfo>
#include <utility>
#include <valarray>
#include <vector>
#if CHECK_CPP_VERSION(CPP11)
    #include <array>
    #include <atomic>
    #include <chrono>
    #include <codecvt>
    #include <condition_variable>
    #include <forward_list>
    #include <future>
    #include <initializer_list>
    #include <mutex>
    #include <random>
    #include <ratio>
    #include <regex>
    #include <scoped_allocator>
    #include <system_error>
    #include <thread>
    #include <tuple>
    #include <typeindex>
    #include <type_traits>
    #include <unordered_map>
    #include <unordered_set>
#endif // CHECK_CPP_VERSION(CPP11)
#if CHECK_CPP_VERSION(CPP14)
    #include <shared_mutex>
#endif // CHECK_CPP_VERSION(CPP14)
#if CHECK_CPP_VERSION(CPP17)
    #include <any>
    #include <charconv>
    #include <execution>
    #include <filesystem>
    #include <optional>
    #include <memory_resource>
    #include <string_view>
    #include <variant>
#endif // CHECK_CPP_VERSION(CPP17)
#if CHECK_CPP_VERSION(CPP20)
    #include <barrier>
    #include <bit>
    #include <compare>
    #include <concepts>
    #if defined(__cpp_impl_coroutine)
        #include <coroutine>
    #endif // defined(__cpp_impl_coroutine)
    #include <latch>
    #include <numbers>
    #include <ranges>
    #include <span>
    #include <stop_token>
    #include <semaphore>
    #include <source_location>
    #include <syncstream>
    #include <version>
#endif // CHECK_CPP_VERSION(CPP20)

#ifdef __linux__
    #include <signal.h>
    #include <unistd.h>
#else // (NOT) __linux__
    #error "Chromelab only works on Linux-based operating systems!"
#endif // __linux__

// External library headers
#ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS
#endif // _CRT_SECURE_NO_WARNINGS
#include <grpcpp/grpcpp.h>
#include <toml++/toml.hpp>

// Lab library headers
#include "labd.grpc.pb.h"

#endif // _PCH_H_
