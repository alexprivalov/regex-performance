/*
 * ModSecurity, http://www.modsecurity.org/
 * Copyright (c) 2021 Trustwave Holdings, Inc. (http://www.trustwave.com/)
 *
 * You may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * If any of the files related to licensing are missing or if you have any
 * other questions related to licensing please contact Trustwave Holdings, Inc.
 * directly using the email address security@modsecurity.org.
 *
 */

#include <string>
#include <vector>

// #ifdef WITH_HS
#include "hyperscan_new.hpp"

namespace modsecurity {
namespace Utils {

HyperscanPattern::HyperscanPattern(const char *pat, size_t patLen,
                                   unsigned int patId) :
                                   pattern(pat), len(patLen), id(patId) {}

HyperscanPm::~HyperscanPm() {
    if (db) {
        hs_free_database(db);
    }
    if (scratch) {
        hs_free_scratch(scratch);
    }
}

void HyperscanPm::addPattern(const char *pat, size_t patLen) {
    printf("Adding pattern: '%s', lenth - %ld\n", pat, patLen);
    if (patLen == 0) {
        return;
    }

    HyperscanPattern p(pat, patLen, num_patterns++);
    patterns.emplace_back(p);
}

bool HyperscanPm::compile(std::string *error) {
    if (patterns.empty()) {
        return false;
    }

    if (hs_valid_platform() != HS_SUCCESS )
    {
        error->assign("This host does not support Hyperscan.");
        return false;
    }

    // The Hyperscan compiler takes its patterns in a group of arrays.
    std::vector<const char *> pats;
    std::vector<unsigned> flags(num_patterns, HS_FLAG_DOTALL | HS_FLAG_MULTILINE | HS_FLAG_SOM_LEFTMOST);//HS_FLAG_CASELESS);
    std::vector<unsigned> ids;

    for (const auto &p : patterns) {
        pats.emplace_back(p.pattern.c_str());
        ids.emplace_back(p.id);
    }

    hs_compile_error_t *compile_error = NULL;
    hs_error_t hs_error = hs_compile_multi(&pats[0], 
                                            &flags[0], 
                                            &ids[0],
                                            num_patterns, 
                                            HS_MODE_BLOCK, 
                                            NULL, 
                                            &db, 
                                            &compile_error);

    if (compile_error != NULL) {
        std::string message(compile_error->message);
        std::string expression = std::to_string(compile_error->expression);
        error->assign("hs_compile_multi() failed: " + message +
                      "(expression: " + expression + ")");
        hs_free_compile_error(compile_error);
        return false;
    }

    if (hs_error != HS_SUCCESS) {
        error->assign("hs_compile_multi() failed: error " +
                      std::to_string(hs_error));
        return false;
    }

    // Allocate Hyperscan scratch space for this database.
    hs_error = hs_alloc_scratch(db, &scratch);

    if (hs_error != HS_SUCCESS) {
        error->assign("hs_alloc_scratch() failed: error " +
                      std::to_string(hs_error));
        return false;
    }

    size_t scratch_size = 0;
    hs_error = hs_scratch_size(scratch, &scratch_size);
    if (hs_error != HS_SUCCESS) {
        error->assign("hs_scratch_size() failed: error " +
                      std::to_string(hs_error));
        return false;
    }

    size_t db_size = 0;
    hs_error = hs_database_size(db, &db_size);
    if (hs_error != HS_SUCCESS) {
        error->assign("hs_database_size() failed: error " +
                      std::to_string(hs_error));
        return false;
    }

    return true;
}

// Context data used by Hyperscan match callback.
struct HyperscanCallbackContext {
    HyperscanPm *pm{nullptr};
    unsigned int num_matches{0};
    unsigned int offset{0};
    std::vector<std::string>& matches;
    const bool terminateAfter1stMatch{false};
};

// Match callback, called by hs_scan for every match.
static
int onMatch(unsigned int id, unsigned long long from, unsigned long long to,
            unsigned int /*flags*/, void *hs_ctx) {
    HyperscanCallbackContext *ctx = static_cast<HyperscanCallbackContext *>(hs_ctx);

    ctx->num_matches++;
    ctx->offset = (unsigned int)to - 1;

    const char* match = ctx->pm->getPatternById(id);
    if (match)
        ctx->matches.push_back(match);

    printf("%s Match for pattern \"%s\" at offset %llu:%llu\n", __func__, match, from, to);
    return ctx->terminateAfter1stMatch; // Terminate matching.
}

int HyperscanPm::search(const char *t, unsigned int tlen, std::vector<std::string>& matches, bool terminateAfter1stMatch) {
    HyperscanCallbackContext ctx{this, 0, 0, matches, terminateAfter1stMatch};

    hs_error_t error = hs_scan(db, t, tlen, 0, scratch, onMatch, &ctx);
    if (error != HS_SUCCESS && error != HS_SCAN_TERMINATED) {
        printf("%s hs_scan() return error code: %d\n", __func__, error);
        // TODO add debug output
        return -1;
    }

    return ctx.num_matches;
}

const char *HyperscanPm::getPatternById(unsigned int patId) const {
    return patterns[patId].pattern.c_str();
}

}  // namespace Utils
}  // namespace modsecurity

// #endif
