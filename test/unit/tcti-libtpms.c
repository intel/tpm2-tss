/* SPDX-License-Identifier: BSD-2-Clause */
/***********************************************************************;
 * Copyright (c) 2015 - 2018, Intel Corporation
 * All rights reserved.
 ***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <setjmp.h>
#include <cmocka.h>

#include "tss2_tcti.h"
#include "tss2_tcti_libtpms.h"

#include "tss2-tcti/tcti-common.h"
#include "tss2-tcti/tcti-libtpms.h"

#define LOGMODULE test
#include "util/log.h"

#define LIBTPMS_DL_HANDLE  0x12345678
#define STATEFILE_PATH     "statefile.bin"
#define STATEFILE_FD       0xAABB
#define STATEFILE_MMAP     mmap_buf
#define STATEFILE_MMAP_NEW mmap_buf_new

/* loaded state */
#define S1_PERMANENT_BUF_LITERAL "aaaaaaaa"
#define S1_PERMANENT_BUF_LEN     8
#define S1_VOLATILE_BUF_LITERAL  "bbbbb"
#define S1_VOLATILE_BUF_LEN      5
#define S1_STATE                 "\0\0\0\x08" S1_PERMANENT_BUF_LITERAL "\0\0\0\x05" S1_VOLATILE_BUF_LITERAL
#define S1_STATE_LEN             (sizeof(uint32_t) + S1_PERMANENT_BUF_LEN + sizeof(uint32_t) + S1_VOLATILE_BUF_LEN)

/* next state */
#define S2_PERMANENT_BUF_LITERAL "xxxxxxxxxxxxx"
#define S2_PERMANENT_BUF_LEN     13
#define S2_VOLATILE_BUF_LITERAL  "yyyyyyy"
#define S2_VOLATILE_BUF_LEN      7
#define S2_STATE                 "\0\0\0\x0D" S2_PERMANENT_BUF_LITERAL "\0\0\0\x07" S2_VOLATILE_BUF_LITERAL
#define S2_STATE_LEN             (sizeof(uint32_t) + S2_PERMANENT_BUF_LEN + sizeof(uint32_t) + S2_VOLATILE_BUF_LEN)

/* big state */
#define S3_PERMANENT_BUF_LITERAL "ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss" \
                                 "ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss" \
                                 "ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss" \
                                 "ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss" \
                                 "ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss" \
                                 "ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss" \
                                 "ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss" \
                                 "ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss" \
                                 "ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss" \
                                 "ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss" \
                                 "ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss" \
                                 "ssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssss"
#define S3_PERMANENT_BUF_LEN     1200
#define S3_VOLATILE_BUF_LITERAL  "tttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttt" \
                                 "tttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttt" \
                                 "tttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttt" \
                                 "tttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttt" \
                                 "tttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttt" \
                                 "tttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttt" \
                                 "tttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttt" \
                                 "tttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttt" \
                                 "tttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttt" \
                                 "tttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttt" \
                                 "tttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttttt"
#define S3_VOLATILE_BUF_LEN      1100
#define S3_STATE                 "\0\0\x04\xB0" S3_PERMANENT_BUF_LITERAL "\0\0\04\x4C" S3_VOLATILE_BUF_LITERAL
#define S3_STATE_LEN             (sizeof(uint32_t) + S3_PERMANENT_BUF_LEN + sizeof(uint32_t) + S3_VOLATILE_BUF_LEN)

char mmap_buf[STATE_MMAP_CHUNK_LEN] = {0};
char mmap_buf_new[2400] = {0};

struct libtpms_callbacks global_callbacks;

/* mock libtpms API */
TPM_RESULT TPMLIB_ChooseTPMVersion(TPMLIB_TPMVersion ver)
{
    check_expected(ver);
    return mock_type(int);
}
TPM_RESULT TPMLIB_RegisterCallbacks(struct libtpms_callbacks *callbacks)
{
    global_callbacks.sizeOfStruct = callbacks->sizeOfStruct;
    global_callbacks.tpm_nvram_init = callbacks->tpm_nvram_init;
    global_callbacks.tpm_nvram_loaddata = callbacks->tpm_nvram_loaddata;
    global_callbacks.tpm_nvram_storedata = callbacks->tpm_nvram_storedata;
    global_callbacks.tpm_nvram_deletename = callbacks->tpm_nvram_deletename;
    global_callbacks.tpm_io_init = callbacks->tpm_io_init;
    global_callbacks.tpm_io_getlocality = callbacks->tpm_io_getlocality;
    global_callbacks.tpm_io_getphysicalpresence = callbacks->tpm_io_getphysicalpresence;
    return mock_type(int);
}
TPM_RESULT TPMLIB_GetState(enum TPMLIB_StateType st, unsigned char **buf, uint32_t *buf_len)
{
    check_expected(st);
    unsigned char *buf_out = mock_type(unsigned char *);
    *buf_len = mock_type(uint32_t);
    *buf = malloc(*buf_len);
    assert_non_null(*buf);
    memcpy(*buf, buf_out, *buf_len);
    return mock_type(int);
}
TPM_RESULT TPMLIB_MainInit(void)
{
    uint32_t ret;
    ret = global_callbacks.tpm_nvram_init();
    assert_int_equal(ret, 0);
    ret = global_callbacks.tpm_io_init();
    assert_int_equal(ret, 0);
    ret = global_callbacks.tpm_nvram_loaddata((unsigned char **) 1,
                                               (uint32_t *) 2,
                                               3,
                                               "4");
    assert_int_equal(ret, TPM_RETRY);
    return mock_type(int);
}
TPM_RESULT TPMLIB_Process(unsigned char **resp_buf, uint32_t *resp_len, uint32_t *resp_buf_len, unsigned char *cmd, uint32_t cmd_len)
{
    uint32_t locality;
    uint32_t ret;
    check_expected_ptr(cmd);
    check_expected(cmd_len);
    ret = global_callbacks.tpm_io_getlocality(&locality, 0);
    assert_int_equal(ret, 0);
    check_expected(locality);

    ret = global_callbacks.tpm_nvram_storedata((unsigned char *) 1, 2, 3, "4");
    assert_int_equal(ret, TPM_SUCCESS);

    unsigned char *buf_out = mock_type(unsigned char *);
    *resp_buf_len = *resp_len = mock_type(uint32_t);
    *resp_buf = malloc(*resp_len);
    assert_non_null(*resp_buf);
    memcpy(*resp_buf, buf_out, *resp_len);
    return mock_type(int);
}
TPM_RESULT TPMLIB_SetState(enum TPMLIB_StateType st, const unsigned char *buf, uint32_t buf_len)
{
    check_expected_ptr(st);
    check_expected_ptr(buf);
    check_expected_ptr(buf_len);
    return mock_type(int);
}
void TPMLIB_Terminate(void)
{
}

