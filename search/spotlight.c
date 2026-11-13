#include "vex.h"
#include "search/spotlight.h"
#include "search/calc.h"

#include <ctype.h>
#include <stdlib.h>

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Spotlight
 * ============================================================================
 * Ranked search and math-query engine: scores candidate strings against a
 * query using tiered relevance (exact > prefix > word-boundary > substring >
 * fuzzy subsequence) and fills a caller-owned SpotlightMatch array, sorted
 * descending by score. Also evaluates in-line calculator queries by
 * delegating to Calc_eval. Pure procedural — no state, no allocation beyond
 * the caller-provided outMatches buffer.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Spotlight (search/spotlight.c — defined in search/spotlight.h)
 * ============================================================================
 * Scores query strings against candidates using tiered relevance ranking and
 * evaluates in-line mathematical calculations.
 *
 * STRUCT FIELDS (Mirroring search/spotlight.h):
 * ----------------------------------------------------------------------------
 *   SpotlightMatch {
 *     u32 id;    // candidate index (or caller-supplied id)
 *     i32 score;  // relevance score, higher is better
 *   }
 *
 * FUNCTION REGISTRY:
 * ----------------------------------------------------------------------------
 * Public Core Functions: (.h)
 *   - Spotlight_rank(query, candidates, ids, candidateCount, outMatches, maxCount)
 *   - Spotlight_tryCalculate(query, outResult)
 *
 * Private Core Functions: (.c static)
 *   - calculate_score(candidate, query)
 *   - compare_matches(a, b.sh)
 * ============================================================================
 */

// Scores exact, prefix, boundary, substring, then subsequence matches in descending tiers.
static int calculate_score(const char *candidate, const char *query) {
    if (!candidate || !query) return 0;
    bytes qlen = strlen(query);
    bytes clen = strlen(candidate);
    if (qlen == 0) return 0;

    // 1. Exact match
    if (strcasecmp(candidate, query) == 0) {
        return 1000;
    }

    // 2. Prefix match
    if (strncasecmp(candidate, query, qlen) == 0) {
        return 800 - (int)(clen - qlen); // shorter candidates ranked higher
    }

    // 3. Word-boundary prefix match (e.g. "bar" matching "foo_bar")
    for (bytes i = 1; i < clen; i++) {
        if ((candidate[i - 1] == '_' || candidate[i - 1] == '-' || isupper((unsigned char)candidate[i])) &&
            strncasecmp(candidate + i, query, qlen) == 0) {
            return 600 - (int)(clen - qlen);
        }
    }

    // 4. Substring match
    char qlower[64];
    char clower[128];
    bytes ql = qlen < sizeof(qlower) - 1 ? qlen : sizeof(qlower) - 1;
    for (bytes i = 0; i < ql; i++) qlower[i] = (char) tolower((unsigned char)query[i]);
    qlower[ql] = '\0';

    bytes cl = clen < sizeof(clower) - 1 ? clen : sizeof(clower) - 1;
    for (bytes i = 0; i < cl; i++) clower[i] = (char) tolower((unsigned char)candidate[i]);
    clower[cl] = '\0';

    if (strstr(clower, qlower) != nullptr) {
        return 400 - (int)(clen - qlen);
    }

    // 5. Fuzzy subsequence match
    bytes qi = 0;
    for (bytes ci = 0; ci < cl && qi < ql; ci++) {
        if (clower[ci] == qlower[qi]) {
            qi++;
        }
    }
    if (qi == ql) {
        return 200 - (int)(clen - qlen);
    }

    return 0;
}

// Orders SpotlightMatch records by descending score for qsort.
static int compare_matches(const_any a, const_any b) {
    const SpotlightMatch *ma = (const SpotlightMatch*) a;
    const SpotlightMatch *mb = (const SpotlightMatch*) b;
    return (*mb).score - (*ma).score; // descending
}

// Writes up to maxCount ranked matches and returns the total number that scored.
bytes Spotlight_rank(const char *query, const char *const *candidates, const u32 *ids,
                      bytes candidateCount, SpotlightMatch *outMatches, bytes maxCount) {
    if (!query || !candidates || !outMatches || maxCount == 0) return 0;

    bytes matchCount = 0;
    bytes total = 0;
    for (bytes i = 0; i < candidateCount; i++) {
        int score = calculate_score(candidates[i], query);
        if (score > 0) {
            total++;
            if (matchCount < maxCount) {
                outMatches[matchCount].id = ids ? ids[i] : (u32) i;
                outMatches[matchCount].score = score;
                matchCount++;
            }
        }
    }

    if (matchCount > 1) {
        qsort(outMatches, matchCount, sizeof(SpotlightMatch), compare_matches);
    }

    return total;   // documented: TOTAL matches found (not capped at maxCount)
}

// Evaluates query as a calculator expression and writes the result on success.
bool Spotlight_tryCalculate(const char *query, d64 *outResult) {
    if (!query || !outResult) return false;
    return Calc_eval(query, outResult);
}
