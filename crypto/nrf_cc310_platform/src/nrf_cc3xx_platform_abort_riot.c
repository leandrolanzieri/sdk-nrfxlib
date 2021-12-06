/**
 * Copyright (c) 2021 HAW Hamburg
 *
 */
#include "periph/pm.h"
#include "nrf_cc3xx_platform_abort.h"

#define ENABLE_DEBUG 1
#include "debug.h"

/** @brief RIOT abort function for nrfxlib
 */
static void abort_function(char const * const reason)
{
	DEBUG("%s\n", reason);
	pm_reboot();
}

/** @brief Definition of abort API to set in nrf_cc3xx_platform
 */
static const nrf_cc3xx_platform_abort_apis_t apis =
{
	.abort_handle = NULL,
	.abort_fn = abort_function,
};

/** @brief Function to initialize the nrf_cc3xx_platform abort APIs.
 */
void nrf_cc3xx_platform_abort_init(void)
{
	nrf_cc3xx_platform_set_abort(&apis);
}