void *__wrap_dlopen(const char *filename, int flags)
{
    LOG_TRACE("Called with filename %s and flags %x", filename, flags);
    check_expected_ptr(filename);
    check_expected(flags);
    return mock_type(void *);
}
int __wrap_dlclose(void *handle)
{
    LOG_TRACE("Called with handle %p", handle);
    check_expected_ptr(handle);
    return mock_type(int);
}
void *__wrap_dlsym(void *handle, const char *symbol)
{
    LOG_TRACE("Called with handle %p and symbol %s", handle, symbol);
    check_expected_ptr(handle);
    check_expected_ptr(symbol);
    return mock_type(void *);
}
void *__wrap_mmap (void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    check_expected_ptr(addr);
    check_expected(len);
    check_expected(prot);
    check_expected(flags);
    check_expected(fd);
    check_expected(offset);
    return mock_type(void *);
}
void *__wrap_mremap(void *old_address, size_t old_size, size_t new_size, int flags)
{
    void *new_address;
    check_expected_ptr(old_address);
    check_expected(old_size);
    check_expected(new_size);
    check_expected(flags);
    new_address = mock_type(void *);
    if (new_address != MAP_FAILED) {
        memcpy(new_address, old_address, old_size);
    }
    return new_address;
}
int __wrap_munmap(void *addr, size_t len)
{
    check_expected_ptr(addr);
    check_expected(len);
    return mock_type(int);
}
int __real_open(const char *pathname, int flags, ...);
int __wrap_open(const char *pathname, int flags, mode_t mode)
{
    if (strncmp(pathname, STATEFILE_PATH, strlen(STATEFILE_PATH)) == 0) {
        check_expected_ptr(pathname);
        check_expected(flags);
        check_expected(mode);
        return mock_type(int);
    } else {
        /* only mock opening of state files as the open() syscall is needed
           for code coverage reports as well */
        return __real_open(pathname, flags, mode);
    }
}
off_t __wrap_lseek(int fd, off_t offset, int whence)
{
    check_expected(fd);
    check_expected(offset);
    check_expected(whence);
    return mock_type(off_t);
}
int __wrap_posix_fallocate(int fd, off_t offset, off_t len)
{
    check_expected(fd);
    check_expected(offset);
    check_expected(len);
    return mock_type(int);
}
int __wrap_truncate(const char *path, off_t length)
{
    check_expected_ptr(path);
    check_expected(length);
    return mock_type(int);
}
int __wrap_close(int fd)
{
    check_expected(fd);
    return mock_type(int);
}

/* When passed all NULL values, we expect TSS2_TCTI_RC_BAD_VALUE. */
static void
tcti_libtpms_init_all_null_test(void **state)
{
    TSS2_RC rc;

    rc = Tss2_Tcti_Libtpms_Init(NULL, NULL, NULL);
    assert_int_equal(rc, TSS2_TCTI_RC_BAD_VALUE);
}

/* When dlopen fails for library names we expect TSS2_TCTI_RC_GENERAL_FAILURE. */
static void
tcti_libtpms_init_dlopen_fail_test(void **state)
{
    size_t tcti_size = 0;
    TSS2_RC ret = TSS2_RC_SUCCESS;
    TSS2_TCTI_CONTEXT *ctx = NULL;

    ret = Tss2_Tcti_Libtpms_Init(NULL, &tcti_size, NULL);
    assert_true(ret == TSS2_RC_SUCCESS);
    ctx = calloc(1, tcti_size);
    assert_non_null(ctx);

    expect_string(__wrap_dlopen, filename, "libtpms.so");
    expect_value(__wrap_dlopen, flags, RTLD_LAZY | RTLD_LOCAL);
    will_return(__wrap_dlopen, NULL);
    expect_string(__wrap_dlopen, filename, "libtpms.so.0");
    expect_value(__wrap_dlopen, flags, RTLD_LAZY | RTLD_LOCAL);
    will_return(__wrap_dlopen, NULL);

    ret = Tss2_Tcti_Libtpms_Init(ctx, &tcti_size, NULL);
    assert_int_equal(ret, TSS2_TCTI_RC_GENERAL_FAILURE);

    free(ctx);
}

/* When dlsym fails for any libtpms symbol, we expect TSS2_TCTI_RC_GENERAL_FAILURE. */
static void
tcti_libtpms_init_dlsym_fail_test(void **state)
{
    size_t tcti_size = 0;
    TSS2_RC ret = TSS2_RC_SUCCESS;
    TSS2_TCTI_CONTEXT *ctx = NULL;

    const char *syms[] = {
        "TPMLIB_ChooseTPMVersion",
        "TPMLIB_RegisterCallbacks",
        "TPMLIB_GetState",
        "TPMLIB_MainInit",
        "TPMLIB_Process",
        "TPMLIB_SetState",
        "TPMLIB_Terminate",
    };

    /* test for every symbol syms[i] */
    for (size_t i = 0; i < ARRAY_LEN(syms); i++) {
        ret = Tss2_Tcti_Libtpms_Init(NULL, &tcti_size, NULL);
        assert_true(ret == TSS2_RC_SUCCESS);
        ctx = calloc(1, tcti_size);
        assert_non_null(ctx);

        expect_string(__wrap_dlopen, filename, "libtpms.so");
        expect_value(__wrap_dlopen, flags, RTLD_LAZY | RTLD_LOCAL);
        will_return(__wrap_dlopen, LIBTPMS_DL_HANDLE);

        /* successfully load all symbols up to (excluding) index i */
        for (size_t j = 0; j < i; j++) {
            expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
            expect_string(__wrap_dlsym, symbol, syms[j]);
            will_return(__wrap_dlsym, (void *) 1);
        }

        /* fail to load sym at index i */
        expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
        expect_string(__wrap_dlsym, symbol, syms[i]);
        will_return(__wrap_dlsym, NULL);

        /* cleanup */
        expect_value(__wrap_dlclose, handle, LIBTPMS_DL_HANDLE);
        will_return(__wrap_dlclose, 0);

        ret = Tss2_Tcti_Libtpms_Init(ctx, &tcti_size, NULL);
        assert_int_equal(ret, TSS2_TCTI_RC_GENERAL_FAILURE);

        free(ctx);
    }
}

