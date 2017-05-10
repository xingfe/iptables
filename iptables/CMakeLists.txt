
add_library(ip6tables
  ip6tables.c ip6tables-restore.c ip6tables-save.c ip6tables-standalone.c
  iptables.c iptables-restore.c iptables-save.c iptables-standalone.c)

add_library(nft nft-arp.c)
