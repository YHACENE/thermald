#include <sys/types.h>
#include <sys/sysctl.h>
#include <err.h>
#include <errno.h>
#include <libutil.h>
#include <signal.h>
#include <stdlib.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>

#include <stdio.h>

#define MIB_LEN_FANCTL	4
#define MIB_LEN_FANLVL	4
#define MIB_LEN_TEMP	5

enum FanCtlMode {
	FANCTL_MANUAL = 0,
	FANCTL_AUTOMATIC
};

struct TempLevel {
	int	temp;
	int	level;
};

/* temperature to fan level mappin, sorted by descending temperature */
static struct TempLevel temp_levels[] = { { .temp = 80, .level = 7 },
	{ .temp = 60, .level = 4 },
	{ .temp = 50, .level = 1 }
};
static const size_t temp_levels_size = sizeof(temp_levels)
    / sizeof(temp_levels[0]);

struct pidfh	*pfh;
int		 debug = 0;
int		 fanctl_state_orig = FANCTL_AUTOMATIC;
size_t		 fanctl_state_orig_size = sizeof(fanctl_state_orig);
int		 mib_fanctl[MIB_LEN_FANCTL];
size_t		 mib_len_fanctl;

static void __dead2	log_err(int, const char *);
static void		log_warn(const char *);
static void		pidfile(void);
static void		sighandler(int);
static void		rmpidfile(void);
static void __dead2	usage(void);

int
main(int argc, char *argv[])
{
	struct pidfh		*pfh;
	struct sigaction	 sigact;
	size_t	mib_len_fanlvl, mib_len_temp;
	int	mib_fanlvl[MIB_LEN_FANLVL], mib_temp[MIB_LEN_TEMP];
	int	c, fanctl_state;

	while ((c = getopt(argc, argv, "d")) != -1) {
		switch (c) {
		case 'd':
			debug = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	openlog("thermald", LOG_PID, LOG_DAEMON);
	if (!debug) {
		if (daemon(0, 0) < 0) {
			err(1, "daemon");
		}
		pidfile();
	}

	/* retrieve MIBs for the sysctls */
	mib_len_fanctl = MIB_LEN_FANCTL;
	if (sysctlnametomib("dev.acpi_ibm.0.fan", &mib_fanctl[0],
	    &mib_len_fanctl) < 0)
		log_err(1, "failed to get fan MIB");
	mib_len_fanlvl = MIB_LEN_FANLVL;
	if (sysctlnametomib("dev.acpi_ibm.0.fan_level", &mib_fanlvl[0],
	    &mib_len_fanlvl) < 0)
		log_err(1, "failed to get fan_level MIB");
	mib_len_temp = MIB_LEN_TEMP;
	if (sysctlnametomib("hw.acpi.thermal.tz0.temperature", &mib_temp[0],
	    &mib_len_temp) < 0)
		log_err(1, "failed to get temperature MIB");

	/* set fan control to manual */
	fanctl_state = FANCTL_MANUAL;
	if (sysctl(&mib_fanctl[0], MIB_LEN_FANCTL, &fanctl_state_orig,
	    &fanctl_state_orig_size, &fanctl_state, sizeof(fanctl_state)) < 0)
		log_err(1, "failed to set fan state");

	bzero(&sigact, sizeof(sigact));
	sigemptyset(&sigact.sa_mask);
	sigact.sa_handler = sighandler;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);

	for (;;) {
		int	currtemp;
		size_t	currtemp_len = 4;

		/* maybe `/var/run/devd.seqpacket.pipe` should be used instead
		 * of sysctl */
		if (sysctl(&mib_temp[0], MIB_LEN_TEMP, &currtemp, &currtemp_len,
		    NULL, (size_t)0) < 0)
			log_err(1, "failed to get temperature");
		currtemp -= 2731;  /* convert to Celsius */
		currtemp /= 10;  /* chop of first decimal */

		int	fanlvl = 0;
		for (size_t i = 0; i < temp_levels_size; i++) {
			if (currtemp > temp_levels[i].temp) {
				fanlvl = temp_levels[i].level;
				break;
			}
		}
		if (sysctl(&mib_fanlvl[0], MIB_LEN_FANLVL, NULL, NULL,
		    &fanlvl, 4) < 0)
			log_err(1, "failed to set fan level");

		sleep(2);
	}

	exit(1);
}

void
log_err(int eval, const char *msg)
{
	if (debug)
		err(eval, "%s", msg);
	syslog(LOG_ERR, "%s: %m", msg);
	exit(eval);
}

void
log_warn(const char *msg)
{
	if (debug)
		warn("%s", msg);
	else
		syslog(LOG_WARNING, "%s", msg);
}

void
pidfile(void)
{
	int otherpid;

	pfh = pidfile_open("/var/run/thermald.pid", 0600, &otherpid);
	if (pfh == NULL) {
		if (errno == EEXIST)
			errx(1, "already running as %jd", (intmax_t)otherpid);
		log_err(1, "pidfile_open");
	}
	if (pidfile_write(pfh) < 0)
		log_warn("pidfile_write");
	atexit(rmpidfile);
}

static void
sighandler(int signum)
{
	switch (signum) {
	case SIGINT:
	case SIGQUIT:
	case SIGTERM:
		/* restore the original fan control state on exit */
		if (sysctl(&mib_fanctl[0], MIB_LEN_FANCTL, NULL, NULL,
		    &fanctl_state_orig, fanctl_state_orig_size) < 0)
			log_err(1, "failed to restore original fan state");
		fprintf(stderr, "exiting\n");
		exit(0);
		break;
	}
}

void
rmpidfile(void)
{
	if (pidfile_remove(pfh) < 0)
		log_warn("pidfile_remove");
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-d]\n", getprogname());
	exit(1);
}
