/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright 2020 Fraunhofer SIT. All rights reserved.
 */
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#include "tss2_tcti.h"
#include "tss2_tcti_spi.h"
#include "tss2_mu.h"
#include "tcti-common.h"
#include "tcti-spi.h"
#include "util/io.h"
#define LOGMODULE tcti
#include "util/log.h"

static inline void spi_tpm_delay_ms(TSS2_TCTI_SPI_CONTEXT* ctx, int milliseconds)
{
    // Sleep a specified amount of milliseconds
    ctx->platform.sleep_ms(ctx->platform.user_data, milliseconds);
}

static inline void spi_tpm_start_timeout(TSS2_TCTI_SPI_CONTEXT* ctx, int milliseconds)
{
    // Start a timeout timer with the specified amount of milliseconds
    ctx->platform.start_timeout(ctx->platform.user_data, milliseconds);
}

static inline bool spi_tpm_timeout_expired(TSS2_TCTI_SPI_CONTEXT* ctx)
{
    // Check if the last started tiemout expired
    return ctx->platform.timeout_expired(ctx->platform.user_data);
}

static inline TSS2_RC spi_tpm_spi_acquire(TSS2_TCTI_SPI_CONTEXT* ctx)
{
    // Reserve SPI bus until transaction is over and keep pulling CS
    return ctx->platform.spi_acquire(ctx->platform.user_data);
} 

static inline TSS2_RC spi_tpm_spi_release(TSS2_TCTI_SPI_CONTEXT* ctx)
{
    // Release SPI bus and release CS
    return ctx->platform.spi_release(ctx->platform.user_data);
} 

static inline TSS2_RC spi_tpm_spi_transfer(TSS2_TCTI_SPI_CONTEXT* ctx, const void *data_out, void *data_in, size_t cnt)
{
    // Perform SPI transaction with cnt bytes
    return ctx->platform.spi_transfer(ctx->platform.user_data, data_out, data_in, cnt);
}

static inline void spi_tpm_platform_finalize(TSS2_TCTI_SPI_CONTEXT* ctx)
{
    // Free user_data and resources inside
    ctx->platform.finalize(ctx->platform.user_data);
}

static inline uint32_t spi_tpm_read_be32(const void *src)
{
    const uint8_t *s = src;
    return (((uint32_t)s[0]) << 24) | (((uint32_t)s[1]) << 16) | (((uint32_t)s[2]) << 8) | (((uint32_t)s[3]) << 0);
}

static TSS2_RC spi_tpm_start_transaction(TSS2_TCTI_SPI_CONTEXT* ctx, enum TCTI_SPI_REGISTER_ACCESS_TYPE access, size_t bytes, uint32_t addr)
{
    TSS2_RC rc;

    // Build spi header
    uint8_t header[4];
    
    // Transaction type and transfer size
    header[0] = ((access == TCTI_SPI_REGISTER_READ) ? 0x80 : 0x00) | (bytes - 1);
    
    // TPM register address
    header[1] = addr >> 16 & 0xff;
    header[2] = addr >> 8  & 0xff;
    header[3] = addr >> 0  & 0xff;

    // Reserve SPI bus until transaction is over and keep pulling CS
    rc = spi_tpm_spi_acquire(ctx);
    if (rc != TSS2_RC_SUCCESS) {
        return TSS2_TCTI_RC_IO_ERROR;
    }

    // Send header
    uint8_t header_response[4];
    rc = spi_tpm_spi_transfer(ctx, header, header_response, 4);
    if (rc != TSS2_RC_SUCCESS) {
        return TSS2_TCTI_RC_IO_ERROR;
    }

    // Wait until the TPM exits the wait state and sends a 1 bit
    uint8_t byte;
    
    // The 1 bit is often already set in the last byte of the transaction header
    byte = header_response[3];
    if (byte & 1) {
        return TSS2_RC_SUCCESS;
    }
    
    // With most current TPMs there shouldn't be any more waitstate at all, but according to
    // the spec, we have to retry until there is no more waitstate inserted. So we try again
    // a few times by reading only one byte at a time and waiting in between.
    uint8_t zero = 0;
    for (int retries = 256; retries > 0; retries--) {
        rc = spi_tpm_spi_transfer(ctx, &zero, &byte, 1);
        if (rc != TSS2_RC_SUCCESS) {
            return TSS2_TCTI_RC_IO_ERROR;
        }
        if (byte & 1) {
            return TSS2_RC_SUCCESS;
        }
        spi_tpm_delay_ms(ctx, 1);
    }

    // The TPM did not exit the wait state in time
    return TSS2_TCTI_RC_IO_ERROR;
}

