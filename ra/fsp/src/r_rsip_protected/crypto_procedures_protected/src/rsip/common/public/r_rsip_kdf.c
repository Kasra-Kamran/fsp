/*
* Copyright (c) 2020 - 2025 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "r_rsip_public.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

#define RSIP_PRV_BYTE_SIZE_KDF_HMAC_SHA256_MIN    (32U)
#define RSIP_PRV_BYTE_SIZE_KDF_HMAC_SHA256_MAX    (64U)
#define RSIP_PRV_BYTE_SIZE_KDF_HMAC_SHA384_MIN    (48U)
#define RSIP_PRV_BYTE_SIZE_KDF_HMAC_SHA384_MAX    (64U)
#define RSIP_PRV_BYTE_SIZE_KDF_HMAC_SHA512_MIN    (64U)
#define RSIP_PRV_BYTE_SIZE_KDF_HMAC_SHA512_MAX    (64U)

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/

#if RSIP_CFG_KDF_HMAC_ENABLE
static rsip_func_kdf_derived_key_import_t select_func_derived_key_import(uint8_t         kdf_mac_type,
                                                                         rsip_key_type_t key_type);
static rsip_ret_t kdf_hmac_update(rsip_ctrl_t * const p_ctrl,
                                  const uint8_t     * p_message,
                                  uint32_t            message_length);
static rsip_ret_t kdf_hmac_sign_finish(rsip_ctrl_t * const p_ctrl, uint8_t * p_mac);

static const uint8_t gs_convert_to_hmac_subtype[] =
{
    [RSIP_KEY_KDF_HMAC_SHA256] = RSIP_KEY_HMAC_SHA256,
    [RSIP_KEY_KDF_HMAC_SHA384] = RSIP_KEY_HMAC_SHA384,
    [RSIP_KEY_KDF_HMAC_SHA512] = RSIP_KEY_HMAC_SHA512,
};

static const uint8_t gs_convert_to_kdf_hmac_subtype[] =
{
    [RSIP_KEY_HMAC_SHA256] = RSIP_KEY_KDF_HMAC_SHA256,
    [RSIP_KEY_HMAC_SHA384] = RSIP_KEY_KDF_HMAC_SHA384,
    [RSIP_KEY_HMAC_SHA512] = RSIP_KEY_KDF_HMAC_SHA512,
};

static const uint8_t gs_kdf_hmac_byte_length_min[] =
{
    [RSIP_KEY_KDF_HMAC_SHA256] = RSIP_PRV_BYTE_SIZE_KDF_HMAC_SHA256_MIN,
    [RSIP_KEY_KDF_HMAC_SHA384] = RSIP_PRV_BYTE_SIZE_KDF_HMAC_SHA384_MIN,
    [RSIP_KEY_KDF_HMAC_SHA512] = RSIP_PRV_BYTE_SIZE_KDF_HMAC_SHA512_MIN,
};

static const uint8_t gs_kdf_hmac_byte_length_max[] =
{
    [RSIP_KEY_KDF_HMAC_SHA256] = RSIP_PRV_BYTE_SIZE_KDF_HMAC_SHA256_MAX,
    [RSIP_KEY_KDF_HMAC_SHA384] = RSIP_PRV_BYTE_SIZE_KDF_HMAC_SHA384_MAX,
    [RSIP_KEY_KDF_HMAC_SHA512] = RSIP_PRV_BYTE_SIZE_KDF_HMAC_SHA512_MAX,
};
#endif

#if RSIP_CFG_KDF_SHA_ENABLE
static const uint8_t gs_convert_to_kdf_sha_subtype[] =
{
    [RSIP_HASH_TYPE_SHA256] = RSIP_DKM_SUBTYPE_SHA_256,
};
#endif

/***********************************************************************************************************************
 * Global variables
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @addtogroup RSIP_PROTECTED
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Prepares a SHA generation.
 *
 * Implements @ref rsip_api_t::kdfshaInit.
 *
 * @par Conditions
 * Argument hash_type must be one of the following:
 *   - @ref RSIP_HASH_TYPE_SHA256
 *
 * @par State transition
 * @parblock
 * This API can only be executed in **STATE_MAIN**, and causes state transition.
 *
 * |Return value|Next state   |
 * |------------|-------------|
 * |FSP_SUCCESS |STATE_KDF_SHA|
 * |Others      |No change    |
 * @endparblock
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_NOT_ENABLED                   Input key type is disabled in this function by configuration.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_SHA_Init (rsip_ctrl_t * const p_ctrl, rsip_hash_type_t const hash_type)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_SHA_ENABLE
    rsip_instance_ctrl_t * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);

    /* Check configuration */
    FSP_ERROR_RETURN(RSIP_HASH_TYPE_SHA256 == hash_type, FSP_ERR_NOT_ENABLED);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_MAIN == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);

    rsip_kdf_sha_handle_t * p_handle = &p_instance_ctrl->handle.kdf_sha;

    /* Reset handle */
    memset(p_handle->buffer1, 0, RSIP_CFG_BYTE_SIZE_KDF_SHA_PLAIN_MSG1_MAX);
    memset(p_handle->buffer2, 0, RSIP_CFG_BYTE_SIZE_KDF_SHA_PLAIN_MSG2_MAX);
    p_handle->buffered_length1          = 0;
    p_handle->buffered_length2          = 0;
    p_handle->total_length              = 0;
    p_handle->wrapped_msg_length        = 0;
    p_handle->actual_wrapped_msg_length = 0;

    /* Set hash type */
    p_handle->type = hash_type;

    /* Set block size */
    switch (hash_type)
    {
        /* SHA-256 */
        case RSIP_HASH_TYPE_SHA256:
        {
            p_handle->block_size = RSIP_PRV_BYTE_SIZE_HASH_BLOCK_SHA1_SHA224_SHA256;
            break;
        }

        default:
        {
            /* Do nothing */
        }
    }

    /* State transition */
    p_instance_ctrl->state = RSIP_STATE_KDF_SHA;
    p_handle->handle_state = RSIP_USER_HANDLE_STATE_INIT;

    err = FSP_SUCCESS;
#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(hash_type);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Inputs wrapped ECDH secret as a message.
 * Input the messages required to perform the SHA operation using this API and R_RSIP_KDF_SHA_Update() in the desired order.
 *
 * Implements @ref rsip_api_t::kdfshaEcdhSecretUpdate.
 *
 * @par Conditions
 * The argument p_wrapped_secret must be input the result of R_RSIP_ECDH_KeyAgree() or R_RSIP_ECDH_PlainKeyAgree().
 * This API cannot be called multiple times in a process of performing SHA operation.
 *
 * @par State transition
 * This API can only be executed in **STATE_KDF_SHA**, and does not cause any state transitions.
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_NOT_ENABLED                   Input key type is disabled in this function by configuration.
 * @retval FSP_ERR_CRYPTO_RSIP_FAIL              Input parameter is invalid.
 *
 * @retval FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT A resource conflict occurred because a hardware resource required
 *                                               by the processing is in use by other processing.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_SHA_ECDHSecretUpdate (rsip_ctrl_t * const                 p_ctrl,
                                           rsip_wrapped_secret_t const * const p_wrapped_secret)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_SHA_ENABLE
    rsip_instance_ctrl_t  * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;
    rsip_kdf_sha_handle_t * p_handle        = &p_instance_ctrl->handle.kdf_sha;

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_wrapped_secret);
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
 #endif

    /* Set function */
    uint8_t subtype = gs_convert_to_kdf_sha_subtype[p_handle->type];
    FSP_ERROR_RETURN(subtype < RSIP_DKM_SUBTYPE_SHA_NUM, FSP_ERR_CRYPTO_RSIP_FATAL);
    rsip_func_kdf_ecdh_secret_key_import_t p_func = gp_func_kdf_sha_ecdh_secret_msg_wrap[subtype];

 #if RSIP_CFG_PARAM_CHECKING_ENABLE

    /* Check configuration */
    FSP_ERROR_RETURN(p_func, FSP_ERR_NOT_ENABLED);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_KDF_SHA == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);
    FSP_ERROR_RETURN(0 == p_handle->wrapped_msg_length, FSP_ERR_INVALID_STATE);

    /* Call function (cast to match the argument type with the primitive function) */
    rsip_ret_t rsip_ret = p_func((const uint32_t *) (p_wrapped_secret->value), (uint32_t *) (p_handle->wrapped_msg));

    /* Check error */
    switch (rsip_ret)
    {
        case RSIP_RET_PASS:
        {
            /* Set length */
            switch (p_wrapped_secret->type)
            {
                /* ECC secp256r1 */
                case RSIP_KEY_ECC_SECP256R1:
                {
                    p_handle->wrapped_msg_length        = RSIP_CFG_BYTE_SIZE_ECDH_WRAPPED_SECRET_256;
                    p_handle->actual_wrapped_msg_length = RSIP_PRV_BYTE_SIZE_ECC_256_PARAM;
                    p_handle->total_length             += p_handle->actual_wrapped_msg_length;

                    /* State transition */
                    p_handle->handle_state = RSIP_USER_HANDLE_STATE_UPDATE;

                    err = FSP_SUCCESS;
                    break;
                }

                default:
                {
                    /* Do nothing */
                }
            }

            break;
        }

        case RSIP_RET_RESOURCE_CONFLICT:
        {
            err = FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT;
            break;
        }

        case RSIP_RET_FAIL:
        {
            err = FSP_ERR_CRYPTO_RSIP_FAIL;
            break;
        }

        default:
        {
            err = FSP_ERR_CRYPTO_RSIP_FATAL;
        }
    }

