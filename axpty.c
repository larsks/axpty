#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#define FLUSHTIMEOUT 500000 /* 0.5 sec */

#define PERROR(s) fprintf(stderr, "*** %s: %s\r", (s), strerror(errno))
#define USAGE()                                                                \
  fputs("Usage: axpty [-p <paclen>] [-n] <path> <argv[0]> ...\r", stderr)

void sigchld_handler(int sig) {
  /* fprintf(stderr, "Caught SIGCHLD, exiting...\r"); */
  exit(0);
}

void convert_cr_lf(unsigned char *buf, int len) {
  while (len-- > 0) {
    if (*buf == '\r')
      *buf = '\n';
    buf++;
  }
}

void convert_lf_cr(unsigned char *buf, int *lenptr) {
  int len = *lenptr, newlen = *lenptr;
  unsigned char *mark = buf;

  while (len-- > 0) {
    if (*buf != '\r') {
      if (*buf == '\n')
        *mark = '\r';
      else
        *mark = *buf;
      mark++;
    } else {
      newlen--;
    }
    buf++;
  }

  *lenptr = newlen;
}

int main(int argc, char **argv) {
  unsigned char buf[4096];
  char *stdoutbuf;
  int parentpty;
  int len;
  int pid;
  int paclen = 256;
  int notranslate = 0;
  fd_set fdset;
  struct timeval tv;
  struct termios term;

  while ((len = getopt(argc, argv, "p:n")) != -1) {
    switch (len) {
    case 'p':
      paclen = atoi(optarg);
      break;
    case 'n':
      notranslate = 1;
      break;
    case ':':
    case '?':
      USAGE();
      exit(1);
    }
  }

  if (argc - optind < 2) {
    USAGE();
    exit(1);
  }

  if ((stdoutbuf = malloc(paclen)) == NULL) {
    PERROR("axpty: malloc");
    exit(1);
  }

  setvbuf(stdout, stdoutbuf, _IOFBF, paclen);

  /* signal(SIGCHLD, sigchld_handler); */
  signal(SIGCHLD, SIG_IGN);

  memset(&term, 0, sizeof(struct termios));
  term.c_cflag &= ~(CSIZE | PARENB);
  term.c_cflag |= CS8;

  pid = forkpty(&parentpty, NULL, &term, NULL);

  if (pid == -1) {
    /* fork error */
    PERROR("axpty: fork");
    exit(1);
  }

  if (pid == 0) {
    /* child */
    execve(argv[optind], argv + optind + 1, NULL);

    /* execve() should not return */
    perror("axpty: execve");
    exit(1);
  }

  /* parent */

  if (fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) == -1) {
    perror("axpty: fcntl");
    exit(1);
  }
  if (fcntl(parentpty, F_SETFL, O_NONBLOCK) == -1) {
    perror("axpty: fcntl");
    exit(1);
  }

  while (1) {
    FD_ZERO(&fdset);
    FD_SET(STDIN_FILENO, &fdset);
    FD_SET(parentpty, &fdset);
    tv.tv_sec = 0;
    tv.tv_usec = FLUSHTIMEOUT;

    len = select(256, &fdset, 0, 0, &tv);
    if (len == -1) {
      perror("axpty: select");
      exit(1);
    }
    if (len == 0) {
      fflush(stdout);
    }

    if (FD_ISSET(STDIN_FILENO, &fdset)) {
      len = read(STDIN_FILENO, buf, sizeof(buf));
      if (len < 0 && errno != EAGAIN) {
        perror("axpty: read");
        break;
      }
      if (len == 0)
        break;
      if (!notranslate)
        convert_cr_lf(buf, len);
      write(parentpty, buf, len);
    }
    if (FD_ISSET(parentpty, &fdset)) {
      len = read(parentpty, buf, paclen);
      if (len < 0 && errno != EAGAIN) {
        perror("axpty: read");
        break;
      }
      if (len == 0)
        break;
      if (!notranslate)
        convert_lf_cr(buf, &len);
      fwrite(buf, 1, len, stdout);
    }
  }

  kill(pid, SIGTERM);
  close(parentpty);
  return 0;
}
