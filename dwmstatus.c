#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/statvfs.h>

#include <X11/Xlib.h>

#include "config.h"

static Display *dpy;


void
fatal(const char *msg)
{
	fprintf(stderr, msg);
	exit(1);
}

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *buf = NULL;

	va_start(fmtargs, fmt);
	if (vasprintf(&buf, fmt, fmtargs) == -1)
		fatal("malloc vasprintf\n");
	va_end(fmtargs);

	return buf;
}

char *
battery(int batfd)
{
	static char buf[8];
	ssize_t rd;

	rd = read(batfd, &buf, sizeof buf);
	buf[rd-1] = '%';
	buf[rd] = 0;
	lseek(batfd, 0, SEEK_SET);

	return buf;
}

char *
mktimes(char *fmt, time_t tim)
{
	static char buf[128];
	struct tm *timtm;
	size_t n;

	
	if (!(timtm = localtime(&tim)))
		fatal("localtime\n");

	if (!(n = strftime(buf, sizeof(buf)-1, fmt, timtm)))
		fatal("strftime == 0\n");
	buf[n] = 0;

	return buf;
}

int
main(int argc, char *argv[])
{
	static char buf[128];
	char *date = NULL;
	char *bat = NULL;
	double avgs[3];
	time_t now = 0;
	time_t lasttime[2] = { 0, 0 };
	int batfd = -1;

	if (!(dpy = XOpenDisplay(NULL)))
		fatal("dwmstatus: cannot open display.\n");

	setenv("TZ", default_tz, 1);

	bat = "?";
	batfd = open("/sys/class/power_supply/BAT0/capacity", O_RDONLY);

	for (;;) {
		/*if (difftime(now, lasttime[0]) >= 60) {*/
			now = time(NULL);
			date = mktimes("%a %d %b %H:%M", now);
			if (batfd != -1)
				bat = battery(batfd);
			if (getloadavg(avgs, 3) < 0)
				fatal("dwmstatus: failed to get loadavg\n");
			lasttime[0] = now;
		/*}*/

		/* | \x0019*/
		snprintf(buf, sizeof buf-1,
		    " AVG %.2f %.2f %.2f | BAT %s | %s ",
		    avgs[0], avgs[1], avgs[2], bat, date);

		XStoreName(dpy, DefaultRootWindow(dpy), buf);
		XSync(dpy, False);

		sleep(60);
	}

	XCloseDisplay(dpy);
	return 0;
}
