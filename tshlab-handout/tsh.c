/**
 * @file tsh.c
 * @author Guyutongxue <1900012983@pku.edu.cn>
 * @brief tsh - A tiny shell program with job control
 * @version 0.1
 * @date 2020-12-02 -
 *
 * @copyright Copyright (c) 2020 Guyutongxue
 *
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#ifdef USE_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Misc manifest constants */
#define MAXLINE 1024   /* max line size */
#define MAXARGS 128    /* max args on a command line */
#define MAXJOBS 16     /* max jobs at any point in time */
#define MAXJID 1 << 16 /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Parsing states */
#define ST_NORMAL 0x0  /* next token is an argument */
#define ST_INFILE 0x1  /* next token is the input file */
#define ST_OUTFILE 0x2 /* next token is the output file */

/* Global variables */
extern char **environ;   /* defined in libc */
char prompt[] = "tsh> "; /* command line prompt (DO NOT CHANGE) */
int verbose = 0;         /* if true, print additional output */
int nextjid = 1;         /* next job ID to allocate */
char sbuf[MAXLINE];      /* for composing sprintf messages */

sigset_t mask, prev_mask;  ///< Mask for blocking signals

struct job_t {           /* The job struct */
  pid_t pid;             /* job PID */
  int jid;               /* job ID [1, 2, ...] */
  int state;             /* UNDEF, BG, FG, or ST */
  char cmdline[MAXLINE]; /* command line */
};
struct job_t job_list[MAXJOBS]; /* The job list */

struct cmdline_tokens {
  int argc;            /* Number of arguments */
  char *argv[MAXARGS]; /* The arguments list */
  char *infile;        /* The input file */
  char *outfile;       /* The output file */
  enum builtins_t {    /* Indicates if argv[0] is a builtin command */
                    BUILTIN_NONE,
                    BUILTIN_QUIT,
                    BUILTIN_JOBS,
                    BUILTIN_BG,
                    BUILTIN_FG,
                    BUILTIN_KILL,
                    BUILTIN_NOHUP
  } builtins;
};

/* End global variables */

/* Function prototypes */

void eval(char *cmdline);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Self defined functions */

bool builtin_command(struct cmdline_tokens *tok, int *p_input_fd,
                     int *p_output_fd, int bg, const char *cmdline);
void close_files(int input_fd, int output_fd);
void do_bgfg(char **argv, int output_fd);
void kill_job(char **argv, int output_fd);
void nohup_eval(struct cmdline_tokens *tok, int input_fd, int output_fd, int bg,
                const char *cmdline);
void waitfg(int pid, int output_fd);

/* Here are helper routines that we've provided for you */

int parseline(const char *cmdline, struct cmdline_tokens *tok);
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *job_list);
int maxjid(struct job_t *job_list);
int addjob(struct job_t *job_list, pid_t pid, int state, const char *cmdline);
int deletejob(struct job_t *job_list, pid_t pid);
pid_t fgpid(struct job_t *job_list);
struct job_t *getjobpid(struct job_t *job_list, pid_t pid);
struct job_t *getjobjid(struct job_t *job_list, int jid);
int pid2jid(pid_t pid);
void listjobs(struct job_t *job_list, int output_fd);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
ssize_t sio_puts(char s[]);
ssize_t sio_putl(long v);
ssize_t sio_put(const char *fmt, ...);
void sio_error(char s[]);

typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/* Begin POSIX wrapper functions */

int Sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
  int result;
  if ((result = sigprocmask(how, set, oldset)) < 0)
    unix_error("sigprocmask error");
  return result;
}

int Sigemptyset(sigset_t *set) {
  int result;
  if ((result = sigemptyset(set)) < 0) unix_error("sigemptyset error");
  return result;
}

int Sigfillset(sigset_t *set) {
  int result;
  if ((result = sigfillset(set)) < 0) unix_error("sigfillset error");
  return result;
}

int Sigaddset(sigset_t *set, int signum) {
  int result;
  if ((result = sigaddset(set, signum)) < 0) unix_error("sigaddset error");
  return result;
}

int Sigdelset(sigset_t *set, int signum) {
  int result;
  if ((result = sigdelset(set, signum)) < 0) unix_error("sigdelset error");
  return result;
}

int Fork(void) {
  pid_t pid;
  if ((pid = fork()) < 0) unix_error("fork error");
  return pid;
}

int Setpgid(pid_t pid, pid_t pgid) {
  int result;
  if ((result = setpgid(pid, pgid)) < 0) unix_error("setpgid error");
  return result;
}

int Write(int fd, const void *buf, size_t n) {
  int result;
  if ((result = write(fd, buf, n)) < 0) {
    fwrite("Error writing to file\n", 1, 23, stderr);
    exit(EXIT_FAILURE);
  }
  return result;
}