#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_wrapped_secret);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Inputs message.
 * Input the messages required to perform the SHA operation using this API and R_RSIP_KDF_SHA_ECDHSecretUpdate() in the desired order.
 *
 * The total message length that can be input is up to 64 bytes before and after calling R_RSIP_KDF_SHA_ECDHSecretUpdate().
 *
 * Implements @ref rsip_api_t::kdfshaUpdate.
 *
 * @par State transition
 * This API can only be executed in **STATE_KDF_SHA**, and does not cause any state transitions.
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_INVALID_SIZE                  Input message_length is illegal.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_SHA_Update (rsip_ctrl_t * const   p_ctrl,
                                 uint8_t const * const p_message,
                                 uint32_t const        message_length)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_SHA_ENABLE
    rsip_instance_ctrl_t * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_message || (0 == message_length));
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_KDF_SHA == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);

    rsip_kdf_sha_handle_t * p_handle = &p_instance_ctrl->handle.kdf_sha;

    if (0 == message_length)
    {
        err = FSP_SUCCESS;
    }
    else
    {
        /* Check if the Wrapped Message exists in the buffer */
        if (0 == p_handle->wrapped_msg_length)
        {
            if (p_handle->buffered_length1 + message_length <= RSIP_CFG_BYTE_SIZE_KDF_SHA_PLAIN_MSG1_MAX)
            {
                /* Copy head of new message to buffer */
                memcpy(p_handle->buffer1 + p_handle->buffered_length1, p_message, message_length);
                p_handle->buffered_length1 += message_length;
                p_handle->total_length     += message_length;

                /* State transition */
                p_handle->handle_state = RSIP_USER_HANDLE_STATE_UPDATE;

                err = FSP_SUCCESS;
            }
            else
            {
                err = FSP_ERR_INVALID_SIZE;
            }
        }
        else
        {
            if (p_handle->buffered_length2 + message_length <= RSIP_CFG_BYTE_SIZE_KDF_SHA_PLAIN_MSG2_MAX)
            {
                /* Copy head of new message to buffer */
                memcpy(p_handle->buffer2 + p_handle->buffered_length2, p_message, message_length);
                p_handle->buffered_length2 += message_length;
                p_handle->total_length     += message_length;

                /* State transition */
                p_handle->handle_state = RSIP_USER_HANDLE_STATE_UPDATE;

                err = FSP_SUCCESS;
            }
            else
            {
                err = FSP_ERR_INVALID_SIZE;
            }
        }
    }

#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_message);
    FSP_PARAMETER_NOT_USED(message_length);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Finalizes a SHA operation and generate DKM (Derived Keying Material).
 *
 * Implements @ref rsip_api_t::kdfshaFinish.
 *
 * @par State transition
 * @parblock
 * This API can only be executed in **STATE_KDF_SHA**, and causes state transition.
 *
 * |Return value         |Next state|
 * |---------------------|----------|
 * |FSP_SUCCESS          |STATE_MAIN|
 * |FSP_ERR_ASSERTION    |No change |
 * |FSP_ERR_NOT_OPEN     |No change |
 * |FSP_ERR_INVALID_STATE|No change |
 * |Others               |STATE_MAIN|
 * @endparblock
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_CRYPTO_RSIP_FAIL              Input parameter is invalid.
 * @retval FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT A resource conflict occurred because a hardware resource required
 *                                               by the processing is in use by other processing.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_SHA_Finish (rsip_ctrl_t * const p_ctrl, rsip_wrapped_dkm_t * const p_wrapped_dkm)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_SHA_ENABLE
    rsip_instance_ctrl_t  * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;
    rsip_kdf_sha_handle_t * p_handle        = &p_instance_ctrl->handle.kdf_sha;

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_wrapped_dkm);
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_KDF_SHA == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);

    /* Call function */
    rsip_ret_t rsip_ret = r_rsip_kdf_sha_final(p_handle, p_wrapped_dkm->value);

    /* Check error */
    switch (rsip_ret)
    {
        case RSIP_RET_PASS:
        {
            /* State transition */
            p_instance_ctrl->state = RSIP_STATE_MAIN;

            p_wrapped_dkm->alg    = RSIP_DKM_ALG_SHA;
            p_wrapped_dkm->blocks = 1;
            switch (p_handle->type)
            {
                /* SHA-256 */
                case RSIP_HASH_TYPE_SHA256:
                {
                    p_wrapped_dkm->subtype      = RSIP_DKM_SUBTYPE_SHA_256;
                    p_wrapped_dkm->block_length = RSIP_CFG_BYTE_SIZE_KDF_WRAPPED_MAC_SHA256;

                    err = FSP_SUCCESS;
                    break;
                }

                default:
                {
                    /* Do nothing */
                }
            }

            break;
        }

        case RSIP_RET_RESOURCE_CONFLICT:
        {
            err = FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT;
            break;
        }

        case RSIP_RET_FAIL:
        {
            err = FSP_ERR_CRYPTO_RSIP_FAIL;
            break;
        }

        default:
        {
            err = FSP_ERR_CRYPTO_RSIP_FATAL;
        }
    }

#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_wrapped_dkm);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Suspends SHA operation.
 *
 * This API releases RSIP resource and outputs intermediate results. Therefore, it can be used in the following cases:
 * - Execute another cryptographic operations during inputting successive chunks of the message.
 * - Reuse intermediate results.
 *
 * Implements @ref rsip_api_t::kdfshaSuspend.
 *
 * @par State transition
 * @parblock
 * This API can only be executed in **STATE_KDF_SHA**, and causes state transition.
 *
 * |Return value|Next state|
 * |------------|----------|
 * |FSP_SUCCESS |STATE_MAIN|
 * |Others      |No change |
 * @endparblock
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_SHA_Suspend (rsip_ctrl_t * const p_ctrl, rsip_kdf_sha_handle_t * const p_handle)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_SHA_ENABLE
    rsip_instance_ctrl_t * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_handle);
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_KDF_SHA == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);

    /* Copy handle */
    *p_handle = p_instance_ctrl->handle.kdf_sha;

    /* Handle state transition */
    p_instance_ctrl->handle.kdf_sha.handle_state = RSIP_USER_HANDLE_STATE_RESUME;

    /* State transition */
    p_instance_ctrl->state = RSIP_STATE_MAIN;

    err = FSP_SUCCESS;
#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_handle);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Resumes SHA operation suspended by R_RSIP_KDF_SHA_Suspend().
 *
 * Implements @ref rsip_api_t::kdfshaResume.
 *
 * @par State transition
 * @parblock
 * This API can only be executed in **STATE_MAIN**, and causes state transition.
 *
 * |Return value|Next state   |
 * |------------|-------------|
 * |FSP_SUCCESS |STATE_KDF_SHA|
 * |Others      |No change    |
 * @endparblock
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_SHA_Resume (rsip_ctrl_t * const p_ctrl, rsip_kdf_sha_handle_t const * const p_handle)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_SHA_ENABLE
    rsip_instance_ctrl_t * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_handle);
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_MAIN == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);

    /* Copy handle */
    p_instance_ctrl->handle.kdf_sha = *p_handle;

    /* State transition */
    p_instance_ctrl->state = RSIP_STATE_KDF_SHA;

    err = FSP_SUCCESS;
