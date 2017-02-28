/*
 * sched.c - scheduling of block updates (timeout, signal or click)
 * Copyright (C) 2014  Vivien Didelot
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/event.h>

#include "io.h"
#include "bar.h"
#include "block.h"
#include "json.h"
#include "log.h"

/* static sigset_t sigset; */

static sigset_t siglist;
int kqueue_fd;

static int
gcd(int a, int b)
{
	while (b != 0)
		a %= b, a ^= b, b ^= a, a ^= b;

	return a;
}

static unsigned int
longest_sleep(struct bar *bar)
{
	unsigned int time = 0;

	if (bar->num > 0 && bar->blocks->interval > 0)
		time = bar->blocks->interval; /* first block's interval */

	if (bar->num < 2)
		return time;

	/* The maximum sleep time is actually the GCD between all block intervals */
	for (unsigned int i = 1; i < bar->num; ++i)
		if ((bar->blocks + i)->interval > 0)
			time = gcd(time, (bar->blocks + i)->interval);

	return time;
}

static int
setup_timer(struct bar *bar)
{
	const unsigned sleeptime = longest_sleep(bar);

	if (!sleeptime) {
		debug("no timer needed");
		return 0;
	}

  struct kevent timer_event;

  EV_SET(&timer_event, 0x01, EVFILT_TIMER, EV_ADD, NOTE_SECONDS, sleeptime, NULL);
  if (kevent(kqueue_fd, &timer_event, 1, NULL, 0 , NULL) < 0){
    errorx("kevent timer %d ", (int)timer_event.ident);
    return 1;
  }
	debug("starting timer with interval of %d seconds", sleeptime);
	return 0;
}

static int
add_signal(int sig)
{
  struct kevent signal_fd;

  EV_SET(&signal_fd, sig, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);

	if (sigaddset(&siglist, sig) == -1){
    errorx("sigaddset(%d)", sig);
    return 1;
  }

  if (kevent(kqueue_fd, &signal_fd, 1, NULL, 0, NULL) < 0) {
    errorx("kqueue_signal(%d)", sig);
    return 1;
  }
  return 0;
}

static int
setup_signals(void)
{

	if (sigemptyset(&siglist) == -1) {
		errorx("sigemptyset");
		return 1;
	}


	/* Control signals */
	add_signal(SIGTERM);
	add_signal(SIGINT);

	/* Block updates (forks) */
	add_signal(SIGCHLD);

	/* Deprecated signals */
	add_signal(SIGUSR1);
	add_signal(SIGUSR2);

	/* Click signal */
	add_signal(SIGIO);

	/* Real-time signals for blocks */
	for (int sig = SIGRTMIN + 1; sig <= SIGRTMAX; ++sig) {
		debug("provide signal %d (%s)", sig, strsignal(sig));
		add_signal(sig);
	}

  /* Block signals for which we are interested in waiting */
  if (sigprocmask(SIG_SETMASK, &siglist, NULL) == -1) {
  errorx("sigprocmask");
  return 1;
  }

	return 0;
}


/*
  NOTE: Parameter sig is here to match the signature for all platform
 */
int
io_signal(int fd, int sig)
{
struct kevent fd_event;

/* register kevent */
/* i don't need signals to register io */

EV_SET(&fd_event, fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, NULL);

if (kevent(kqueue_fd, &fd_event, 1, NULL, 0, NULL) < 0){
/* TODO: make sure all errors are included */
errorx("kevent %d", kqueue_fd);
return -1;
}

return 0;
}

int
sched_init(struct bar *bar)
{
  if ((kqueue_fd = kqueue()) < 0)
    errorx("kqueue_fd)");

	if (setup_signals())
		return 1;

	if (setup_timer(bar))
		return 1;

	/* Setup event I/O for stdin (clicks) */
	if (!isatty(STDIN_FILENO))
		if (io_signal(STDIN_FILENO, 0))
			return 1;

	return 0;
}

void
sched_start(struct bar *bar)
{

  struct kevent recv;
  int ret;
	/*
	 * Initial display (for static blocks and loading labels),
	 * and first forks (for commands with an interval).
	 */
	json_print_bar(bar);
	bar_poll_timed(bar);

	while (1) {

    ret = kevent(kqueue_fd, NULL, 0, &recv, 1, NULL);
		if (ret == -1) {
			/* Hiding the bar may interrupt this system call */
			if (errno == EINTR)
				continue;

			errorx("kevent");
			break;
		}

    /* quit? */
    if (recv.filter == EVFILT_SIGNAL &&
        (recv.ident == SIGTERM || recv.ident == SIGINT)){
      debug("kqueue interrupt or terminate");
      break;
    }

    switch(recv.filter){

    case EVFILT_SIGNAL:
      debug("kqueue event %d (%s)received (EVFILT_SIGNAL)", (int)recv.ident,
            strsignal(recv.ident));
      if(recv.ident == SIGCHLD) {
        /* Child(ren) dead? */
        bar_poll_exited(bar);
        json_print_bar(bar);
      } else if (recv.ident == SIGIO) {
        /* Block clicked? */
        bar_poll_clicked(bar);
      } else if (recv.ident > SIGRTMIN && recv.ident <= SIGRTMAX) {
        /* Blocks signaled? */
        bar_poll_signaled(bar, recv.ident - SIGRTMIN);
      } else if (recv.ident == SIGUSR1 || recv.ident == SIGUSR2) {
        /* Deprecated signals? */
        error("SIGUSR{1,2} are deprecated, ignoring.");
      }
      break;

    case EVFILT_READ:
      /* Persistent block ready to be read? */
      debug("kqueue event %d received (EVFILT_READ)", (int)recv.ident);
			bar_poll_readable(bar, recv.ident);
			json_print_bar(bar);
      break;

    case EVFILT_TIMER:
      debug("kqueue event %d received (EVFILT_TIMER)", (int)recv.ident);
			bar_poll_outdated(bar);
      break;

    default:
      errorx("unknown kevent received");
    }
  }

	/*
	 * Unblock signals (so subsequent syscall can be interrupted)
	 * and wait for child processes termination.
	 */
	if (sigprocmask(SIG_UNBLOCK, &siglist, NULL) == -1)
		errorx("sigprocmask");
	while (waitpid(-1, NULL, 0) > 0)
		continue;

	debug("quit scheduling");
}
