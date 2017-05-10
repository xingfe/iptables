/*
 * Arturo Borrero Gonzalez <arturo@debian.org> adapted
 * this code to libxtables for arptables-compat in 2015
 */

#include "iptables/nft-arp.h"
#include "iptables/nft.h"
#include <getopt.h>
#include <limits.h>
#include <linux/netfilter_arp/arpt_mangle.h>
#include <netdb.h>
#include <netinet/ether.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xtables.h>

static void arpmangle_print_help(void) {
    printf("mangle target options:\n"
           "--mangle-ip-s IP address\n"
           "--mangle-ip-d IP address\n"
           "--mangle-mac-s MAC address\n"
           "--mangle-mac-d MAC address\n"
           "--mangle-target target (DROP, CONTINUE or ACCEPT -- default is "
           "ACCEPT)\n");
}

#define MANGLE_IPS '1'
#define MANGLE_IPT '2'
#define MANGLE_DEVS '3'
#define MANGLE_DEVT '4'
#define MANGLE_TARGET '5'

static struct option arpmangle_opts[] = {
    {.name = "mangle-ip-s", .has_arg = true, .val = MANGLE_IPS},
    {.name = "mangle-ip-d", .has_arg = true, .val = MANGLE_IPT},
    {.name = "mangle-mac-s", .has_arg = true, .val = MANGLE_DEVS},
    {.name = "mangle-mac-d", .has_arg = true, .val = MANGLE_DEVT},
    {.name = "mangle-target", .has_arg = true, .val = MANGLE_TARGET},
    XT_GETOPT_TABLEEND,
};

static void arpmangle_init(struct xt_entry_target *target) {
    struct arpt_mangle *mangle = (struct arpt_mangle *)target->data;

    mangle->target = NF_ACCEPT;
}

static int arpmangle_parse(int c, char **argv, int invert, unsigned int *flags,
                           const void *entry, struct xt_entry_target **target) {
    struct arpt_mangle *mangle = (struct arpt_mangle *)(*target)->data;
    struct in_addr *ipaddr, mask;
    struct ether_addr *macaddr;
    const struct arpt_entry *e = (const struct arpt_entry *)entry;
    unsigned int nr;
    int ret = 1;

    memset(&mask, 0, sizeof(mask));

    switch (c) {
    case MANGLE_IPS:
        xtables_ipparse_any(optarg, &ipaddr, &mask, &nr);
        mangle->u_s.src_ip.s_addr = ipaddr->s_addr;
        free(ipaddr);
        mangle->flags |= ARPT_MANGLE_SIP;
        break;
    case MANGLE_IPT:
        xtables_ipparse_any(optarg, &ipaddr, &mask, &nr);
        mangle->u_t.tgt_ip.s_addr = ipaddr->s_addr;
        free(ipaddr);
        mangle->flags |= ARPT_MANGLE_TIP;
        break;
    case MANGLE_DEVS:
        if (e->arp.arhln_mask == 0)
            xtables_error(PARAMETER_PROBLEM, "no --h-length defined");
        if (e->arp.invflags & ARPT_INV_ARPHLN)
            xtables_error(PARAMETER_PROBLEM, "! --h-length not allowed for "
                                             "--mangle-mac-s");
        if (e->arp.arhln != 6)
            xtables_error(PARAMETER_PROBLEM, "only --h-length 6 supported");
        macaddr = ether_aton(optarg);
        if (macaddr == NULL)
            xtables_error(PARAMETER_PROBLEM, "invalid source MAC");
        memcpy(mangle->src_devaddr, macaddr, e->arp.arhln);
        mangle->flags |= ARPT_MANGLE_SDEV;
        break;
    case MANGLE_DEVT:
        if (e->arp.arhln_mask == 0)
            xtables_error(PARAMETER_PROBLEM, "no --h-length defined");
        if (e->arp.invflags & ARPT_INV_ARPHLN)
            xtables_error(PARAMETER_PROBLEM,
                          "! hln not allowed for --mangle-mac-d");
        if (e->arp.arhln != 6)
            xtables_error(PARAMETER_PROBLEM, "only --h-length 6 supported");
        macaddr = ether_aton(optarg);
        if (macaddr == NULL)
            xtables_error(PARAMETER_PROBLEM, "invalid target MAC");
        memcpy(mangle->tgt_devaddr, macaddr, e->arp.arhln);
        mangle->flags |= ARPT_MANGLE_TDEV;
        break;
    case MANGLE_TARGET:
        if (!strcmp(optarg, "DROP"))
            mangle->target = NF_DROP;
        else if (!strcmp(optarg, "ACCEPT"))
            mangle->target = NF_ACCEPT;
        else if (!strcmp(optarg, "CONTINUE"))
            mangle->target = XT_CONTINUE;
        else
            xtables_error(PARAMETER_PROBLEM, "bad target for --mangle-target");
        break;
    default:
        ret = 0;
    }

    return ret;
}

static void arpmangle_final_check(unsigned int flags) {}

static void print_mac(const unsigned char *mac, int l) {
    int j;

    for (j = 0; j < l; j++)
        printf("%02x%s", mac[j], (j == l - 1) ? "" : ":");
}

static void arpmangle_print(const void *ip,
                            const struct xt_entry_target *target, int numeric) {
    struct arpt_mangle *m = (struct arpt_mangle *)(target->data);
    char buf[100];

    if (m->flags & ARPT_MANGLE_SIP) {
        if (numeric)
            sprintf(buf, "%s", xtables_ipaddr_to_numeric(&(m->u_s.src_ip)));
        else
            sprintf(buf, "%s", xtables_ipaddr_to_anyname(&(m->u_s.src_ip)));
        printf("--mangle-ip-s %s ", buf);
    }
    if (m->flags & ARPT_MANGLE_SDEV) {
        printf("--mangle-mac-s ");
        print_mac((unsigned char *)m->src_devaddr, 6);
        printf(" ");
    }
    if (m->flags & ARPT_MANGLE_TIP) {
        if (numeric)
            sprintf(buf, "%s", xtables_ipaddr_to_numeric(&(m->u_t.tgt_ip)));
        else
            sprintf(buf, "%s", xtables_ipaddr_to_anyname(&(m->u_t.tgt_ip)));
        printf("--mangle-ip-d %s ", buf);
    }
    if (m->flags & ARPT_MANGLE_TDEV) {
        printf("--mangle-mac-d ");
        print_mac((unsigned char *)m->tgt_devaddr, 6);
        printf(" ");
    }
    if (m->target != NF_ACCEPT) {
        printf("--mangle-target ");
        if (m->target == NF_DROP)
            printf("DROP ");
        else
            printf("CONTINUE ");
    }
}

static struct xtables_target arpmangle_target = {
    .name = "mangle",
    .revision = 0,
    .version = XTABLES_VERSION,
    .family = NFPROTO_ARP,
    .size = XT_ALIGN(sizeof(struct arpt_mangle)),
    .userspacesize = XT_ALIGN(sizeof(struct arpt_mangle)),
    .help = arpmangle_print_help,
    .init = arpmangle_init,
    .parse = arpmangle_parse,
    .final_check = arpmangle_final_check,
    .print = arpmangle_print,
    .extra_opts = arpmangle_opts,
};

void _init(void) { xtables_register_target(&arpmangle_target); }