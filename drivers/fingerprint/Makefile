#
# Makefile for the sensors drivers.
#

# Each configuration option enables a list of files.

# Stock sets this from the top-level Makefile (KBUILD_FP_SENSOR_CFLAGS) for
# every non-factory build. The sensor is owned by TZ (BLSP3 + its BAM pipes),
# so the kernel must not touch the bus: no spi_setup, no type check, no spi_sync.
ccflags-$(CONFIG_SENSORS_FINGERPRINT) := -DENABLE_SENSORS_FPRINT_SECURE

obj-$(CONFIG_SENSORS_FINGERPRINT)	+= fingerprint_sysfs.o
obj-$(CONFIG_SENSORS_ET5XX)		+= et5xx-spi.o et5xx-spi_data_transfer.o
