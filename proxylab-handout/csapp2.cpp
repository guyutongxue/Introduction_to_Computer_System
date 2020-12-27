/**
 * @file csapp2.cpp
 * @author Guyutongxue (guyutongxue@163.com)
 * @brief A C++ version of original @c csapp.c
 * @date 2020-12-25
 * This file use some C++ feature to provide more easy-to-use interface
 * @copyright Copyright (c) 2020 Guyutongxue
 *
 */

// This file is distributed under WTFPL <https://www.wtfpl.net> 
// WITHOUT ANY WARRANTY.

#include "./csapp2.h"

namespace csapp {

using namespace std::literals;

/**************************
 * Error-handling functions
 **************************/
/* Unix-style error */
[[noreturn]] void unix_error(const char* msg) {
  throw SystemException(msg + ": "s + strerror(errno));
}

/* Posix-style error */
[[noreturn]] void posix_error(int code, const char* msg) {
  throw SystemException(msg + ": "s + strerror(code));
}

/* Getaddrinfo-style error */
[[noreturn]] void gai_error(int code, const char* msg) {
  throw GaiException(msg + ": "s + gai_strerror(code), code);
}

/* Application error */
[[noreturn]] void app_error(const char* msg) { throw SystemException(msg); }

/* Obsolete gethostbyname error */
[[noreturn]] void dns_error(const char* msg) { throw SystemException(msg); }

/*********************************************
 * Wrappers for Unix process control functions
 ********************************************/

pid_t Fork() {
  pid_t pid;

  if ((pid = fork()) < 0) unix_error("Fork error");
  return pid;
}

void Execve(const char* filename, char* const* argv, char* const* envp) {
  if (execve(filename, argv, envp) < 0) unix_error("Execve error");
}

pid_t Wait(int* status) {
  pid_t pid;

  if ((pid = wait(status)) < 0) unix_error("Wait error");
  return pid;
}

pid_t Waitpid(pid_t pid, int* iptr, int options) {
  pid_t retpid;

  if ((retpid = waitpid(pid, iptr, options)) < 0) unix_error("Waitpid error");
  return (retpid);
}

void Kill(pid_t pid, int signum) {
  int rc;

  if ((rc = kill(pid, signum)) < 0) unix_error("Kill error");
}

void Pause() {
  pause();
  return;
}

unsigned int Sleep(unsigned int secs) {
  unsigned int rc;

  if (static_cast<signed>(rc = sleep(secs)) < 0) unix_error("Sleep error");
  return rc;
}

unsigned int Alarm(unsigned int seconds) { return alarm(seconds); }

void Setpgid(pid_t pid, pid_t pgid) {
  int rc;

  if ((rc = setpgid(pid, pgid)) < 0) unix_error("Setpgid error");
  return;
}

pid_t Getpgrp() { return getpgrp(); }

/************************************
 * Wrappers for Unix signal functions
 ***********************************/

handler_t* Signal(int signum, handler_t* handler) {
  struct sigaction action, old_action;

  action.sa_handler = handler;
  sigemptyset(&action.sa_mask); /* Block sigs of type being handled */
  action.sa_flags = SA_RESTART; /* Restart syscalls if possible */

  if (sigaction(signum, &action, &old_action) < 0) unix_error("Signal error");
  return (old_action.sa_handler);
}

void Sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
  if (sigprocmask(how, set, oldset) < 0) unix_error("Sigprocmask error");
  return;
}

void Sigemptyset(sigset_t* set) {
  if (sigemptyset(set) < 0) unix_error("Sigemptyset error");
  return;
}

void Sigfillset(sigset_t* set) {
  if (sigfillset(set) < 0) unix_error("Sigfillset error");
  return;
}

void Sigaddset(sigset_t* set, int signum) {
  if (sigaddset(set, signum) < 0) unix_error("Sigaddset error");
  return;
}

void Sigdelset(sigset_t* set, int signum) {
  if (sigdelset(set, signum) < 0) unix_error("Sigdelset error");
  return;
}