static TSS2_RC spi_tpm_end_transaction(TSS2_TCTI_SPI_CONTEXT* ctx)
{
    // Release CS (ends the transaction) and release the bus for other devices
    return spi_tpm_spi_release(ctx);
}

static void spi_tpm_log_register_access(enum TCTI_SPI_REGISTER_ACCESS_TYPE access, uint32_t reg_number, const void *buffer, size_t cnt, char* err) {
    // Print register access debug information
    char* access_str = (access == TCTI_SPI_REGISTER_READ) ? "READ" : "WRITE";
    if (err != NULL) {
        LOG_ERROR("%s register %#02x (%zu bytes) %s", access_str, reg_number, cnt, err);
    } else {
        LOGBLOB_TRACE(buffer, cnt, "%s register %#02x (%zu bytes)", access_str, reg_number, cnt);
    }
}

static TSS2_RC spi_tpm_read_reg(TSS2_TCTI_SPI_CONTEXT* ctx, uint32_t reg_number, void *buffer, size_t cnt)
{
    TSS2_RC rc;
    enum TCTI_SPI_REGISTER_ACCESS_TYPE access = TCTI_SPI_REGISTER_READ;
    
    // Check maximum register transfer size is 64 byte
    assert(cnt <= 64);

    // Start read transaction
    rc = spi_tpm_start_transaction(ctx, access, cnt, reg_number);
    if (rc != TSS2_RC_SUCCESS) {
        spi_tpm_log_register_access(access, reg_number, NULL, cnt, "failed in transaction start");
        spi_tpm_end_transaction(ctx);
        return TSS2_TCTI_RC_IO_ERROR;
    }
    // Read register
    rc = spi_tpm_spi_transfer(ctx, NULL, buffer, cnt);
    if (rc != TSS2_RC_SUCCESS) {
        spi_tpm_log_register_access(access, reg_number, NULL, cnt, "failed in transfer");
        spi_tpm_end_transaction(ctx);
        return TSS2_TCTI_RC_IO_ERROR;
    }
    // End transaction
    rc = spi_tpm_end_transaction(ctx);
    if (rc != TSS2_RC_SUCCESS) {
        spi_tpm_log_register_access(access, reg_number, NULL, cnt, "failed ending the transaction");
        return TSS2_TCTI_RC_IO_ERROR;
    }

    // Print debug information and return success
    spi_tpm_log_register_access(access, reg_number, buffer, cnt, NULL);
    return TSS2_RC_SUCCESS;
}

