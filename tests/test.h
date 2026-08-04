// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdlib>
#include <exception>
#include <iostream>
#include <type_traits>
#include <utility>

#define CHECK(expression)                                                     \
  do                                                                          \
  {                                                                           \
    if (!(expression))                                                        \
    {                                                                         \
      std::cerr << __FILE__ << ':' << __LINE__                                \
                << ": check failed: " #expression << '\n';                   \
      std::exit(EXIT_FAILURE);                                                 \
    }                                                                         \
  } while (false)

template<class Exception, class Callable>
void
check_throws(Callable&& callable)
{
  static_assert(std::is_base_of_v<std::exception, Exception>);

  try
  {
    std::forward<Callable>(callable)();
  }
  catch (const Exception&)
  {
    return;
  }
  catch (const std::exception& error)
  {
    std::cerr << "unexpected exception type: " << error.what() << '\n';
    std::exit(EXIT_FAILURE);
  }

  std::cerr << "expected exception was not thrown\n";
  std::exit(EXIT_FAILURE);
}

#define TEST(expression) CHECK(expression)
#define TEST_EQ(lhs, rhs) CHECK((lhs) == (rhs))
#define TEST_NE(lhs, rhs) CHECK((lhs) != (rhs))
#define TEST_THROWS(Exception, ...)                                            \
  check_throws<Exception>([&] { (void)(__VA_ARGS__); })