int Sigismember(const sigset_t* set, int signum) {
  int rc;
  if ((rc = sigismember(set, signum)) < 0) unix_error("Sigismember error");
  return rc;
}

int Sigsuspend(const sigset_t* set) {
  int rc = sigsuspend(set); /* always returns -1 */
  if (errno != EINTR) unix_error("Sigsuspend error");
  return rc;
}

/*************************************************************
 * The Sio (Signal-safe I/O) package - simple reentrant output
 * functions that are safe for signal handlers.
 *************************************************************/

/* Private sio functions */

/* sio_reverse - Reverse a string (from K&R) */
static void sio_reverse(char* s) {
  int c, i, j;

  for (i = 0, j = strlen(s) - 1; i < j; i++, j--) {
    c = s[i];
    s[i] = s[j];
    s[j] = c;
  }
}

/* sio_ltoa - Convert long to base b string (from K&R) */
static void sio_ltoa(long v, char* s, int b) {
  int c, i = 0;

  do {
    s[i++] = ((c = (v % b)) < 10) ? c + '0' : c - 10 + 'a';
  } while ((v /= b) > 0);
  s[i] = '\0';
  sio_reverse(s);
}

/* sio_strlen - Return length of string (from K&R) */
static size_t sio_strlen(const char* s) {
  int i = 0;

  while (s[i] != '\0') ++i;
  return i;
}

/* Public Sio functions */

ssize_t sio_puts(const char* s) /* Put string */
{
  return write(STDOUT_FILENO, s, sio_strlen(s));  // line:csapp:siostrlen
}

ssize_t sio_putl(long v) /* Put long */
{
  char s[128];

  sio_ltoa(v, s, 10); /* Based on K&R itoa() */  // line:csapp:sioltoa
  return sio_puts(s);
}

void sio_error(const char* s) /* Put error message and exit */
{
  sio_puts(s);
  _exit(1);  // line:csapp:sioexit
}

/*******************************
 * Wrappers for the SIO routines
 ******************************/
ssize_t Sio_putl(long v) {
  ssize_t n;

  if ((n = sio_putl(v)) < 0) sio_error("Sio_putl error");
  return n;
}

ssize_t Sio_puts(const char* s) {
  ssize_t n;

  if ((n = sio_puts(s)) < 0) sio_error("Sio_puts error");
  return n;
}

void Sio_error(const char* s) { sio_error(s); }

/********************************
 * Wrappers for Unix I/O routines
 ********************************/

int Open(const char* pathname, int flags, mode_t mode) {
  int rc;

  if ((rc = open(pathname, flags, mode)) < 0) unix_error("Open error");
  return rc;
}

ssize_t Read(int fd, void* buf, size_t count) {
  ssize_t rc;

  if ((rc = read(fd, buf, count)) < 0) unix_error("Read error");
  return rc;
}

ssize_t Write(int fd, const void* buf, size_t count) {
  ssize_t rc;

  if ((rc = write(fd, buf, count)) < 0) unix_error("Write error");
  return rc;
}

off_t Lseek(int fildes, off_t offset, int whence) {
  off_t rc;

  if ((rc = lseek(fildes, offset, whence)) < 0) unix_error("Lseek error");
  return rc;
}

void Close(int fd) {
  int rc;

  if ((rc = close(fd)) < 0) unix_error("Close error");
}

int Select(int n, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
           struct timeval* timeout) {
  int rc;

  if ((rc = select(n, readfds, writefds, exceptfds, timeout)) < 0)
    unix_error("Select error");
  return rc;
}

int Dup2(int fd1, int fd2) {
  int rc;

  if ((rc = dup2(fd1, fd2)) < 0) unix_error("Dup2 error");
  return rc;
}

void Stat(const char* filename, struct stat* buf) {
  if (stat(filename, buf) < 0) unix_error("Stat error");
}

void Fstat(int fd, struct stat* buf) {
  if (fstat(fd, buf) < 0) unix_error("Fstat error");
}

