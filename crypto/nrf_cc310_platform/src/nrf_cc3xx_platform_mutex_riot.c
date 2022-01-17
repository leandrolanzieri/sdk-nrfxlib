/**
 * Copyright (c) 2021 HAW Hamburg
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stdint.h>
#include <stddef.h>

#include "mutex.h"

#define ENABLE_DEBUG 0
#include "debug.h"

#include "nrf_cc3xx_platform_defines.h"
#include "nrf_cc3xx_platform_mutex.h"

#define CONFIG_NRFXLIB_MUTEX_NUMOF  64

typedef struct {
    bool used;
    mutex_t mutex;
} mutex_pool_item_t;

static mutex_pool_item_t _mutex_pool[CONFIG_NRFXLIB_MUTEX_NUMOF] = {
        {
            .used = false,
            .mutex = MUTEX_INIT
        }
    };
static mutex_t _allocation_mutex = MUTEX_INIT;

static mutex_t *_alloc_mutex(void) {
    mutex_lock(&_allocation_mutex);
    mutex_t *ret = NULL;

    for (unsigned i = 0; i < CONFIG_NRFXLIB_MUTEX_NUMOF; i++) {
        if (!_mutex_pool[i].used) {
            ret = &_mutex_pool[i].mutex;
            _mutex_pool[i].used = true;
            mutex_init(ret);
            break;
        }
    }

    mutex_unlock(&_allocation_mutex);
    return ret;
}

static void _dealloc_mutex(mutex_t *mutex) {
    mutex_lock(&_allocation_mutex);

    mutex_pool_item_t *item = container_of(mutex, mutex_pool_item_t, mutex);
    item->used = false;

    mutex_unlock(&_allocation_mutex);
}

/** @brief external reference to the cc3xx platform mutex APIs*/
extern nrf_cc3xx_platform_mutex_apis_t  platform_mutex_apis;

/** @brief Definition of mutex for symmetric cryptography
 */
static mutex_t sym_mutex_int = MUTEX_INIT;

/** @brief Definition of mutex for asymmetric cryptography
 */
static mutex_t asym_mutex_int = MUTEX_INIT;

/** @brief Definition of mutex for random number generation
 */
static mutex_t rng_mutex_int = MUTEX_INIT;

/** @brief Definition of mutex for power management changes
 */
static mutex_t power_mutex_int = MUTEX_INIT;

/** @brief Definition of mutex for heap allocations using buffer_mem_alloc.c
 */
static mutex_t heap_mutex_int = MUTEX_INIT;


/**@brief Definition of RTOS-independent symmetric cryptography mutex
 * with NRF_CC3XX_PLATFORM_MUTEX_MASK_IS_VALID set to indicate that
 * allocation is unnecessary
*/
static nrf_cc3xx_platform_mutex_t sym_mutex =
{
    .mutex = &sym_mutex_int,
    .flags = NRF_CC3XX_PLATFORM_MUTEX_MASK_IS_VALID
};


/**@brief Definition of RTOS-independent asymmetric cryptography mutex
 * with NRF_CC3XX_PLATFORM_MUTEX_MASK_IS_VALID set to indicate that
 * allocation is unnecessary
*/
static nrf_cc3xx_platform_mutex_t asym_mutex =
{
    .mutex = &asym_mutex_int,
    .flags = NRF_CC3XX_PLATFORM_MUTEX_MASK_IS_VALID
};


/**@brief Definition of RTOS-independent random number generation mutex
 * with NRF_CC3XX_PLATFORM_MUTEX_MASK_IS_VALID set to indicate that
 * allocation is unnecessary
*/
static nrf_cc3xx_platform_mutex_t rng_mutex =
{
    .mutex = &rng_mutex_int,
    .flags = NRF_CC3XX_PLATFORM_MUTEX_MASK_IS_VALID
};


/**@brief Definition of RTOS-independent power management mutex
 * with NRF_CC3XX_PLATFORM_MUTEX_MASK_IS_VALID set to indicate that
 * allocation is unnecessary
*/
static nrf_cc3xx_platform_mutex_t power_mutex =
{
    .mutex = &power_mutex_int,
    .flags = NRF_CC3XX_PLATFORM_MUTEX_MASK_IS_VALID
};

/** @brief Definition of RTOS-independent heap allocation mutex
 *  with NRF_CC3XX_PLATFORM_MUTEX_MASK_IS_VALID set to indicate that
 *  allocation is unnecessary
 *
 * @note This symbol can't be static as it is referenced in the replacement
 *       file mbemory_buffer_alloc.c inside the heap structure.
 */
nrf_cc3xx_platform_mutex_t heap_mutex =
{
    .mutex = &heap_mutex_int,
    .flags = NRF_CC3XX_PLATFORM_MUTEX_MASK_IS_VALID
};


/** @brief Static function to unlock a mutex
 */
