/* sysver -- system version information tool for OpenBSD
 * SPDX-License-Identifier: Zlib
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <sys/cdefs.h>

#ifndef BSD_KERN_PATH
# define BSD_KERN_PATH    "/bsd"
# define REQUIRE_ROOT
#endif

#ifndef __progname
extern const char *__progname;
#endif

/* Command structure. */
struct command_option {
	int code;
	const char *cmd;
};

/* Find the position of a character in the haystack. */
static size_t strpchr(const char *haystack, const char c)
{
	const char *p;
	size_t i;

	for (i = 0, p = haystack; *p != '\0'; p++, i++) {
		if (*p == c)
			return (i);
	}

	return (0);
}

/* Consume a pointer until the provided size, without
   modifying the original pointer. */
__inline
static unsigned char *consltill(const unsigned char *p, size_t n)
{
	const unsigned char *s;

	s = p;
	while (n--)
		s++;

	return ((unsigned char *)s);
}

/* Invoke sysctl() and return the string content in a
   static buffer. */
static char *get_sysctl_string(int class_id, int key_id)
{
        int mib[2];
	size_t len;
	static char buf[60];

	mib[0] = class_id;
	mib[1] = key_id;
	len = sizeof(buf)/sizeof(buf[0]);
	if (sysctl(mib, 2, &buf, &len, NULL, 0) == -1)
		err(EXIT_FAILURE, "sysctl()");

	return (buf);
}

/* Invoke sysctl() and return the integer value. */
static int get_sysctl_number(int class_id, int key_id)
{
	int mib[2];
	size_t len;
        int res;

	mib[0] = class_id;
	mib[1] = key_id;
	len = sizeof(int);
	if (sysctl(mib, 2, &res, &len, NULL, 0) == -1)
		err(EXIT_FAILURE, "sysctl()");

	return (res);
}

/* Get the version string of the currently installed kernel. */
static char *get_inst_kern_ver(void)
{
	int fd;
        unsigned char buf[2048], *p;
	const unsigned char *end;
	ssize_t nread;
	char *dup;
	size_t pos_dist;

#ifdef REQUIRE_ROOT
	/* Reading kernel /bsd requires root priviledge. */
        if (getuid() != 0) {
		if (geteuid() != 0)
			errx(EXIT_FAILURE, "%s must be run as root", __progname);
		else
			errx(EXIT_FAILURE, "getuid(): %d != geteuid(): %d",
			     getuid(), geteuid());
	}
#endif

	fd = open(BSD_KERN_PATH, O_RDONLY);
	if (fd == -1)
		err(EXIT_FAILURE, "open()");

        while ((nread = read(fd, buf, sizeof(buf))) > 0) {
		for (p = buf, end = p + nread; p < end; p++) {
			/* Look for @(#) string. */
			if (*p == '@' && *(p + 1) == '(' &&
			    *(p + 2) == '#' && *(p + 3) == ')') {
				/* Consume leading 4 bytes "@(#)" (without quotes). */
				p = consltill(p, 4);
				if ((pos_dist = strpchr(p, '\n')) > 0)
					p[pos_dist] = '\0';

				close(fd);
				/* Casting from unsigned char to const char. */
				if ((dup = strdup((const char *)p)) == NULL)
					err(EXIT_FAILURE, "strdup()");

				return (dup);
		        }
		}
        }

	close(fd);
	return (NULL);
}

/* Print sysname, nodename, release, version, and machine name.
   Similar to uname(1) with -a parameter. */
static void get_uname_info(int only_version)
{
	struct utsname uts;

	if (uname(&uts) == -1)
		err(EXIT_FAILURE, "uname()");

	if (only_version)
		fprintf(stdout, "%s %s\n", uts.sysname, uts.release);
	else
		fprintf(stdout, "%s %s %s %s %s\n", uts.sysname, uts.nodename,
			uts.release, uts.version, uts.machine);
}

/* A tiny, only single argument "parser". */ 
static int match_args(struct command_option *cb, const char **argv)
{
	if (*(argv + 1) == NULL)
		return (-1);
	/* If you're iffy about this, it's just argv[1][0] and argv[1][1]. */
	if ((**(argv + 1) == '-' &&
	     *(*(argv + 1) + 1) == '\0'))
		return (-2);

	/* Ignore the program name. */
        for (argv++; (*cb).code != 0 ||
		     (*cb).cmd != NULL; cb++) {
		/* Don't accept anything if *argv is NULL. */
		if (*argv == NULL)
			break;
		if (strstr((*cb).cmd, *argv))
			return ((*cb).code);
        }

	return (0);
}

/* Print the usage. */
__dead
static void print_usage(int status)
{
	FILE *out;

	out = (status == EXIT_SUCCESS) ? stdout : stderr;
        fputs("usage\n"
	      " -machine\tmachine architecture\n"
	      " -model\t\tcpu model\n"
	      " -ncpu\t\tnumber of cpus\n"
	      " -border\tbyte order\n"
	      " -oncpu\t\tnumber of online cpus\n"
	      " -issmt\t\tis smt enabled?\n"
	      " -uname\t\tequivalent of uname -a\n"
	      " -bsd\t\topenbsd version\n"
	      " -ikern\t\tcurrently installed kernel\n"
	      " -help\t\tshow me\n", out);
	exit(status);
}

int main(__unused int argc, const char **argv)
{
	char *kern_ver;
	const char *byte_order;
	int byte_out;
	struct command_option cb[] = {
		{ 1,  "-machine" },
		{ 2,  "-model"   },
		{ 3,  "-ncpu"    },
		{ 4,  "-border"  },
		{ 5,  "-oncpu"   },
		{ 6,  "-issmt"   },
		{ 7,  "-uname"   },
		{ 8,  "-bsdver"  },
		{ 9,  "-ikern"   },
		{ 10, "-help"    },
		{ 0,  NULL       },
	};

	switch (match_args(cb, argv)) {
	case 1:
		fprintf(stdout, "%s\n",
			get_sysctl_string(CTL_HW, HW_MACHINE));
		break;
	case 2:
		fprintf(stdout, "%s\n",
			get_sysctl_string(CTL_HW, HW_MODEL));
		break;
	case 3:
		fprintf(stdout, "%d\n",
			get_sysctl_number(CTL_HW, HW_NCPU));
		break;
	case 4:
	        byte_out = get_sysctl_number(CTL_HW, HW_BYTEORDER);
		switch (byte_out) {
		case 1234:
			byte_order = "little";
			break;
		case 4321:
			byte_order = "big";
			break;
		default:
			byte_order = "mixed";
			break;
		}
		fprintf(stdout, "%s\n", byte_order);
		break;
	case 5:
		fprintf(stdout, "%d\n",
			get_sysctl_number(CTL_HW, HW_NCPUONLINE));
		break;
	case 6:
		fprintf(stdout, "%s\n",
			get_sysctl_number(CTL_HW, HW_SMT) ? "yes" : "no");
		break;
	case 7:
	        get_uname_info(0);
		break;
	case 8:
		get_uname_info(1);
		break;
	case 9:
	        if ((kern_ver = get_inst_kern_ver()) == NULL) {
		        fputs("unknown (I can't find it!)\n", stderr);
		} else {
			fprintf(stdout, "%s\n", kern_ver);
			free(kern_ver);
		}
		break;
	case 10:
		print_usage(EXIT_SUCCESS);
	case 0:
		errx(EXIT_FAILURE, "'%s' option is invalid.", *(argv + 1));
	case -1:
	        print_usage(EXIT_FAILURE);
	case -2:
		errx(EXIT_FAILURE, "'%s' expects an option.", *(argv + 1));
	}
}