static TSS2_RC spi_tpm_write_reg(TSS2_TCTI_SPI_CONTEXT* ctx, uint32_t reg_number, const void *buffer, size_t cnt)
{
    TSS2_RC rc;
    enum TCTI_SPI_REGISTER_ACCESS_TYPE access = TCTI_SPI_REGISTER_WRITE;
    
    // Check maximum register transfer size is 64 byte
    assert(cnt <= 64);

    // Start write transaction
    rc = spi_tpm_start_transaction(ctx, access, cnt, reg_number);
    if (rc != TSS2_RC_SUCCESS) {
        spi_tpm_end_transaction(ctx);
        spi_tpm_log_register_access(access, reg_number, buffer, cnt, "failed in transaction start");
        return TSS2_TCTI_RC_IO_ERROR;
    }
    // Write register
    rc = spi_tpm_spi_transfer(ctx, buffer, NULL, cnt);
    if (rc != TSS2_RC_SUCCESS) {
        spi_tpm_end_transaction(ctx);
        spi_tpm_log_register_access(access, reg_number, buffer, cnt, "failed in transfer");
        return TSS2_TCTI_RC_IO_ERROR;
    }
    // End transaction
    rc = spi_tpm_end_transaction(ctx);
    if (rc != TSS2_RC_SUCCESS) {
        spi_tpm_log_register_access(access, reg_number, NULL, cnt, "failed ending the transaction");
        return TSS2_TCTI_RC_IO_ERROR;
    }

    // Print debug information and return success
    spi_tpm_log_register_access(access, reg_number, buffer, cnt, NULL);
    return TSS2_RC_SUCCESS;
}

static uint32_t spi_tpm_read_sts_reg(TSS2_TCTI_SPI_CONTEXT* ctx)
{
    uint32_t status = 0;
    spi_tpm_read_reg(ctx, TCTI_SPI_TPM_STS_REG, &status, sizeof(status));
    return status;
}

static void spi_tpm_write_sts_reg(TSS2_TCTI_SPI_CONTEXT* ctx, uint32_t status)
{
    spi_tpm_write_reg(ctx, TCTI_SPI_TPM_STS_REG, &status, sizeof(status));
}

static uint32_t spi_tpm_get_burst_count(TSS2_TCTI_SPI_CONTEXT* ctx)
{
    uint32_t status = spi_tpm_read_sts_reg(ctx);
    return (status & TCTI_SPI_TPM_STS_BURST_COUNT_MASK) >> TCTI_SPI_TPM_STS_BURST_COUNT_SHIFT;
}

static uint8_t spi_tpm_read_access_reg(TSS2_TCTI_SPI_CONTEXT* ctx)
{
    uint8_t access = 0;
    spi_tpm_read_reg(ctx, TCTI_SPI_TPM_ACCESS_REG, &access, sizeof(access));
    return access;
}

static void spi_tpm_write_access_reg(TSS2_TCTI_SPI_CONTEXT* ctx, uint8_t access_bit)
{
    // Writes to access register can set only 1 bit at a time
    assert (!(access_bit & (access_bit - 1)));
    spi_tpm_write_reg(ctx, TCTI_SPI_TPM_ACCESS_REG, &access_bit, sizeof(access_bit));
}

static TSS2_RC spi_tpm_claim_locality(TSS2_TCTI_SPI_CONTEXT* ctx)
{
    uint8_t access;
    access = spi_tpm_read_access_reg(ctx);
    
    // Check if locality 0 is busy
    if (access & TCTI_SPI_TPM_ACCESS_ACTIVE_LOCALITY) {
        LOG_ERROR("Locality 0 is busy, status: %#x", access);
        return TSS2_TCTI_RC_IO_ERROR;
    }

    // Request locality 0
    spi_tpm_write_access_reg(ctx, TCTI_SPI_TPM_ACCESS_REQUEST_USE);
    access = spi_tpm_read_access_reg(ctx);
    if (access & (TCTI_SPI_TPM_ACCESS_VALID | TCTI_SPI_TPM_ACCESS_ACTIVE_LOCALITY)) {
        LOG_DEBUG("Claimed locality 0");
        return TSS2_RC_SUCCESS;
    }

    LOG_ERROR("Failed to claim locality 0, status: %#x", access);
    return TSS2_TCTI_RC_IO_ERROR;
}

