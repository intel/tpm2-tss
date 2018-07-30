/* SPDX-License-Identifier: BSD-2 */
/*******************************************************************************
 * Copyright 2017-2018, Fraunhofer SIT sponsored by Infineon Technologies AG
 * All rights reserved.
 ******************************************************************************/

#include "tss2_mu.h"
#include "tss2_sys.h"
#include "tss2_esys.h"

#include "esys_types.h"
#include "esys_iutil.h"
#include "esys_mu.h"
#include "tpm2_type_check.h"
#define LOGMODULE esys
#include "util/log.h"

/** Check values of command parameters */
static TSS2_RC 
check_parameter (
    const TPM2B_TIMEOUT *timeout,
    const TPM2B_DIGEST *cpHashA,
    const TPM2B_NONCE *policyRef,
    const TPM2B_NAME *authName,
    const TPMT_TK_AUTH *ticket)
{
    TSS2_RC r;
    r = iesys_TPM2B_TIMEOUT_check(timeout);
    return_if_error(r,"Bad value for parameter timeout "
                    "of type type: TPM2B_TIMEOUT.");
    r = iesys_TPM2B_DIGEST_check(cpHashA);
    return_if_error(r,"Bad value for parameter cpHashA "
                    "of type type: TPM2B_DIGEST.");
    r = iesys_TPM2B_NONCE_check(policyRef);
    return_if_error(r,"Bad value for parameter policyRef "
                    "of type type: TPM2B_NONCE.");
    r = iesys_TPM2B_NAME_check(authName);
    return_if_error(r,"Bad value for parameter authName "
                    "of type type: TPM2B_NAME.");
    r = iesys_TPMT_TK_AUTH_check(ticket);
    return_if_error(r,"Bad value for parameter ticket "
                    "of type type: TPMT_TK_AUTH.");
    return TSS2_RC_SUCCESS;
}

/** Store command parameters inside the ESYS_CONTEXT for use during _Finish */
static void store_input_parameters (
    ESYS_CONTEXT *esysContext,
    ESYS_TR policySession,
    const TPM2B_TIMEOUT *timeout,
    const TPM2B_DIGEST *cpHashA,
    const TPM2B_NONCE *policyRef,
    const TPM2B_NAME *authName,
    const TPMT_TK_AUTH *ticket)
{
    esysContext->in.PolicyTicket.policySession = policySession;
    if (timeout == NULL) {
        esysContext->in.PolicyTicket.timeout = NULL;
    } else {
        esysContext->in.PolicyTicket.timeoutData = *timeout;
        esysContext->in.PolicyTicket.timeout =
            &esysContext->in.PolicyTicket.timeoutData;
    }
    if (cpHashA == NULL) {
        esysContext->in.PolicyTicket.cpHashA = NULL;
    } else {
        esysContext->in.PolicyTicket.cpHashAData = *cpHashA;
        esysContext->in.PolicyTicket.cpHashA =
            &esysContext->in.PolicyTicket.cpHashAData;
    }
    if (policyRef == NULL) {
        esysContext->in.PolicyTicket.policyRef = NULL;
    } else {
        esysContext->in.PolicyTicket.policyRefData = *policyRef;
        esysContext->in.PolicyTicket.policyRef =
            &esysContext->in.PolicyTicket.policyRefData;
    }
    if (authName == NULL) {
        esysContext->in.PolicyTicket.authName = NULL;
    } else {
        esysContext->in.PolicyTicket.authNameData = *authName;
        esysContext->in.PolicyTicket.authName =
            &esysContext->in.PolicyTicket.authNameData;
    }
    if (ticket == NULL) {
        esysContext->in.PolicyTicket.ticket = NULL;
    } else {
        esysContext->in.PolicyTicket.ticketData = *ticket;
        esysContext->in.PolicyTicket.ticket =
            &esysContext->in.PolicyTicket.ticketData;
    }
}