#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_handle);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Concatenates two wrapped DKMs.
 *
 * DKM1 || DKM2 is output to p_wrapped_dkm1.
 *
 * Implements @ref rsip_api_t::kdfshaDkmConcatenate.
 *
 * @par State transition
 * This API can be executed in **any state** including STATE_INITIAL, and does not cause any state transitions.
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_CRYPTO_RSIP_FAIL              Input parameter is invalid.
 * @retval FSP_ERR_INVALID_SIZE                  Any length is illegal.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_SHA_DKMConcatenate (rsip_wrapped_dkm_t * const       p_wrapped_dkm1,
                                         rsip_wrapped_dkm_t const * const p_wrapped_dkm2,
                                         uint32_t const                   wrapped_dkm1_buffer_length)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_SHA_ENABLE
 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_wrapped_dkm1);
    FSP_ASSERT(p_wrapped_dkm2);

    /* Check dkm type */
    FSP_ERROR_RETURN(p_wrapped_dkm1->alg == p_wrapped_dkm2->alg, FSP_ERR_CRYPTO_RSIP_FAIL);
    FSP_ERROR_RETURN(p_wrapped_dkm1->subtype == p_wrapped_dkm2->subtype, FSP_ERR_CRYPTO_RSIP_FAIL);
 #endif

    uint32_t len1 = p_wrapped_dkm1->block_length * p_wrapped_dkm1->blocks;
    uint32_t len2 = p_wrapped_dkm2->block_length * p_wrapped_dkm2->blocks;

    /* Check length */
    FSP_ERROR_RETURN(wrapped_dkm1_buffer_length >= (sizeof(rsip_wrapped_dkm_t) + len1 + len2), FSP_ERR_INVALID_SIZE);

    /* Copy DKM */
    memcpy(p_wrapped_dkm1->value + len1, p_wrapped_dkm2->value, len2);
    p_wrapped_dkm1->blocks += p_wrapped_dkm2->blocks;

    err = FSP_SUCCESS;
#else
    FSP_PARAMETER_NOT_USED(p_wrapped_dkm1);
    FSP_PARAMETER_NOT_USED(p_wrapped_dkm2);
    FSP_PARAMETER_NOT_USED(wrapped_dkm1_buffer_length);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Outputs a wrapped key from DKM information.
 *
 * Implements @ref rsip_api_t::kdfshaDerivedKeyImport.
 *
 * @par Conditions
 * @parblock
 * Argument p_wrapped_dkm must be input the result of R_RSIP_KDF_SHA_Finish() or R_RSIP_KDF_SHA_DKMConcatenate().
 * Argument key_type must be one of the following:
 *  - @ref RSIP_KEY_TYPE_AES_128
 *  - @ref RSIP_KEY_TYPE_AES_256
 * @endparblock
 *
 * @par State transition
 * This API can only be executed in **STATE_MAIN**, and does not cause any state transitions.
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_NOT_ENABLED                   Input key type is disabled in this function by configuration.
 * @retval FSP_ERR_CRYPTO_RSIP_FAIL              Input parameter is invalid.
 *
 * @retval FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT A resource conflict occurred because a hardware resource required
 *                                               by the processing is in use by other processing.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_SHA_DerivedKeyImport (rsip_ctrl_t * const              p_ctrl,
                                           rsip_wrapped_dkm_t const * const p_wrapped_dkm,
                                           rsip_key_type_t const            key_type,
                                           uint32_t const                   position,
                                           rsip_wrapped_key_t * const       p_wrapped_key)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_SHA_ENABLE
    rsip_instance_ctrl_t * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_wrapped_key);
    FSP_ASSERT(p_wrapped_dkm);
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
 #endif

    /* Set function */
    uint8_t subtype = r_rsip_key_type_to_subtype(key_type);
    rsip_func_kdf_derived_key_import_t p_func = gp_func_kdf_sha_derived_key_import_aes[p_wrapped_dkm->subtype][subtype];

 #if RSIP_CFG_PARAM_CHECKING_ENABLE

    /* Check configuration */
    FSP_ERROR_RETURN(p_func, FSP_ERR_NOT_ENABLED);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_MAIN == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);

    uint32_t blocks[1] = {bswap_32big(p_wrapped_dkm->blocks)};
    uint32_t pos[1]    = {bswap_32big(position)};

    /* Call function (cast to match the argument type with the primitive function) */
    rsip_ret_t rsip_ret =
        p_func((const uint32_t *) p_wrapped_dkm->value, blocks, pos, (uint32_t *) p_wrapped_key->value);

    /* Check error */
    switch (rsip_ret)
    {
        case RSIP_RET_PASS:
        {
            p_wrapped_key->alg     = r_rsip_key_type_to_alg(key_type);
            p_wrapped_key->subtype = r_rsip_key_type_to_subtype(key_type);

            err = FSP_SUCCESS;
            break;
        }

        case RSIP_RET_RESOURCE_CONFLICT:
        {
            err = FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT;
            break;
        }

        case RSIP_RET_FAIL:
        {
            err = FSP_ERR_CRYPTO_RSIP_FAIL;
            break;
        }

        default:
        {
            err = FSP_ERR_CRYPTO_RSIP_FATAL;
        }
    }

#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_wrapped_dkm);
    FSP_PARAMETER_NOT_USED(key_type);
    FSP_PARAMETER_NOT_USED(position);
    FSP_PARAMETER_NOT_USED(p_wrapped_key);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Outputs a initial vector from DKM information.
 *
 * Implements @ref rsip_api_t::kdfshaDerivedIvWrap.
 *
 * @par Conditions
 * @parblock
 * Argument p_wrapped_dkm must be input the result of R_RSIP_KDF_SHA_Finish() or R_RSIP_KDF_SHA_DKMConcatenate().
 * Argument initial_vector_type must be one of the following:
 *  - @ref RSIP_INITIAL_VECTOR_TYPE_AES_16_BYTE
 * @endparblock
 *
 * @par State transition
 * This API can only be executed in **STATE_MAIN**, and does not cause any state transitions.
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_NOT_ENABLED                   Input key type is disabled in this function by configuration.
 * @retval FSP_ERR_CRYPTO_RSIP_FAIL              Input parameter is invalid.
 *
 * @retval FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT A resource conflict occurred because a hardware resource required
 *                                               by the processing is in use by other processing.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_SHA_DerivedIVWrap (rsip_ctrl_t * const              p_ctrl,
                                        rsip_wrapped_dkm_t const * const p_wrapped_dkm,
                                        rsip_initial_vector_type_t const initial_vector_type,
                                        uint32_t const                   position,
                                        uint8_t const * const            p_tls_sequence_num,
                                        uint8_t * const                  p_wrapped_initial_vector)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_SHA_ENABLE
    rsip_instance_ctrl_t * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_tls_sequence_num || (RSIP_INITIAL_VECTOR_TYPE_AES_16_BYTE == initial_vector_type) ||
               (RSIP_INITIAL_VECTOR_TYPE_AES_TLS12_CBC == initial_vector_type));
    FSP_ASSERT(p_wrapped_initial_vector);
    FSP_ASSERT(p_wrapped_dkm);
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);

    /* Check dkm type */
    FSP_ERROR_RETURN(RSIP_DKM_ALG_SHA == p_wrapped_dkm->alg, FSP_ERR_CRYPTO_RSIP_FAIL);
 #endif

    /* Set function */
    rsip_func_kdf_derived_iv_wrap_t p_func =
        gp_func_kdf_sha_derived_iv_wrap[p_wrapped_dkm->subtype][initial_vector_type];

 #if RSIP_CFG_PARAM_CHECKING_ENABLE

    /* Check configuration */
    FSP_ERROR_RETURN(p_func, FSP_ERR_NOT_ENABLED);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_MAIN == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);

    uint32_t blocks[1] = {bswap_32big(p_wrapped_dkm->blocks)};
    uint32_t pos[1]    = {bswap_32big(position)};

    /* Call function (cast to match the argument type with the primitive function) */
    rsip_ret_t rsip_ret =
        p_func((const uint32_t *) p_wrapped_dkm->value,
               blocks,
               pos,
               (const uint32_t *) p_tls_sequence_num,
               (uint32_t *) p_wrapped_initial_vector);

    /* Check error */
    switch (rsip_ret)
    {
        case RSIP_RET_PASS:
        {
            err = FSP_SUCCESS;
            break;
        }

        case RSIP_RET_RESOURCE_CONFLICT:
        {
            err = FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT;
            break;
        }

        case RSIP_RET_FAIL:
        {
            err = FSP_ERR_CRYPTO_RSIP_FAIL;
            break;
        }

        default:
        {
            err = FSP_ERR_CRYPTO_RSIP_FATAL;
        }
    }

