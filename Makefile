PROG =	thermald
# SRCS =	${PROG}.c
# OBJS =	${SRCS:.c=.o}
MAN =

FILES =		thermald.rc
FILESNAME =	thermald
FILESDIR =	${DESTDIR}${PREFIX}/etc/rc.d
FILESMODE =	644

LDADD =	-lutil

CSTD =	iso9899:2011
WARN =	6

PREFIX?=	/usr/local
BINDIR =	${PREFIX}/bin

.include <bsd.prog.mk>
