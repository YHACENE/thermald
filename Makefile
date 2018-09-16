PROG =	thermald
# SRCS =	${PROG}.c
# OBJS =	${SRCS:.c=.o}
MAN =

LDADD =	-lutil

CSTD =	iso9899:2011
WARN =	6

.include <bsd.prog.mk>
