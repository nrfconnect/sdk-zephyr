/*
 * Copyright (c) 2015 - 2018, Nordic Semiconductor ASA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NRF_ACL_H__
#define NRF_ACL_H__

#include <nrfx.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TODO Should this be here? AFAICS we can not use KConfig here*/
#define NRF_ACL_PERMISSIONS_SIZE_MAX_VAL 512*1024

/**
 * @enum nrf_acl_instance_t
 * @brief ACL instances.
 */
typedef enum
{
    NRF_ACL0 = 0,  /**< Instance 0. */
    NRF_ACL1 = 1,  /**< Instance 1. */
    NRF_ACL2 = 2,  /**< Instance 2. */
    NRF_ACL3 = 3,  /**< Instance 3. */
    NRF_ACL4 = 4,  /**< Instance 4. */
    NRF_ACL5 = 5,  /**< Instance 5. */
    NRF_ACL6 = 6,  /**< Instance 6. */
    NRF_ACL7 = 7,  /**< Instance 7. */
    NRF_ACL_NUM_INSTANCES
} nrf_acl_instance_t;

/* TODO - Have these as define instead? Since their type does not need to be exposed */
/** @brief ACL Read permissions. */
typedef enum
{
    NRF_ACL_PERMISSIONS_READ_ENABLE  = \
        (ACL_ACL_PERM_READ_Enable  << ACL_ACL_PERM_READ_Pos) & ACL_ACL_PERM_READ_Msk, /**< Read enable. */
    NRF_ACL_PERMISSIONS_READ_DISABLE = \
        (ACL_ACL_PERM_READ_Disable << ACL_ACL_PERM_READ_Pos) & ACL_ACL_PERM_READ_Msk, /**< Read disable. */
} nrf_acl_permissions_read_t;

/** @brief ACL Write permissions. */
typedef enum
{
    NRF_ACL_PERMISSIONS_WRITE_ENABLE  = \
        (ACL_ACL_PERM_WRITE_Enable  << ACL_ACL_PERM_WRITE_Pos) & ACL_ACL_PERM_WRITE_Msk, /**< Write enable. */
    NRF_ACL_PERMISSIONS_WRITE_DISABLE = \
        (ACL_ACL_PERM_WRITE_Disable << ACL_ACL_PERM_WRITE_Pos) & ACL_ACL_PERM_WRITE_Msk, /**< Write disable. */
} nrf_acl_permissions_write_t;

/** @brief ACL permissions. */
typedef enum
{
    NRF_ACL_PERMISSIONS_NO_PROTECTION = 0, /**< No protection. Illegal value. */
    NRF_ACL_PERMISSIONS_READ_NO_WRITE    = NRF_ACL_PERMISSIONS_READ_ENABLE  | NRF_ACL_PERMISSIONS_WRITE_DISABLE, /**< Read allowed, write disallowed. */
    NRF_ACL_PERMISSIONS_NO_READ_WRITE    = NRF_ACL_PERMISSIONS_READ_DISABLE | NRF_ACL_PERMISSIONS_WRITE_ENABLE,  /**< Read disallowed, write allowed. */
    NRF_ACL_PERMISSIONS_NO_READ_NO_WRITE = NRF_ACL_PERMISSIONS_READ_DISABLE | NRF_ACL_PERMISSIONS_WRITE_DISABLE, /**< Read disallowed, write disallowed. */
} nrf_acl_permissions_t;

/**
 * @brief Function for setting ACL control for given instance.
 *
 * @param[in] instance ACL instance to use.
 * @param[in] address  Word aligned start address. Must be page aligned.
 * @param[in] size     Size of region to protect in bytes. Must be page aligned.
 * @param[in] perm     Permissions to set for region to protect.
 */
__STATIC_INLINE void nrf_acl_access_control_set(nrf_acl_instance_t instance,
                                                uint32_t address,
                                                size_t size,
                                                nrf_acl_permissions_t perm);

/**
 * @brief Function for getting the configured region address of a specific ACL instance.
 *
 * @param[in] instance ACL instance to use.
 *
 * @return Configured region address of given ACL instance.
 */
__STATIC_INLINE uint32_t nrf_acl_access_control_address_get(nrf_acl_instance_t instance);

/**
 * @brief Function for getting the configured region size of a specific ACL instance.
 *
 * @param[in] instance ACL instance to use.
 *
 * @return Configured region size of given ACL instance.
 */
__STATIC_INLINE size_t nrf_acl_access_control_size_get(nrf_acl_instance_t instance);

/**
 * @brief Function for getting the configured region permissions of a specific ACL instance.
 *
 * @param[in] instance ACL instance to use.
 *
 * @return Configured permissions of given ACL instance.
 */
__STATIC_INLINE nrf_acl_permissions_t nrf_acl_access_control_perm_get(nrf_acl_instance_t instance);

#ifndef SUPPRESS_INLINE_IMPLEMENTATION

__STATIC_INLINE void nrf_acl_access_control_set(nrf_acl_instance_t instance,
                                                uint32_t address,
                                                size_t size,
                                                nrf_acl_permissions_t perm)
{
    NRFX_ASSERT(perm != NRF_ACL_PERMISSIONS_NO_PROTECTION);
    NRFX_ASSERT(instance < NRF_ACL_NUM_INSTANCES);
    NRFX_ASSERT(address != 0);
    NRFX_ASSERT(address % NRF_FICR->CODEPAGESIZE == 0);
    NRFX_ASSERT(size <= NRF_ACL_PERMISSIONS_SIZE_MAX_VAL);

    NRF_ACL->ACL[instance].ADDR = address;
    NRF_ACL->ACL[instance].SIZE = size;
    NRF_ACL->ACL[instance].PERM = perm;
}

__STATIC_INLINE uint32_t nrf_acl_access_control_address_get(nrf_acl_instance_t instance)
{
    return NRF_ACL->ACL[instance].ADDR;
}

__STATIC_INLINE size_t nrf_acl_access_control_size_get(nrf_acl_instance_t instance)
{
    return NRF_ACL->ACL[instance].SIZE;
}

__STATIC_INLINE nrf_acl_permissions_t nrf_acl_access_control_perm_get(nrf_acl_instance_t instance)
{
    return (nrf_acl_permissions_t)NRF_ACL->ACL[instance].PERM;
}

#endif // SUPPRESS_INLINE_IMPLEMENTATION

/** @} */

#ifdef __cplusplus
}
#endif

#endif // NRF_ACL_H__
