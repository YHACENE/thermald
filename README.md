# Thermald

A simple FreeBSD daemon to control the fan of my ThinkPad T410.

It uses the sysctls from the `acpi_ibm(4)` interface to control the fan speed
according to the temperature.

## Installation

Run `make install` to install it to `/usr/local`. The variables `${DESTDIR}`
and `${PREFIX}` are available to tune the install location.

Run `sysrc thermald_enable=YES` to have `thermald` started automatically on
boot.
