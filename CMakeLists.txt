cmake_minimum_required(VERSION 3.7)

include_directories(include)
include_directories(iptables)
include_directories(libnftnl/include)
include_directories(.)
# todo
set(libxtables_vmajor "2")

add_subdirectory(libipq)
add_subdirectory(libiptc)
add_subdirectory(libxtables)
add_subdirectory(extensions)
add_subdirectory(iptables)

configure_file(include/xtables-version.h.in include/xtables-version.h)
configure_file(include/iptables/internal.h.in include/iptables/internal.h)
