/* Code to save the iptables state, in human readable-form. */
/* (C) 1999 by Paul 'Rusty' Russell <rusty@rustcorp.com.au> and
 * (C) 2000-2002 by Harald Welte <laforge@gnumonks.org>
 *
 * This code is distributed under the terms of GNU GPL v2
 *
 */
#include "iptables-multi.h"
#include "iptables.h"
#include "libiptc/libiptc.h"
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int show_counters;

static const struct option options[] = {
    {.name = "counters", .has_arg = false, .val = 'c'},
    {.name = "dump", .has_arg = false, .val = 'd'},
    {.name = "table", .has_arg = true, .val = 't'},
    {.name = "modprobe", .has_arg = true, .val = 'M'},
    {NULL},
};

/* Debugging prototype. */
static int for_each_table(int (*func)(const char *tablename)) {
    int ret = 1;
    FILE *procfile = NULL;
    char tablename[XT_TABLE_MAXNAMELEN + 1];
    static const char filename[] = "/proc/net/ip_tables_names";

    procfile = fopen(filename, "re");
    if (!procfile) {
        if (errno == ENOENT)
            return ret;
        fprintf(stderr, "Failed to list table names in %s: %s\n", filename,
                strerror(errno));
        exit(1);
    }

    while (fgets(tablename, sizeof(tablename), procfile)) {
        if (tablename[strlen(tablename) - 1] != '\n')
            xtables_error(OTHER_PROBLEM, "Badly formed tablename `%s'\n",
                          tablename);
        tablename[strlen(tablename) - 1] = '\0';
        ret &= func(tablename);
    }

    fclose(procfile);
    return ret;
}

static int do_output(const char *tablename) {
    struct xtc_handle *h;
    const char *chain = NULL;

    if (!tablename)
        return for_each_table(&do_output);

    h = iptc_init(tablename);
    if (h == NULL) {
        xtables_load_ko(xtables_modprobe_program, false);
        h = iptc_init(tablename);
    }
    if (!h)
        xtables_error(OTHER_PROBLEM, "Cannot initialize: %s\n",
                      iptc_strerror(errno));

    time_t now = time(NULL);

    printf("# Generated by iptables-save v%s on %s", IPTABLES_VERSION,
           ctime(&now));
    printf("*%s\n", tablename);

    /* Dump out chain names first,
     * thereby preventing dependency conflicts */
    for (chain = iptc_first_chain(h); chain; chain = iptc_next_chain(h)) {

        printf(":%s ", chain);
        if (iptc_builtin(chain, h)) {
            struct xt_counters count;
            printf("%s ", iptc_get_policy(chain, &count, h));
            printf("[%llu:%llu]\n", (unsigned long long)count.pcnt,
                   (unsigned long long)count.bcnt);
        } else {
            printf("- [0:0]\n");
        }
    }

    for (chain = iptc_first_chain(h); chain; chain = iptc_next_chain(h)) {
        const struct ipt_entry *e;

        /* Dump out rules */
        e = iptc_first_rule(chain, h);
        while (e) {
            print_rule4(e, h, chain, show_counters);
            e = iptc_next_rule(e, h);
        }
    }

    now = time(NULL);
    printf("COMMIT\n");
    printf("# Completed on %s", ctime(&now));
    iptc_free(h);

    return 1;
}

/* Format:
 * :Chain name POLICY packets bytes
 * rule
 */
int iptables_save_main(int argc, char *argv[]) {
    const char *tablename = NULL;
    int c;

    iptables_globals.program_name = "iptables-save";
    c = xtables_init_all(&iptables_globals, NFPROTO_IPV4);
    if (c < 0) {
        fprintf(stderr, "%s/%s Failed to initialize xtables\n",
                iptables_globals.program_name,
                iptables_globals.program_version);
        exit(1);
    }
#if defined(ALL_INCLUSIVE) || defined(NO_SHARED_LIBS)
    init_extensions();
    init_extensions4();
#endif

    while ((c = getopt_long(argc, argv, "bcdt:M:", options, NULL)) != -1) {
        switch (c) {
        case 'b':
            fprintf(stderr, "-b/--binary option is not implemented\n");
            break;
        case 'c':
            show_counters = 1;
            break;

        case 't':
            /* Select specific table. */
            tablename = optarg;
            break;
        case 'M':
            xtables_modprobe_program = optarg;
            break;
        case 'd':
            do_output(tablename);
            exit(0);
        default:
            fprintf(stderr, "Look at manual page `iptables-save.8' for more "
                            "information.\n");
            exit(1);
        }
    }

    if (optind < argc) {
        fprintf(stderr, "Unknown arguments found on commandline\n");
        exit(1);
    }

    return !do_output(tablename);
}