/* When open fails to open the state file, we expect TSS2_TCTI_RC_IO_ERROR. */
static void
tcti_libtpms_init_state_open_fail_test(void **state)
{
    size_t tcti_size = 0;
    TSS2_RC ret = TSS2_RC_SUCCESS;
    TSS2_TCTI_CONTEXT *ctx = NULL;

    ret = Tss2_Tcti_Libtpms_Init(NULL, &tcti_size, NULL);
    assert_true(ret == TSS2_RC_SUCCESS);
    ctx = calloc(1, tcti_size);
    assert_non_null(ctx);

    expect_string(__wrap_dlopen, filename, "libtpms.so");
    expect_value(__wrap_dlopen, flags, RTLD_LAZY | RTLD_LOCAL);
    will_return(__wrap_dlopen, LIBTPMS_DL_HANDLE);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_ChooseTPMVersion");
    will_return(__wrap_dlsym, &TPMLIB_ChooseTPMVersion);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_RegisterCallbacks");
    will_return(__wrap_dlsym, &TPMLIB_RegisterCallbacks);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_GetState");
    will_return(__wrap_dlsym, &TPMLIB_GetState);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_MainInit");
    will_return(__wrap_dlsym, &TPMLIB_MainInit);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_Process");
    will_return(__wrap_dlsym, &TPMLIB_Process);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_SetState");
    will_return(__wrap_dlsym, &TPMLIB_SetState);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_Terminate");
    will_return(__wrap_dlsym, &TPMLIB_Terminate);

    /* fail open */
    expect_string(__wrap_open, pathname, STATEFILE_PATH);
    expect_value(__wrap_open, flags, O_RDWR | O_CREAT);
    expect_value(__wrap_open, mode, 0644);
    will_return(__wrap_open, -1);

    /* cleanup */
    expect_value(__wrap_dlclose, handle, LIBTPMS_DL_HANDLE);
    will_return(__wrap_dlclose, 0);

    ret = Tss2_Tcti_Libtpms_Init(ctx, &tcti_size, STATEFILE_PATH);
    assert_int_equal(ret, TSS2_TCTI_RC_IO_ERROR);

    free(ctx);
}

/* When lseek fails on the state file, we expect TSS2_TCTI_RC_IO_ERROR. */
static void
tcti_libtpms_init_state_lseek_fail_test(void **state)
{
    size_t tcti_size = 0;
    TSS2_RC ret = TSS2_RC_SUCCESS;
    TSS2_TCTI_CONTEXT *ctx = NULL;

    ret = Tss2_Tcti_Libtpms_Init(NULL, &tcti_size, NULL);
    assert_true(ret == TSS2_RC_SUCCESS);
    ctx = calloc(1, tcti_size);
    assert_non_null(ctx);

    expect_string(__wrap_dlopen, filename, "libtpms.so");
    expect_value(__wrap_dlopen, flags, RTLD_LAZY | RTLD_LOCAL);
    will_return(__wrap_dlopen, LIBTPMS_DL_HANDLE);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_ChooseTPMVersion");
    will_return(__wrap_dlsym, &TPMLIB_ChooseTPMVersion);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_RegisterCallbacks");
    will_return(__wrap_dlsym, &TPMLIB_RegisterCallbacks);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_GetState");
    will_return(__wrap_dlsym, &TPMLIB_GetState);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_MainInit");
    will_return(__wrap_dlsym, &TPMLIB_MainInit);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_Process");
    will_return(__wrap_dlsym, &TPMLIB_Process);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_SetState");
    will_return(__wrap_dlsym, &TPMLIB_SetState);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_Terminate");
    will_return(__wrap_dlsym, &TPMLIB_Terminate);

    expect_string(__wrap_open, pathname, STATEFILE_PATH);
    expect_value(__wrap_open, flags, O_RDWR | O_CREAT);
    expect_value(__wrap_open, mode, 0644);
    will_return(__wrap_open, STATEFILE_FD);

    /* fail to lseek */
    expect_value(__wrap_lseek, fd, STATEFILE_FD);
    expect_value(__wrap_lseek, offset, 0L);
    expect_value(__wrap_lseek, whence, SEEK_END);
    will_return(__wrap_lseek, -1);

    /* cleanup */
    expect_value(__wrap_close, fd, STATEFILE_FD);
    will_return(__wrap_close, 0);

    expect_value(__wrap_dlclose, handle, LIBTPMS_DL_HANDLE);
    will_return(__wrap_dlclose, 0);

    ret = Tss2_Tcti_Libtpms_Init(ctx, &tcti_size, STATEFILE_PATH);
    assert_int_equal(ret, TSS2_TCTI_RC_IO_ERROR);

    free(ctx);
}

/* When posix_fallocate fails on the state file, we expect TSS2_TCTI_RC_IO_ERROR. */
static void
tcti_libtpms_init_state_posix_fallocate_fail_test(void **state)
{
    size_t tcti_size = 0;
    TSS2_RC ret = TSS2_RC_SUCCESS;
    TSS2_TCTI_CONTEXT *ctx = NULL;

    ret = Tss2_Tcti_Libtpms_Init(NULL, &tcti_size, NULL);
    assert_true(ret == TSS2_RC_SUCCESS);
    ctx = calloc(1, tcti_size);
    assert_non_null(ctx);

    expect_string(__wrap_dlopen, filename, "libtpms.so");
    expect_value(__wrap_dlopen, flags, RTLD_LAZY | RTLD_LOCAL);
    will_return(__wrap_dlopen, LIBTPMS_DL_HANDLE);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_ChooseTPMVersion");
    will_return(__wrap_dlsym, &TPMLIB_ChooseTPMVersion);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_RegisterCallbacks");
    will_return(__wrap_dlsym, &TPMLIB_RegisterCallbacks);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_GetState");
    will_return(__wrap_dlsym, &TPMLIB_GetState);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_MainInit");
    will_return(__wrap_dlsym, &TPMLIB_MainInit);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_Process");
    will_return(__wrap_dlsym, &TPMLIB_Process);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_SetState");
    will_return(__wrap_dlsym, &TPMLIB_SetState);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_Terminate");
    will_return(__wrap_dlsym, &TPMLIB_Terminate);

    expect_string(__wrap_open, pathname, STATEFILE_PATH);
    expect_value(__wrap_open, flags, O_RDWR | O_CREAT);
    expect_value(__wrap_open, mode, 0644);
    will_return(__wrap_open, STATEFILE_FD);

    expect_value(__wrap_lseek, fd, STATEFILE_FD);
    expect_value(__wrap_lseek, offset, 0L);
    expect_value(__wrap_lseek, whence, SEEK_END);
    will_return(__wrap_lseek, S1_STATE_LEN);

    /* fail to posix_fallocate */
    expect_value(__wrap_posix_fallocate, fd, STATEFILE_FD);
    expect_value(__wrap_posix_fallocate, offset, 0);
    expect_value(__wrap_posix_fallocate, len, STATE_MMAP_CHUNK_LEN);
    will_return(__wrap_posix_fallocate, -1);

    /* cleanup */
    expect_value(__wrap_close, fd, STATEFILE_FD);
    will_return(__wrap_close, 0);

    expect_value(__wrap_dlclose, handle, LIBTPMS_DL_HANDLE);
    will_return(__wrap_dlclose, 0);

    ret = Tss2_Tcti_Libtpms_Init(ctx, &tcti_size, STATEFILE_PATH);
    assert_int_equal(ret, TSS2_TCTI_RC_IO_ERROR);

    free(ctx);
}

