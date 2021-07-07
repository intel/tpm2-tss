/* SPDX-License-Identifier: BSD-2-Clause */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "tss2_esys.h"
#define LOGMODULE esys
#include "util/log.h"

TSS2_LOG_HANDLER
Esys_SetLogHandler(
    TSS2_LOG_HANDLER new_handler)
{
    return set_log_handler(new_handler);
}
