/**
 * @file csapp2.h
 * @author Guyutongxue (guyutongxue@163.com)
 * @brief A C++ version of original @c csapp.h
 * @date 2020-12-25
 * This file use some C++ feature to provide more easy-to-use interface
 * @copyright Copyright (c) 2020
 * 
 */

// This file is distributed under WTFPL <https://www.wtfpl.net> 
// WITHOUT ANY WARRANTY.

#ifndef CSAPP2_H
#define CSAPP2_H

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <tuple>

namespace csapp {

/* Default file permissions are DEF_MODE & ~DEF_UMASK */

constexpr const int DEF_MODE{S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH |
                             S_IWOTH};
constexpr const int DEF_UMASK{S_IWGRP | S_IWOTH};

/* Simplifies calls to bind(), connect(), and accept() */
using SA = struct sockaddr;

/* Persistent state for the robust I/O (Rio) package */
struct rio_t;

/* External variables */
// extern int h_errno;    /* Defined by BIND for DNS errors */
extern char** environ; /* Defined by libc */

/* Misc constants */
constexpr const size_t MAXLINE{8192}; /* Max text line length */
constexpr const size_t MAXBUF{8192};  /* Max I/O buffer size */
constexpr const int LISTENQ{1024};    /* Second argument to listen() */

/* Our own error-handling functions */
[[noreturn]] void unix_error(const char* msg);
[[noreturn]] void posix_error(int code, const char* msg);
[[noreturn]] void dns_error(const char* msg);
[[noreturn]] void gai_error(int code, const char* msg);
[[noreturn]] void app_error(const char* msg);

/* Process control wrappers */
pid_t Fork();
void Execve(const char* filename, char* const* argv, char* const* envp);
pid_t Wait(int* status);
pid_t Waitpid(pid_t pid, int* iptr, int options);
void Kill(pid_t pid, int signum);
unsigned int Sleep(unsigned int secs);
void Pause();
unsigned int Alarm(unsigned int seconds);
void Setpgid(pid_t pid, pid_t pgid);
pid_t Getpgrp();

/* Signal wrappers */
typedef void handler_t(int);
handler_t* Signal(int signum, handler_t* handler);
void Sigprocmask(int how, const sigset_t* set, sigset_t* oldset);
void Sigemptyset(sigset_t* set);
void Sigfillset(sigset_t* set);
void Sigaddset(sigset_t* set, int signum);
void Sigdelset(sigset_t* set, int signum);
int Sigismember(const sigset_t* set, int signum);
int Sigsuspend(const sigset_t* set);

/* Sio (Signal-safe I/O) routines */
ssize_t sio_puts(const char* s);
ssize_t sio_putl(long v);
void sio_error(const char* s);

/* Sio wrappers */
ssize_t Sio_puts(const char* s);
ssize_t Sio_putl(long v);
void Sio_error(const char* s);

/* Unix I/O wrappers */
int Open(const char* pathname, int flags, mode_t mode);
ssize_t Read(int fd, void* buf, size_t count);
ssize_t Write(int fd, const void* buf, size_t count);
off_t Lseek(int fildes, off_t offset, int whence);
void Close(int fd);
int Select(int n, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
           struct timeval* timeout);
int Dup2(int fd1, int fd2);
void Stat(const char* filename, struct stat* buf);
void Fstat(int fd, struct stat* buf);

/* Directory wrappers */
DIR* Opendir(const char* name);
struct dirent* Readdir(DIR* dirp);
int Closedir(DIR* dirp);

/* Memory mapping wrappers */
void* Mmap(void* addr, size_t len, int prot, int flags, int fd, off_t offset);
void Munmap(void* start, size_t length);

/* Standard I/O wrappers */
void Fclose(FILE* fp);
FILE* Fdopen(int fd, const char* type);
char* Fgets(char* ptr, int n, FILE* stream);
FILE* Fopen(const char* filename, const char* mode);
void Fputs(const char* ptr, FILE* stream);
size_t Fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
void Fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);

/* Dynamic storage allocation wrappers */
void* Malloc(size_t size);
void* Realloc(void* ptr, size_t size);
void* Calloc(size_t nmemb, size_t size);
void Free(void* ptr);