/* When mmap fails on the state file, we expect TSS2_TCTI_RC_IO_ERROR. */
static void
tcti_libtpms_init_state_mmap_fail_test(void **state)
{
    size_t tcti_size = 0;
    TSS2_RC ret = TSS2_RC_SUCCESS;
    TSS2_TCTI_CONTEXT *ctx = NULL;

    ret = Tss2_Tcti_Libtpms_Init(NULL, &tcti_size, NULL);
    assert_true(ret == TSS2_RC_SUCCESS);
    ctx = calloc(1, tcti_size);
    assert_non_null(ctx);

    expect_string(__wrap_dlopen, filename, "libtpms.so");
    expect_value(__wrap_dlopen, flags, RTLD_LAZY | RTLD_LOCAL);
    will_return(__wrap_dlopen, LIBTPMS_DL_HANDLE);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_ChooseTPMVersion");
    will_return(__wrap_dlsym, &TPMLIB_ChooseTPMVersion);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_RegisterCallbacks");
    will_return(__wrap_dlsym, &TPMLIB_RegisterCallbacks);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_GetState");
    will_return(__wrap_dlsym, &TPMLIB_GetState);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_MainInit");
    will_return(__wrap_dlsym, &TPMLIB_MainInit);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_Process");
    will_return(__wrap_dlsym, &TPMLIB_Process);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_SetState");
    will_return(__wrap_dlsym, &TPMLIB_SetState);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_Terminate");
    will_return(__wrap_dlsym, &TPMLIB_Terminate);

    expect_string(__wrap_open, pathname, STATEFILE_PATH);
    expect_value(__wrap_open, flags, O_RDWR | O_CREAT);
    expect_value(__wrap_open, mode, 0644);
    will_return(__wrap_open, STATEFILE_FD);

    expect_value(__wrap_lseek, fd, STATEFILE_FD);
    expect_value(__wrap_lseek, offset, 0L);
    expect_value(__wrap_lseek, whence, SEEK_END);
    will_return(__wrap_lseek, S1_STATE_LEN);

    expect_value(__wrap_posix_fallocate, fd, STATEFILE_FD);
    expect_value(__wrap_posix_fallocate, offset, 0);
    expect_value(__wrap_posix_fallocate, len, STATE_MMAP_CHUNK_LEN);
    will_return(__wrap_posix_fallocate, 0);

    /* fail to mmap */
    expect_value(__wrap_mmap, addr, NULL);
    expect_value(__wrap_mmap, len, STATE_MMAP_CHUNK_LEN);
    expect_value(__wrap_mmap, prot, PROT_READ | PROT_WRITE);
    expect_value(__wrap_mmap, flags, MAP_SHARED);
    expect_value(__wrap_mmap, fd, STATEFILE_FD);
    expect_value(__wrap_mmap, offset, 0);
    will_return(__wrap_mmap, MAP_FAILED);

    /* cleanup */
    expect_value(__wrap_close, fd, STATEFILE_FD);
    will_return(__wrap_close, 0);

    expect_value(__wrap_dlclose, handle, LIBTPMS_DL_HANDLE);
    will_return(__wrap_dlclose, 0);

    ret = Tss2_Tcti_Libtpms_Init(ctx, &tcti_size, STATEFILE_PATH);
    assert_int_equal(ret, TSS2_TCTI_RC_IO_ERROR);

    free(ctx);
}

/*
 * This is a utility function used by other tests to setup a TCTI context. It
 * effectively wraps the init / allocate / init pattern as well as priming the
 * mock functions necessary for a the successful call to
 * 'Tss2_Tcti_Libtpms_Init'.
 */
static TSS2_TCTI_CONTEXT*
tcti_libtpms_init_from_conf(const char *conf)
{
    size_t tcti_size = 0;
    TSS2_RC ret = TSS2_RC_SUCCESS;
    TSS2_TCTI_CONTEXT *ctx = NULL;
    TSS2_TCTI_LIBTPMS_CONTEXT *tcti_libtpms;

    memcpy(mmap_buf, S1_STATE, S1_STATE_LEN);

    fprintf(stderr, "%s: before first init\n", __func__);
    ret = Tss2_Tcti_Libtpms_Init(NULL, &tcti_size, NULL);
    assert_true(ret == TSS2_RC_SUCCESS);
    ctx = calloc(1, tcti_size);
    assert_non_null(ctx);

    fprintf(stderr, "%s: before second_init\n", __func__);
    expect_string(__wrap_dlopen, filename, "libtpms.so");
    expect_value(__wrap_dlopen, flags, RTLD_LAZY | RTLD_LOCAL);
    will_return(__wrap_dlopen, LIBTPMS_DL_HANDLE);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_ChooseTPMVersion");
    will_return(__wrap_dlsym, &TPMLIB_ChooseTPMVersion);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_RegisterCallbacks");
    will_return(__wrap_dlsym, &TPMLIB_RegisterCallbacks);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_GetState");
    will_return(__wrap_dlsym, &TPMLIB_GetState);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_MainInit");
    will_return(__wrap_dlsym, &TPMLIB_MainInit);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_Process");
    will_return(__wrap_dlsym, &TPMLIB_Process);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_SetState");
    will_return(__wrap_dlsym, &TPMLIB_SetState);

    expect_value(__wrap_dlsym, handle, LIBTPMS_DL_HANDLE);
    expect_string(__wrap_dlsym, symbol, "TPMLIB_Terminate");
    will_return(__wrap_dlsym, &TPMLIB_Terminate);

    if (conf != NULL) {
        expect_string(__wrap_open, pathname, STATEFILE_PATH);
        expect_value(__wrap_open, flags, O_RDWR | O_CREAT);
        expect_value(__wrap_open, mode, 0644);
        will_return(__wrap_open, STATEFILE_FD);

        expect_value(__wrap_lseek, fd, STATEFILE_FD);
        expect_value(__wrap_lseek, offset, 0L);
        expect_value(__wrap_lseek, whence, SEEK_END);
        will_return(__wrap_lseek, S1_STATE_LEN);

        expect_value(__wrap_posix_fallocate, fd, STATEFILE_FD);
        expect_value(__wrap_posix_fallocate, offset, 0);
        expect_value(__wrap_posix_fallocate, len, STATE_MMAP_CHUNK_LEN);
        will_return(__wrap_posix_fallocate, 0);

        expect_value(__wrap_mmap, addr, NULL);
        expect_value(__wrap_mmap, len, STATE_MMAP_CHUNK_LEN);
        expect_value(__wrap_mmap, prot, PROT_READ | PROT_WRITE);
        expect_value(__wrap_mmap, flags, MAP_SHARED);
        expect_value(__wrap_mmap, fd, STATEFILE_FD);
        expect_value(__wrap_mmap, offset, 0);
        will_return(__wrap_mmap, STATEFILE_MMAP);

        expect_value(__wrap_close, fd, STATEFILE_FD);
        will_return(__wrap_close, 0);

        expect_value(TPMLIB_SetState, st, TPMLIB_STATE_PERMANENT);
        expect_value(TPMLIB_SetState, buf, STATEFILE_MMAP + sizeof(uint32_t));
        expect_value(TPMLIB_SetState, buf_len, S1_PERMANENT_BUF_LEN);
        will_return(TPMLIB_SetState, 0);

        expect_value(TPMLIB_SetState, st, TPMLIB_STATE_VOLATILE);
        expect_value(TPMLIB_SetState, buf, STATEFILE_MMAP + sizeof(uint32_t) + S1_PERMANENT_BUF_LEN + sizeof(uint32_t));
        expect_value(TPMLIB_SetState, buf_len, S1_VOLATILE_BUF_LEN);
        will_return(TPMLIB_SetState, 0);
    }

    expect_value(TPMLIB_ChooseTPMVersion, ver, TPMLIB_TPM_VERSION_2);
    will_return(TPMLIB_ChooseTPMVersion, 0);
    will_return(TPMLIB_RegisterCallbacks, 0);
    will_return(TPMLIB_MainInit, 0);

    ret = Tss2_Tcti_Libtpms_Init(ctx, &tcti_size, conf);
    fprintf(stderr, "%s: after second init\n", __func__);
    assert_int_equal(ret, TSS2_RC_SUCCESS);

    tcti_libtpms = (TSS2_TCTI_LIBTPMS_CONTEXT*) ctx;
    if (conf != NULL) {
        assert_string_equal(tcti_libtpms->state_path, STATEFILE_PATH);
        assert_ptr_equal(tcti_libtpms->state_mmap, STATEFILE_MMAP);
        assert_int_equal(tcti_libtpms->state_mmap_len, STATE_MMAP_CHUNK_LEN);
        assert_int_equal(tcti_libtpms->state_len, S1_STATE_LEN);
        assert_memory_equal(tcti_libtpms->state_mmap, S1_STATE, S1_STATE_LEN);
    } else {
        assert_ptr_equal(tcti_libtpms->state_path, NULL);
        assert_ptr_equal(tcti_libtpms->state_mmap, NULL);
        assert_int_equal(tcti_libtpms->state_mmap_len, 0);
        assert_int_equal(tcti_libtpms->state_len, 0);
    }

    return ctx;
}