/** One-Call function for TPM2_PolicyTicket
 *
 * This function invokes the TPM2_PolicyTicket command in a one-call
 * variant. This means the function will block until the TPM response is
 * available. All input parameters are const. The memory for non-simple output
 * parameters is allocated by the function implementation.
 *
 * @param[in,out] esysContext The ESYS_CONTEXT.
 * @param[in]  policySession Handle for the policy session being extended.
 * @param[in]  shandle1 First session handle.
 * @param[in]  shandle2 Second session handle.
 * @param[in]  shandle3 Third session handle.
 * @param[in]  timeout Time when authorization will expire.
 * @param[in]  cpHashA Digest of the command parameters to which this
 *             authorization is limited.
 * @param[in]  policyRef Reference to a qualifier for the policy - may be the
 *             Empty Buffer.
 * @param[in]  authName Name of the object that provided the authorization.
 * @param[in]  ticket An authorization ticket returned by the TPM in response to a
 *             TPM2_PolicySigned() or TPM2_PolicySecret().
 * @retval TSS2_RC_SUCCESS on success
 * @retval ESYS_RC_SUCCESS if the function call was a success.
 * @retval TSS2_ESYS_RC_BAD_REFERENCE if the esysContext or required input
 *         pointers or required output handle references are NULL.
 * @retval TSS2_ESYS_RC_BAD_CONTEXT: if esysContext corruption is detected.
 * @retval TSS2_ESYS_RC_MEMORY: if the ESAPI cannot allocate enough memory for
 *         internal operations or return parameters.
 * @retval TSS2_ESYS_RC_BAD_SEQUENCE: if the context has an asynchronous
 *         operation already pending.
 * @retval TSS2_ESYS_RC_INSUFFICIENT_RESPONSE: if the TPM's response does not
 *          at least contain the tag, response length, and response code.
 * @retval TSS2_ESYS_RC_MALFORMED_RESPONSE: if the TPM's response is corrupted.
 * @retval TSS2_ESYS_RC_MULTIPLE_DECRYPT_SESSIONS: if more than one session has
 *         the 'decrypt' attribute bit set.
 * @retval TSS2_ESYS_RC_MULTIPLE_ENCRYPT_SESSIONS: if more than one session has
 *         the 'encrypt' attribute bit set.
 * @retval TSS2_ESYS_RC_BAD_TR: if any of the ESYS_TR objects are unknown to the
 *         ESYS_CONTEXT or are of the wrong type or if required ESYS_TR objects
 *         are ESYS_TR_NONE.
 * @retval TSS2_ESYS_RC_NO_ENCRYPT_PARAM: if one of the sessions has the
 *         'encrypt' attribute set and the command does not support encryption
 *          of the first response parameter.
 * @retval TSS2_RCs produced by lower layers of the software stack may be
 *         returned to the caller unaltered unless handled internally.
 */
TSS2_RC
Esys_PolicyTicket(
    ESYS_CONTEXT *esysContext,
    ESYS_TR policySession,
    ESYS_TR shandle1,
    ESYS_TR shandle2,
    ESYS_TR shandle3,
    const TPM2B_TIMEOUT *timeout,
    const TPM2B_DIGEST *cpHashA,
    const TPM2B_NONCE *policyRef,
    const TPM2B_NAME *authName,
    const TPMT_TK_AUTH *ticket)
{
    TSS2_RC r;

    r = Esys_PolicyTicket_Async(esysContext,
                policySession,
                shandle1,
                shandle2,
                shandle3,
                timeout,
                cpHashA,
                policyRef,
                authName,
                ticket);
    return_if_error(r, "Error in async function");

    /* Set the timeout to indefinite for now, since we want _Finish to block */
    int32_t timeouttmp = esysContext->timeout;
    esysContext->timeout = -1;
    /*
     * Now we call the finish function, until return code is not equal to
     * from TSS2_BASE_RC_TRY_AGAIN.
     * Note that the finish function may return TSS2_RC_TRY_AGAIN, even if we
     * have set the timeout to -1. This occurs for example if the TPM requests
     * a retransmission of the command via TPM2_RC_YIELDED.
     */
    do {
        r = Esys_PolicyTicket_Finish(esysContext);
        /* This is just debug information about the reattempt to finish the
           command */
        if ((r & ~TSS2_RC_LAYER_MASK) == TSS2_BASE_RC_TRY_AGAIN)
            LOG_DEBUG("A layer below returned TRY_AGAIN: %" PRIx32
                      " => resubmitting command", r);
    } while ((r & ~TSS2_RC_LAYER_MASK) == TSS2_BASE_RC_TRY_AGAIN);

    /* Restore the timeout value to the original value */
    esysContext->timeout = timeouttmp;
    return_if_error(r, "Esys Finish");

    return TSS2_RC_SUCCESS;
}

