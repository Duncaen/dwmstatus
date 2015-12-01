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
#include <sys/statvfs.h>

#include <X11/Xlib.h>

#include <alsa/asoundlib.h>
#include <alsa/control.h>

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
readfile(char *base, char *file)
{
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	if (fd == NULL)
		return NULL;
	free(path);

	if (fgets(line, sizeof(line)-1, fd) == NULL)
		return NULL;
	fclose(fd);

	return smprintf("%s", line);
}

char *
freespace(char *mntpt)
{
	struct statvfs fs;
	double total, used = 0;

	if ((statvfs(mntpt, &fs)) < 0) {
		fprintf(stderr, "can't get info on disk.\n");
		return("?");
	}

	total = (fs.f_blocks * fs.f_frsize);
	used = (fs.f_blocks - fs.f_bfree) * fs.f_frsize ;
	return (smprintf("%.0f", (used / total * 100)));
}

/*
 * Linux seems to change the filenames after suspend/hibernate
 * according to a random scheme. So just check for both possibilities.
 */
char *
battery(char *base)
{
	char *co;
	int descap, remcap;

	descap = -1;
	remcap = -1;

	co = readfile(base, "present");
	if (co == NULL || co[0] != '1') {
		if (co != NULL) free(co);
		return smprintf("?");
	}
	free(co);

	co = readfile(base, "charge_full_design");
	if (co == NULL) {
		co = readfile(base, "energy_full_design");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &descap);
	free(co);

	co = readfile(base, "charge_now");
	if (co == NULL) {
		co = readfile(base, "energy_now");
		if (co == NULL)
			return smprintf("");
	}
	sscanf(co, "%d", &remcap);
	free(co);

	if (remcap < 0 || descap < 0)
		return smprintf("invalid");

	return smprintf("%.0f", ((float)remcap / (float)descap) * 100);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	memset(buf, 0, sizeof(buf));
	setenv("TZ", tzname, 1);

	tim = time(NULL);
	timtm = localtime(&tim);

	if (timtm == NULL)
		fatal("localtime\n");

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm))
		fatal("strftime == 0\n");

	return smprintf(buf);
}

int
main(int argc, char *argv[])
{
	char *status = NULL;
	char *date = NULL;
	char *bat = NULL;
	char *home = NULL;
	char *root = NULL;
	char *vol = NULL;
	double avgs[3];
	time_t now = 0;
	time_t lasttime[2] = { 0, 0 };
	snd_hctl_t *hctl;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_value_t *control;

	// To find card and subdevice: /proc/asound/, aplay -L, amixer controls
	snd_hctl_open(&hctl, "hw:0", 0);
	snd_hctl_load(hctl);

	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);

	// amixer controls
	snd_ctl_elem_id_set_name(id, "Master Playback Volume");
	snd_hctl_elem_t *elem = snd_hctl_find_elem(hctl, id);
	snd_ctl_elem_value_alloca(&control);
	snd_ctl_elem_value_set_id(control, id);

	if (!(dpy = XOpenDisplay(NULL)))
		fatal("dwmstatus: cannot open display.\n");

	for (;;) {
		now = time(NULL);
		if (difftime(now, lasttime[0]) >= 60) {
			free(date);
			free(bat);
			date = mktimes("%a %d %b %H:%M", default_tz);
			bat = battery("/sys/class/power_supply/BAT0/");
			lasttime[0] = now;
		}
		if (difftime(now, lasttime[1]) >= 300) {
			free(home);
			free(root);
			home = freespace("/home");
			root = freespace("/");
			lasttime[1] = now;
		}

		if (getloadavg(avgs, 3) < 0)
			fatal("dwmstatus: failed to get loadavg\n");

		snd_hctl_elem_read(elem, control);
		vol = smprintf("%.0d", snd_ctl_elem_value_get_integer(control, 0));

		status = smprintf("VOL: %s%% | AVG: %.2f %.2f %.2f | SSD: / %s%% ~ %s%% | BAT: %s% | %s",
				vol, avgs[0], avgs[1], avgs[2], root, home, bat, date);

		XStoreName(dpy, DefaultRootWindow(dpy), status);
		XSync(dpy, False);

		free(status);
		free(vol);
		sleep(1);
	}

	XCloseDisplay(dpy);
	snd_hctl_close(hctl);
	return 0;
}