static void
tcti_libtpms_locality_success_test(void **state)
{
    TSS2_TCTI_CONTEXT *ctx = (TSS2_TCTI_CONTEXT*)*state;
    TSS2_RC rc;
    unsigned char cmd[] = {0x80, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x01, 0x44, 0x00, 0x00};
    unsigned char rsp[] = {0x80, 0x01, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00};

    rc = Tss2_Tcti_SetLocality(ctx, 4);
    assert_int_equal(rc, TSS2_RC_SUCCESS);

    expect_value(TPMLIB_Process, cmd, cmd);
    expect_value(TPMLIB_Process, cmd_len, sizeof(cmd));
    expect_value(TPMLIB_Process, locality, 4); /* expect locality 4 */
    will_return(TPMLIB_Process, rsp);
    will_return(TPMLIB_Process, sizeof(rsp));
    will_return(TPMLIB_Process, 123);

    rc = Tss2_Tcti_Transmit(ctx, sizeof(cmd), cmd);
    assert_int_equal(rc, TSS2_TCTI_RC_IO_ERROR);
}

static void
tcti_libtpms_transmit_success_test(void **state)
{
    TSS2_TCTI_CONTEXT *ctx = (TSS2_TCTI_CONTEXT*)*state;
    TSS2_TCTI_LIBTPMS_CONTEXT *tcti_libtpms = (TSS2_TCTI_LIBTPMS_CONTEXT*) ctx;
    TSS2_RC rc;
    unsigned char cmd[] = {0x80, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x01, 0x44, 0x00, 0x00};
    unsigned char rsp[] = {0x80, 0x01, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00};

    expect_value(TPMLIB_Process, cmd, cmd);
    expect_value(TPMLIB_Process, cmd_len, sizeof(cmd));
    expect_value(TPMLIB_Process, locality, 0);
    will_return(TPMLIB_Process, rsp);
    will_return(TPMLIB_Process, sizeof(rsp));
    will_return(TPMLIB_Process, 0);

    expect_value(TPMLIB_GetState, st, TPMLIB_STATE_PERMANENT);
    will_return(TPMLIB_GetState, S2_PERMANENT_BUF_LITERAL);
    will_return(TPMLIB_GetState, S2_PERMANENT_BUF_LEN);
    will_return(TPMLIB_GetState, 0);
    expect_value(TPMLIB_GetState, st, TPMLIB_STATE_VOLATILE);
    will_return(TPMLIB_GetState, S2_VOLATILE_BUF_LITERAL);
    will_return(TPMLIB_GetState, S2_VOLATILE_BUF_LEN);
    will_return(TPMLIB_GetState, 0);

    rc = Tss2_Tcti_Transmit(ctx, sizeof(cmd), cmd);
    assert_int_equal(rc, TSS2_RC_SUCCESS);

    assert_memory_equal(tcti_libtpms->response_buffer, rsp, sizeof(rsp));
    assert_int_equal(tcti_libtpms->response_buffer_len, sizeof(rsp));
    assert_int_equal(tcti_libtpms->response_len, sizeof(rsp));

    assert_string_equal(tcti_libtpms->state_path, STATEFILE_PATH);
    assert_ptr_equal(tcti_libtpms->state_mmap, STATEFILE_MMAP);
    assert_int_equal(tcti_libtpms->state_mmap_len, STATE_MMAP_CHUNK_LEN);
    assert_int_equal(tcti_libtpms->state_len, S2_STATE_LEN);
    assert_memory_equal(tcti_libtpms->state_mmap, S2_STATE, S2_STATE_LEN);
}