#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_wrapped_dkm);
    FSP_PARAMETER_NOT_USED(initial_vector_type);
    FSP_PARAMETER_NOT_USED(position);
    FSP_PARAMETER_NOT_USED(p_tls_sequence_num);
    FSP_PARAMETER_NOT_USED(p_wrapped_initial_vector);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Converts wrapped data to wrapped HMAC key for KDF.
 *
 * Implements @ref rsip_api_t::kdfMacKeyImport.
 *
 * @par Conditions
 * @parblock
 * Argument key_type must be one of the following:
 *  - @ref RSIP_KEY_TYPE_HMAC_SHA256
 *  - @ref RSIP_KEY_TYPE_HMAC_SHA384
 *  - @ref RSIP_KEY_TYPE_HMAC_SHA512
 *
 * The argument p_wrapped_mac must be input the result of R_RSIP_KDF_HMAC_SignFinish().
 * @endparblock
 *
 * Argument kdf_data_length depends on key type.
 * - 32 to 64 ( @ref RSIP_KEY_TYPE_HMAC_SHA256)
 * - 48 to 64 ( @ref RSIP_KEY_TYPE_HMAC_SHA384)
 * - 64       ( @ref RSIP_KEY_TYPE_HMAC_SHA512)
 *
 * @par State transition
 * This API can only be executed in **STATE_MAIN**, and does not cause any state transitions.
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_NOT_ENABLED                   Input key type is disabled in this function by configuration.
 * @retval FSP_ERR_CRYPTO_RSIP_FAIL              Input parameter is invalid.
 *
 * @retval FSP_ERR_INVALID_SIZE                  Any length is illegal.
 *
 * @retval FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT A resource conflict occurred because a hardware resource required
 *                                               by the processing is in use by other processing.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_MACKeyImport (rsip_ctrl_t * const              p_ctrl,
                                   rsip_key_type_t const            key_type,
                                   rsip_wrapped_mac_t const * const p_wrapped_mac,
                                   uint32_t const                   key_length,
                                   rsip_wrapped_key_t * const       p_wrapped_key)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_HMAC_ENABLE
    rsip_instance_ctrl_t * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;

    uint8_t subtype = gs_convert_to_kdf_hmac_subtype[r_rsip_key_type_to_subtype(key_type)];
    FSP_ERROR_RETURN(subtype < RSIP_KEY_KDF_HMAC_NUM, FSP_ERR_CRYPTO_RSIP_FAIL);

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_wrapped_mac);
    FSP_ASSERT(p_wrapped_key);
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);

    /* Check key type */
    uint8_t alg = r_rsip_key_type_to_alg(key_type);
    FSP_ERROR_RETURN(RSIP_ALG_HMAC == alg, FSP_ERR_CRYPTO_RSIP_FAIL);

    /* Check configuration */
    FSP_ERROR_RETURN(gp_func_kdf_mac_key_import[subtype], FSP_ERR_NOT_ENABLED);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_MAIN == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);

    /* Check length */
    FSP_ERROR_RETURN(gs_kdf_hmac_byte_length_min[subtype] <= key_length, FSP_ERR_INVALID_SIZE);
    FSP_ERROR_RETURN(gs_kdf_hmac_byte_length_max[subtype] >= key_length, FSP_ERR_INVALID_SIZE);

    uint32_t blocks[1] = {bswap_32big((gs_kdf_hmac_byte_length_min[subtype] == key_length) ? 1 : 2)};
    uint32_t len[1]    = {bswap_32big(key_length)};

    /* Call function */
    rsip_ret_t rsip_ret = gp_func_kdf_mac_key_import[subtype]((const uint32_t *) p_wrapped_mac->value,
                                                              (const uint32_t *) blocks,
                                                              (const uint32_t *) len,
                                                              (uint32_t *) p_wrapped_key->value);

    /* Check error */
    switch (rsip_ret)
    {
        case RSIP_RET_PASS:
        {
            p_wrapped_key->alg     = RSIP_ALG_KDF_HMAC;
            p_wrapped_key->subtype = subtype;

            err = FSP_SUCCESS;
            break;
        }

        case RSIP_RET_RESOURCE_CONFLICT:
        {
            err = FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT;
            break;
        }

        case RSIP_RET_FAIL:
        {
            err = FSP_ERR_CRYPTO_RSIP_FAIL;
            break;
        }

        default:
        {
            err = FSP_ERR_CRYPTO_RSIP_FATAL;
        }
    }

#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(key_type);
    FSP_PARAMETER_NOT_USED(p_wrapped_mac);
    FSP_PARAMETER_NOT_USED(key_length);
    FSP_PARAMETER_NOT_USED(p_wrapped_key);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Converts wrapped ECDH secret to wrapped HMAC key for KDF.
 *
 * Implements @ref rsip_api_t::kdfEcdhSecretKeyImport.
 *
 * @par Conditions
 * @parblock
 * Argument key_type must be one of the following:
 *  - @ref RSIP_KEY_TYPE_HMAC_SHA256
 *  - @ref RSIP_KEY_TYPE_HMAC_SHA384
 *  - @ref RSIP_KEY_TYPE_HMAC_SHA512
 * @endparblock
 *
 * @par State transition
 * This API can only be executed in **STATE_MAIN**, and does not cause any state transitions.
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_NOT_ENABLED                   Input key type is disabled in this function by configuration.
 * @retval FSP_ERR_CRYPTO_RSIP_FAIL              Input parameter is invalid.
 *
 * @retval FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT A resource conflict occurred because a hardware resource required
 *                                               by the processing is in use by other processing.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_ECDHSecretKeyImport (rsip_ctrl_t * const                 p_ctrl,
                                          rsip_key_type_t const               key_type,
                                          rsip_wrapped_secret_t const * const p_wrapped_secret,
                                          rsip_wrapped_key_t * const          p_wrapped_key)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_HMAC_ENABLE
    rsip_instance_ctrl_t * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;

    uint8_t subtype = gs_convert_to_kdf_hmac_subtype[r_rsip_key_type_to_subtype(key_type)];
    FSP_ERROR_RETURN(subtype < RSIP_KEY_KDF_HMAC_NUM, FSP_ERR_CRYPTO_RSIP_FAIL);

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_wrapped_secret);
    FSP_ASSERT(p_wrapped_key);
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);

    /* Check key type */
    uint8_t alg = r_rsip_key_type_to_alg(key_type);
    FSP_ERROR_RETURN(RSIP_ALG_HMAC == alg, FSP_ERR_CRYPTO_RSIP_FAIL);

    /* Check configuration */
    FSP_ERROR_RETURN(gp_func_kdf_ecdh_secret_key_import[subtype], FSP_ERR_NOT_ENABLED);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_MAIN == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);

    /* Call function */
    rsip_ret_t rsip_ret =
        gp_func_kdf_ecdh_secret_key_import[subtype]((const uint32_t *) p_wrapped_secret->value,
                                                    (uint32_t *) p_wrapped_key->value);

    /* Check error */
    switch (rsip_ret)
    {
        case RSIP_RET_PASS:
        {
            p_wrapped_key->alg     = RSIP_ALG_KDF_HMAC;
            p_wrapped_key->subtype = subtype;

            err = FSP_SUCCESS;
            break;
        }

        case RSIP_RET_RESOURCE_CONFLICT:
        {
            err = FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT;
            break;
        }

        case RSIP_RET_FAIL:
        {
            err = FSP_ERR_CRYPTO_RSIP_FAIL;
            break;
        }

        default:
        {
            err = FSP_ERR_CRYPTO_RSIP_FATAL;
        }
    }

#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(key_type);
    FSP_PARAMETER_NOT_USED(p_wrapped_secret);
    FSP_PARAMETER_NOT_USED(p_wrapped_key);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Prepares a HMAC generation.
 *
 * Implements @ref rsip_api_t::kdfhmacInit.
 *
 * @par Conditions
 * Argument p_wrapped_key must be one of the following:
 *  - Output data p_wrapped_key from R_RSIP_KDF_ECDHSecretKeyImport().
 *  - Key type of p_wrapped_key is RSIP_KEY_TYPE_HMAC_xxx.
 *
 * @par State transition
 * @parblock
 * This API can only be executed in **STATE_MAIN**, and causes state transition.
 *
 * |Return value|Next state    |
 * |------------|--------------|
 * |FSP_SUCCESS |STATE_KDF_HMAC|
 * |Others      |No change     |
 * @endparblock
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_NOT_ENABLED                   Input key type is disabled in this function by configuration.
 * @retval FSP_ERR_CRYPTO_RSIP_FAIL              Input parameter is invalid.
 * @retval FSP_ERR_CRYPTO_RSIP_KEY_SET_FAIL      Input key value is illegal.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_HMAC_Init (rsip_ctrl_t * const p_ctrl, rsip_wrapped_key_t const * const p_wrapped_key)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_HMAC_ENABLE
    rsip_instance_ctrl_t * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_wrapped_key);
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);

    /* Check key type */
    FSP_ERROR_RETURN((RSIP_ALG_KDF_HMAC == p_wrapped_key->alg) || (RSIP_ALG_HMAC == p_wrapped_key->alg),
                     FSP_ERR_CRYPTO_RSIP_KEY_SET_FAIL);
 #endif

    /* Get subtype in RSIP_KEY_HMAC_SHAxxx format */
    uint8_t subtype = p_wrapped_key->subtype;
    if (RSIP_ALG_KDF_HMAC == p_wrapped_key->alg)
    {
        subtype = gs_convert_to_hmac_subtype[subtype];
        FSP_ERROR_RETURN(subtype < RSIP_KEY_HMAC_NUM, FSP_ERR_CRYPTO_RSIP_FAIL);
    }

 #if RSIP_CFG_PARAM_CHECKING_ENABLE

    /* Check configuration */
    FSP_ERROR_RETURN(g_hmac_enabled[subtype], FSP_ERR_NOT_ENABLED);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_MAIN == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);

    rsip_kdf_hmac_handle_t * p_handle = &p_instance_ctrl->handle.kdf_hmac;

    /* Reset handle */
    p_handle->buffered_length           = 0;
    p_handle->total_length              = 0;
    p_handle->wrapped_msg_length        = 0;
    p_handle->actual_wrapped_msg_length = 0;

    /* Copy wrapped key */
    memcpy(p_handle->wrapped_key, p_wrapped_key, RSIP_CFG_BYTE_SIZE_WRAPPED_KEY_KDF_HMAC_MAX);

    /* Set block size */
    switch (subtype)
    {
        /* SHA-256 */
        case RSIP_KEY_HMAC_SHA256:
        {
            p_handle->block_size = RSIP_PRV_BYTE_SIZE_HASH_BLOCK_SHA1_SHA224_SHA256;
            break;
        }

        /* SHA-384, SHA-512 */
        default:
        {
            p_handle->block_size = RSIP_PRV_BYTE_SIZE_HASH_BLOCK_SHA384_SHA512;
            break;
        }
    }

    /* State transition */
    p_instance_ctrl->state = RSIP_STATE_KDF_HMAC;
    p_handle->handle_state = RSIP_USER_HANDLE_STATE_INIT;

    err = FSP_SUCCESS;
