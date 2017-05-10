/* Shared library add-on to iptables to add TRACE target support. */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/netfilter/x_tables.h>
#include <xtables.h>

static int trace_xlate(struct xt_xlate *xl,
                       const struct xt_xlate_tg_params *params) {
    xt_xlate_add(xl, "nftrace set 1");
    return 1;
}

static struct xtables_target trace_target = {
    .family = NFPROTO_UNSPEC,
    .name = "TRACE",
    .version = XTABLES_VERSION,
    .size = XT_ALIGN(0),
    .userspacesize = XT_ALIGN(0),
    .xlate = trace_xlate,
};

void _init(void) { xtables_register_target(&trace_target); }