static void
tcti_libtpms_receive_success_test(void **state)
{
    TSS2_TCTI_CONTEXT *ctx = (TSS2_TCTI_CONTEXT*)*state;
    TSS2_TCTI_LIBTPMS_CONTEXT *tcti_libtpms = (TSS2_TCTI_LIBTPMS_CONTEXT*) ctx;
    TSS2_TCTI_COMMON_CONTEXT *tcti_common = tcti_common_context_cast(ctx);
    TSS2_RC rc;
    unsigned char rsp[] = {0x80, 0x01, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00};
    unsigned char rsp_out[sizeof(rsp)];
    size_t rsp_len_out = 0;

    tcti_common->state = TCTI_STATE_RECEIVE;
    tcti_libtpms->response_buffer = malloc(sizeof(rsp));
    assert_non_null(tcti_libtpms->response_buffer);
    memcpy(tcti_libtpms->response_buffer, rsp, sizeof(rsp));
    tcti_libtpms->response_buffer_len = sizeof(rsp);
    tcti_libtpms->response_len = sizeof(rsp);

    /* response get size */
    rc = Tss2_Tcti_Receive(ctx, &rsp_len_out, NULL, TSS2_TCTI_TIMEOUT_BLOCK);
    assert_int_equal(rc, TSS2_RC_SUCCESS);
    assert_int_equal(rsp_len_out, sizeof(rsp));
    assert_int_equal(tcti_common->state, TCTI_STATE_RECEIVE);

    /* get response */
    rc = Tss2_Tcti_Receive(ctx, &rsp_len_out, rsp_out, TSS2_TCTI_TIMEOUT_BLOCK);
    assert_int_equal(rc, TSS2_RC_SUCCESS);
    assert_memory_equal(rsp_out, rsp, rsp_len_out);
    assert_int_equal(rsp_len_out, sizeof(rsp));
    assert_int_equal(tcti_common->state, TCTI_STATE_TRANSMIT);

    assert_ptr_equal(tcti_libtpms->response_buffer, NULL);
    assert_int_equal(tcti_libtpms->response_buffer_len, 0);
    assert_int_equal(tcti_libtpms->response_len, 0);
}

static void
tcti_libtpms_remap_state_success_test(void **state)
{
    TSS2_TCTI_CONTEXT *ctx = (TSS2_TCTI_CONTEXT*)*state;
    TSS2_TCTI_LIBTPMS_CONTEXT *tcti_libtpms = (TSS2_TCTI_LIBTPMS_CONTEXT*) ctx;
    TSS2_RC rc;
    unsigned char cmd[] = {0x80, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x01, 0x44, 0x00, 0x00};
    unsigned char rsp[] = {0x80, 0x01, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00};

    expect_value(TPMLIB_Process, cmd, cmd);
    expect_value(TPMLIB_Process, cmd_len, sizeof(cmd));
    expect_value(TPMLIB_Process, locality, 0);
    will_return(TPMLIB_Process, rsp);
    will_return(TPMLIB_Process, sizeof(rsp));
    will_return(TPMLIB_Process, 0);

    expect_value(TPMLIB_GetState, st, TPMLIB_STATE_PERMANENT);
    will_return(TPMLIB_GetState, S3_PERMANENT_BUF_LITERAL);
    will_return(TPMLIB_GetState, S3_PERMANENT_BUF_LEN);
    will_return(TPMLIB_GetState, 0);
    expect_value(TPMLIB_GetState, st, TPMLIB_STATE_VOLATILE);
    will_return(TPMLIB_GetState, S3_VOLATILE_BUF_LITERAL);
    will_return(TPMLIB_GetState, S3_VOLATILE_BUF_LEN);
    will_return(TPMLIB_GetState, 0);

    expect_value(__wrap_mremap, old_address, STATEFILE_MMAP);
    expect_value(__wrap_mremap, old_size, STATE_MMAP_CHUNK_LEN);
    expect_value(__wrap_mremap, new_size, STATE_MMAP_CHUNK_LEN * 2);
    expect_value(__wrap_mremap, flags, MREMAP_MAYMOVE);
    will_return(__wrap_mremap, STATEFILE_MMAP_NEW);

    expect_string(__wrap_open, pathname, STATEFILE_PATH);
    expect_value(__wrap_open, flags, O_RDWR | O_CREAT);
    expect_value(__wrap_open, mode, 0644);
    will_return(__wrap_open, STATEFILE_FD);

    expect_value(__wrap_posix_fallocate, fd, STATEFILE_FD);
    expect_value(__wrap_posix_fallocate, offset, 0);
    expect_value(__wrap_posix_fallocate, len, STATE_MMAP_CHUNK_LEN * 2);
    will_return(__wrap_posix_fallocate, 0);

    expect_value(__wrap_close, fd, STATEFILE_FD);
    will_return(__wrap_close, 0);

    rc = Tss2_Tcti_Transmit(ctx, sizeof(cmd), cmd);
    assert_int_equal(rc, TSS2_RC_SUCCESS);

    assert_string_equal(tcti_libtpms->state_path, STATEFILE_PATH);
    assert_ptr_equal(tcti_libtpms->state_mmap, STATEFILE_MMAP_NEW);
    assert_int_equal(tcti_libtpms->state_mmap_len, STATE_MMAP_CHUNK_LEN * 2);
    assert_int_equal(tcti_libtpms->state_len, S3_STATE_LEN);
    assert_memory_equal(tcti_libtpms->state_mmap, S3_STATE, S3_STATE_LEN);
}

/* Have mremap fail during state remap (transmit), expect TSS2_TCTI_RC_IO_ERROR */
static void
tcti_libtpms_remap_state_mremap_fail_test(void **state)
{
    TSS2_TCTI_CONTEXT *ctx = (TSS2_TCTI_CONTEXT*)*state;
    TSS2_TCTI_LIBTPMS_CONTEXT *tcti_libtpms = (TSS2_TCTI_LIBTPMS_CONTEXT*) ctx;
    TSS2_RC rc;
    unsigned char cmd[] = {0x80, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x01, 0x44, 0x00, 0x00};
    unsigned char rsp[] = {0x80, 0x01, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00};

    expect_value(TPMLIB_Process, cmd, cmd);
    expect_value(TPMLIB_Process, cmd_len, sizeof(cmd));
    expect_value(TPMLIB_Process, locality, 0);
    will_return(TPMLIB_Process, rsp);
    will_return(TPMLIB_Process, sizeof(rsp));
    will_return(TPMLIB_Process, 0);

    expect_value(TPMLIB_GetState, st, TPMLIB_STATE_PERMANENT);
    will_return(TPMLIB_GetState, S3_PERMANENT_BUF_LITERAL);
    will_return(TPMLIB_GetState, S3_PERMANENT_BUF_LEN);
    will_return(TPMLIB_GetState, 0);
    expect_value(TPMLIB_GetState, st, TPMLIB_STATE_VOLATILE);
    will_return(TPMLIB_GetState, S3_VOLATILE_BUF_LITERAL);
    will_return(TPMLIB_GetState, S3_VOLATILE_BUF_LEN);
    will_return(TPMLIB_GetState, 0);

    expect_value(__wrap_mremap, old_address, STATEFILE_MMAP);
    expect_value(__wrap_mremap, old_size, STATE_MMAP_CHUNK_LEN);
    expect_value(__wrap_mremap, new_size, STATE_MMAP_CHUNK_LEN * 2);
    expect_value(__wrap_mremap, flags, MREMAP_MAYMOVE);
    will_return(__wrap_mremap, MAP_FAILED);

    rc = Tss2_Tcti_Transmit(ctx, sizeof(cmd), cmd);
    assert_int_equal(rc, TSS2_TCTI_RC_IO_ERROR);

    /* reallocating memory (and thus storing) failed for S3, we're still at S1 */
    assert_string_equal(tcti_libtpms->state_path, STATEFILE_PATH);
    assert_ptr_equal(tcti_libtpms->state_mmap, STATEFILE_MMAP);
    assert_int_equal(tcti_libtpms->state_mmap_len, STATE_MMAP_CHUNK_LEN);
    assert_int_equal(tcti_libtpms->state_len, S1_STATE_LEN);
    assert_memory_equal(tcti_libtpms->state_mmap, S1_STATE, S1_STATE_LEN);
}