/*********************************
 * Wrappers for directory function
 *********************************/

DIR* Opendir(const char* name) {
  DIR* dirp = opendir(name);

  if (!dirp) unix_error("opendir error");
  return dirp;
}

struct dirent* Readdir(DIR* dirp) {
  struct dirent* dep;

  errno = 0;
  dep = readdir(dirp);
  if ((dep == NULL) && (errno != 0)) unix_error("readdir error");
  return dep;
}

int Closedir(DIR* dirp) {
  int rc;

  if ((rc = closedir(dirp)) < 0) unix_error("closedir error");
  return rc;
}

/***************************************
 * Wrappers for memory mapping functions
 ***************************************/
void* Mmap(void* addr, size_t len, int prot, int flags, int fd, off_t offset) {
  void* ptr;

  if ((ptr = mmap(addr, len, prot, flags, fd, offset)) == ((void*)-1))
    unix_error("mmap error");
  return (ptr);
}

void Munmap(void* start, size_t length) {
  if (munmap(start, length) < 0) unix_error("munmap error");
}

/***************************************************
 * Wrappers for dynamic storage allocation functions
 ***************************************************/

void* Malloc(size_t size) {
  void* p;

  if ((p = std::malloc(size)) == NULL) unix_error("Malloc error");
  return p;
}

void* Realloc(void* ptr, size_t size) {
  void* p;

  if ((p = std::realloc(ptr, size)) == NULL) unix_error("Realloc error");
  return p;
}

void* Calloc(size_t nmemb, size_t size) {
  void* p;

  if ((p = std::calloc(nmemb, size)) == NULL) unix_error("Calloc error");
  return p;
}

void Free(void* ptr) { free(ptr); }

/******************************************
 * Wrappers for the Standard I/O functions.
 ******************************************/
void Fclose(FILE* fp) {
  if (fclose(fp) != 0) unix_error("Fclose error");
}

FILE* Fdopen(int fd, const char* type) {
  FILE* fp;

  if ((fp = fdopen(fd, type)) == NULL) unix_error("Fdopen error");

  return fp;
}

char* Fgets(char* ptr, int n, FILE* stream) {
  char* rptr;

  if (((rptr = fgets(ptr, n, stream)) == NULL) && ferror(stream))
    app_error("Fgets error");

  return rptr;
}

FILE* Fopen(const char* filename, const char* mode) {
  FILE* fp;

  if ((fp = fopen(filename, mode)) == NULL) unix_error("Fopen error");

  return fp;
}

void Fputs(const char* ptr, FILE* stream) {
  if (fputs(ptr, stream) == EOF) unix_error("Fputs error");
}

size_t Fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
  size_t n;

  if (((n = fread(ptr, size, nmemb, stream)) < nmemb) && ferror(stream))
    unix_error("Fread error");
  return n;
}

void Fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream) {
  if (fwrite(ptr, size, nmemb, stream) < nmemb) unix_error("Fwrite error");
}

/****************************
 * Sockets interface wrappers
 ****************************/

int Socket(int domain, int type, int protocol) {
  int rc;

  if ((rc = socket(domain, type, protocol)) < 0) unix_error("Socket error");
  return rc;
}

void Setsockopt(int s, int level, int optname, const void* optval, int optlen) {
  int rc;

  if ((rc = setsockopt(s, level, optname, optval, optlen)) < 0)
    unix_error("Setsockopt error");
}

// Bind moved to .h

void Listen(int s, int backlog) {
  int rc;

  if ((rc = listen(s, backlog)) < 0) unix_error("Listen error");
}

// Accept moved to .h

// Connect moved to .h

/*******************************
 * Protocol-independent wrappers
 *******************************/
void Getaddrinfo(const char* node, const char* service,
                 const struct addrinfo* hints, struct addrinfo** res) {
  int rc;

  if ((rc = getaddrinfo(node, service, hints, res)) != 0)
    gai_error(rc, "Getaddrinfo error");
}

// Getnameinfo moved to .h