/** Asynchronous function for TPM2_PolicyTicket
 *
 * This function invokes the TPM2_PolicyTicket command in a asynchronous
 * variant. This means the function will return as soon as the command has been
 * sent downwards the stack to the TPM. All input parameters are const.
 * In order to retrieve the TPM's response call Esys_PolicyTicket_Finish.
 *
 * @param[in,out] esysContext The ESYS_CONTEXT.
 * @param[in]  policySession Handle for the policy session being extended.
 * @param[in]  shandle1 First session handle.
 * @param[in]  shandle2 Second session handle.
 * @param[in]  shandle3 Third session handle.
 * @param[in]  timeout Time when authorization will expire.
 * @param[in]  cpHashA Digest of the command parameters to which this
 *             authorization is limited.
 * @param[in]  policyRef Reference to a qualifier for the policy - may be the
 *             Empty Buffer.
 * @param[in]  authName Name of the object that provided the authorization.
 * @param[in]  ticket An authorization ticket returned by the TPM in response to a
 *             TPM2_PolicySigned() or TPM2_PolicySecret().
 * @retval ESYS_RC_SUCCESS if the function call was a success.
 * @retval TSS2_ESYS_RC_BAD_REFERENCE if the esysContext or required input
 *         pointers or required output handle references are NULL.
 * @retval TSS2_ESYS_RC_BAD_CONTEXT: if esysContext corruption is detected.
 * @retval TSS2_ESYS_RC_MEMORY: if the ESAPI cannot allocate enough memory for
 *         internal operations or return parameters.
 * @retval TSS2_RCs produced by lower layers of the software stack may be
           returned to the caller unaltered unless handled internally.
 * @retval TSS2_ESYS_RC_MULTIPLE_DECRYPT_SESSIONS: if more than one session has
 *         the 'decrypt' attribute bit set.
 * @retval TSS2_ESYS_RC_MULTIPLE_ENCRYPT_SESSIONS: if more than one session has
 *         the 'encrypt' attribute bit set.
 * @retval TSS2_ESYS_RC_BAD_TR: if any of the ESYS_TR objects are unknown to the
           ESYS_CONTEXT or are of the wrong type or if required ESYS_TR objects
           are ESYS_TR_NONE.
 * @retval TSS2_ESYS_RC_NO_ENCRYPT_PARAM: if one of the sessions has the
 *         'encrypt' attribute set and the command does not support encryption
 *          of the first response parameter.
 */
