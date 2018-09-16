# Thermald

A simple daemon to control the fan of my ThinkPad T410.

It uses the sysctls from the `acpi_ibm(4)` interface to control the fan speed
according to the temperature.