/* Have open fail during state remap (transmit), expect TSS2_TCTI_RC_IO_ERROR */
static void
tcti_libtpms_remap_state_posix_fallocate_fail_test(void **state)
{
    TSS2_TCTI_CONTEXT *ctx = (TSS2_TCTI_CONTEXT*)*state;
    TSS2_TCTI_LIBTPMS_CONTEXT *tcti_libtpms = (TSS2_TCTI_LIBTPMS_CONTEXT*) ctx;
    TSS2_RC rc;
    unsigned char cmd[] = {0x80, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x01, 0x44, 0x00, 0x00};
    unsigned char rsp[] = {0x80, 0x01, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00};

    expect_value(TPMLIB_Process, cmd, cmd);
    expect_value(TPMLIB_Process, cmd_len, sizeof(cmd));
    expect_value(TPMLIB_Process, locality, 0);
    will_return(TPMLIB_Process, rsp);
    will_return(TPMLIB_Process, sizeof(rsp));
    will_return(TPMLIB_Process, 0);

    expect_value(TPMLIB_GetState, st, TPMLIB_STATE_PERMANENT);
    will_return(TPMLIB_GetState, S3_PERMANENT_BUF_LITERAL);
    will_return(TPMLIB_GetState, S3_PERMANENT_BUF_LEN);
    will_return(TPMLIB_GetState, 0);
    expect_value(TPMLIB_GetState, st, TPMLIB_STATE_VOLATILE);
    will_return(TPMLIB_GetState, S3_VOLATILE_BUF_LITERAL);
    will_return(TPMLIB_GetState, S3_VOLATILE_BUF_LEN);
    will_return(TPMLIB_GetState, 0);

    expect_value(__wrap_mremap, old_address, STATEFILE_MMAP);
    expect_value(__wrap_mremap, old_size, STATE_MMAP_CHUNK_LEN);
    expect_value(__wrap_mremap, new_size, STATE_MMAP_CHUNK_LEN * 2);
    expect_value(__wrap_mremap, flags, MREMAP_MAYMOVE);
    will_return(__wrap_mremap, STATEFILE_MMAP_NEW);

    expect_string(__wrap_open, pathname, STATEFILE_PATH);
    expect_value(__wrap_open, flags, O_RDWR | O_CREAT);
    expect_value(__wrap_open, mode, 0644);
    will_return(__wrap_open, STATEFILE_FD);

    expect_value(__wrap_posix_fallocate, fd, STATEFILE_FD);
    expect_value(__wrap_posix_fallocate, offset, 0);
    expect_value(__wrap_posix_fallocate, len, STATE_MMAP_CHUNK_LEN * 2);
    will_return(__wrap_posix_fallocate, -5);

    /* cleanup */
    expect_value(__wrap_close, fd, STATEFILE_FD);
    will_return(__wrap_close, 0);

    rc = Tss2_Tcti_Transmit(ctx, sizeof(cmd), cmd);
    assert_int_equal(rc, TSS2_TCTI_RC_IO_ERROR);

    /* storing failed for S3, but we could allocate more memory, still S1 */
    assert_string_equal(tcti_libtpms->state_path, STATEFILE_PATH);
    assert_ptr_equal(tcti_libtpms->state_mmap, STATEFILE_MMAP_NEW);
    assert_int_equal(tcti_libtpms->state_mmap_len, STATE_MMAP_CHUNK_LEN * 2);
    assert_int_equal(tcti_libtpms->state_len, S1_STATE_LEN);
    assert_memory_equal(tcti_libtpms->state_mmap, S1_STATE, S1_STATE_LEN);
}

/* Have open fail during state remap (transmit), expect TSS2_TCTI_RC_IO_ERROR */
static void
tcti_libtpms_no_statefile_success_test(void **state)
{
    TSS2_TCTI_CONTEXT *ctx = (TSS2_TCTI_CONTEXT*)*state;
    TSS2_TCTI_LIBTPMS_CONTEXT *tcti_libtpms = (TSS2_TCTI_LIBTPMS_CONTEXT*) ctx;
    TSS2_TCTI_COMMON_CONTEXT *tcti_common = tcti_common_context_cast(ctx);
    TSS2_RC rc;
    unsigned char cmd[] = {0x80, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x01, 0x44, 0x00, 0x00};
    unsigned char rsp[] = {0x80, 0x01, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00};
    unsigned char rsp_out[sizeof(rsp)];
    size_t rsp_len_out = sizeof(rsp);

    expect_value(TPMLIB_Process, cmd, cmd);
    expect_value(TPMLIB_Process, cmd_len, sizeof(cmd));
    expect_value(TPMLIB_Process, locality, 0);
    will_return(TPMLIB_Process, rsp);
    will_return(TPMLIB_Process, sizeof(rsp));
    will_return(TPMLIB_Process, 0);

    rc = Tss2_Tcti_Transmit(ctx, sizeof(cmd), cmd);
    assert_int_equal(rc, TPM2_RC_SUCCESS);

    /* expect no state */
    assert_ptr_equal(tcti_libtpms->state_path, NULL);
    assert_ptr_equal(tcti_libtpms->state_mmap, NULL);
    assert_int_equal(tcti_libtpms->state_mmap_len, 0);
    assert_int_equal(tcti_libtpms->state_len, 0);

    rc = Tss2_Tcti_Receive(ctx, &rsp_len_out, rsp_out, TSS2_TCTI_TIMEOUT_BLOCK);
    assert_int_equal(rc, TSS2_RC_SUCCESS);

    assert_memory_equal(rsp_out, rsp, rsp_len_out);
    assert_int_equal(rsp_len_out, sizeof(rsp));
    assert_int_equal(tcti_common->state, TCTI_STATE_TRANSMIT);

    assert_ptr_equal(tcti_libtpms->response_buffer, NULL);
    assert_int_equal(tcti_libtpms->response_buffer_len, 0);
    assert_int_equal(tcti_libtpms->response_len, 0);

    /* expect no state */
    assert_ptr_equal(tcti_libtpms->state_path, NULL);
    assert_ptr_equal(tcti_libtpms->state_mmap, NULL);
    assert_int_equal(tcti_libtpms->state_mmap_len, 0);
    assert_int_equal(tcti_libtpms->state_len, 0);
}