#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_wrapped_key);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Inputs wrapped MAC as a message.
 *
 * Implements @ref rsip_api_t::kdfhmacMacUpdate.
 *
 * @par Conditions
 * The argument p_wrapped_mac must be input the result of R_RSIP_KDF_HMAC_SignFinish().
 *
 * @par State transition
 * This API can only be executed in **STATE_KDF_HMAC**, and does not cause any state transitions.
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_HMAC_MACUpdate (rsip_ctrl_t * const p_ctrl, rsip_wrapped_mac_t const * const p_wrapped_mac)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_HMAC_ENABLE
    rsip_instance_ctrl_t   * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;
    rsip_kdf_hmac_handle_t * p_handle        = &p_instance_ctrl->handle.kdf_hmac;

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_wrapped_mac);
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_KDF_HMAC == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);
    FSP_ERROR_RETURN(0 == p_handle->buffered_length, FSP_ERR_INVALID_STATE);
    FSP_ERROR_RETURN(0 == p_handle->total_length, FSP_ERR_INVALID_STATE);
    FSP_ERROR_RETURN(0 == p_handle->wrapped_msg_length, FSP_ERR_INVALID_STATE);

    /* Copy wrapped MAC */
    memcpy(p_handle->wrapped_msg, p_wrapped_mac->value, RSIP_CFG_BYTE_SIZE_KDF_WRAPPED_MSG_MAX);

    /* Set length */
    switch (p_wrapped_mac->subtype)
    {
        /* SHA-256 */
        case RSIP_KEY_KDF_HMAC_SHA256:
        {
            p_handle->wrapped_msg_length        = RSIP_CFG_BYTE_SIZE_KDF_WRAPPED_MAC_SHA256;
            p_handle->actual_wrapped_msg_length = RSIP_PRV_BYTE_SIZE_DIGEST_SHA256;
            break;
        }

        /* SHA-384 */
        case RSIP_KEY_KDF_HMAC_SHA384:
        {
            p_handle->wrapped_msg_length        = RSIP_CFG_BYTE_SIZE_KDF_WRAPPED_MAC_SHA384;
            p_handle->actual_wrapped_msg_length = RSIP_PRV_BYTE_SIZE_DIGEST_SHA384;
            break;
        }

        /* SHA-512 */
        default:
        {
            p_handle->wrapped_msg_length        = RSIP_CFG_BYTE_SIZE_KDF_WRAPPED_MAC_SHA512;
            p_handle->actual_wrapped_msg_length = RSIP_PRV_BYTE_SIZE_DIGEST_SHA512;
            break;
        }
    }

    err = FSP_SUCCESS;
#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_wrapped_mac);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Inputs wrapped ECDH secret as a message.
 *
 * Implements @ref rsip_api_t::kdfhmacEcdhSecretUpdate.
 *
 * @par Conditions
 * The argument p_wrapped_secret must be input the result of R_RSIP_ECDH_KeyAgree() or R_RSIP_ECDH_PlainKeyAgree().
 *
 * @par State transition
 * This API can only be executed in **STATE_KDF_HMAC**, and does not cause any state transitions.
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_NOT_ENABLED                   Input key type is disabled in this function by configuration.
 * @retval FSP_ERR_CRYPTO_RSIP_FAIL              Input parameter is invalid.
 *
 * @retval FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT A resource conflict occurred because a hardware resource required
 *                                               by the processing is in use by other processing.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_HMAC_ECDHSecretUpdate (rsip_ctrl_t * const                 p_ctrl,
                                            rsip_wrapped_secret_t const * const p_wrapped_secret)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_HMAC_ENABLE
    rsip_instance_ctrl_t   * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;
    rsip_kdf_hmac_handle_t * p_handle        = &p_instance_ctrl->handle.kdf_hmac;

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_wrapped_secret);
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
 #endif

    /* Get subtype in RSIP_KEY_KDF_HMAC_SHAxxx format */
    rsip_wrapped_key_t * p_wrapped_key = (rsip_wrapped_key_t *) p_handle->wrapped_key;
    uint8_t              subtype       = p_wrapped_key->subtype;
    if (RSIP_ALG_HMAC == p_wrapped_key->alg)
    {
        subtype = gs_convert_to_kdf_hmac_subtype[subtype];
        FSP_ERROR_RETURN(subtype < RSIP_KEY_KDF_HMAC_NUM, FSP_ERR_CRYPTO_RSIP_FATAL);
    }

    /* Set function */
    rsip_func_kdf_ecdh_secret_key_import_t p_func = gp_func_kdf_ecdh_secret_msg_wrap[subtype];

 #if RSIP_CFG_PARAM_CHECKING_ENABLE

    /* Check configuration */
    FSP_ERROR_RETURN(p_func, FSP_ERR_NOT_ENABLED);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_KDF_HMAC == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);
    FSP_ERROR_RETURN(0 == p_handle->buffered_length, FSP_ERR_INVALID_STATE);
    FSP_ERROR_RETURN(0 == p_handle->total_length, FSP_ERR_INVALID_STATE);
    FSP_ERROR_RETURN(0 == p_handle->wrapped_msg_length, FSP_ERR_INVALID_STATE);

    /* Call function (cast to match the argument type with the primitive function) */
    rsip_ret_t rsip_ret = p_func((const uint32_t *) (p_wrapped_secret->value), (uint32_t *) (p_handle->wrapped_msg));

    /* Check error */
    switch (rsip_ret)
    {
        case RSIP_RET_PASS:
        {
            /* Set length */
            switch (p_wrapped_secret->type)
            {
                /* ECC secp256r1 */
                case RSIP_KEY_ECC_SECP256R1:
                {
                    p_handle->wrapped_msg_length        = RSIP_CFG_BYTE_SIZE_ECDH_WRAPPED_SECRET_256;
                    p_handle->actual_wrapped_msg_length = RSIP_PRV_BYTE_SIZE_ECC_256_PARAM;
                    break;
                }

                /* ECC secp384r1 */
                case RSIP_KEY_ECC_SECP384R1:
                {
                    p_handle->wrapped_msg_length        = RSIP_CFG_BYTE_SIZE_ECDH_WRAPPED_SECRET_384;
                    p_handle->actual_wrapped_msg_length = RSIP_PRV_BYTE_SIZE_ECC_384_PARAM;
                    break;
                }

                /* ECC secp521r1 */
                default:
                {
                    p_handle->wrapped_msg_length        = RSIP_CFG_BYTE_SIZE_ECDH_WRAPPED_SECRET_521;
                    p_handle->actual_wrapped_msg_length = RSIP_PRV_BYTE_SIZE_ECC_521_PARAM;
                    break;
                }
            }

            err = FSP_SUCCESS;
            break;
        }

        case RSIP_RET_RESOURCE_CONFLICT:
        {
            err = FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT;
            break;
        }

        case RSIP_RET_FAIL:
        {
            err = FSP_ERR_CRYPTO_RSIP_FAIL;
            break;
        }

        default:
        {
            err = FSP_ERR_CRYPTO_RSIP_FATAL;
        }
    }

