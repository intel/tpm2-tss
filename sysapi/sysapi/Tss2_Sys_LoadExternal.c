/***********************************************************************;
 * Copyright (c) 2015 - 2017, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
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
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 ***********************************************************************/

#include "sapi/tpm20.h"
#include "sysapi_util.h"

TSS2_RC Tss2_Sys_LoadExternal_Prepare(
    TSS2_SYS_CONTEXT *sysContext,
    TPM2B_SENSITIVE *inPrivate,
    TPM2B_PUBLIC *inPublic,
    TPMI_RH_HIERARCHY hierarchy)
{
    TSS2_RC rval;

    if (!sysContext)
        return TSS2_SYS_RC_BAD_REFERENCE;

    rval = CommonPreparePrologue(sysContext, TPM_CC_LoadExternal);

    /* If no private key is specified, set the private key size field to 0 */
    if (!inPrivate) {
        SYS_CONTEXT->decryptNull = 1;

        rval = Tss2_MU_UINT16_Marshal(0, SYS_CONTEXT->cmdBuffer,
                                      SYS_CONTEXT->maxCmdSize,
                                      &SYS_CONTEXT->nextData);
    } else {

        rval = Tss2_MU_TPM2B_SENSITIVE_Marshal(inPrivate,
                                               SYS_CONTEXT->cmdBuffer,
                                               SYS_CONTEXT->maxCmdSize,
                                               &SYS_CONTEXT->nextData);
    }

    if (rval)
        return rval;

   rval = Tss2_MU_TPM2B_PUBLIC_Marshal(inPublic, SYS_CONTEXT->cmdBuffer,
                                       SYS_CONTEXT->maxCmdSize,
                                       &SYS_CONTEXT->nextData);
    if (rval)
        return rval;

   rval = Tss2_MU_UINT32_Marshal(hierarchy, SYS_CONTEXT->cmdBuffer,
                                 SYS_CONTEXT->maxCmdSize,
                                 &SYS_CONTEXT->nextData);
    if (rval)
        return rval;

    SYS_CONTEXT->decryptAllowed = 1;
    SYS_CONTEXT->encryptAllowed = 1;
    SYS_CONTEXT->authAllowed = 1;

    return CommonPrepareEpilogue(sysContext);
}

TSS2_RC Tss2_Sys_LoadExternal_Complete(
    TSS2_SYS_CONTEXT *sysContext,
    TPM_HANDLE *objectHandle,
    TPM2B_NAME *name)
{
    TSS2_RC rval;

    if (!sysContext)
        return TSS2_SYS_RC_BAD_REFERENCE;

    rval = Tss2_MU_UINT32_Unmarshal(SYS_CONTEXT->cmdBuffer,
                                    SYS_CONTEXT->maxCmdSize,
                                    &SYS_CONTEXT->nextData,
                                    objectHandle);
    if (rval)
        return rval;

    rval = CommonComplete(sysContext);
    if (rval)
        return rval;

    return Tss2_MU_TPM2B_NAME_Unmarshal(SYS_CONTEXT->cmdBuffer,
                                        SYS_CONTEXT->maxCmdSize,
                                        &SYS_CONTEXT->nextData, name);
}

TSS2_RC Tss2_Sys_LoadExternal(
    TSS2_SYS_CONTEXT *sysContext,
    TSS2_SYS_CMD_AUTHS const *cmdAuthsArray,
    TPM2B_SENSITIVE *inPrivate,
    TPM2B_PUBLIC *inPublic,
    TPMI_RH_HIERARCHY hierarchy,
    TPM_HANDLE *objectHandle,
    TPM2B_NAME *name,
    TSS2_SYS_RSP_AUTHS *rspAuthsArray)
{
    TSS2_RC rval;

    rval = Tss2_Sys_LoadExternal_Prepare(sysContext, inPrivate, inPublic, hierarchy);
    if (rval)
        return rval;

    rval = CommonOneCall(sysContext, cmdAuthsArray, rspAuthsArray);
    if (rval)
        return rval;

    return Tss2_Sys_LoadExternal_Complete(sysContext, objectHandle, name);
}
