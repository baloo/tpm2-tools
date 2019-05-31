/* SPDX-License-Identifier: BSD-3-Clause */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

#include <tss2/tss2_sys.h>

#include "files.h"
#include "log.h"
#include "pcr.h"
#include "tpm2_options.h"
#include "tpm2_policy.h"
#include "tpm2_session.h"
#include "tpm2_tool.h"
#include "tpm2_util.h"

typedef struct tpm2_policylocality_ctx tpm2_policylocality_ctx;
struct tpm2_policylocality_ctx {
   const char *session_path;
   TPMA_LOCALITY locality;
   const char *out_policy_dgst_path;
   TPM2B_DIGEST *policy_digest;
   tpm2_session *session;
};

static tpm2_policylocality_ctx ctx;

static bool on_option(char key, char *value) {

    switch (key) {
    case 'S':
        ctx.session_path = value;
        break;
    case 'o':
        ctx.out_policy_dgst_path = value;
        break;
    }
    return true;
}

bool is_input_option_args_valid(void) {

    if (!ctx.session_path) {
        LOG_ERR("Must specify -S session file.");
        return false;
    }

    return true;
}

bool on_arg (int argc, char **argv) {

    if (argc > 1) {
        LOG_ERR("Specify only the TPM2 locality.");
        return false;
    }

    if (!argc) {
        LOG_ERR("TPM2 locality must be specified.");
        return false;
    }

    bool result = tpm2_util_string_to_uint8(argv[0], &ctx.locality);
    if (!result) {
        LOG_ERR("Could not convert locality to number, got: \"%s\"",
                argv[0]);
        return false;
    }

    return true;
}

bool tpm2_tool_onstart(tpm2_options **opts) {

    static struct option topts[] = {
        { "session",            required_argument,  NULL,   'S' },
        { "out-policy-file",    required_argument,  NULL,   'o' },
    };

    *opts = tpm2_options_new("S:o:", ARRAY_LEN(topts), topts, on_option,
                             on_arg, 0);

    return *opts != NULL;
}

tool_rc tpm2_tool_onrun(ESYS_CONTEXT *ectx, tpm2_option_flags flags) {

    UNUSED(flags);

    bool retval = is_input_option_args_valid();
    if (!retval) {
        return tool_rc_option_error;
    }

    ctx.session = tpm2_session_restore(ectx, ctx.session_path, false);
    if (!ctx.session) {
        return tool_rc_general_error;
    }

    bool result = tpm2_policy_build_policylocality(ectx, ctx.session,
        ctx.locality);
    if (!result) {
        LOG_ERR("Could not build TPM policy_locality");
        return tool_rc_general_error;
    }

    result = tpm2_policy_get_digest(ectx, ctx.session, &ctx.policy_digest);
    if (!result) {
        LOG_ERR("Could not build tpm policy");
        return tool_rc_general_error;
    }

    tpm2_util_hexdump(ctx.policy_digest->buffer, ctx.policy_digest->size);
    tpm2_tool_output("\n");

    if (ctx.out_policy_dgst_path) {
        result = files_save_bytes_to_file(ctx.out_policy_dgst_path,
                    ctx.policy_digest->buffer, ctx.policy_digest->size);
        if (!result) {
            LOG_ERR("Failed to save policy digest into file \"%s\"",
                    ctx.out_policy_dgst_path);
            return tool_rc_general_error;
        }
    }

    return tool_rc_success;
}

tool_rc tpm2_tool_onstop(ESYS_CONTEXT *ectx) {
    UNUSED(ectx);
    free(ctx.policy_digest);
    return tpm2_session_close(&ctx.session);
}