/*
 * This is a utility function to setup the "default" TCTI context.
 */
static int
tcti_libtpms_setup(void **state)
{
    fprintf(stderr, "%s: before tcti_libtpms_init_from_conf\n", __func__);
    *state = tcti_libtpms_init_from_conf(STATEFILE_PATH);
    fprintf(stderr, "%s: done\n", __func__);
    return 0;
}
/*
 * This is a utility function to setup the "default" TCTI context.
 */
static int
tcti_libtpms_setup_no_statefile(void **state)
{
    fprintf(stderr, "%s: before tcti_libtpms_init_from_conf\n", __func__);
    *state = tcti_libtpms_init_from_conf(NULL);
    fprintf(stderr, "%s: done\n", __func__);
    return 0;
}
/*
 * This is a utility function to teardown a TCTI context allocated by the
 * tcti_libtpms_setup function. Will expect no state file.
 */
static int
tcti_libtpms_teardown_no_statefile(void **state)
{
    TSS2_TCTI_CONTEXT *ctx = (TSS2_TCTI_CONTEXT*) *state;

    expect_value(__wrap_dlclose, handle, LIBTPMS_DL_HANDLE);
    will_return(__wrap_dlclose, 0);

    Tss2_Tcti_Finalize(ctx);
    free(ctx);
    return 0;
}
/*
 * This is a utility function to teardown a TCTI context allocated by the
 * tcti_libtpms_setup function. Will expect libtpms state 1.
 */
static int
tcti_libtpms_teardown_s1(void **state)
{
    TSS2_TCTI_CONTEXT *ctx = (TSS2_TCTI_CONTEXT*) *state;
    TSS2_TCTI_LIBTPMS_CONTEXT *tcti_libtpms = (TSS2_TCTI_LIBTPMS_CONTEXT*) ctx;

    expect_value(__wrap_dlclose, handle, LIBTPMS_DL_HANDLE);
    will_return(__wrap_dlclose, 0);

    if (tcti_libtpms->state_mmap != NULL) {
        expect_value(__wrap_munmap, addr, tcti_libtpms->state_mmap);
        expect_value(__wrap_munmap, len, tcti_libtpms->state_mmap_len);
        will_return(__wrap_munmap, 0);
    }

    expect_string(__wrap_truncate, path, STATEFILE_PATH);
    expect_value(__wrap_truncate, length, S1_STATE_LEN);
    will_return(__wrap_truncate, 0);

    Tss2_Tcti_Finalize(ctx);
    free(ctx);
    return 0;
}
/*
 * This is a utility function to teardown a TCTI context allocated by the
 * tcti_libtpms_setup function. Will expect libtpms state 2.
 */
static int
tcti_libtpms_teardown_s2(void **state)
{
    TSS2_TCTI_CONTEXT *ctx = (TSS2_TCTI_CONTEXT*) *state;

    expect_value(__wrap_dlclose, handle, LIBTPMS_DL_HANDLE);
    will_return(__wrap_dlclose, 0);

    expect_value(__wrap_munmap, addr, STATEFILE_MMAP);
    expect_value(__wrap_munmap, len, STATE_MMAP_CHUNK_LEN);
    will_return(__wrap_munmap, 0);

    expect_string(__wrap_truncate, path, STATEFILE_PATH);
    expect_value(__wrap_truncate, length, S2_STATE_LEN);
    will_return(__wrap_truncate, 0);

    Tss2_Tcti_Finalize(ctx);
    free(ctx);
    return 0;
}
/*
 * This is a utility function to teardown a TCTI context allocated by the
 * tcti_libtpms_setup function. Will expect libtpms state 3.
 */
static int
tcti_libtpms_teardown_s3(void **state)
{
    TSS2_TCTI_CONTEXT *ctx = (TSS2_TCTI_CONTEXT*) *state;

    expect_value(__wrap_dlclose, handle, LIBTPMS_DL_HANDLE);
    will_return(__wrap_dlclose, 0);

    expect_value(__wrap_munmap, addr, STATEFILE_MMAP_NEW);
    expect_value(__wrap_munmap, len, STATE_MMAP_CHUNK_LEN * 2);
    will_return(__wrap_munmap, 0);

    expect_string(__wrap_truncate, path, STATEFILE_PATH);
    expect_value(__wrap_truncate, length, S3_STATE_LEN);
    will_return(__wrap_truncate, 0);

    Tss2_Tcti_Finalize(ctx);
    free(ctx);
    return 0;
}

int
main(int   argc,
     char *argv[])
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(tcti_libtpms_init_all_null_test),
        cmocka_unit_test(tcti_libtpms_init_dlopen_fail_test),
        cmocka_unit_test(tcti_libtpms_init_dlsym_fail_test),
        cmocka_unit_test(tcti_libtpms_init_state_open_fail_test),
        cmocka_unit_test(tcti_libtpms_init_state_lseek_fail_test),
        cmocka_unit_test(tcti_libtpms_init_state_posix_fallocate_fail_test),
        cmocka_unit_test(tcti_libtpms_init_state_mmap_fail_test),
        cmocka_unit_test_setup_teardown(tcti_libtpms_no_statefile_success_test,
                                        tcti_libtpms_setup_no_statefile,
                                        tcti_libtpms_teardown_no_statefile),
        cmocka_unit_test_setup_teardown(tcti_libtpms_receive_success_test,
                                        tcti_libtpms_setup,
                                        tcti_libtpms_teardown_s1),
        cmocka_unit_test_setup_teardown(tcti_libtpms_locality_success_test,
                                        tcti_libtpms_setup,
                                        tcti_libtpms_teardown_s1),
        cmocka_unit_test_setup_teardown(tcti_libtpms_transmit_success_test,
                                        tcti_libtpms_setup,
                                        tcti_libtpms_teardown_s2),
        cmocka_unit_test_setup_teardown(tcti_libtpms_remap_state_success_test,
                                        tcti_libtpms_setup,
                                        tcti_libtpms_teardown_s3),
        cmocka_unit_test_setup_teardown(tcti_libtpms_remap_state_mremap_fail_test,
                                        tcti_libtpms_setup,
                                        tcti_libtpms_teardown_s1),
        cmocka_unit_test_setup_teardown(tcti_libtpms_remap_state_posix_fallocate_fail_test,
                                        tcti_libtpms_setup,
                                        tcti_libtpms_teardown_s1),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
