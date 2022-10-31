#include <getopt.h>  // getopt, optarg
#include <stdlib.h>  // exit, atoi, malloc, free
#include <stdio.h>   // printf, fprintf, stderr, fopen, fclose, FILE
#include <limits.h>  // ULONG_MAX
#include <string.h>  // strcmp, strerror
#include <errno.h>   // errno

/* fast base-2 integer logarithm */
#define INT_LOG2(x) (31 - __builtin_clz(x))
#define NOT_POWER2(x) (__builtin_clz(x) + __builtin_ctz(x) != 31)

/* tag_bits = ADDRESS_LENGTH - set_bits - block_bits */
#define ADDRESS_LENGTH 64

/**
 * Print program usage (no need to modify).
 */
static void print_usage() {
    printf("Usage: csim [-hv] -S <num> -K <num> -B <num> -p <policy> -t <file>\n");
    printf("Options:\n");
    printf("  -h           Print this help message.\n");
    printf("  -v           Optional verbose flag.\n");
    printf("  -S <num>     Number of sets.           (must be > 0)\n");
    printf("  -K <num>     Number of lines per set.  (must be > 0)\n");
    printf("  -B <num>     Number of bytes per line. (must be > 0)\n");
    printf("  -p <policy>  Eviction policy. (one of 'FIFO', 'LRU')\n");
    printf("  -t <file>    Trace file.\n\n");
    printf("Examples:\n");
    printf("$ ./csim    -S 16  -K 1 -B 16 -p LRU -t traces/yi2.trace\n");
    printf("$ ./csim -v -S 256 -K 2 -B 16 -p LRU -t traces/yi2.trace\n");
}

/* Parameters set by command-line args (no need to modify) */
int verbose = 0;   // print trace if 1
int S = 0;         // number of sets
int K = 0;         // lines per set
int B = 0;         // bytes per line

typedef enum { FIFO = 1, LRU = 2 } Policy;
Policy policy;     // 0 (undefined) by default

FILE *trace_fp = NULL;

/**
 * Parse input arguments and set verbose, S, K, B, policy, trace_fp.
 *
 * TODO: Finish implementation
 */
static void parse_arguments(int argc, char **argv) {
    char c;
    while ((c = getopt(argc, argv, "S:K:B:p:t:vh")) != -1) {
        switch(c) {
            case 'S':
                S = atoi(optarg);
                if (NOT_POWER2(S)) {
                    fprintf(stderr, "ERROR: S must be a power of 2\n");
                    exit(1);
                }
                break;
            case 'K':
                // TODO
                K = atoi(optarg);
                break;
            case 'B':
                // TODO
                B = atoi(optarg);
                if (NOT_POWER2(B)) {
                    fprintf(stderr, "ERROR: B must be a power of 2\n");
                    exit(1);
                }
                break;
            case 'p':
                if (!strcmp(optarg, "FIFO")) {
                    policy = FIFO;
                }
                // TODO: parse LRU, exit with error for unknown policy
                else if (!strcmp(optarg, "LRU")) {
                    policy = LRU;
                }
                else {
                    fprintf(stderr, "ERROR: p must be either LRU or FIFO");
                    exit(1);
                }
                break;
            case 't':
                // TODO: open file trace_fp for reading
                trace_fp = fopen(optarg,"r");
                if (!trace_fp) {
                    fprintf(stderr, "ERROR: %s: %s\n", optarg, strerror(errno));
                    exit(1);
                }                
                break;
            case 'v':
                // TODO
                verbose = 1;
                break;
            case 'h':
                // TODO
                print_usage();
                exit(0);
            default:
                print_usage();
                exit(1);
        }
    }
    /* Make sure that all required command line args were specified and valid */
    if (S <= 0 || K <= 0 || B <= 0 || policy == 0 || !trace_fp) {
        printf("ERROR: Negative or missing command line arguments\n");
        print_usage();
        if (trace_fp) {
            fclose(trace_fp);
        }
        exit(1);
    }

    /* Other setup if needed */



}

/**
 * Cache data structures
 * TODO: Define your own!
 */
struct cacheline {
    int tag, valid, numops;
} *cacheline;

struct cacheline** sets;


/**
 * Allocate cache data structures.
 *
 * This function dynamically allocates (with malloc) data structures for each of
 * the `S` sets and `K` lines per set.
 *
 * TODO: Implement
 */