#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_wrapped_secret);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Inputs message.
 *
 * Implements @ref rsip_api_t::kdfhmacUpdate.
 *
 * @par State transition
 * This API can only be executed in **STATE_KDF_HMAC**, and does not cause any state transitions.
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_CRYPTO_RSIP_FAIL              Input parameter is invalid.
 * @retval FSP_ERR_CRYPTO_RSIP_KEY_SET_FAIL      Input key is illegal.
 * @retval FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT A resource conflict occurred because a hardware resource required
 *                                               by the processing is in use by other processing.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_HMAC_Update (rsip_ctrl_t * const   p_ctrl,
                                  uint8_t const * const p_message,
                                  uint32_t const        message_length)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_HMAC_ENABLE
    rsip_instance_ctrl_t * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_message || (0 == message_length));
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_KDF_HMAC == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);

    rsip_kdf_hmac_handle_t * p_handle = &p_instance_ctrl->handle.kdf_hmac;

    if (0 == message_length)
    {
        err = FSP_SUCCESS;
    }
    else
    {
        rsip_ret_t      rsip_ret      = RSIP_RET_PASS;
        const uint8_t * p_msg_pos     = p_message;
        uint32_t        processed_len = 0;

        /* (0) Remaining message in buffer and head of new input message (wrapped) */
        if ((0 != p_handle->actual_wrapped_msg_length) &&
            (p_handle->block_size < (p_handle->actual_wrapped_msg_length + p_handle->buffered_length + message_length)))
        {
            uint32_t len = p_handle->block_size - p_handle->buffered_length - p_handle->actual_wrapped_msg_length;

            /* Copy head of new message to buffer */
            memcpy(p_handle->buffer + p_handle->buffered_length, p_msg_pos, len);

            /* Call function */
            rsip_ret = kdf_hmac_update(p_ctrl, p_handle->buffer, p_handle->block_size);

            p_handle->wrapped_msg_length        = 0;
            p_handle->actual_wrapped_msg_length = 0;
            p_handle->buffered_length           = 0;
            p_handle->total_length             += p_handle->block_size;
            processed_len += len;
        }

        /* (1) Remaining message in buffer and head of new input message */
        if ((0 != p_handle->buffered_length) &&
            (p_handle->block_size < (p_handle->buffered_length + message_length)))
        {
            uint32_t len = p_handle->block_size - p_handle->buffered_length;

            /* Copy head of new message to buffer */
            memcpy(p_handle->buffer + p_handle->buffered_length, p_msg_pos, len);

            /* Call function */
            rsip_ret = kdf_hmac_update(p_ctrl, p_handle->buffer, p_handle->block_size);

            p_handle->buffered_length = 0;
            p_handle->total_length   += p_handle->block_size;
            processed_len            += len;
        }

        /* (2) New input message except last block */
        if ((p_handle->block_size < message_length - processed_len) && (RSIP_RET_PASS == rsip_ret))
        {
            uint32_t len = ((message_length - processed_len - 1) / p_handle->block_size) *
                           p_handle->block_size;

            /* Call function */
            rsip_ret = kdf_hmac_update(p_ctrl, p_message + processed_len, len);

            p_handle->total_length += len;
            processed_len          += len;
        }

        /* Check error */
        switch (rsip_ret)
        {
            case RSIP_RET_PASS:
            {
                /* (3) Last block */
                memcpy(p_handle->buffer + p_handle->buffered_length,
                       p_message + processed_len,
                       message_length - processed_len);
                p_handle->buffered_length += message_length - processed_len;

                err = FSP_SUCCESS;
                break;
            }

            case RSIP_RET_RESOURCE_CONFLICT:
            {
                err = FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT;
                break;
            }

            case RSIP_RET_FAIL:
            {
                err = FSP_ERR_CRYPTO_RSIP_FAIL;
                break;
            }

            case RSIP_RET_KEY_FAIL:
            {
                err = FSP_ERR_CRYPTO_RSIP_KEY_SET_FAIL;
                break;
            }

            default:
            {
                err = FSP_ERR_CRYPTO_RSIP_FATAL;
            }
        }
    }

#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_message);
    FSP_PARAMETER_NOT_USED(message_length);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Finalizes a HMAC generation.
 *
 * Implements @ref rsip_api_t::kdfhmacSignFinish.
 *
 * @par State transition
 * @parblock
 * This API can only be executed in **STATE_KDF_HMAC**, and causes state transition.
 *
 * |Return value         |Next state|
 * |---------------------|----------|
 * |FSP_SUCCESS          |STATE_MAIN|
 * |FSP_ERR_ASSERTION    |No change |
 * |FSP_ERR_NOT_OPEN     |No change |
 * |FSP_ERR_INVALID_STATE|No change |
 * |Others               |STATE_MAIN|
 * @endparblock
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_CRYPTO_RSIP_FAIL              Input parameter is invalid.
 * @retval FSP_ERR_CRYPTO_RSIP_KEY_SET_FAIL      Input key is illegal.
 * @retval FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT A resource conflict occurred because a hardware resource required
 *                                               by the processing is in use by other processing.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_HMAC_SignFinish (rsip_ctrl_t * const p_ctrl, rsip_wrapped_mac_t * const p_wrapped_mac)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_HMAC_ENABLE
    rsip_instance_ctrl_t   * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;
    rsip_kdf_hmac_handle_t * p_handle        = &p_instance_ctrl->handle.kdf_hmac;

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_wrapped_mac);
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
 #endif

    /* Get subtype in RSIP_KEY_KDF_HMAC_SHAxxx format */
    rsip_wrapped_key_t * p_wrapped_key = (rsip_wrapped_key_t *) p_handle->wrapped_key;
    uint8_t              subtype       = p_wrapped_key->subtype;
    if (RSIP_ALG_HMAC == p_wrapped_key->alg)
    {
        subtype = gs_convert_to_kdf_hmac_subtype[subtype];
        FSP_ERROR_RETURN(subtype < RSIP_KEY_KDF_HMAC_NUM, FSP_ERR_CRYPTO_RSIP_FATAL);
    }

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_KDF_HMAC == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);

    /* Call function */
    rsip_ret_t rsip_ret = kdf_hmac_sign_finish(p_ctrl, p_wrapped_mac->value);

    /* Check error */
    switch (rsip_ret)
    {
        case RSIP_RET_PASS:
        {
            /* State transition */
            p_instance_ctrl->state = RSIP_STATE_MAIN;

            p_wrapped_mac->alg    = RSIP_ALG_KDF_HMAC;
            p_wrapped_mac->blocks = 1;
            switch (subtype)
            {
                /* SHA-256 */
                case RSIP_KEY_KDF_HMAC_SHA256:
                {
                    p_wrapped_mac->subtype      = RSIP_KEY_KDF_HMAC_SHA256;
                    p_wrapped_mac->block_length = RSIP_CFG_BYTE_SIZE_KDF_WRAPPED_MAC_SHA256;
                    break;
                }

                /* SHA-384 */
                case RSIP_KEY_KDF_HMAC_SHA384:
                {
                    p_wrapped_mac->subtype      = RSIP_KEY_KDF_HMAC_SHA384;
                    p_wrapped_mac->block_length = RSIP_CFG_BYTE_SIZE_KDF_WRAPPED_MAC_SHA384;
                    break;
                }

                /* SHA-512 */
                default:
                {
                    p_wrapped_mac->subtype      = RSIP_KEY_KDF_HMAC_SHA512;
                    p_wrapped_mac->block_length = RSIP_CFG_BYTE_SIZE_KDF_WRAPPED_MAC_SHA512;
                    break;
                }
            }

            err = FSP_SUCCESS;
            break;
        }

        case RSIP_RET_RESOURCE_CONFLICT:
        {
            err = FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT;
            break;
        }

        case RSIP_RET_FAIL:
        {
            err = FSP_ERR_CRYPTO_RSIP_FAIL;
            break;
        }

        case RSIP_RET_KEY_FAIL:
        {
            err = FSP_ERR_CRYPTO_RSIP_KEY_SET_FAIL;
            break;
        }

        default:
        {
            err = FSP_ERR_CRYPTO_RSIP_FATAL;
        }
    }

#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_wrapped_mac);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Suspends HMAC operation.
 *
 * This API releases RSIP resource and outputs intermediate results. Therefore, it can be used in the following cases:
 * - Execute another cryptographic operations during inputting successive chunks of the message.
 * - Reuse intermediate results.
 *
 * Implements @ref rsip_api_t::kdfhmacSuspend.
 *
 * @par State transition
 * @parblock
 * This API can only be executed in **STATE_KDF_HMAC**, and causes state transition.
 *
 * |Return value|Next state|
 * |------------|----------|
 * |FSP_SUCCESS |STATE_MAIN|
 * |Others      |No change |
 * @endparblock
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_HMAC_Suspend (rsip_ctrl_t * const p_ctrl, rsip_kdf_hmac_handle_t * const p_handle)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_HMAC_ENABLE
    rsip_instance_ctrl_t * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_handle);
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_KDF_HMAC == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);

    rsip_ret_t rsip_ret;
    switch (p_instance_ctrl->handle.kdf_hmac.handle_state)
    {
        /* If the state is update, call suspension function */
        case RSIP_USER_HANDLE_STATE_UPDATE:
        {
            /* Read internal state */
            rsip_ret = r_rsip_kdf_hmac_suspend(p_instance_ctrl->handle.kdf_hmac.internal_state);

            /* Handle state transition */
            p_instance_ctrl->handle.kdf_hmac.handle_state = RSIP_USER_HANDLE_STATE_RESUME;
            break;
        }

        default:
        {
            rsip_ret = RSIP_RET_PASS;
        }
    }

    /* Check error */
    switch (rsip_ret)
    {
        case RSIP_RET_PASS:
        {
            /* Copy handle */
            *p_handle = p_instance_ctrl->handle.kdf_hmac;

            err = FSP_SUCCESS;
            break;
        }

        default:
        {
            err = FSP_ERR_CRYPTO_RSIP_FATAL;
        }
    }

    /* State transition */
    p_instance_ctrl->state = RSIP_STATE_MAIN;