void Freeaddrinfo(struct addrinfo* res) { freeaddrinfo(res); }

void Inet_ntop(int af, const void* src, char* dst, socklen_t size) {
  if (!inet_ntop(af, src, dst, size)) unix_error("Inet_ntop error");
}

void Inet_pton(int af, const char* src, void* dst) {
  int rc;

  rc = inet_pton(af, src, dst);
  if (rc == 0)
    app_error("inet_pton error: invalid dotted-decimal address");
  else if (rc < 0)
    unix_error("Inet_pton error");
}

/*******************************************
 * DNS interface wrappers.
 *
 * NOTE: These are obsolete because they are not thread safe. Use
 * getaddrinfo and getnameinfo instead
 ***********************************/

#ifdef USE_UNSAFE_FUNC

struct hostent* Gethostbyname(const char* name) {
  struct hostent* p;

  if ((p = gethostbyname(name)) == NULL) dns_error("Gethostbyname error");
  return p;
}

struct hostent* Gethostbyaddr(const char* addr, int len, int type) {
  struct hostent* p;

  if ((p = gethostbyaddr(addr, len, type)) == NULL)
    dns_error("Gethostbyaddr error");
  return p;
}

#endif

/************************************************
 * Wrappers for Pthreads thread control functions
 ************************************************/

void Pthread_create(pthread_t* tidp, pthread_attr_t* attrp,
                    void* (*routine)(void*), void* argp) {
  int rc;

  if ((rc = pthread_create(tidp, attrp, routine, argp)) != 0)
    posix_error(rc, "Pthread_create error");
}

void Pthread_cancel(pthread_t tid) {
  int rc;

  if ((rc = pthread_cancel(tid)) != 0) posix_error(rc, "Pthread_cancel error");
}

void Pthread_join(pthread_t tid, void** thread_return) {
  int rc;

  if ((rc = pthread_join(tid, thread_return)) != 0)
    posix_error(rc, "Pthread_join error");
}

void Pthread_detach(pthread_t tid) {
  int rc;

  if ((rc = pthread_detach(tid)) != 0) posix_error(rc, "Pthread_detach error");
}

void Pthread_exit(void* retval) { pthread_exit(retval); }

pthread_t Pthread_self(void) { return pthread_self(); }

void Pthread_once(pthread_once_t* once_control, void (*init_function)()) {
  pthread_once(once_control, init_function);
}

/*******************************
 * Wrappers for Posix semaphores
 *******************************/

void Sem_init(sem_t* sem, int pshared, unsigned int value) {
  if (sem_init(sem, pshared, value) < 0) unix_error("Sem_init error");
}

void P(sem_t* sem) {
  if (sem_wait(sem) < 0) unix_error("P error");
}

void V(sem_t* sem) {
  if (sem_post(sem) < 0) unix_error("V error");
}

/****************************************
 * The Rio package - Robust I/O functions
 ****************************************/

/*
 * rio_readn - Robustly read n bytes (unbuffered)
 */
ssize_t rio_readn(int fd, char* usrbuf, size_t n) {
  size_t nleft = n;
  ssize_t nread;
  char* bufp = usrbuf;

  while (nleft > 0) {
    if ((nread = read(fd, bufp, nleft)) < 0) {
      if (errno == EINTR) /* Interrupted by sig handler return */
        nread = 0;        /* and call read() again */
      else
        return -1; /* errno set by read() */
    } else if (nread == 0)
      break; /* EOF */
    nleft -= nread;
    bufp += nread;
  }
  return (n - nleft); /* return >= 0 */
}

/*
 * rio_writen - Robustly write n bytes (unbuffered)
 */
ssize_t rio_writen(int fd, const char* usrbuf, size_t n) {
  size_t nleft = n;
  ssize_t nwritten;
  const char* bufp = usrbuf;

  while (nleft > 0) {
    if ((nwritten = write(fd, bufp, nleft)) <= 0) {
      if (errno == EINTR) /* Interrupted by sig handler return */
        nwritten = 0;     /* and call write() again */
      else
        return -1; /* errno set by write() */
    }
    nleft -= nwritten;
    bufp += nwritten;
  }
  return n;
}

