/* SPDX-License-Identifier: BSD-2-Clause */
/***********************************************************************;
 * Copyright (c) 2015 - 2018, Intel Corporation
 * All rights reserved.
 ***********************************************************************/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "tss2_sys.h"
#define LOGMODULE sys
#include "util/log.h"

TSS2_LOG_HANDLER
Tss2_Sys_SetLogHandler(
    TSS2_LOG_HANDLER new_handler)
{
    return set_log_handler(new_handler);
}
