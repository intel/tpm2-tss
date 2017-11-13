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

#include "sapi/tpm20.h"
#include "sample.h"
#include "sysapi_util.h"

void InitEntities()
{
    int i;
    for( i = 0; i < MAX_NUM_ENTITIES; i++ )
    {
        entities[i].entityHandle = TPM_HT_NO_HANDLE;
    }
}

TSS2_RC AddEntity( TPM_HANDLE entityHandle, TPM2B_AUTH *auth )
{
    int i;
    TSS2_RC rval = TPM_RC_FAILURE;

    for( i = 0; i < MAX_NUM_ENTITIES; i++ )
    {
        if( entities[i].entityHandle == TPM_HT_NO_HANDLE )
        {
            entities[i].entityHandle = entityHandle;
            CopySizedByteBuffer((TPM2B *)&entities[i].entityAuth, (TPM2B *)auth);

            if( ( entityHandle >> HR_SHIFT ) == TPM_HT_NV_INDEX )
            {
                entities[i].nvNameChanged = 0;
            }

            rval = TPM_RC_SUCCESS;
            break;
        }
    }
    return rval;
}

TSS2_RC DeleteEntity( TPM_HANDLE entityHandle )
{
    int i;
    TSS2_RC rval = TPM_RC_FAILURE;

    for( i = 0; i < MAX_NUM_ENTITIES; i++ )
    {
        if( entities[i].entityHandle == entityHandle )
        {
            entities[i].entityHandle = TPM_HT_NO_HANDLE;
            rval = TPM_RC_SUCCESS;
            break;
        }
    }
    return rval;
}

TSS2_RC GetEntityAuth( TPM_HANDLE entityHandle, TPM2B_AUTH *auth )
{
    int i;
    TSS2_RC rval = TPM_RC_FAILURE;

    for( i = 0; i < MAX_NUM_ENTITIES; i++ )
    {
        if( entities[i].entityHandle == entityHandle )
        {
            CopySizedByteBuffer((TPM2B *)auth, (TPM2B *)&entities[i].entityAuth);
            rval = TPM_RC_SUCCESS;
            break;
        }
    }
    return rval;
}


TSS2_RC GetEntity( TPM_HANDLE entityHandle, ENTITY **entity )
{
    int i;
    TSS2_RC rval = TPM_RC_FAILURE;

    for( i = 0; i < MAX_NUM_ENTITIES; i++ )
    {
        if( entities[i].entityHandle == entityHandle )
        {
            *entity = &( entities[i] );
            rval = TPM_RC_SUCCESS;
        }
    }
    return rval;
}