static void mutex_init_platform(nrf_cc3xx_platform_mutex_t *mutex)
{
    mutex_t *riot_mutex;
    DEBUG("mutex_init_platform\n");
    /* Ensure that the mutex is valid (not NULL) */
    if (mutex == NULL) {
        platform_abort_apis.abort_fn("mutex_init called with NULL parameter");
    }

    /* Allocate if this has not been initialized statically */
    if (mutex->flags == NRF_CC3XX_PLATFORM_MUTEX_MASK_INVALID &&
        mutex->mutex == NULL) {
        /* Use mutex from the pool */
        riot_mutex = _alloc_mutex();
        if (!riot_mutex) {
            DEBUG("mutex_init_platform: could not allocate mutex\n");
            platform_abort_apis.abort_fn("could not allocate mutex\n");
            return;
        }

        mutex_init(riot_mutex);
        mutex->mutex = riot_mutex;

        /* Set a flag to ensure that mutex is deallocated by the freeing operation */
        mutex->flags |= NRF_CC3XX_PLATFORM_MUTEX_MASK_IS_ALLOCATED;
    }

    riot_mutex = mutex->mutex;
    mutex_init(riot_mutex);

    /* Set the mask to indicates that the mutex is valid */
    mutex->flags |= NRF_CC3XX_PLATFORM_MUTEX_MASK_IS_VALID;
}

/** @brief Static function to free a mutex
 */
static void mutex_free_platform(nrf_cc3xx_platform_mutex_t *mutex)
{
    DEBUG("mutex_free_platform\n");
    /* Ensure that the mutex is valid (not NULL) */
    if (mutex == NULL) {
        platform_abort_apis.abort_fn("mutex_free called with NULL parameter");
    }

    if (mutex->flags == NRF_CC3XX_PLATFORM_MUTEX_MASK_INVALID) {
        /*Nothing to free*/
        return;
    }

    /* Check if the mutex was allocated or being statically defined */
    if ((mutex->flags & NRF_CC3XX_PLATFORM_MUTEX_MASK_IS_ALLOCATED) != 0) {
        _dealloc_mutex(mutex->mutex);
        mutex->mutex = NULL;
    }
    else {
        mutex_init(mutex->mutex);
    }
    // mutex_unlock(mutex->mutex);

    /* Reset the mutex to invalid state */
    mutex->flags = NRF_CC3XX_PLATFORM_MUTEX_MASK_INVALID;
}


/** @brief Static function to lock a mutex
 */
static int mutex_lock_platform(nrf_cc3xx_platform_mutex_t *mutex)
{
    DEBUG("mutex_lock_platform\n");
    /* Ensure that the mutex is valid (not NULL) */
    if (mutex == NULL) {
        return NRF_CC3XX_PLATFORM_ERROR_PARAM_NULL;
    }

    /* Ensure that the mutex has been initialized */
    if (mutex->flags == NRF_CC3XX_PLATFORM_MUTEX_MASK_INVALID) {
        return NRF_CC3XX_PLATFORM_ERROR_MUTEX_NOT_INITIALIZED;
    }

    mutex_lock(mutex->mutex);
    return NRF_CC3XX_PLATFORM_SUCCESS;
}


/** @brief Static function to unlock a mutex
 */
static int mutex_unlock_platform(nrf_cc3xx_platform_mutex_t *mutex)
{
     DEBUG("mutex_unlock_platform\n");
    /* Ensure that the mutex is valid (not NULL) */
    if (mutex == NULL) {
        platform_abort_apis.abort_fn("mutex_unlock called with NULL parameter");
    }

    /* Ensure that the mutex has been initialized */
    if (mutex->flags == NRF_CC3XX_PLATFORM_MUTEX_MASK_INVALID) {
        return NRF_CC3XX_PLATFORM_ERROR_MUTEX_NOT_INITIALIZED;
    }

    mutex_unlock(mutex->mutex);
    return NRF_CC3XX_PLATFORM_SUCCESS;
}


/**@brief Constant definition of mutex APIs to set in nrf_cc3xx_platform
 */
const nrf_cc3xx_platform_mutex_apis_t mutex_apis =
{
    .mutex_init_fn = mutex_init_platform,
    .mutex_free_fn = mutex_free_platform,
    .mutex_lock_fn = mutex_lock_platform,
    .mutex_unlock_fn = mutex_unlock_platform
};


/** @brief Constant definition of mutexes to set in nrf_cc3xx_platform
 */
const nrf_cc3xx_platform_mutexes_t mutexes =
{
    .sym_mutex = &sym_mutex,
    .asym_mutex = &asym_mutex,
    .rng_mutex = &rng_mutex,
    .reserved = NULL,
    .power_mutex = &power_mutex
};

/** @brief Function to initialize the nrf_cc3xx_platform mutex APIs.
 */
void nrf_cc3xx_platform_mutex_init(void)
{
    DEBUG("platform mutex init\n");
    nrf_cc3xx_platform_set_mutexes(&mutex_apis, &mutexes);
}
