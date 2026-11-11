#ifndef SEARCH_SPOTLIGHT_H
#define SEARCH_SPOTLIGHT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// search/spotlight.h — Ranked Spotlight Search & Math Query Engine.
//
// Single Class Per File Law: Spotlight.
//
// Provides ranked candidate matching (Exact > Prefix > Word Boundary > Fuzzy)
// and handles calculator spotlight queries.

typedef struct SpotlightMatch {
    u32 id;
    i32 score; // higher is better
} SpotlightMatch;

// Ranks candidate strings against query. Fills outMatches up to maxCount.
// Returns total number of matches found.
bytes Spotlight_rank(const char *query, const char *const *candidates, const u32 *ids,
                      bytes candidateCount, SpotlightMatch *outMatches, bytes maxCount);

// Evaluates whether a query string represents a math expression and computes its result.
bool Spotlight_tryCalculate(const char *query, d64 *outResult);

#endif
