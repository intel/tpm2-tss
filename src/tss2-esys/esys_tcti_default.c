#include "tss2_mu.h"
#include "tss2_sys.h"
#include "tss2_tcti_device.h"
#include "tss2_tcti_mssim.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#define LOGMODULE esys
#include "util/log.h"

#ifndef ESYS_TCTI_DEFAULT_MODULE
#define ESYS_TCTI_DEFAULT_MODULE Mssim
#endif
#ifndef ESYS_TCTI_DEFAULT_CONFIG
#define ESYS_TCTI_DEFAULT_CONFIG tcp://127.0.0.1:2321
#endif

#define _CONC(a,b,c) a ## b ## c
#define _XCONC(a,b,c) _CONC(a,b,c)
#define Tss2_Tcti_Default_Init _XCONC(Tss2_Tcti_, ESYS_TCTI_DEFAULT_MODULE, _Init)

#define _STR(A) #A
#define _XSTR(A) _STR(A)

/*
 * Initialize a TCTI instance.
 * The caller is returned a TCTI context structure that is allocated by this
 * function. This structure must be freed by the caller.
 */
TSS2_RC
get_tcti_default(TSS2_TCTI_CONTEXT ** tcticontext)
{
    size_t size;
    TSS2_RC rc;
    TSS2_TCTI_CONTEXT *tcti_ctx;

    rc = Tss2_Tcti_Default_Init(NULL, &size, _XSTR(ESYS_TCTI_DEFAULT_CONFIG));
    if (rc != TSS2_RC_SUCCESS) {
        LOG_ERROR("Failed to get allocation size for tcti context: 0x%x\n", rc);
        return rc;
    }
    tcti_ctx = (TSS2_TCTI_CONTEXT *) calloc(1, size);
    if (tcti_ctx == NULL) {
        LOG_ERROR("Allocation for tcti context failed: %s\n",
                strerror(errno));
        return TSS2_BASE_RC_MEMORY;
    }
    rc = Tss2_Tcti_Default_Init(tcti_ctx, &size, _XSTR(ESYS_TCTI_DEFAULT_CONFIG));
    if (rc != TSS2_RC_SUCCESS) {
        LOG_ERROR("Failed to initialize tcti context: 0x%x\n", rc);
        free(tcti_ctx);
        return rc;
    }
    *tcticontext = tcti_ctx;
    return TSS2_RC_SUCCESS;
}