int Dup2(int oldfd, int newfd) {
  int result;
  if ((result = dup2(oldfd, newfd)) < 0) {
    fwrite("Error with I/O redirection", 1, 27, stderr);
    exit(EXIT_FAILURE);
  }
  return result;
}
/* End POSIX wrapper functions */

/**
 * @brief The shell's main routine
 *
 * @param argc
 * @param argv
 * @return int
 */
int main(int argc, char **argv) {
  char c;
#ifdef USE_READLINE
  char *cmdline = NULL;
#else
  char cmdline[MAXLINE]; /* cmdline for fgets */
#endif
  int emit_prompt = 1; /* emit prompt (default) */

  /* Redirect stderr to stdout (so that driver will get all output
   * on the pipe connected to stdout) */
  dup2(1, 2);

  /* Parse the command line */
  while ((c = getopt(argc, argv, "hvp")) != EOF) {
    switch (c) {
      case 'h': /* print help message */
        usage();
        break;
      case 'v': /* emit additional diagnostic info */
        verbose = 1;
        break;
      case 'p':          /* don't print a prompt */
        emit_prompt = 0; /* handy for automatic testing */
        break;
      default:
        usage();
    }
  }

  /* Install the signal handlers */

  /* These are the ones you will need to implement */
  Signal(SIGINT, sigint_handler);   /* ctrl-c */
  Signal(SIGTSTP, sigtstp_handler); /* ctrl-z */
  Signal(SIGCHLD, sigchld_handler); /* Terminated or stopped child */
  Signal(SIGTTIN, SIG_IGN);
  Signal(SIGTTOU, SIG_IGN);

  /* This one provides a clean way to kill the shell */
  Signal(SIGQUIT, sigquit_handler);

  /* Initialize the job list */
  initjobs(job_list);

  /* Execute the shell's read/eval loop */
  while (1) {
#ifndef USE_READLINE
    if (emit_prompt) {
      printf("%s", prompt);
      fflush(stdout);
    }
#endif
    if ((
#ifdef USE_READLINE
            (cmdline = readline(emit_prompt ? "tsh> " : ""))
#else
            fgets(cmdline, MAXLINE, stdin)
#endif
            == NULL) &&
        ferror(stdin))
      app_error("fgets error");
    if (feof(stdin)) {
      /* End of file (ctrl-d) */
      printf("\n");
      fflush(stdout);
      fflush(stderr);
#ifdef USE_READLINE
      free(cmdline);
#endif
      exit(0);
    }

#ifdef USE_READLINE
    add_history(cmdline);
#else
    /* Remove the trailing newline */
    cmdline[strlen(cmdline) - 1] = '\0';
#endif
    /* Evaluate the command line */
    eval(cmdline);

    fflush(stdout);
    fflush(stdout);
#ifdef USE_READLINE
    free(cmdline);
#endif
  }

  exit(0); /* control never reaches here */
}

/**
 * @brief Evaluate the command line that the user has just typed in
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
 * @param cmdline Raw command line string
 */
void eval(char *cmdline) {
  int bg; /* should the job run in bg or fg? */
  struct cmdline_tokens tok;
  int input_fd = STDIN_FILENO, output_fd = STDOUT_FILENO;
  pid_t pid;  ///< PID of job
  int state;  ///< State of job, BG or FG

  /* Parse command line */
  bg = parseline(cmdline, &tok);

  if (bg == -1) /* parsing error */
    return;
  if (tok.argv[0] == NULL) /* ignore empty lines */
    return;
  Sigemptyset(&mask);
  Sigaddset(&mask, SIGCHLD);
  Sigaddset(&mask, SIGINT);
  Sigaddset(&mask, SIGTSTP);
  Sigprocmask(SIG_BLOCK, &mask, &prev_mask);
  if (builtin_command(&tok, &input_fd, &output_fd, bg, cmdline)) {
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    if (input_fd != STDIN_FILENO) close(input_fd);
    if (output_fd != STDOUT_FILENO) close(output_fd);
    return;
  }
  if ((pid = Fork()) == 0) {
    // Child process
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    Setpgid(0, 0);
    if (input_fd != STDIN_FILENO) {
      Dup2(input_fd, STDIN_FILENO);
      close(input_fd);
    }
    if (output_fd != STDOUT_FILENO) {
      Dup2(output_fd, STDOUT_FILENO);
      close(output_fd);
    }
    Signal(SIGTTIN, SIG_DFL);
    Signal(SIGTTOU, SIG_DFL);
    if (execve(tok.argv[0], tok.argv, environ) < 0) {
      printf("%s: Command not found\n", tok.argv[0]);
      exit(EXIT_SUCCESS);
    }
  } else {
    // Parent process
    state = bg ? BG : FG;
    addjob(job_list, pid, state, cmdline);
    if (bg) {
      printf("[%d] (%d) %s\n", pid2jid(pid), pid, cmdline);
    } else {
      waitfg(pid, STDOUT_FILENO);
    }
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    if (input_fd != STDIN_FILENO) close(input_fd);
    if (output_fd != STDOUT_FILENO) close(output_fd);
  }
}