#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_handle);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Resumes HMAC operation suspended by R_RSIP_KDF_HMAC_Suspend().
 *
 * Implements @ref rsip_api_t::kdfhmacResume.
 *
 * @par State transition
 * @parblock
 * This API can only be executed in **STATE_MAIN**, and causes state transition.
 *
 * |Return value|Next state    |
 * |------------|--------------|
 * |FSP_SUCCESS |STATE_KDF_HMAC|
 * |Others      |No change     |
 * @endparblock
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_HMAC_Resume (rsip_ctrl_t * const p_ctrl, rsip_kdf_hmac_handle_t const * const p_handle)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_HMAC_ENABLE
    rsip_instance_ctrl_t * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_handle);
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_MAIN == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);

    /* Copy handle */
    p_instance_ctrl->handle.kdf_hmac = *p_handle;

    /* State transition */
    p_instance_ctrl->state = RSIP_STATE_KDF_HMAC;

    err = FSP_SUCCESS;
#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_handle);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Concatenates two wrapped MACs.
 *
 * MAC1 || MAC2 is output to p_wrapped_mac1.
 *
 * Implements @ref rsip_api_t::kdfMacConcatenate.
 *
 * @par State transition
 * This API can be executed in **any state** including STATE_INITIAL, and does not cause any state transitions.
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_CRYPTO_RSIP_FAIL              Input parameter is invalid.
 * @retval FSP_ERR_INVALID_SIZE                  Any length is illegal.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_MACConcatenate (rsip_wrapped_mac_t * const       p_wrapped_mac1,
                                     rsip_wrapped_mac_t const * const p_wrapped_mac2,
                                     uint32_t const                   wrapped_mac1_buffer_length)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_HMAC_ENABLE
 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_wrapped_mac1);
    FSP_ASSERT(p_wrapped_mac2);

    /* Check mac type */
    FSP_ERROR_RETURN(p_wrapped_mac1->alg == p_wrapped_mac2->alg, FSP_ERR_CRYPTO_RSIP_FAIL);
    FSP_ERROR_RETURN(p_wrapped_mac1->subtype == p_wrapped_mac2->subtype, FSP_ERR_CRYPTO_RSIP_FAIL);
 #endif

    uint32_t len1 = p_wrapped_mac1->block_length * p_wrapped_mac1->blocks;
    uint32_t len2 = p_wrapped_mac2->block_length * p_wrapped_mac2->blocks;

    /* Check length */
    FSP_ERROR_RETURN(wrapped_mac1_buffer_length >= (sizeof(rsip_wrapped_mac_t) + len1 + len2), FSP_ERR_INVALID_SIZE);

    /* Copy MAC */
    memcpy(p_wrapped_mac1->value + len1, p_wrapped_mac2->value, len2);
    p_wrapped_mac1->blocks += p_wrapped_mac2->blocks;

    err = FSP_SUCCESS;
#else
    FSP_PARAMETER_NOT_USED(p_wrapped_mac1);
    FSP_PARAMETER_NOT_USED(p_wrapped_mac2);
    FSP_PARAMETER_NOT_USED(wrapped_mac1_buffer_length);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Outputs a wrapped key from KDF output.
 *
 * Implements @ref rsip_api_t::kdfDerivedKeyImport.
 *
 * @par Conditions
 * @parblock
 * Argument p_wrapped_mac must be input the result of R_RSIP_KDF_HMAC_SignFinish() or R_RSIP_KDF_MACConcatenate().
 * Argument key_type must be one of the following:
 *  - @ref RSIP_KEY_TYPE_AES_128
 *  - @ref RSIP_KEY_TYPE_AES_256
 *  - @ref RSIP_KEY_TYPE_HMAC_SHA256
 *  - @ref RSIP_KEY_TYPE_HMAC_SHA384
 *  - @ref RSIP_KEY_TYPE_HMAC_SHA512
 * @endparblock
 *
 * @par State transition
 * This API can only be executed in **STATE_MAIN**, and does not cause any state transitions.
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_NOT_ENABLED                   Input key type is disabled in this function by configuration.
 * @retval FSP_ERR_CRYPTO_RSIP_FAIL              Input parameter is invalid.
 *
 * @retval FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT A resource conflict occurred because a hardware resource required
 *                                               by the processing is in use by other processing.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_DerivedKeyImport (rsip_ctrl_t * const              p_ctrl,
                                       rsip_wrapped_mac_t const * const p_wrapped_mac,
                                       rsip_key_type_t const            key_type,
                                       uint32_t const                   position,
                                       rsip_wrapped_key_t * const       p_wrapped_key)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_HMAC_ENABLE
    rsip_instance_ctrl_t * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_wrapped_key);
    FSP_ASSERT(p_wrapped_mac);
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);
 #endif

    /* Set function */
    rsip_func_kdf_derived_key_import_t p_func = select_func_derived_key_import(p_wrapped_mac->subtype, key_type);

 #if RSIP_CFG_PARAM_CHECKING_ENABLE

    /* Check configuration */
    FSP_ERROR_RETURN(p_func, FSP_ERR_NOT_ENABLED);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_MAIN == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);

    uint32_t blocks[1] = {bswap_32big(p_wrapped_mac->blocks)};
    uint32_t pos[1]    = {bswap_32big(position)};

    /* Call function (cast to match the argument type with the primitive function) */
    rsip_ret_t rsip_ret =
        p_func((const uint32_t *) p_wrapped_mac->value, blocks, pos, (uint32_t *) p_wrapped_key->value);

    /* Check error */
    switch (rsip_ret)
    {
        case RSIP_RET_PASS:
        {
            p_wrapped_key->alg     = r_rsip_key_type_to_alg(key_type);
            p_wrapped_key->subtype = r_rsip_key_type_to_subtype(key_type);

            err = FSP_SUCCESS;
            break;
        }

        case RSIP_RET_RESOURCE_CONFLICT:
        {
            err = FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT;
            break;
        }

        case RSIP_RET_FAIL:
        {
            err = FSP_ERR_CRYPTO_RSIP_FAIL;
            break;
        }

        default:
        {
            err = FSP_ERR_CRYPTO_RSIP_FATAL;
        }
    }

#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_wrapped_mac);
    FSP_PARAMETER_NOT_USED(key_type);
    FSP_PARAMETER_NOT_USED(position);
    FSP_PARAMETER_NOT_USED(p_wrapped_key);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * Outputs a initial vector from KDF output.
 *
 * Implements @ref rsip_api_t::kdfDerivedIvWrap.
 *
 * @par Conditions
 * @parblock
 * Argument p_wrapped_mac must be input the result of R_RSIP_KDF_HMAC_SignFinish() or R_RSIP_KDF_MACConcatenate().
 * Argument initial_vector_type must be one of the following:
 *  - @ref RSIP_INITIAL_VECTOR_TYPE_AES_16_BYTE
 * @endparblock
 *
 * @par State transition
 * This API can only be executed in **STATE_MAIN**, and does not cause any state transitions.
 *
 * @retval FSP_SUCCESS                           Normal termination.
 * @retval FSP_ERR_ASSERTION                     A required parameter is NULL.
 * @retval FSP_ERR_NOT_OPEN                      Module is not open.
 * @retval FSP_ERR_INVALID_STATE                 Internal state is illegal.
 * @retval FSP_ERR_NOT_ENABLED                   Input key type is disabled in this function by configuration.
 * @retval FSP_ERR_CRYPTO_RSIP_FAIL              Input parameter is invalid.
 *
 * @retval FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT A resource conflict occurred because a hardware resource required
 *                                               by the processing is in use by other processing.
 * @retval FSP_ERR_CRYPTO_RSIP_FATAL             Software corruption is detected.
 * @retval FSP_ERR_UNSUPPORTED                   This API is not supported on this device.
 **********************************************************************************************************************/
