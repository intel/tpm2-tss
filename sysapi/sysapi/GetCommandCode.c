//**********************************************************************;
// Copyright (c) 2015, Intel Corporation
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//**********************************************************************;

#include <tpm20.h>   
#include <tss2_sysapi_util.h>

TSS2_RC Tss2_Sys_GetCommandCode(
    TSS2_SYS_CONTEXT *sysContext,
    UINT8 (*commandCode)[4]  
    )
{
    TSS2_RC rval = TSS2_RC_SUCCESS;
    
    if( sysContext == NULL || commandCode == NULL )
    {
        rval = TSS2_SYS_RC_BAD_REFERENCE;
    }
    else if( SYS_CONTEXT->previousStage == CMD_STAGE_INITIALIZE )
    {
        rval = TSS2_SYS_RC_BAD_SEQUENCE;
    }
    else
    {
        *(UINT32 *)*commandCode = CHANGE_ENDIAN_DWORD( SYS_CONTEXT->commandCodeSwapped );
    }
    
    return( rval );
}