TSS2_RC
Esys_PolicyTicket_Async(
    ESYS_CONTEXT *esysContext,
    ESYS_TR policySession,
    ESYS_TR shandle1,
    ESYS_TR shandle2,
    ESYS_TR shandle3,
    const TPM2B_TIMEOUT *timeout,
    const TPM2B_DIGEST *cpHashA,
    const TPM2B_NONCE *policyRef,
    const TPM2B_NAME *authName,
    const TPMT_TK_AUTH *ticket)
{
    TSS2_RC r;
    LOG_TRACE("context=%p, policySession=%"PRIx32 ", timeout=%p,"
              "cpHashA=%p, policyRef=%p, authName=%p,"
              "ticket=%p",
              esysContext, policySession, timeout, cpHashA, policyRef,
              authName, ticket);
    TSS2L_SYS_AUTH_COMMAND auths;
    RSRC_NODE_T *policySessionNode;

    /* Check context, sequence correctness and set state to error for now */
    if (esysContext == NULL) {
        LOG_ERROR("esyscontext is NULL.");
        return TSS2_ESYS_RC_BAD_REFERENCE;
    }
    r = iesys_check_sequence_async(esysContext);
    if (r != TSS2_RC_SUCCESS)
        return r;
    esysContext->state = _ESYS_STATE_INTERNALERROR;

    /* Check and store input parameters */
    r = check_session_feasibility(shandle1, shandle2, shandle3, 0);
    return_state_if_error(r, _ESYS_STATE_INIT, "Check session usage");
    r = check_parameter(timeout,
                        cpHashA,
                        policyRef,
                        authName,
                        ticket);
    return_state_if_error(r, _ESYS_STATE_INIT, "Bad Value");

    store_input_parameters(esysContext, policySession,
                timeout,
                cpHashA,
                policyRef,
                authName,
                ticket);

    /* Retrieve the metadata objects for provided handles */
    r = esys_GetResourceObject(esysContext, policySession, &policySessionNode);
    return_state_if_error(r, _ESYS_STATE_INIT, "policySession unknown.");

    /* Initial invocation of SAPI to prepare the command buffer with parameters */
    r = Tss2_Sys_PolicyTicket_Prepare(esysContext->sys,
                (policySessionNode == NULL) ? TPM2_RH_NULL : policySessionNode->rsrc.handle,
                timeout,
                cpHashA,
                policyRef,
                authName,
                ticket);
    return_state_if_error(r, _ESYS_STATE_INIT, "SAPI Prepare returned error.");

    /* Calculate the cpHash Values */
    r = init_session_tab(esysContext, shandle1, shandle2, shandle3);
    return_state_if_error(r, _ESYS_STATE_INIT, "Initialize session resources");
    iesys_compute_session_value(esysContext->session_tab[0], NULL, NULL);
    iesys_compute_session_value(esysContext->session_tab[1], NULL, NULL);
    iesys_compute_session_value(esysContext->session_tab[2], NULL, NULL);

    /* Generate the auth values and set them in the SAPI command buffer */
    r = iesys_gen_auths(esysContext, policySessionNode, NULL, NULL, &auths);
    return_state_if_error(r, _ESYS_STATE_INIT, "Error in computation of auth values");
    esysContext->authsCount = auths.count;
    r = Tss2_Sys_SetCmdAuths(esysContext->sys, &auths);
    return_state_if_error(r, _ESYS_STATE_INIT, "SAPI error on SetCmdAuths");

    /* Trigger execution and finish the async invocation */
    r = Tss2_Sys_ExecuteAsync(esysContext->sys);
    return_state_if_error(r, _ESYS_STATE_INTERNALERROR, "Finish (Execute Async)");

    esysContext->state = _ESYS_STATE_SENT;

    return r;
}

/** Asynchronous finish function for TPM2_PolicyTicket
 *
 * This function returns the results of a TPM2_PolicyTicket command
 * invoked via Esys_PolicyTicket_Finish. All non-simple output parameters
 * are allocated by the function's implementation. NULL can be passed for every
 * output parameter if the value is not required.
 *
 * @param[in,out] esysContext The ESYS_CONTEXT.
 * @retval TSS2_RC_SUCCESS on success
 * @retval ESYS_RC_SUCCESS if the function call was a success.
 * @retval TSS2_ESYS_RC_BAD_REFERENCE if the esysContext or required input
 *         pointers or required output handle references are NULL.
 * @retval TSS2_ESYS_RC_BAD_CONTEXT: if esysContext corruption is detected.
 * @retval TSS2_ESYS_RC_MEMORY: if the ESAPI cannot allocate enough memory for
 *         internal operations or return parameters.
 * @retval TSS2_ESYS_RC_BAD_SEQUENCE: if the context has an asynchronous
 *         operation already pending.
 * @retval TSS2_ESYS_RC_TRY_AGAIN: if the timeout counter expires before the
 *         TPM response is received.
 * @retval TSS2_ESYS_RC_INSUFFICIENT_RESPONSE: if the TPM's response does not
 *          at least contain the tag, response length, and response code.
 * @retval TSS2_ESYS_RC_MALFORMED_RESPONSE: if the TPM's response is corrupted.
 * @retval TSS2_RCs produced by lower layers of the software stack may be
 *         returned to the caller unaltered unless handled internally.
 */