static TSS2_RC spi_tpm_wait_for_status(TSS2_TCTI_SPI_CONTEXT* ctx, uint32_t status_mask, uint32_t status_expected, int32_t timeout)
{
    uint32_t status;
    bool blocking = (timeout == TSS2_TCTI_TIMEOUT_BLOCK);
    if (!blocking) {
        spi_tpm_start_timeout(ctx, timeout);
    }
    
    // Wait for the expected status with or without timeout
    do {
        status = spi_tpm_read_sts_reg(ctx);
        // Return success on expected status
        if ((status & status_mask) == status_expected) {
            return TSS2_RC_SUCCESS;
        }
        // Delay next poll by 8ms to avoid spamming the TPM
        spi_tpm_delay_ms(ctx, 8);
    } while (blocking || !spi_tpm_timeout_expired(ctx));

    // Timed out
    return TSS2_TCTI_RC_TRY_AGAIN;
}

static inline size_t spi_tpm_size_t_min(size_t a, size_t b) {
    if (a < b) {
        return a;
    }
    return b;
}

static void spi_tpm_fifo_transfer(TSS2_TCTI_SPI_CONTEXT* ctx, uint8_t* transfer_buffer, size_t transfer_size, enum TCTI_SPI_FIFO_TRANSFER_DIRECTION direction)
{
    size_t transaction_size;
    size_t burst_count;
    size_t handled_so_far = 0;

    do {
        do {
            // Can be zero when TPM is busy
            burst_count = spi_tpm_get_burst_count(ctx);
        } while (!burst_count);

        transaction_size = transfer_size - handled_so_far;
        transaction_size = spi_tpm_size_t_min(transaction_size, burst_count);
        transaction_size = spi_tpm_size_t_min(transaction_size, 64);

        if (direction == TCTI_SPI_FIFO_RECEIVE){
            spi_tpm_read_reg(ctx, TCTI_SPI_TPM_DATA_FIFO_REG, (void*)(transfer_buffer + handled_so_far), transaction_size);
        } else {
            spi_tpm_write_reg(ctx, TCTI_SPI_TPM_DATA_FIFO_REG, (const void*)(transfer_buffer + handled_so_far), transaction_size);
        }

        handled_so_far += transaction_size;

    } while (handled_so_far != transfer_size);
}

/*
 * This function wraps the "up-cast" of the opaque TCTI context type to the
 * type for the device TCTI context. The only safe-guard we have to ensure
 * this operation is possible is the magic number for the device TCTI context.
 * If passed a NULL context, or the magic number check fails, this function
 * will return NULL.
 */
TSS2_TCTI_SPI_CONTEXT* tcti_spi_context_cast (TSS2_TCTI_CONTEXT *tcti_ctx)
{
    if (tcti_ctx != NULL && TSS2_TCTI_MAGIC (tcti_ctx) == TCTI_SPI_MAGIC) {
        return (TSS2_TCTI_SPI_CONTEXT*)tcti_ctx;
    }
    return NULL;
}

/*
 * This function down-casts the device TCTI context to the common context
 * defined in the tcti-common module.
 */
TSS2_TCTI_COMMON_CONTEXT* tcti_spi_down_cast (TSS2_TCTI_SPI_CONTEXT *tcti_spi)
{
    if (tcti_spi == NULL) {
        return NULL;
    }
    return &tcti_spi->common;
}

