/*
 * jsmn.c — the single translation unit that emits the jsmn implementation
 * exactly once, producing jsmn.o.
 *
 * Every OTHER source file that needs the parser includes <jsmn.h> with
 * JSMN_HEADER defined first (declarations only), so the implementation symbols
 * (jsmn_init, jsmn_parse) are defined here and nowhere else. Linking this one
 * object alongside those header-only consumers therefore produces no duplicate
 * symbols — the same vendoring discipline already used for argtable3.c.
 *
 * jsmn is zero-allocation: the caller supplies a fixed jsmntok_t[] buffer, so
 * a JSON-RPC request can be parsed entirely on a worker thread's stack with no
 * shared or heap state — exactly what the multithreaded MCP server needs.
 */
#include "jsmn.h"
