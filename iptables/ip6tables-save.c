/* Code to save the ip6tables state, in human readable-form. */
/* Author:  Andras Kis-Szabo <kisza@sch.bme.hu>
 * Original code: iptables-save
 * Authors: Paul 'Rusty' Russel <rusty@linuxcare.com.au> and
 *          Harald Welte <laforge@gnumonks.org>
 * This code is distributed under the terms of GNU GPL v2
 */
#include "ip6tables-multi.h"
#include "ip6tables.h"
#include "libiptc/libip6tc.h"
#include <arpa/inet.h>
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
    static const char filename[] = "/proc/net/ip6_tables_names";

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

    h = ip6tc_init(tablename);
    if (h == NULL) {
        xtables_load_ko(xtables_modprobe_program, false);
        h = ip6tc_init(tablename);
    }
    if (!h)
        xtables_error(OTHER_PROBLEM, "Cannot initialize: %s\n",
                      ip6tc_strerror(errno));

    time_t now = time(NULL);

    printf("# Generated by ip6tables-save v%s on %s", IPTABLES_VERSION,
           ctime(&now));
    printf("*%s\n", tablename);

    /* Dump out chain names first,
     * thereby preventing dependency conflicts */
    for (chain = ip6tc_first_chain(h); chain; chain = ip6tc_next_chain(h)) {

        printf(":%s ", chain);
        if (ip6tc_builtin(chain, h)) {
            struct xt_counters count;
            printf("%s ", ip6tc_get_policy(chain, &count, h));
            printf("[%llu:%llu]\n", (unsigned long long)count.pcnt,
                   (unsigned long long)count.bcnt);
        } else {
            printf("- [0:0]\n");
        }
    }

    for (chain = ip6tc_first_chain(h); chain; chain = ip6tc_next_chain(h)) {
        const struct ip6t_entry *e;

        /* Dump out rules */
        e = ip6tc_first_rule(chain, h);
        while (e) {
            print_rule6(e, h, chain, show_counters);
            e = ip6tc_next_rule(e, h);
        }
    }

    now = time(NULL);
    printf("COMMIT\n");
    printf("# Completed on %s", ctime(&now));
    ip6tc_free(h);

    return 1;
}

/* Format:
 * :Chain name POLICY packets bytes
 * rule
 */
int ip6tables_save_main(int argc, char *argv[]) {
    const char *tablename = NULL;
    int c;

    ip6tables_globals.program_name = "ip6tables-save";
    c = xtables_init_all(&ip6tables_globals, NFPROTO_IPV6);
    if (c < 0) {
        fprintf(stderr, "%s/%s Failed to initialize xtables\n",
                ip6tables_globals.program_name,
                ip6tables_globals.program_version);
        exit(1);
    }
#if defined(ALL_INCLUSIVE) || defined(NO_SHARED_LIBS)
    init_extensions();
    init_extensions6();
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
            fprintf(stderr, "Look at manual page `ip6tables-save.8' for more "
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
