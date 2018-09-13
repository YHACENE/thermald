#include <sys/types.h>
#include <sys/sysctl.h>
#include <err.h>
#include <signal.h>
#include <stdlib.h>
#include <strings.h>
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

const char	*procname;
int		fanctl_state_orig = FANCTL_AUTOMATIC;
size_t		fanctl_state_orig_size = sizeof(fanctl_state_orig);
int		mib_fanctl[MIB_LEN_FANCTL];
size_t		mib_len_fanctl;

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
			err(1, "failed to restore original fan state");
		exit(0);
		break;
	}
}

int
main(int argc, char *argv[])
{
	struct sigaction	sigact;
	size_t	mib_len_fanlvl, mib_len_temp;
	int	mib_fanlvl[MIB_LEN_FANLVL], mib_temp[MIB_LEN_TEMP];
	int	fanctl_state;

	procname = argv[0];
	/* retrieve MIBs for the sysctls */
	mib_len_fanctl = MIB_LEN_FANCTL;
	if (sysctlnametomib("dev.acpi_ibm.0.fan", &mib_fanctl[0],
	    &mib_len_fanctl) < 0)
		err(1, "failed to get fan MIB");
	mib_len_fanlvl = MIB_LEN_FANLVL;
	if (sysctlnametomib("dev.acpi_ibm.0.fan_level", &mib_fanlvl[0],
	    &mib_len_fanlvl) < 0)
		err(1, "failed to get fan_level MIB");
	mib_len_temp = MIB_LEN_TEMP;
	if (sysctlnametomib("hw.acpi.thermal.tz0.temperature", &mib_temp[0],
	    &mib_len_temp) < 0)
		err(1, "failed to get temperature MIB");

	/* set fan control to manual */
	fanctl_state = FANCTL_MANUAL;
	if (sysctl(&mib_fanctl[0], MIB_LEN_FANCTL, &fanctl_state_orig,
	    &fanctl_state_orig_size, &fanctl_state, sizeof(fanctl_state)) < 0)
		err(1, "failed to set fan state");

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
			err(1, "failed to get temperature");
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
			err(1, "failed to set fan level");

		printf("temp: %d, fan lvl: %d\n", currtemp, fanlvl);
		sleep(2);
	}

	exit(1);
}