/*
 * rio_read - This is a wrapper for the Unix read() function that
 *    transfers min(n, rio_cnt) bytes from an internal buffer to a user
 *    buffer, where n is the number of bytes requested by the user and
 *    rio_cnt is the number of unread bytes in the internal buffer. On
 *    entry, rio_read() refills the internal buffer via a call to
 *    read() if the internal buffer is empty.
 */
static ssize_t rio_read(csapp::rio_t* rp, char* usrbuf, size_t n) {
  int cnt;

  while (rp->rio_cnt <= 0) { /* Refill if buf is empty */
    rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));
    if (rp->rio_cnt < 0) {
      if (errno != EINTR) /* Interrupted by sig handler return */
        return -1;
    } else if (rp->rio_cnt == 0) /* EOF */
      return 0;
    else
      rp->rio_bufptr = rp->rio_buf; /* Reset buffer ptr */
  }

  /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
  cnt = n;
  if (rp->rio_cnt < static_cast<signed>(n)) cnt = rp->rio_cnt;
  memcpy(usrbuf, rp->rio_bufptr, cnt);
  rp->rio_bufptr += cnt;
  rp->rio_cnt -= cnt;
  return cnt;
}

/*
 * rio_readinitb - Associate a descriptor with a read buffer and reset buffer
 */
static void rio_readinitb(rio_t* rp, int fd) {
  rp->rio_fd = fd;
  rp->rio_cnt = 0;
  rp->rio_bufptr = rp->rio_buf;
}

/*
 * rio_readnb - Robustly read n bytes (buffered)
 */
static ssize_t rio_readnb(rio_t* rp, void* usrbuf, size_t n) {
  size_t nleft = n;
  ssize_t nread;
  char* bufp = static_cast<char*>(usrbuf);

  while (nleft > 0) {
    if ((nread = rio_read(rp, bufp, nleft)) < 0)
      return -1; /* errno set by read() */
    else if (nread == 0)
      break; /* EOF */
    nleft -= nread;
    bufp += nread;
  }
  return (n - nleft); /* return >= 0 */
}

/*
 * rio_readlineb - Robustly read a text line (buffered)
 */
static ssize_t rio_readlineb(rio_t* rp, void* usrbuf, size_t maxlen) {
  size_t n;
  int rc;
  char c;
  char* bufp = static_cast<char*>(usrbuf);

  for (n = 1; n < maxlen; n++) {
    if ((rc = rio_read(rp, &c, 1)) == 1) {
      *bufp++ = c;
      if (c == '\n') {
        n++;
        break;
      }
    } else if (rc == 0) {
      if (n == 1)
        return 0; /* EOF, no data read */
      else
        break; /* EOF, some data was read */
    } else
      return -1; /* Error */
  }
  *bufp = 0;
  return n - 1;
}

/**********************************
 * Wrappers for robust I/O routines, rewrite with class
 **********************************/

size_t Rio::readn(int fd, char* s, size_t bytes) {
  if (ssize_t n{rio_readn(fd, s, bytes)}; n < 0)
    unix_error("Rio_readn error");
  else
    return static_cast<size_t>(n);
}

void Rio::writen(int fd, const char* s, size_t bytes) {
  if (static_cast<size_t>(rio_writen(fd, s, bytes)) != bytes)
    unix_error("Rio_writen error");
}

void Rio::writen(int fd, const std::string_view& s) {
  writen(fd, s.begin(), s.size());
}

Rio::Rio(int fd) { rio_readinitb(&rio, fd); }

size_t Rio::readnb(char* s, size_t bytes) {
  if (ssize_t rc = rio_readnb(&rio, s, bytes); rc < 0)
    unix_error("Rio_readnb error");
  else
    return static_cast<size_t>(rc);
}

std::string Rio::readnb(size_t bytes) {
  char usrbuf[MAXLINE];
  readnb(usrbuf, bytes);
  return std::string(usrbuf);
}