/**
 * @brief Check if @c tok is builtin command.
 * If @c tok is builtin command, execute it and return @c true ;
 * else return @c false while pass back @c input_fd and @c output_fd .
 * @param tok Command line tokens
 * @param p_input_fd Passed back @c input_fd
 * @param p_output_fd Passed back @c output_fd
 * @param bg Whether background executing
 * @param cmdline Raw command line string
 * @return true Builtin command executed. (Successful or not)
 * @return false Not a builtin command.
 */
bool builtin_command(struct cmdline_tokens *tok, int *p_input_fd,
                     int *p_output_fd, int bg, const char *cmdline) {
  int input_fd = STDIN_FILENO, output_fd = STDOUT_FILENO;
  if (tok->infile && (input_fd = open(tok->infile, O_RDONLY), input_fd < 0)) {
    fprintf(stderr, "Error: %s No such file or directory\n", tok->infile);
    return true;
  }
  if (tok->outfile &&
      (output_fd = open(tok->outfile, O_TRUNC | O_CREAT | O_WRONLY),
       output_fd < 0)) {
    fprintf(stderr, "Error: %s Couldn't open file\n", tok->outfile);
    return true;
  }
  switch (tok->builtins) {
    case BUILTIN_QUIT:
      fflush(stdout);
      fflush(stderr);
      exit(EXIT_SUCCESS);
    case BUILTIN_JOBS:
      listjobs(job_list, output_fd);
      close_files(input_fd, output_fd);
      return true;
    case BUILTIN_BG:
    case BUILTIN_FG:
      do_bgfg(tok->argv, output_fd);
      close_files(input_fd, output_fd);
      return true;
    case BUILTIN_KILL:
      kill_job(tok->argv, output_fd);
      close_files(input_fd, output_fd);
      return true;
    case BUILTIN_NOHUP:
      nohup_eval(tok, input_fd, output_fd, bg, cmdline);
      close_files(input_fd, output_fd);
      return true;
    default:
      *p_input_fd = input_fd;
      *p_output_fd = output_fd;
      return false;
  }
}

/**
 * @brief Close file associated with given fd
 * Won't close stdin and/or stdout
 * @param input_fd Input file descriptor
 * @param output_fd Output file descriptor
 */
void close_files(int input_fd, int output_fd) {
  if (input_fd != STDIN_FILENO && close(input_fd) < 0) {
    unix_error("close (close_files) error");
  }
  if (output_fd != STDOUT_FILENO && close(output_fd) < 0) {
    unix_error("close (close_files) error");
  }
}

/**
 * @brief Execute @c bg or @c fg builtin command
 *
 * @param argv Arguments passed by
 * @param output_fd Where write output
 */
void do_bgfg(char **argv, int output_fd) {
  char s[1060];
  int pid, jid;
  struct job_t *target_job;
  if (!argv[1]) {
    memset(s, 0, sizeof(s));
    sprintf(s, "%s command requires PID or %%jobid argument\n", argv[0]);
    Write(output_fd, s, strlen(s));
    return;
  }
  if (isdigit(argv[1][0])) {
    pid = atoi(argv[1]);
    target_job = getjobpid(job_list, pid);
    if (!target_job) {
      memset(s, 0, sizeof(s));
      sprintf(s, "(%d): No such process\n", pid);
      Write(output_fd, s, strlen(s));
      return;
    }
  } else if (argv[1][0] == '%') {
    jid = atoi(argv[1] + 1);
    target_job = getjobjid(job_list, jid);
    if (!target_job) {
      memset(s, 0, sizeof(s));
      sprintf(s, "%s: No such job\n", argv[1]);
      Write(output_fd, s, strlen(s));
      return;
    }
  } else {
    memset(s, 0, sizeof(s));
    sprintf(s, "%s: argument must be a PID or %%jobid\n", argv[0]);
    Write(output_fd, s, strlen(s));
    return;
  }
  if (!strcmp(argv[0], "bg")) {
    if (kill(-target_job->pid, SIGCONT) < 0) unix_error("kill (bg) error");
    memset(s, 0, sizeof(s));
    sprintf(s, "[%d] (%d) %s\n", target_job->jid, target_job->pid,
            target_job->cmdline);
    Write(output_fd, s, strlen(s));
  } else if (!strcmp(argv[0], "fg")) {
    if (kill(-target_job->pid, SIGCONT) < 0) unix_error("kill (fg) error");
    waitfg(target_job->pid, output_fd);
  } else {
    memset(s, 0, sizeof(s));
    sprintf(s, "do_bgfg: Internal error\n");
    Write(output_fd, s, strlen(s));
  }
}

