/**
 * @file proxy.cpp
 * @author Guyutongxue (1900012983@pku.edu.cn)
 * @brief
 * @version 0.1
 * @date 2020-12-23 ~ 2020-12-26
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
/**
 * @brief Case-insensitive comparison (boost::iequals)
 * works like C++20 std::string::starts_with
 * @param s The source string
 * @param t The short pattern string
 * @return Whether @c s is start with @c t
 */
bool starts_with(const std::string& s, const std::string_view& t) {
  return std::equal(t.begin(), t.end(), s.begin(),
                    [](unsigned char a, unsigned char b) {
                      return std::tolower(a) == std::tolower(b);
                    });
}
// small functions for removing trailing "\\r\\n"
// Because std::string::erase will modify original string,
// this group of functions only accept rvalue-ref as argument

/**
 * @brief Remove left-trailing white-space-character
 *
 * @param src The string to be trimmed
 * @return Trimmed string
 */
std::string ltrim(std::string&& src) {
  src.erase(src.begin(),
            std::find_if(src.begin(), src.end(),
                         [](unsigned char c) { return !std::isspace(c); }));
  return src;
}

/**
 * @brief Remove right-trailing white-space-character
 *
 * @param src The string to be trimmed
 * @return Trimmed string
 */
std::string rtrim(std::string&& src) {
  src.erase(std::find_if(src.rbegin(), src.rend(),
                         [](unsigned char c) { return !std::isspace(c); })
                .base(),
            src.end());
  return src;
}

/**
 * @brief Remove both-side-trailing white-space-character
 *
 * @param src The string to be trimmed
 * @return Trimmed string
 */
std::string trim(std::string&& src) {
  return ltrim(std::move(src)), rtrim(std::move(src)), src;
}

}  // namespace utils

/**
 * @brief User-Agent that writeup provided
 *
 */
static constexpr const std::string_view user_agent{
    "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) "
    "Gecko/20120305 "
    "Firefox/10.0.3\r\n"sv};

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

/**
 * @brief Deal with request from client
 *
 * @param connfd Connect-file-descriptor
 */
void deal(int connfd) {
  try {
    // Client-reading RIO
    csapp::Rio c_r_rio(connfd);
    // Get request line from client
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
    // Get cache
    if (auto cache_read = cache_get(uri); cache_read.has_value()) {
      std::clog << "URI \"" << uri << "\" cached. Writing...";
      csapp::Rio::writen(connfd, cache_read.value().data(),
                         cache_read.value().size());
      csapp::Close(connfd);
      std::clog << "Done" << std::endl;
      return;
    }
    auto line_info{parse_uri(uri)};
    // Get request header
    const std::string server_header = get_server_header(c_r_rio, line_info);
    // Split request line from client, and make request line to server
    const auto& [host, path, port]{line_info};
    std::clog << "Host: " << host << '\n'
              << "Path: " << path << '\n'
              << "Port: " << port << '\n';
    const std::string server_line = method + ' ' + path + " HTTP/1.0\r\n";
    std::clog << server_line << server_header << std::endl;
    // Open connection to server
    int server_fd{
        csapp::open_clientfd(host.c_str(), std::to_string(port).c_str())};
    // Server-reading RIO
    csapp::Rio s_r_rio(server_fd);
    // Send request line and request header to server
    csapp::Rio::writen(server_fd, server_line);
    csapp::Rio::writen(server_fd, server_header);
    CacheContent cache_write{};
    std::size_t cache_size{0};
    // std::string is not good for storing binary stream
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
    // Set cache
    if (cache_size < MAX_OBJECT_SIZE) {
      std::clog << "Setting cache for \"" << uri << "\"...";
      cache_set(uri, cache_write);
      std::clog << "Done." << std::endl;
    }
  } catch (const csapp::ProxyException& e) {
    // Exceptions from proxy-ish-func, like RIO etc.
    std::cerr << "Catch proxy exception: " << e.what() << std::endl;
    response_error(connfd, 500, "Internal Server Error", "Proxy: "s + e.what());
  } catch (const std::exception& e) {
    // Exceptions from other-func, like string parsing error
    std::cerr << "Catch non-proxy exception: " << e.what() << std::endl;
    response_error(connfd, 500, "Internal Server Error", e.what());
  } catch (...) {
    // Should never happened
    std::cerr << "Catch unrecognized exception." << std::endl;
    std::exit(EXIT_FAILURE);
  }
}

/**
 * @brief Parse URI to three part
 * Split URI to 3 part:
 * - Host: used in @c Host: header send to server
 * - Path: used in request line send to server
 * - Port: used in open_clientfd
 * @param uri The URI to parse
 * @return (host, path, port) tuple
 */
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

/**
 * @brief Get request header from client, while making new request header
 * Dealing with special rules on @c Host: , @c Connection: , @c User-Agent: etc.
 * @param client RIO object to read request header from client
 * @param info Parsed (host, path, port) from @c parse_uri
 * @return The request header which will be sent to server
 */
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

/**
 * @brief Returning error to client
 * If error occurs in this stage, do nothing.
 * @param connfd Client connect-file-descriptor
 * @param code HTTP status code (4** or 5**)
 * @param msg The message correspond to @c code
 * @param info More info on this error
 */
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
  try {
    csapp::Rio::writen(connfd, oss.str());
  } catch (...) {
    // Who cares???
  }
}