size_t Rio::readlineb(char* s, size_t maxlen) {
  if (ssize_t rc = rio_readlineb(&rio, s, maxlen); rc < 0)
    unix_error("Rio_readlineb error");
  else
    return static_cast<size_t>(rc);
}

std::string Rio::readlineb(size_t maxlen) {
  char usrbuf[MAXLINE]{};
  readlineb(usrbuf, maxlen);
  return std::string(usrbuf);
}

/********************************
 * Client/server helper functions
 ********************************/
/*
 * open_clientfd - Open connection to server at <hostname, port> and
 *     return a socket descriptor ready for reading and writing. This
 *     function is reentrant and protocol-independent.
 *
 *     On error, returns -1 and sets errno.
 */
int open_clientfd(const char* hostname, const char* port) {
  int clientfd;
  struct addrinfo hints;
  struct addrinfo* listp;
  struct addrinfo* p;

  /* Get a list of potential server addresses */
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_socktype = SOCK_STREAM; /* Open a connection */
  hints.ai_flags = AI_NUMERICSERV; /* ... using a numeric port arg. */
  hints.ai_flags |= AI_ADDRCONFIG; /* Recommended for connections */
  Getaddrinfo(hostname, port, &hints, &listp);

  /* Walk the list for one that we can successfully connect to */
  for (p = listp; p; p = p->ai_next) {
    /* Create a socket descriptor */
    if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
      continue; /* Socket failed, try the next */

    /* Connect to the server */
    if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1) break; /* Success */
    Close(clientfd);
    /* Connect failed, try another */  // line:netp:openclientfd:closefd
  }

  /* Clean up */
  Freeaddrinfo(listp);
  if (!p) /* All connects failed */
    return -1;
  else /* The last connect succeeded */
    return clientfd;
}

/*
 * open_listenfd - Open and return a listening socket on port. This
 *     function is reentrant and protocol-independent.
 *
 *     On error, returns -1 and sets errno.
 */
int open_listenfd(const char* port) {
  struct addrinfo hints;
  struct addrinfo* listp;
  struct addrinfo* p;
  int listenfd, optval = 1;
  /* Get a list of potential server addresses */
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_socktype = SOCK_STREAM;             /* Accept connections */
  hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* ... on any IP address */
  hints.ai_flags |= AI_NUMERICSERV;            /* ... using port number */
  Getaddrinfo(NULL, port, &hints, &listp);

  /* Walk the list for one that we can bind to */
  for (p = listp; p; p = p->ai_next) {
    // struct sockaddr_in* a = (struct sockaddr_in*)(p->ai_addr);
    // printf("%s %d\n",inet_ntoa(a->sin_addr) ,ntohs(a->sin_port));
    /* Create a socket descriptor */
    if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
      continue; /* Socket failed, try the next */

    /* Eliminates "Address already in use" error from bind */
    Setsockopt(listenfd, SOL_SOCKET,
               SO_REUSEADDR,  // line:netp:csapp:setsockopt
               (const void*)&optval, sizeof(int));
    /* Bind the descriptor to the address */
    if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) break; /* Success */
    Close(listenfd); /* Bind failed, try the next */
  }

  /* Clean up */
  Freeaddrinfo(listp);
  if (!p) /* No address worked */
    return -1;

  /* Make it a listening socket ready to accept connection requests */
  if (listen(listenfd, LISTENQ) < 0) {
    Close(listenfd);
    return -1;
  }
  return listenfd;
}

/****************************************************
 * Wrappers for reentrant protocol-independent helpers
 ****************************************************/
int Open_clientfd(const char* hostname, const char* port) {
  int rc;

  if ((rc = open_clientfd(hostname, port)) < 0)
    unix_error("Open_clientfd error");
  return rc;
}

int Open_listenfd(const char* port) {
  int rc;

  if ((rc = open_listenfd(port)) < 0) unix_error("Open_listenfd error");
  return rc;
}

}  // namespace csapp