/**
 * @brief Kill a job
 *
 * @param argv Arguments passed by
 * @param output_fd Where write output
 */
void kill_job(char **argv, int output_fd) {
  int pid, jid;
  struct job_t *target_job;
  if (!argv[1]) {
    memset(sbuf, 0, sizeof(sbuf));
    sprintf(sbuf, "%s command requires PID or %%jobid argument\n", argv[0]);
    Write(output_fd, sbuf, strlen(sbuf));
    return;
  }
  if (isdigit(argv[1][0])) {
    pid = atoi(argv[1]);
    target_job = getjobpid(job_list, pid);
    if (!target_job) {
      memset(sbuf, 0, sizeof(sbuf));
      sprintf(sbuf, "(%d): No such process\n", pid);
      Write(output_fd, sbuf, strlen(sbuf));
      return;
    }
  } else if (argv[1][0] == '%') {
    jid = atoi(argv[1] + 1);
    target_job = getjobjid(job_list, jid);
    if (!target_job) {
      memset(sbuf, 0, sizeof(sbuf));
      sprintf(sbuf, "%s: No such job\n", argv[1]);
      Write(output_fd, sbuf, strlen(sbuf));
      return;
    }
  } else {
    memset(sbuf, 0, sizeof(sbuf));
    sprintf(sbuf, "%s: argument must be a PID or %%jobid\n", argv[0]);
    Write(output_fd, sbuf, strlen(sbuf));
    return;
  }
  if (kill(-target_job->pid, SIGTERM) < 0) unix_error("kill (kill) error");
  return;
}

/**
 * @brief Eval a non-builtin-command with no SIGHUP response
 *
 * @param tok Command line tokens
 * @param input_fd Where read input
 * @param output_fd Where write output
 * @param bg Whether background executing
 * @param cmdline Raw command line string
 */
void nohup_eval(struct cmdline_tokens *tok, int input_fd, int output_fd, int bg,
                const char *cmdline) {
  int state;
  int pid;
  if ((pid = Fork()) == 0) {
    // Child process
    Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    Sigemptyset(&mask);
    Sigaddset(&mask, SIGHUP);
    Sigprocmask(SIG_BLOCK, &mask, NULL);
    Setpgid(0, 0);
    if (input_fd != STDIN_FILENO) {
      Dup2(input_fd, STDIN_FILENO);
      close(input_fd);
    }
    if (output_fd != STDOUT_FILENO) {
      Dup2(output_fd, STDOUT_FILENO);
      close(output_fd);
    }
    Signal(SIGTTIN, SIG_DFL);
    Signal(SIGTTOU, SIG_DFL);
    if (execve(tok->argv[1], &tok->argv[1], environ) < 0) {
      printf("%s: Command not found\n", tok->argv[1]);
      exit(EXIT_SUCCESS);
    }
  } else {
    // Parent process
    state = bg ? BG : FG;
    addjob(job_list, pid, state, cmdline);
    if (bg) {
      printf("[%d] (%d) %s \n", pid2jid(pid), pid, cmdline);
    } else {
      waitfg(pid, STDOUT_FILENO);
    }
  }
}

/**
 * @brief Wait until foreground job finished
 *
 * @param pid Foreground job PID
 * @param output_fd Where write output
 */
void waitfg(int pid, int output_fd) {
  struct job_t *target_job;
  target_job = getjobpid(job_list, pid);
  while (pid == target_job->pid && target_job->state == FG)
    sigsuspend(&prev_mask);
  if (verbose) {
    memset(sbuf, 0, sizeof(sbuf));
    sprintf(sbuf, "waitfg: Process (%d) no longer the fg process\n", pid);
    Write(output_fd, sbuf, strlen(sbuf));
  }
}

/**
 * @brief Parse the command line and build the argv array.
 * The string elements of tok (e.g., argv[], infile, outfile)
 * are statically allocated inside parseline() and will be
 * overwritten the next time this function is invoked.
 * @param cmdline The command line, in the form:
 *                command [arguments...] [< infile] [> oufile] [&]
 * @param tok Pointer to a cmdline_tokens structure. The elements of this
 *            structure will be populated with the parsed tokens. Characters
 *            enclosed in single or double quotes are treated as a single
 *            argument.
 * @return int
 *   1:        if the user has requested a BG job
 *   0:        if the user has requested a FG job
 *  -1:        if cmdline is incorrectly formatted
 */