TSS2_RC tcti_spi_receive (TSS2_TCTI_CONTEXT* tcti_context, size_t *response_size, unsigned char *response_buffer, int32_t timeout)
{
    TSS2_RC rc;
    TSS2_TCTI_SPI_CONTEXT* tcti_spi = tcti_spi_context_cast (tcti_context);
    TSS2_TCTI_COMMON_CONTEXT* tcti_common = tcti_spi_down_cast (tcti_spi);
    
    if (tcti_spi == NULL) {
        return TSS2_TCTI_RC_BAD_CONTEXT;
    }
    
    rc = tcti_common_receive_checks (tcti_common, response_size, TCTI_SPI_MAGIC);
    if (rc != TSS2_RC_SUCCESS) {
        return rc;
    }

    // Use ctx as a shorthand for tcti_spi
    TSS2_TCTI_SPI_CONTEXT* ctx = tcti_spi;

    // Expected status bits for valid status and data availabe
    uint32_t expected_status_bits = TCTI_SPI_TPM_STS_VALID | TCTI_SPI_TPM_STS_DATA_AVAIL;

    // Check if we already have received the header
    if (tcti_common->header.size == 0) {
        // Wait for response to be ready
        rc = spi_tpm_wait_for_status(ctx, expected_status_bits, expected_status_bits, timeout);
        if (rc != TSS2_RC_SUCCESS) {
            LOG_ERROR("Failed waiting for status");
            // Return rc from wait_for_status(). May be TRY_AGAIN after timeout.
            return rc;
        }

        // Read only response header into context header buffer
        rc = spi_tpm_read_reg(ctx, TCTI_SPI_TPM_DATA_FIFO_REG, ctx->header, TCTI_SPI_RESP_HEADER_SIZE);
        if (rc != TSS2_RC_SUCCESS) {
            LOG_ERROR("Failed reading response header");
            return TSS2_TCTI_RC_IO_ERROR;
        }

        // Find out the total payload size, skipping the two byte tag and update tcti_common
        tcti_common->header.size = spi_tpm_read_be32(ctx->header + 2);
        LOG_TRACE("Read response size from response header: %" PRIu32 " bytes", tcti_common->header.size);
    }

    // Check if response size is requested
    if (response_buffer == NULL) {
        *response_size = tcti_common->header.size;
        LOG_TRACE("Caller requested response size. Returning size of %zu bytes", *response_size);
        return TSS2_RC_SUCCESS;
    }

    // Check if response fits in buffer and update response size
    if (tcti_common->header.size > *response_size) {
        LOG_ERROR("TPM response too long (%" PRIu32 " bytes)", tcti_common->header.size);
        return TSS2_TCTI_RC_INSUFFICIENT_BUFFER;
    }
    *response_size = tcti_common->header.size;

    // Receive the TPM response
    LOG_TRACE("Reading response of size %" PRIu32, tcti_common->header.size);
    
    // Copy already received header into response buffer
    memcpy(response_buffer, ctx->header, TCTI_SPI_RESP_HEADER_SIZE);

    // Read all but the last byte in the FIFO
    size_t bytes_to_go = tcti_common->header.size - 1 - TCTI_SPI_RESP_HEADER_SIZE;
    spi_tpm_fifo_transfer(ctx, response_buffer + TCTI_SPI_RESP_HEADER_SIZE, bytes_to_go, TCTI_SPI_FIFO_RECEIVE);

    // Verify that there is still data to read
    uint32_t status = spi_tpm_read_sts_reg(ctx);
    if ((status & expected_status_bits) != expected_status_bits) {
        LOG_ERROR("Unexpected intermediate status %#x",status);
        return TSS2_TCTI_RC_IO_ERROR;
    }

    // Read the last byte
    rc = spi_tpm_read_reg(ctx, TCTI_SPI_TPM_DATA_FIFO_REG, response_buffer + tcti_common->header.size - 1, 1);
    if (rc != TSS2_RC_SUCCESS) {
        return TSS2_TCTI_RC_IO_ERROR;
    }

    // Verify that there is no more data available
    status = spi_tpm_read_sts_reg(ctx);
    if ((status & expected_status_bits) != TCTI_SPI_TPM_STS_VALID) {
        LOG_ERROR("Unexpected final status %#x", status);
        return TSS2_TCTI_RC_IO_ERROR;
    }

    LOGBLOB_DEBUG(response_buffer, tcti_common->header.size, "Response buffer received:");

    // Set the TPM back to idle state
    spi_tpm_write_sts_reg(ctx, TCTI_SPI_TPM_STS_COMMAND_READY);

    tcti_common->header.size = 0;
    tcti_common->state = TCTI_STATE_TRANSMIT;

    return TSS2_RC_SUCCESS;
}