TSS2_RC
Esys_PolicyTicket_Finish(
    ESYS_CONTEXT *esysContext)
{
    TSS2_RC r;
    LOG_TRACE("context=%p",
              esysContext);

    if (esysContext == NULL) {
        LOG_ERROR("esyscontext is NULL.");
        return TSS2_ESYS_RC_BAD_REFERENCE;
    }

    /* Check for correct sequence and set sequence to irregular for now */
    if (esysContext->state != _ESYS_STATE_SENT) {
        LOG_ERROR("Esys called in bad sequence.");
        return TSS2_ESYS_RC_BAD_SEQUENCE;
    }
    esysContext->state = _ESYS_STATE_INTERNALERROR;

    /*Receive the TPM response and handle resubmissions if necessary. */
    r = Tss2_Sys_ExecuteFinish(esysContext->sys, esysContext->timeout);
    if ((r & ~TSS2_RC_LAYER_MASK) == TSS2_BASE_RC_TRY_AGAIN) {
        LOG_DEBUG("A layer below returned TRY_AGAIN: %" PRIx32, r);
        esysContext->state = _ESYS_STATE_SENT;
        return r;
    }
    /* This block handle the resubmission of TPM commands given a certain set of
     * TPM response codes. */
    if (r == TPM2_RC_RETRY || r == TPM2_RC_TESTING || r == TPM2_RC_YIELDED) {
        LOG_DEBUG("TPM returned RETRY, TESTING or YIELDED, which triggers a "
            "resubmission: %" PRIx32, r);
        if (esysContext->submissionCount >= _ESYS_MAX_SUBMISSIONS) {
            LOG_WARNING("Maximum number of (re)submissions has been reached.");
            esysContext->state = _ESYS_STATE_INIT;
            return r;
        }
        esysContext->state = _ESYS_STATE_RESUBMISSION;
        r = Esys_PolicyTicket_Async(esysContext,
                esysContext->in.PolicyTicket.policySession,
                esysContext->session_type[0],
                esysContext->session_type[1],
                esysContext->session_type[2],
                esysContext->in.PolicyTicket.timeout,
                esysContext->in.PolicyTicket.cpHashA,
                esysContext->in.PolicyTicket.policyRef,
                esysContext->in.PolicyTicket.authName,
                esysContext->in.PolicyTicket.ticket);
        if (r != TSS2_RC_SUCCESS) {
            LOG_WARNING("Error attempting to resubmit");
            /* We do not set esysContext->state here but inherit the most recent
             * state of the _async function. */
            return r;
        }
        r = TSS2_ESYS_RC_TRY_AGAIN;
        LOG_DEBUG("Resubmission initiated and returning RC_TRY_AGAIN.");
        return r;
    }
    /* The following is the "regular error" handling. */
    if (iesys_tpm_error(r)) {
        LOG_WARNING("Received TPM Error");
        esysContext->state = _ESYS_STATE_INIT;
        return r;
    } else if (r != TSS2_RC_SUCCESS) {
        LOG_ERROR("Received a non-TPM Error");
        esysContext->state = _ESYS_STATE_INTERNALERROR;
        return r;
    }

    /*
     * Now the verification of the response (hmac check) and if necessary the
     * parameter decryption have to be done.
     */
    r = iesys_check_response(esysContext);
    return_state_if_error(r, _ESYS_STATE_INTERNALERROR, "Error: check response");
    /*
     * After the verification of the response we call the complete function
     * to deliver the result.
     */
    r = Tss2_Sys_PolicyTicket_Complete(esysContext->sys);
    return_state_if_error(r, _ESYS_STATE_INTERNALERROR, "Received error from SAPI"
                        " unmarshaling" );
    esysContext->state = _ESYS_STATE_INIT;

    return TSS2_RC_SUCCESS;
}