int parseline(const char *cmdline, struct cmdline_tokens *tok) {
  static char array[MAXLINE];        /* holds local copy of command line */
  const char delims[10] = " \t\r\n"; /* argument delimiters (white-space) */
  char *buf = array;                 /* ptr that traverses command line */
  char *next;                        /* ptr to the end of the current arg */
  char *endbuf;                      /* ptr to end of cmdline string */
  int is_bg;                         /* background job? */

  /* indicates if the next token is the input or output file */
  int parsing_state;

  if (cmdline == NULL) {
    (void)fprintf(stderr, "Error: command line is NULL\n");
    return -1;
  }

  (void)strncpy(buf, cmdline, MAXLINE);
  endbuf = buf + strlen(buf);

  tok->infile = NULL;
  tok->outfile = NULL;

  /* Build the argv list */
  parsing_state = ST_NORMAL;
  tok->argc = 0;

  while (buf < endbuf) {
    /* Skip the white-spaces */
    buf += strspn(buf, delims);
    if (buf >= endbuf) break;

    /* Check for I/O redirection specifiers */
    if (*buf == '<') {
      if (tok->infile) {
        (void)fprintf(stderr, "Error: Ambiguous I/O redirection\n");
        return -1;
      }
      parsing_state |= ST_INFILE;
      buf++;
      continue;
    }
    if (*buf == '>') {
      if (tok->outfile) {
        (void)fprintf(stderr, "Error: Ambiguous I/O redirection\n");
        return -1;
      }
      parsing_state |= ST_OUTFILE;
      buf++;
      continue;
    }

    if (*buf == '\'' || *buf == '\"') {
      /* Detect quoted tokens */
      buf++;
      next = strchr(buf, *(buf - 1));
    } else {
      /* Find next delimiter */
      next = buf + strcspn(buf, delims);
    }

    if (next == NULL) {
      /* Returned by strchr(); this means that the closing
         quote was not found. */
      (void)fprintf(stderr, "Error: unmatched %c.\n", *(buf - 1));
      return -1;
    }

    /* Terminate the token */
    *next = '\0';

    /* Record the token as either the next argument or the i/o file */
    switch (parsing_state) {
      case ST_NORMAL:
        tok->argv[tok->argc++] = buf;
        break;
      case ST_INFILE:
        tok->infile = buf;
        break;
      case ST_OUTFILE:
        tok->outfile = buf;
        break;
      default:
        (void)fprintf(stderr, "Error: Ambiguous I/O redirection\n");
        return -1;
    }
    parsing_state = ST_NORMAL;

    /* Check if argv is full */
    if (tok->argc >= MAXARGS - 1) break;

    buf = next + 1;
  }

  if (parsing_state != ST_NORMAL) {
    (void)fprintf(stderr, "Error: must provide file name for redirection\n");
    return -1;
  }

  /* The argument list must end with a NULL pointer */
  tok->argv[tok->argc] = NULL;

  if (tok->argc == 0) /* ignore blank line */
    return 1;

  if (!strcmp(tok->argv[0], "quit")) { /* quit command */
    tok->builtins = BUILTIN_QUIT;
  } else if (!strcmp(tok->argv[0], "jobs")) { /* jobs command */
    tok->builtins = BUILTIN_JOBS;
  } else if (!strcmp(tok->argv[0], "bg")) { /* bg command */
    tok->builtins = BUILTIN_BG;
  } else if (!strcmp(tok->argv[0], "fg")) { /* fg command */
    tok->builtins = BUILTIN_FG;
  } else if (!strcmp(tok->argv[0], "kill")) { /* kill command */
    tok->builtins = BUILTIN_KILL;
  } else if (!strcmp(tok->argv[0], "nohup")) { /* kill command */
    tok->builtins = BUILTIN_NOHUP;
  } else {
    tok->builtins = BUILTIN_NONE;
  }

  /* Should the job run in the background? */
  if ((is_bg = (*tok->argv[tok->argc - 1] == '&')) != 0)
    tok->argv[--tok->argc] = NULL;

  return is_bg;
}

/*****************
 * Signal handlers
 *****************/

/**
 * @brief
 * The kernel sends a SIGCHLD to the shell whenever
 * a child job terminates (becomes a zombie), or stops because it
 * received a SIGSTOP, SIGTSTP, SIGTTIN or SIGTTOU signal. The
 * handler reaps all available zombie children, but doesn't wait
 * for any other currently running children to terminate.
 * @param sig
 */