void tcti_spi_finalize (TSS2_TCTI_CONTEXT* tcti_context)
{
    TSS2_TCTI_SPI_CONTEXT *tcti_spi = tcti_spi_context_cast (tcti_context);
    TSS2_TCTI_COMMON_CONTEXT *tcti_common = tcti_spi_down_cast (tcti_spi);
    
    if (tcti_spi == NULL) {
        return;
    }
    tcti_common->state = TCTI_STATE_FINAL;

    // Free platform struct user data and resources inside
    spi_tpm_platform_finalize(tcti_spi);
}

TSS2_RC tcti_spi_transmit (TSS2_TCTI_CONTEXT *tcti_ctx, size_t size, const uint8_t *cmd_buf)
{
    TSS2_RC rc;
    TSS2_TCTI_SPI_CONTEXT *tcti_spi = tcti_spi_context_cast (tcti_ctx);
    TSS2_TCTI_COMMON_CONTEXT *tcti_common = tcti_spi_down_cast (tcti_spi);
    tpm_header_t header;
    
    if (tcti_spi == NULL) {
        return TSS2_TCTI_RC_BAD_CONTEXT;
    }
    TSS2_TCTI_SPI_CONTEXT* ctx = tcti_spi;

    rc = tcti_common_transmit_checks (tcti_common, cmd_buf, TCTI_SPI_MAGIC);
    if (rc != TSS2_RC_SUCCESS) {
        return rc;
    }
    rc = header_unmarshal (cmd_buf, &header);
    if (rc != TSS2_RC_SUCCESS) {
        return rc;
    }
    if (header.size != size) {
        LOG_ERROR("Buffer size parameter: %zu, and TPM2 command header size "
                  "field: %" PRIu32 " disagree.", size, header.size);
        return TSS2_TCTI_RC_BAD_VALUE;
    }

    LOGBLOB_DEBUG (cmd_buf, size, "Sending command with TPM_CC %#x and size %" PRIu32,
               header.code, header.size);

    // Tell TPM to expect command
    spi_tpm_write_sts_reg(ctx, TCTI_SPI_TPM_STS_COMMAND_READY);

    // Send command
    spi_tpm_fifo_transfer(ctx, (void*)cmd_buf, size, TCTI_SPI_FIFO_TRANSMIT);

    // Tell TPM to start processing the command
    spi_tpm_write_sts_reg(ctx, TCTI_SPI_TPM_STS_GO);

    tcti_common->state = TCTI_STATE_RECEIVE;
    return TSS2_RC_SUCCESS;
}

TSS2_RC tcti_spi_cancel (TSS2_TCTI_CONTEXT* tcti_context)
{
    (void)(tcti_context);
    return TSS2_TCTI_RC_NOT_IMPLEMENTED;
}

TSS2_RC tcti_spi_get_poll_handles (TSS2_TCTI_CONTEXT* tcti_context, TSS2_TCTI_POLL_HANDLE *handles, size_t *num_handles)
{
    (void)(tcti_context);
    (void)(handles);
    (void)(num_handles);
    return TSS2_TCTI_RC_NOT_IMPLEMENTED;
}

TSS2_RC tcti_spi_set_locality (TSS2_TCTI_CONTEXT* tcti_context, uint8_t locality)
{
    (void)(tcti_context);
    (void)(locality);
    return TSS2_TCTI_RC_NOT_IMPLEMENTED;
}

