/* SPDX-License-Identifier: BSD-2-Clause */
/*******************************************************************************
 * Copyright 2017-2018, Fraunhofer SIT sponsored by Infineon Technologies AG
 * All rights reserved.
 ******************************************************************************/
#ifndef AUX_UTIL_H
#define AUX_UTIL_H

#include <stdbool.h>

#include "tss2_tpm2_types.h"
#ifdef __cplusplus
extern "C" {
#endif

#define SAFE_FREE(S) if((S) != NULL) {free((void*) (S)); (S)=NULL;}

#define TPM2_ERROR_FORMAT "%s%s (0x%08x)"
#define TPM2_ERROR_TEXT(r) "Error", "Code", r

#define return_if_error(r,msg) \
    if (r != TSS2_RC_SUCCESS) { \
        LOG_ERROR("%s " TPM2_ERROR_FORMAT, msg, TPM2_ERROR_TEXT(r)); \
        return r;  \
    }

#define return_state_if_error(r,s,msg)      \
    if (r != TSS2_RC_SUCCESS) { \
        LOG_ERROR("%s " TPM2_ERROR_FORMAT, msg, TPM2_ERROR_TEXT(r)); \
        esysContext->state = s; \
        return r;  \
    }

#define return_error(r,msg) \
    { \
        LOG_ERROR("%s " TPM2_ERROR_FORMAT, msg, TPM2_ERROR_TEXT(r)); \
        return r;  \
    }

#define goto_state_if_error(r,s,msg,label) \
    if (r != TSS2_RC_SUCCESS) { \
        LOG_ERROR("%s " TPM2_ERROR_FORMAT, msg, TPM2_ERROR_TEXT(r)); \
        esysContext->state = s; \
        goto label;  \
    }

#define goto_if_null(p,msg,ec,label) \
    if ((p) == NULL) { \
        LOG_ERROR("%s ", (msg)); \
        r = (ec); \
        goto label;  \
    }

#define goto_if_error(r,msg,label) \
    if (r != TSS2_RC_SUCCESS) { \
        LOG_ERROR("%s " TPM2_ERROR_FORMAT, msg, TPM2_ERROR_TEXT(r)); \
        goto label;  \
    }

#define goto_error(r,v,msg,label, ...)              \
    { r = v;  \
      LOG_ERROR(TPM2_ERROR_FORMAT " " msg, TPM2_ERROR_TEXT(r), ## __VA_ARGS__); \
      goto label; \
    }

#define return_if_null(p,msg,ec) \
    if (p == NULL) { \
        LOG_ERROR("%s ", msg); \
        return ec; \
    }

#define return_if_notnull(p,msg,ec) \
    if (p != NULL) { \
        LOG_ERROR("%s ", msg); \
        return ec; \
    }

#define exit_if_error(r,msg) \
    if (r != TSS2_RC_SUCCESS) { \
        LOG_ERROR("%s " TPM2_ERROR_FORMAT, msg, TPM2_ERROR_TEXT(r)); \
        exit(1);  \
    }

#define set_return_code(r_max, r, msg) \
    if (r != TSS2_RC_SUCCESS) { \
        LOG_ERROR("%s " TPM2_ERROR_FORMAT, msg, TPM2_ERROR_TEXT(r)); \
        r_max = r; \
    }

static inline TSS2_RC
tss2_fmt_p1_error_to_rc(UINT16 err)
{
    return TPM2_RC_1+TPM2_RC_P+err;
}

static inline bool
tss2_is_expected_error(TSS2_RC rc)
{
    /* Success is always expected */
    if (rc == TSS2_RC_SUCCESS) {
        return true;
    }

    /*
     * drop the layer, any part of the TSS stack can gripe about this error
     * if it wants too.
     */
    rc &= ~TSS2_RC_LAYER_MASK;

    /*
     * Format 1, parameter 1 errors plus the below RC's
     * contain everything we care about:
     *   - TPM2_RC_CURVE
     *   - TPM2_RC_HASH
     *   - TPM2_RC_ASYMMETRIC
     *   - TPM2_RC_KEY_SIZE
     */
    if (rc == tss2_fmt_p1_error_to_rc(TPM2_RC_CURVE)
            || rc == tss2_fmt_p1_error_to_rc(TPM2_RC_VALUE)
            || rc == tss2_fmt_p1_error_to_rc(TPM2_RC_HASH)
            || rc == tss2_fmt_p1_error_to_rc(TPM2_RC_ASYMMETRIC)
            || rc == tss2_fmt_p1_error_to_rc(TPM2_RC_KEY_SIZE)) {
        return true;
    }

    return false;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AUX_UTIL_H */