void sigchld_handler(int sig) {
  int pid, jid;
  int status;
  sigset_t mask, prev_mask;
  int prev_errno = errno;
  struct job_t *target_job;
  if (verbose) sio_puts("sigchld_handler: entering\n");
  if (sigfillset(&mask) < 0) sio_error("sigfillset error in sigchld_handler\n");
  if (sigprocmask(SIG_BLOCK, &mask, &prev_mask) < 0)
    sio_error("sigprocmask error in sigchld_handler\n");
  while (true) {
    pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);
    if (pid <= 0) break;
    if (WIFSTOPPED(status)) {
      target_job = getjobpid(job_list, pid);
      if (!target_job) {
        sio_put("Lost track of (%d)\n", pid);
        return;
      }
      target_job->state = ST;
      sio_put("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), pid,
              WSTOPSIG(status));
    } else if (WIFSIGNALED(status)) {
      jid = pid2jid(pid);
      if (deletejob(job_list, pid) && verbose)
        sio_put("sigchld_handler: Job [%d] (%d) deleted\n", jid, pid);
      sio_put("Job [%d] (%d) terminated by signal %d\n", jid, pid,
              WTERMSIG(status));
    } else if (WIFEXITED(status)) {
      jid = pid2jid(pid);
      if (deletejob(job_list, pid) && verbose)
        sio_put("sigchld_handler: Job [%d] (%d) deleted\n", jid, pid);
      if (verbose)
        sio_put("sigchld_handler: Job [%d] (%d) terminates OK (status %d)\n",
                jid, pid, WEXITSTATUS(pid));
    } else {
      if (!WIFCONTINUED(status)) sio_error("waitpid_error");
      target_job = getjobpid(job_list, pid);
      if (!target_job) sio_put("Lost track of (%d)\n", pid);
      if (target_job->state != FG) target_job->state = BG;
    }
  }
  if (pid && (pid != -1 || errno != ECHILD))
    sio_error("sigchld_handler wait error");
  if (sigprocmask(SIG_SETMASK, &prev_mask, NULL) < 0)
    sio_error("sigprocmask error in sigchld_handler\n");
  if (verbose) sio_put("sigchld_handler: exiting\n");
  errno = prev_errno;
}

/**
 * @brief
 * The kernel sends a SIGINT to the shell whenver the
 * user types ctrl-c at the keyboard.  Catch it and send it along
 * to the foreground job.
 * @param sig
 */
void sigint_handler(int sig) {
  int pid;
  sigset_t mask, prev_mask;
  int prev_errno = errno;
  if (verbose) sio_put("sigint_handler: entering\n");
  if (sigfillset(&mask) < 0) sio_error("sigfillset error in sigint_handler\n");
  if (sigprocmask(SIG_BLOCK, &mask, &prev_mask) < 0)
    sio_error("sigprocmask error in sigint_handler\n");
  pid = fgpid(job_list);
  if (pid > 0) {
    if (kill(-pid, SIGINT) < 0) sio_error("kill (sigint) error");
    if (verbose) sio_put("sigint_handler: Job (%d) killed\n", pid);
  }
  if (sigprocmask(SIG_SETMASK, &prev_mask, NULL) < 0)
    sio_error("sigprocmask error in sigint_handler\n");
  if (verbose) sio_put("sigint_handler: exiting\n");
  errno = prev_errno;
}

/**
 * @brief
 * The kernel sends a SIGTSTP to the shell whenever
 * the user types ctrl-z at the keyboard. Catch it and suspend the
 * foreground job by sending it a SIGTSTP.
 * @param sig
 */
void sigtstp_handler(int sig) {
  int pid;
  sigset_t mask, prev_mask;
  int prev_errno = errno;
  if (verbose) sio_put("sigstp_handler: entering\n");
  if (sigfillset(&mask) < 0) sio_error("sigfillset error in sigstp_handler\n");
  if (sigprocmask(SIG_BLOCK, &mask, &prev_mask) < 0)
    sio_error("sigprocmask error in sigstp_handler\n");
  pid = fgpid(job_list);
  if (pid > 0) {
    if (kill(-pid, SIGTSTP) < 0) sio_error("kill (tstp) error");
    if (verbose)
      sio_put("sigstp_handler: Job [%d] (%d) stopped\n", pid2jid(pid), pid);
  }
  if (sigprocmask(SIG_SETMASK, &prev_mask, NULL) < 0)
    sio_error("sigprocmask error in sigstp_handler\n");
  if (verbose) sio_put("sigstp_handler: exiting\n");
  errno = prev_errno;
}

/**
 * @brief
 * The driver program can gracefully terminate the
 * child shell by sending it a SIGQUIT signal.
 * @param sig
 */