/* Sockets interface wrappers */
int Socket(int domain, int type, int protocol);
void Setsockopt(int s, int level, int optname, const void* optval, int optlen);
template <typename SockAddrT>
void Bind(int sockfd, const SockAddrT& my_sock);
void Listen(int s, int backlog);
template <typename SockAddrT>
int Accept(int s, SockAddrT& addr, socklen_t* socklen = nullptr);
template <typename SockAddrT>
void Connect(int sockfd, const SockAddrT& serv);

/* Protocol independent wrappers */
void Getaddrinfo(const char* node, const char* service,
                 const struct addrinfo* hints, struct addrinfo** res);
template <typename SockAddrT>
auto GetnameInfo(const SockAddrT& sa) -> std::pair<std::string, std::string>;
void Freeaddrinfo(struct addrinfo* res);
void Inet_ntop(int af, const void* src, char* dst, socklen_t size);
void Inet_pton(int af, const char* src, void* dst);

/* DNS wrappers */
#ifdef USE_UNSAFE_FUNC
struct hostent* Gethostbyname(const char* name);
struct hostent* Gethostbyaddr(const char* addr, int len, int type);
#endif

/* Pthreads thread control wrappers */
void Pthread_create(pthread_t* tidp, pthread_attr_t* attrp,
                    void* (*routine)(void*), void* argp);
void Pthread_join(pthread_t tid, void** thread_return);
void Pthread_cancel(pthread_t tid);
void Pthread_detach(pthread_t tid);
void Pthread_exit(void* retval);
pthread_t Pthread_self(void);
void Pthread_once(pthread_once_t* once_control, void (*init_function)());

/* POSIX semaphore wrappers */
void Sem_init(sem_t* sem, int pshared, unsigned int value);
void P(sem_t* sem);
void V(sem_t* sem);

/* Rio (Robust I/O) package */

static constexpr const size_t RIO_BUFSIZE{8192};

struct rio_t {
  int rio_fd;                /* Descriptor for this internal buf */
  int rio_cnt;               /* Unread bytes in internal buf */
  char* rio_bufptr;          /* Next unread byte in internal buf */
  char rio_buf[RIO_BUFSIZE]; /* Internal buffer */
};

// Class wrapper for RIO package
class Rio {
 private:
  rio_t rio;

 public:
  static size_t readn(int fd, char* s, size_t bytes);
  static void writen(int fd, const char* s, size_t bytes);
  static void writen(int fd, const std::string_view& s);
  Rio(int fd);
  size_t readnb(char* s, size_t bytes);
  std::string readnb(size_t bytes);
  size_t readlineb(char* s, size_t maxlen);
  std::string readlineb(size_t maxlen);
};

class ProxyException : public std::exception {
 private:
  std::string msg;

 public:
  ProxyException(const std::string& msg) : msg{msg} {}
  ~ProxyException() {}
  const char* what() const noexcept { return msg.c_str(); }
};

/* Reentrant protocol-independent client/server helpers */
int open_clientfd(const char* hostname, const char* port);
int open_listenfd(const char* port);

/* Wrappers for reentrant protocol-independent client/server helpers */
int Open_clientfd(const char* hostname, const char* port);
int Open_listenfd(const char* port);

// Template function implementation

template <typename SockAddrT>
void Bind(int sockfd, const SockAddrT& my_sock) {
  if (int rc =
          bind(sockfd, reinterpret_cast<const SA*>(&my_sock), sizeof(my_sock));
      rc < 0)
    unix_error("Bind error");
}

template <typename SockAddrT>
int Accept(int s, SockAddrT& addr, socklen_t* socklen) {
  int rc;
  socklen_t len{sizeof(addr)};
  if (!socklen) socklen = &len;
  if ((rc = accept(s, reinterpret_cast<SA*>(&addr), socklen)) < 0)
    unix_error("Accept error");
  return rc;
}

template <typename SockAddrT>
void Connect(int sockfd, const SockAddrT& serv) {
  if (int rc =
          connect(sockfd, reinterpret_cast<const SA*>(&serv), sizeof(serv));
      rc < 0)
    unix_error("Connect error");
}

template <typename SockAddrT>
auto Getnameinfo(const SockAddrT& sa, int flags) {
  int rc;
  char host[MAXLINE];
  char serv[MAXLINE];
  if ((rc = getnameinfo(reinterpret_cast<const SA*>(&sa), sizeof(sa), host,
                        MAXLINE, serv, MAXLINE, flags)) != 0)
    gai_error(rc, "Getnameinfo error");
  return std::make_pair<std::string, std::string>(host, serv);
}

}  // namespace csapp

#endif  // CSAPP2_H