static void allocate_cache() {
    sets = (struct cacheline**)malloc(sizeof(struct cacheline*)*S);
    for (int i = 0; i < S; ++i) {
        sets[i] = (struct cacheline*)malloc(sizeof(struct cacheline)*K);
    }
    for (int i = 0; i < S; ++i) {
        for (int j = 0; j < K; ++j) {
            sets[i][j].tag = 0;
            sets[i][j].valid = 0;
            sets[i][j].numops = 0;
        }
    }
}

/**
 * Deallocate cache data structures.
 *
 * This function deallocates (with free) the cache data structures of each
 * set and line.
 *
 * TODO: Implement
 */
static void free_cache() {
    for (int i = 0; i < S; ++i) {
        free(sets[i]);
    }
    free(sets);
}

/* Counters used to record cache statistics */
int miss_count     = 0;
int hit_count      = 0;
int eviction_count = 0;

/**
 * Simulate a memory access.
 *
 * If the line is already in the cache, increase `hit_count`; otherwise,
 * increase `miss_count`; increase `eviction_count` if another line must be
 * evicted. This function also updates the metadata used to implement eviction
 * policies (LRU, FIFO).
 *
 * TODO: Implement
 */
static void access_data(unsigned long addr) {
    int b = INT_LOG2(B);
    int s = INT_LOG2(S);
    int set_ind, t;
    set_ind = (int) ((addr >> b) & ((1L << s) - 1));
    t = (int) (addr >> (b + s));
    for (int i = 0; i < K; ++i) {
        if ((sets[set_ind][i].tag == t) && sets[set_ind][i].valid) {
            hit_count++;
            if (policy == LRU) {
                for (int j = 0; j < K; ++j) {
                    sets[set_ind][j].numops--;
                }
                sets[set_ind][i].numops = K;
            }
            if (verbose) {
                printf("hit ");
            }
            return;
        }
    }
    miss_count++;
    if (verbose) {
        printf("miss ");
    }
    int lowind = 0;
    for (int k = 0; k < K; ++k) {
        if (sets[set_ind][k].numops < sets[set_ind][lowind].numops) {
            lowind = k;
        }
    }
    for (int j = 0; j < K; ++j) {
        sets[set_ind][j].numops--;
    }
    sets[set_ind][lowind].numops = K;
    if (sets[set_ind][lowind].valid) {
        eviction_count++;
        if (verbose) {
            printf("eviction ");
        }
    }
    sets[set_ind][lowind].tag = t;
    sets[set_ind][lowind].valid = 1;
}

/**
 * Replay the input trace.
 *
 * This function:
 * - reads lines (e.g., using fgets) from the file handle `trace_fp` (a global variable)
 * - skips lines not starting with ` S`, ` L` or ` M`
 * - parses the memory address (unsigned long, in hex) and len (unsigned int, in decimal)
 *   from each input line
 * - calls `access_data(address)` for each access to a cache line
 *
 * TODO: Implement
 */
static void replay_trace() {
    char op;
    unsigned int bytes;
    unsigned long add;    
    while (fscanf(trace_fp,"%c",&op) != EOF) {
        if (op == ' ') {
            if ((fscanf(trace_fp,"%c",&op) != EOF)) {
                if (op == 'M' || op == 'L' || op == 'S') {
                    fscanf(trace_fp,"%lx,%u",&add,&bytes);
                    if (verbose) {
                        printf("%c %lx,%u ",op,add,bytes);
                    }
                    if (op == 'L') {
                        access_data(add);
                        for (unsigned long m = add + 1; m < (add + bytes); ++m) {
                            if (m % B == 0) {
                                access_data(m);
                            }
                        }
                    }
                    else if (op == 'S') {
                        access_data(add);
                        for (unsigned long m = add + 1; m < (add + bytes); ++m) {
                            if (m % B == 0) {
                                access_data(m);
                            }
                        }
                    }
                    else if (op == 'M') {
                        access_data(add);
                        access_data(add);
                        for (unsigned long m = add + 1; m < (add + bytes); ++m) {
                            if (m % B == 0) {
                                access_data(m);
                                access_data(m);
                            }
                        }
                    }
                }
            }
        }
        if (verbose && (op == 'M' || op == 'L' || op == 'S')) {
            printf("\n");
        }
    }
}

/**
 * Print cache statistics (DO NOT MODIFY).
 */
static void print_summary(int hits, int misses, int evictions) {
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
}

int main(int argc, char **argv) {
    parse_arguments(argc, argv);  // set global variables used by simulation
    allocate_cache();             // allocate data structures of cache
    replay_trace();               // simulate the trace and update counts
    free_cache();                 // deallocate data structures of cache
    fclose(trace_fp);             // close trace file
    print_summary(hit_count, miss_count, eviction_count);  // print counts
    return 0;
}
