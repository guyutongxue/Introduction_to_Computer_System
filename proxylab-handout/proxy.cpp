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
#include <algorithm>
#include <iostream>
#include <sstream>
#include <thread>

#include "./cache.h"
#include "./csapp2.h"

using namespace std::literals;
using csapp::MAXLINE;

namespace utils {
// Case-insensitive comparison (boost::iequals)
// C++20 std::string::starts_with
bool starts_with(const std::string& s, const std::string_view& t) {
  return std::equal(t.begin(), t.end(), s.begin(),
                    [](unsigned char a, unsigned char b) {
                      return std::tolower(a) == std::tolower(b);
                    });
}
// small functions for removing trailing "\\r\\n"
std::string ltrim(std::string&& src) {
  src.erase(src.begin(),
            std::find_if(src.begin(), src.end(),
                         [](unsigned char c) { return !std::isspace(c); }));
  return src;
}
std::string rtrim(std::string&& src) {
  src.erase(std::find_if(src.rbegin(), src.rend(),
                         [](unsigned char c) { return !std::isspace(c); })
                .base(),
            src.end());
  return src;
}
std::string trim(std::string&& src) {
  return ltrim(std::move(src)), rtrim(std::move(src)), src;
}

}  // namespace utils

static constexpr const std::string_view user_agent{
    "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:83.0) "
    "Gecko/20100101 "
    "Firefox/83.0\r\n"sv};

void deal(int);
auto parse_uri(const std::string& uri)
    -> std::tuple<std::string, std::string, std::uint16_t>;
std::string get_server_header(
    csapp::Rio& client,
    std::tuple<std::string, std::string, std::uint16_t>& info);
void response_error(int fd, int code, const std::string_view& msg,
                    const std::string& info = "");

int main(int argc, char** argv) {
  csapp::Signal(SIGPIPE, SIG_IGN);
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " <port>" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  int listenfd{csapp::Open_listenfd(argv[1])};
  std::clog << "Start listening on port " << argv[1] << std::endl;
  while (true) {
    sockaddr_storage client_addr;
    int connfd{csapp::Accept(listenfd, client_addr)};
    const auto [host, port]{csapp::Getnameinfo(client_addr, 0)};
    std::cout << "Accepted connection from " << host << ":" << port
              << std::endl;
    std::thread(deal, connfd).detach();
  }
}

void deal(int connfd) {
  try {
    // Client reading RIO
    csapp::Rio c_r_rio(connfd);
    // Get request line
    std::istringstream iss(c_r_rio.readlineb(MAXLINE));
    std::string method, uri, version;
    iss >> method >> uri >> version;
    if (iss.fail()) {
      response_error(connfd, 400, "Bad Request");
      return;
    }
    std::clog << "Method : " << method << '\n';
    std::clog << "URI    : " << uri << '\n';
    std::clog << "Version: " << version << '\n';
    if (method != "GET") {
      response_error(connfd, 501, "Not Implemented",
                     "This proxy cannot deal with Non-GET requests.");
      return;
    }
    auto cache_read = cache_get(uri);
    if (cache_read.has_value()) {
      csapp::Rio::writen(connfd, cache_read.value().data(),
                         cache_read.value().size());
    }
    auto line_info{parse_uri(uri)};
    // Get request header
    const std::string server_header = get_server_header(c_r_rio, line_info);
    auto& [host, path, port]{line_info};
    std::clog << "Host: " << host << '\n'
              << "Path: " << path << '\n'
              << "Port: " << port << '\n';
    const std::string server_line = method + ' ' + path + " HTTP/1.0\r\n";
    std::clog << server_line << server_header << std::endl;
    int server_fd{
        csapp::open_clientfd(host.c_str(), std::to_string(port).c_str())};
    csapp::Rio s_r_rio(server_fd);
    csapp::Rio::writen(server_fd, server_line);
    csapp::Rio::writen(server_fd, server_header);
    // std::string is not good for storing binary stream
    CacheContent cache_write{};
    std::size_t cache_size{0};
    for (std::array<char, MAXLINE> line{};
         size_t size = s_r_rio.readlineb(line.data(), MAXLINE);) {
      std::clog << "Recieve " << size << " bytes\n";
      csapp::Rio::writen(connfd, line.data(), size);
      if (cache_size + size < MAX_OBJECT_SIZE) {
        std::copy(line.begin(), line.begin() + size,
                  cache_write.begin() + cache_size);
        cache_size += size;
      }
    }
    csapp::Close(server_fd);
    csapp::Close(connfd);
  } catch (const csapp::ProxyException& e) {
    std::cerr << "Catch proxy exception: " << e.what() << std::endl;
    response_error(connfd, 500, "Internal Server Error", "Proxy: "s + e.what());
  } catch (const std::exception& e) {
    std::cerr << "Catch non-proxy exception: " << e.what() << std::endl;
    response_error(connfd, 500, "Internal Server Error", e.what());
  } catch (...) {
    std::cerr << "Catch unrecognized exception." << std::endl;
    std::exit(EXIT_FAILURE);
  }
}

auto parse_uri(const std::string& uri)
    -> std::tuple<std::string, std::string, std::uint16_t> {
  std::uint16_t port{80};
  std::string host{uri};
  std::string path("/");
  auto double_slash_pos{uri.find("//")};
  if (double_slash_pos != std::string::npos) {
    host = uri.substr(double_slash_pos + 2);
  }
  auto slash_pos{host.find('/')};
  if (slash_pos != std::string::npos) {
    path = host.substr(slash_pos);
  }
  host = host.substr(0, slash_pos);
  auto colon_pos(host.find(':'));
  if (colon_pos != std::string::npos) {
    port = std::stoi(host.substr(colon_pos + 1));
    host = host.substr(0, colon_pos);
  }
  return std::make_tuple(host, path, port);
}

std::string get_server_header(
    csapp::Rio& client,
    std::tuple<std::string, std::string, std::uint16_t>& info) {
  std::ostringstream oss;

  bool has_host{false};
  for (std::string line;
       (line = utils::rtrim(client.readlineb(MAXLINE))).size();) {
    if (utils::starts_with(line, "Host:"sv)) {
      has_host = true;
    }
    if (!utils::starts_with(line, "Connection:"sv) &&
        !utils::starts_with(line, "Proxy-Connection:"sv) &&
        !utils::starts_with(line, "User-Agent:"sv)) {
      oss << line << "\r\n";
    }
  }
  if (!has_host) {
    oss << std::get<0>(info) << "\r\n";
  }
  oss << "Connection: close\r\n"
      << "Proxy-Connection: close\r\n"
      << "User-Agent: " << user_agent << "\r\n";
  return oss.str();
}

void response_error(int connfd, int code, const std::string_view& msg,
                    const std::string& info) {
  std::ostringstream oss;
  oss << "HTTP/1.0 " << code << " " << msg << "\r\n";
  std::ostringstream content;
  content << R"(<!DOCTYPE html>
<html>
<head>
  <title> Proxy Error </title>
</head>
<body>
  <h1> )" << code
          << " " << msg << R"( </h1>
  <p>)" << info
          << R"( </p>
  <hr>
  CS:APP ProxyLab (Ubuntu 20.04)
</body>
</html>
)";
  const std::string content_str{content.str()};
  oss << "Content-Type: text/html\r\n";
  oss << "Content-Length: " << content_str.size() << "\r\n";
  oss << "\r\n";
  oss << content_str << std::endl;
  csapp::Rio::writen(connfd, oss.str());
}