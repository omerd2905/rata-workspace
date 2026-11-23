#include "core/status.h"

const char *rata_status_str(RataStatus status) {
    switch (status) {
        case RATA_OK:
            return "ok";
        case RATA_ERR_INVALID_ARG:
            return "invalid argument";
        case RATA_ERR_OUT_OF_MEMORY:
            return "out of memory";
        case RATA_ERR_DIV_ZERO:
            return "division by zero";
        case RATA_ERR_SINGULAR:
            return "singular matrix or degenerate system";
        case RATA_ERR_NO_CONVERGENCE:
            return "numerical solver did not converge";
        case RATA_ERR_PARSE:
            return "parse error";
        case RATA_ERR_RANGE:
            return "value out of valid range";
        case RATA_ERR_UNSTABLE:
            return "unstable or diverging system";
        case RATA_ERR_IO:
            return "I/O error";
        case RATA_ERR_FORMAT:
            return "unsupported or malformed file format";
        case RATA_ERR_NEWER_VERSION:
            return "file format version is newer than this version of Rata Workspace supports";
        case RATA_ERR_AUTH:
            return "authentication failed or token lacks access";
        case RATA_ERR_VERIFY:
            return "cryptographic signature or integrity verification failed";
        default:
            return "unknown error";
    }
}
