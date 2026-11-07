#ifndef KERNEL_ACPI_H
#define KERNEL_ACPI_H

#include <stdint.h>

/* Initialize ACPI reboot support. */
int acpi_init(void);

/* Check whether ACPI reboot is available. */
int acpi_reboot_supported(void);

/* Reboot the system using ACPI. */
void acpi_reboot(void);

#endif