fsp_err_t R_RSIP_KDF_DerivedIVWrap (rsip_ctrl_t * const              p_ctrl,
                                    rsip_wrapped_mac_t const * const p_wrapped_mac,
                                    rsip_initial_vector_type_t const initial_vector_type,
                                    uint32_t const                   position,
                                    uint8_t const * const            p_tls_sequence_num,
                                    uint8_t * const                  p_wrapped_initial_vector)
{
    fsp_err_t err = FSP_ERR_CRYPTO_RSIP_FATAL;

#if RSIP_CFG_KDF_HMAC_ENABLE
    rsip_instance_ctrl_t * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;

 #if RSIP_CFG_PARAM_CHECKING_ENABLE
    FSP_ASSERT(p_instance_ctrl);
    FSP_ASSERT(p_tls_sequence_num || (RSIP_INITIAL_VECTOR_TYPE_AES_16_BYTE == initial_vector_type) ||
               (RSIP_INITIAL_VECTOR_TYPE_AES_TLS12_CBC == initial_vector_type));
    FSP_ASSERT(p_wrapped_initial_vector);
    FSP_ASSERT(p_wrapped_mac);
    FSP_ERROR_RETURN(RSIP_OPEN == p_instance_ctrl->open, FSP_ERR_NOT_OPEN);

    /* Check mac type */
    FSP_ERROR_RETURN(RSIP_ALG_KDF_HMAC == p_wrapped_mac->alg, FSP_ERR_CRYPTO_RSIP_FAIL);
 #endif

    /* Set function */
    rsip_func_kdf_derived_iv_wrap_t p_func = gp_func_kdf_derived_iv_wrap[p_wrapped_mac->subtype][initial_vector_type];

 #if RSIP_CFG_PARAM_CHECKING_ENABLE

    /* Check configuration */
    FSP_ERROR_RETURN(p_func, FSP_ERR_NOT_ENABLED);
 #endif

    /* Check state */
    FSP_ERROR_RETURN(RSIP_STATE_MAIN == p_instance_ctrl->state, FSP_ERR_INVALID_STATE);

    uint32_t blocks[1] = {bswap_32big(p_wrapped_mac->blocks)};
    uint32_t pos[1]    = {bswap_32big(position)};

    /* Call function (cast to match the argument type with the primitive function) */
    rsip_ret_t rsip_ret =
        p_func((const uint32_t *) p_wrapped_mac->value,
               blocks,
               pos,
               (const uint32_t *) p_tls_sequence_num,
               (uint32_t *) p_wrapped_initial_vector);

    /* Check error */
    switch (rsip_ret)
    {
        case RSIP_RET_PASS:
        {
            err = FSP_SUCCESS;
            break;
        }

        case RSIP_RET_RESOURCE_CONFLICT:
        {
            err = FSP_ERR_CRYPTO_RSIP_RESOURCE_CONFLICT;
            break;
        }

        case RSIP_RET_FAIL:
        {
            err = FSP_ERR_CRYPTO_RSIP_FAIL;
            break;
        }

        default:
        {
            err = FSP_ERR_CRYPTO_RSIP_FATAL;
        }
    }

#else
    FSP_PARAMETER_NOT_USED(p_ctrl);
    FSP_PARAMETER_NOT_USED(p_wrapped_mac);
    FSP_PARAMETER_NOT_USED(initial_vector_type);
    FSP_PARAMETER_NOT_USED(position);
    FSP_PARAMETER_NOT_USED(p_tls_sequence_num);
    FSP_PARAMETER_NOT_USED(p_wrapped_initial_vector);

    err = FSP_ERR_UNSUPPORTED;
#endif

    return err;
}

/*******************************************************************************************************************//**
 * @} (end addtogroup RSIP_PROTECTED)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/

#if RSIP_CFG_KDF_HMAC_ENABLE

/*******************************************************************************************************************//**
 * Select primitive function of key import from key type.
 *
 * @param[in] kdf_mac_type KDF MAC type.
 * @param[in] key_type     Key type.
 *
 * @return Pointer to function.
 ***********************************************************************************************************************/
static rsip_func_kdf_derived_key_import_t select_func_derived_key_import (uint8_t         kdf_mac_type,
                                                                          rsip_key_type_t key_type)
{
    rsip_func_kdf_derived_key_import_t ret = NULL;
    uint8_t alg     = r_rsip_key_type_to_alg(key_type);
    uint8_t subtype = r_rsip_key_type_to_subtype(key_type);

    switch (alg)
    {
        case RSIP_ALG_AES:
        {
            ret = gp_func_kdf_derived_key_import_aes[kdf_mac_type][subtype];
            break;
        }

        case RSIP_ALG_HMAC:
        {
            ret = gp_func_kdf_derived_key_import_hmac[kdf_mac_type][subtype];
            break;
        }

        default:
        {
            /* Do nothing */
        }
    }

    return ret;
}

/*******************************************************************************************************************//**
 * Inputs message.
 *
 * @param[in,out] p_ctrl         Pointer to control block.
 * @param[in]     p_message      Pointer to message. The length is message_length.
 * @param[in]     message_length Byte length of message (0 or more bytes).
 *
 * @return The return value of the internally called primitive function.
 **********************************************************************************************************************/
static rsip_ret_t kdf_hmac_update (rsip_ctrl_t * const p_ctrl, const uint8_t * p_message, uint32_t message_length)
{
    rsip_instance_ctrl_t   * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;
    rsip_kdf_hmac_handle_t * p_handle        = &p_instance_ctrl->handle.kdf_hmac;

    rsip_ret_t ret;
    switch (p_handle->handle_state)
    {
        case RSIP_USER_HANDLE_STATE_INIT:
        {
            ret = r_rsip_kdf_hmac_init_update((rsip_wrapped_key_t *) p_handle->wrapped_key,
                                              p_message,
                                              message_length - p_handle->actual_wrapped_msg_length,
                                              p_handle->wrapped_msg,
                                              p_handle->wrapped_msg_length,
                                              p_handle->internal_state);
            break;
        }

        case RSIP_USER_HANDLE_STATE_RESUME:
        {
            ret = r_rsip_kdf_hmac_resume_update((rsip_wrapped_key_t *) p_handle->wrapped_key,
                                                p_message,
                                                message_length,
                                                p_handle->wrapped_msg,
                                                p_handle->wrapped_msg_length,
                                                p_handle->internal_state);
            break;
        }

        case RSIP_USER_HANDLE_STATE_UPDATE:
        default:
        {
            ret = r_rsip_kdf_hmac_update((rsip_wrapped_key_t *) p_handle->wrapped_key,
                                         p_message,
                                         message_length,
                                         p_handle->internal_state);
        }
    }

    /* Check error */
    switch (ret)
    {
        case RSIP_RET_PASS:
        {
            /* State transition */
            p_handle->handle_state = RSIP_USER_HANDLE_STATE_UPDATE;
            break;
        }

        default:
        {
            /* State transition (abort) */
            p_instance_ctrl->state = RSIP_STATE_MAIN;
        }
    }

    return ret;
}

/*******************************************************************************************************************//**
 * Finalizes a HMAC generation.
 *
 * @param[in,out] p_ctrl   Pointer to control block.
 * @param[out]    p_mac    Pointer to destination of message digest. The length depends on MAC type.
 *
 * @return The return value of the internally called primitive function.
 **********************************************************************************************************************/
static rsip_ret_t kdf_hmac_sign_finish (rsip_ctrl_t * const p_ctrl, uint8_t * p_mac)
{
    rsip_instance_ctrl_t   * p_instance_ctrl = (rsip_instance_ctrl_t *) p_ctrl;
    rsip_kdf_hmac_handle_t * p_handle        = &p_instance_ctrl->handle.kdf_hmac;
    rsip_ret_t               ret;
    switch (p_handle->handle_state)
    {
        case RSIP_USER_HANDLE_STATE_INIT:
        {
            ret =
                r_rsip_kdf_hmac_init_final((rsip_wrapped_key_t *) p_handle->wrapped_key,
                                           p_handle->buffer,
                                           p_handle->buffered_length,
                                           p_handle->wrapped_msg,
                                           p_handle->wrapped_msg_length,
                                           p_handle->actual_wrapped_msg_length,
                                           p_mac);
            break;
        }

        case RSIP_USER_HANDLE_STATE_RESUME:
        {
            ret = r_rsip_kdf_hmac_resume_final((rsip_wrapped_key_t *) p_handle->wrapped_key,
                                               p_handle->buffer,
                                               p_handle->buffered_length,
                                               p_handle->total_length + p_handle->buffered_length,
                                               p_mac,
                                               p_handle->internal_state);
            break;
        }

        case RSIP_USER_HANDLE_STATE_UPDATE:
        default:
        {
            ret = r_rsip_kdf_hmac_final((rsip_wrapped_key_t *) p_handle->wrapped_key,
                                        p_handle->buffer,
                                        p_handle->buffered_length,
                                        p_handle->total_length + p_handle->buffered_length,
                                        p_mac,
                                        p_handle->internal_state);
        }
    }

    /* State transition */
    p_instance_ctrl->state = RSIP_STATE_MAIN;

    return ret;
}

#endif