TSS2_RC Tss2_Tcti_Spi_Init (TSS2_TCTI_CONTEXT* tcti_context, size_t* size, TSS2_TCTI_SPI_PLATFORM platform_conf)
{
    TSS2_RC rc;
    TSS2_TCTI_SPI_CONTEXT* tcti_spi;
    TSS2_TCTI_COMMON_CONTEXT* tcti_common;

    // Check if context size is requested
    if (tcti_context == NULL) {
        *size = sizeof (TSS2_TCTI_SPI_CONTEXT);
        return TSS2_RC_SUCCESS;
    }

    // Init TCTI context
    TSS2_TCTI_MAGIC (tcti_context) = TCTI_SPI_MAGIC;
    TSS2_TCTI_VERSION (tcti_context) = TCTI_VERSION;
    TSS2_TCTI_TRANSMIT (tcti_context) = tcti_spi_transmit;
    TSS2_TCTI_RECEIVE (tcti_context) = tcti_spi_receive;
    TSS2_TCTI_FINALIZE (tcti_context) = tcti_spi_finalize;
    TSS2_TCTI_CANCEL (tcti_context) = tcti_spi_cancel;
    TSS2_TCTI_GET_POLL_HANDLES (tcti_context) = tcti_spi_get_poll_handles;
    TSS2_TCTI_SET_LOCALITY (tcti_context) = tcti_spi_set_locality;
    TSS2_TCTI_MAKE_STICKY (tcti_context) = tcti_make_sticky_not_implemented;
    
    // Init SPI TCTI context
    tcti_spi = tcti_spi_context_cast (tcti_context);
    tcti_common = tcti_spi_down_cast (tcti_spi);
    tcti_common->state = TCTI_STATE_TRANSMIT;
    memset (&tcti_common->header, 0, sizeof (tcti_common->header));
    tcti_common->locality = 0;
    
    // Copy platform struct into context
    tcti_spi->platform = platform_conf;

    // Probe TPM
    TSS2_TCTI_SPI_CONTEXT* ctx = tcti_spi;
    LOG_DEBUG("Probing TPM...");
    uint32_t did_vid = 0;
    for (int retries = 100; retries > 0; retries--) {
        // In case of failed read div_vid is set to zero
        spi_tpm_read_reg(ctx, TCTI_SPI_TPM_DID_VID_REG, &did_vid, sizeof(did_vid));
        if (did_vid != 0) break;
        // TPM might be resetting, let's retry in a bit
        spi_tpm_delay_ms(ctx, 10);
    }
    if (did_vid == 0) {
        LOG_ERROR("Probing TPM failed");
        return TSS2_TCTI_RC_IO_ERROR;
    }    
    LOG_DEBUG("Probing TPM successful");

    // Claim locality
    LOG_DEBUG("Claiming TPM locality...");
    rc = spi_tpm_claim_locality(ctx);
    if (rc != TSS2_RC_SUCCESS) {
        return TSS2_TCTI_RC_IO_ERROR;
    }
    
    // Wait up to 200ms for TPM to become ready
    LOG_DEBUG("Waiting for TPM to become ready...");
    uint32_t expected_status_bits = TCTI_SPI_TPM_STS_COMMAND_READY;
    rc = spi_tpm_wait_for_status(ctx, expected_status_bits, expected_status_bits, 200);
    if (rc != TSS2_RC_SUCCESS) {
        LOG_ERROR("Failed waiting for TPM to become ready");
        return rc;
    }
    LOG_DEBUG("TPM is ready");

    // Get rid
    uint8_t rid = 0;
    spi_tpm_read_reg(ctx, TCTI_SPI_TPM_RID_REG, &rid, sizeof(rid));
    
    // Print device details
    uint16_t vendor_id, device_id, revision;
    vendor_id = did_vid & 0xffff;
    device_id = did_vid >> 16;
    revision = rid;
    LOG_INFO("Connected to TPM with vid:did:rid of %4.4x:%4.4x:%2.2x", vendor_id, device_id, revision);
    
    return TSS2_RC_SUCCESS;
}

static const TSS2_TCTI_INFO tss2_tcti_info = {
    .version = TCTI_VERSION,
    .name = "tcti-spi",
    .description = "Platform independent TCTI for communication with TPMs over SPI.",
    .config_help = "TSS2_TCTI_SPI_PLATFORM struct containing platform methods. See tss2_tcti_spi.h for more information.",
    
    /*
     * The Tss2_Tcti_Spi_Init method has a different signature than required by .init due too
     * our custom platform_conf parameter, so we can't expose it here and it has to be used directly.
     */
    .init = NULL,
};

const TSS2_TCTI_INFO* Tss2_Tcti_Info (void)
{
    return &tss2_tcti_info;
}
