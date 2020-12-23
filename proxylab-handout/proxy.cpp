/**
 * @file proxy.cpp
 * @author Guyutongxue (1900012983@pku.edu.cn)
 * @brief
 * @version 0.1
 * @date 2020-12-23 ~ ?
 * I really don't understand why you guys are so eager
 * to make trouble for students.
 * @copyright Copyright (c) 2020
 *
 */
#include <iostream>
#include <string>

// Recommended max cache and object sizes
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

using namespace std::literals;

static constexpr const std::string_view userAgent{
    "User-Agent: "
    "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:83.0) "
    "Gecko/20100101 "
    "Firefox/83.0\r\n"sv};

int main() {
  std::cout << userAgent;
  std::cout.flush();
}