void sigquit_handler(int sig) {
  sio_error("Terminating after receipt of SIGQUIT signal\n");
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/**
 * @brief Clear the entries in a job struct
 *
 * @param job
 */
void clearjob(struct job_t *job) {
  job->pid = 0;
  job->jid = 0;
  job->state = UNDEF;
  job->cmdline[0] = '\0';
}

/**
 * @brief Initialize the job list
 *
 * @param job_list
 */
void initjobs(struct job_t *job_list) {
  int i;

  for (i = 0; i < MAXJOBS; i++) clearjob(&job_list[i]);
}

/**
 * @brief Returns largest allocated job ID
 *
 * @param job_list
 * @return int
 */
int maxjid(struct job_t *job_list) {
  int i, max = 0;

  for (i = 0; i < MAXJOBS; i++)
    if (job_list[i].jid > max) max = job_list[i].jid;
  return max;
}

/**
 * @brief Add a job to the job list
 *
 * @param job_list
 * @param pid
 * @param state
 * @param cmdline
 * @return int
 */
int addjob(struct job_t *job_list, pid_t pid, int state, const char *cmdline) {
  int i;

  if (pid < 1) return 0;

  for (i = 0; i < MAXJOBS; i++) {
    if (job_list[i].pid == 0) {
      job_list[i].pid = pid;
      job_list[i].state = state;
      job_list[i].jid = nextjid++;
      if (nextjid > MAXJOBS) nextjid = 1;
      strcpy(job_list[i].cmdline, cmdline);
      if (verbose) {
        printf("Added job [%d] %d %s\n", job_list[i].jid, job_list[i].pid,
               job_list[i].cmdline);
      }
      return 1;
    }
  }
  printf("Tried to create too many jobs\n");
  return 0;
}

/**
 * @brief Delete a job whose PID=pid from the job list
 *
 * @param job_list
 * @param pid
 * @return int
 */
int deletejob(struct job_t *job_list, pid_t pid) {
  int i;

  if (pid < 1) return 0;

  for (i = 0; i < MAXJOBS; i++) {
    if (job_list[i].pid == pid) {
      clearjob(&job_list[i]);
      nextjid = maxjid(job_list) + 1;
      return 1;
    }
  }
  return 0;
}

/**
 * @brief Return PID of current foreground job, 0 if no such job
 *
 * @param job_list
 * @return pid_t
 */
pid_t fgpid(struct job_t *job_list) {
  int i;

  for (i = 0; i < MAXJOBS; i++)
    if (job_list[i].state == FG) return job_list[i].pid;
  return 0;
}

/**
 * @brief Find a job (by PID) on the job list
 *
 * @param job_list
 * @param pid
 * @return struct job_t*
 */
struct job_t *getjobpid(struct job_t *job_list, pid_t pid) {
  int i;

  if (pid < 1) return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (job_list[i].pid == pid) return &job_list[i];
  return NULL;
}

/**
 * @brief Find a job (by JID) on the job list
 *
 * @param job_list
 * @param jid
 * @return struct job_t*
 */
struct job_t *getjobjid(struct job_t *job_list, int jid) {
  int i;

  if (jid < 1) return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (job_list[i].jid == jid) return &job_list[i];
  return NULL;
}

/**
 * @brief Map process ID to job ID
 *
 * @param pid
 * @return int
 */
int pid2jid(pid_t pid) {
  int i;

  if (pid < 1) return 0;
  for (i = 0; i < MAXJOBS; i++)
    if (job_list[i].pid == pid) {
      return job_list[i].jid;
    }
  return 0;
}

/**
 * @brief Print the job list
 *
 * @param job_list
 * @param output_fd
 */
void listjobs(struct job_t *job_list, int output_fd) {
  int i;
  char buf[MAXLINE << 2];

  for (i = 0; i < MAXJOBS; i++) {
    memset(buf, '\0', MAXLINE);
    if (job_list[i].pid != 0) {
      sprintf(buf, "[%d] (%d) ", job_list[i].jid, job_list[i].pid);
      if (write(output_fd, buf, strlen(buf)) < 0) {
        fprintf(stderr, "Error writing to output file\n");
        exit(1);
      }
      memset(buf, '\0', MAXLINE);
      switch (job_list[i].state) {
        case BG:
          sprintf(buf, "Running    ");
          break;
        case FG:
          sprintf(buf, "Foreground ");
          break;
        case ST:
          sprintf(buf, "Stopped    ");
          break;
        default:
          sprintf(buf, "listjobs: Internal error: job[%d].state=%d ", i,
                  job_list[i].state);
      }
      if (write(output_fd, buf, strlen(buf)) < 0) {
        fprintf(stderr, "Error writing to output file\n");
        exit(1);
      }
      memset(buf, '\0', MAXLINE);
      sprintf(buf, "%s\n", job_list[i].cmdline);
      if (write(output_fd, buf, strlen(buf)) < 0) {
        fprintf(stderr, "Error writing to output file\n");
        exit(1);
      }
    }
  }
}
/******************************
 * end job list helper routines
 ******************************/

/***********************
 * Other helper routines
 ***********************/

/**
 * @brief print a help message
 *
 */
void usage(void) {
  printf("Usage: shell [-hvp]\n");
  printf("   -h   print this message\n");
  printf("   -v   print additional diagnostic information\n");
  printf("   -p   do not emit a command prompt\n");
  exit(1);
}

/**
 * @brief unix-style error routine
 *
 * @param msg
 */
void unix_error(char *msg) {
  fprintf(stdout, "%s: %s\n", msg, strerror(errno));
  exit(1);
}

/**
 * @brief application-style error routine
 *
 * @param msg
 */
void app_error(char *msg) {
  fprintf(stdout, "%s\n", msg);
  exit(1);
}

/* Private sio_functions */

/**
 * @brief Reverse a string (from K&R)
 *
 * @param s
 */
static void sio_reverse(char s[]) {
  int c, i, j;

  for (i = 0, j = strlen(s) - 1; i < j; i++, j--) {
    c = s[i];
    s[i] = s[j];
    s[j] = c;
  }
}

/**
 * @brief Convert long to base b string (from K&R)
 *
 * @param v
 * @param s
 * @param b
 */
static void sio_ltoa(long v, char s[], int b) {
  int c, i = 0;

  do {
    s[i++] = ((c = (v % b)) < 10) ? c + '0' : c - 10 + 'a';
  } while ((v /= b) > 0);
  s[i] = '\0';
  sio_reverse(s);
}

/**
 * @brief Return length of string (from K&R)
 *
 * @param s
 * @return size_t
 */
static size_t sio_strlen(const char s[]) {
  int i = 0;

  while (s[i] != '\0') ++i;
  return i;
}

/**
 * @brief Copy len chars from fmt to s (by Ding Rui)
 *
 * @param s
 * @param fmt
 * @param len
 */
void sio_copy(char *s, const char *fmt, size_t len) {
  if (!len) return;

  for (size_t i = 0; i < len; i++) s[i] = fmt[i];
}

/* Public Sio functions */

/**
 * @brief Put string
 *
 * @param s
 * @return ssize_t
 */
ssize_t sio_puts(char s[]) { return write(STDOUT_FILENO, s, sio_strlen(s)); }

/**
 * @brief Put long
 *
 * @param v
 * @return ssize_t
 */
ssize_t sio_putl(long v) {
  char s[128];

  sio_ltoa(v, s, 10); /* Based on K&R itoa() */
  return sio_puts(s);
}

/**
 * @brief Put to the console. only understands %d
 *
 * @param fmt
 * @param ...
 * @return ssize_t
 */
ssize_t sio_put(const char *fmt, ...) {
  va_list ap;
  char str[MAXLINE];  // formatted string
  char arg[128];
  const char *mess = "sio_put: Line too long!\n";
  int i = 0, j = 0;
  int sp = 0;
  int v;

  if (fmt == 0) return -1;

  va_start(ap, fmt);
  while (fmt[j]) {
    if (fmt[j] != '%') {
      j++;
      continue;
    }

    sio_copy(str + sp, fmt + i, j - i);
    sp += j - i;

    switch (fmt[j + 1]) {
      case 0:
        va_end(ap);
        if (sp >= MAXLINE) {
          write(STDOUT_FILENO, mess, sio_strlen(mess));
          return -1;
        }

        str[sp] = 0;
        return write(STDOUT_FILENO, str, sp);

      case 'd':
        v = va_arg(ap, int);
        sio_ltoa(v, arg, 10);
        sio_copy(str + sp, arg, sio_strlen(arg));
        sp += sio_strlen(arg);
        i = j + 2;
        j = i;
        break;

      case '%':
        sio_copy(str + sp, "%", 1);
        sp += 1;
        i = j + 2;
        j = i;
        break;

      default:
        sio_copy(str + sp, fmt + j, 2);
        sp += 2;
        i = j + 2;
        j = i;
        break;
    }
  }  // end while

  sio_copy(str + sp, fmt + i, j - i);
  sp += j - i;

  va_end(ap);
  if (sp >= MAXLINE) {
    write(STDOUT_FILENO, mess, sio_strlen(mess));
    return -1;
  }

  str[sp] = 0;
  return write(STDOUT_FILENO, str, sp);
}

/* Put error message and exit */
void sio_error(char s[]) {
  sio_puts(s);
  _exit(1);
}

/**
 * @brief wrapper for the sigaction function
 *
 * @param signum
 * @param handler
 * @return handler_t*
 */
handler_t *Signal(int signum, handler_t *handler) {
  struct sigaction action, old_action;

  action.sa_handler = handler;
  sigemptyset(&action.sa_mask); /* block sigs of type being handled */
  action.sa_flags = SA_RESTART; /* restart syscalls if possible */

  if (sigaction(signum, &action, &old_action) < 0) unix_error("Signal error");
  return (old_action.sa_handler);
}
