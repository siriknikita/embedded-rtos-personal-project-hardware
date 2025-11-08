/* config/lwipopts.h — RP2040 (Pico W) + CYW43 + FreeRTOS (NO_SYS=0).
 * Works with pico_cyw43_arch_lwip_sys_freertos.
 */
#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* ===== System / OS integration ===== */
#define NO_SYS                         0
#define SYS_LIGHTWEIGHT_PROT           1
#define LWIP_TCPIP_CORE_LOCKING        1

/* ===== Memory ===== */
#define MEM_LIBC_MALLOC                1
#define MEMP_MEM_MALLOC                1
#define MEM_ALIGNMENT                  4

/* ===== Protocols ===== */
#define LWIP_IPV4                      1
#define LWIP_IPV6                      0
#define LWIP_ETHERNET                  1
#define LWIP_ARP                       1
#define LWIP_ICMP                      1
#define LWIP_RAW                       0

/* ===== High-level APIs =====
 * We are using BSD-like sockets (getaddrinfo/connect/read/write).
 */
#define LWIP_NETCONN                   1
#define LWIP_SOCKET                    1

/* Prevent timeval redefinition: Pico’s toolchain already provides it */
#define LWIP_TIMEVAL_PRIVATE           0

/* (Optional) keep BSD names without macro remaps; 1 is also fine.
 * If you see name clashes/macros you dislike, set to 2. */
#define LWIP_COMPAT_SOCKETS            1

/* ===== DHCP / DNS ===== */
#define LWIP_DHCP                      1
#define LWIP_DNS                       1
#define DNS_TABLE_SIZE                 4
#define DNS_MAX_NAME_LENGTH            256

/* ===== UDP / TCP ===== */
#define LWIP_UDP                       1
#define LWIP_TCP                       1
#define TCP_MSS                        1460
#define TCP_SND_BUF                    (8 * TCP_MSS)
#define TCP_WND                        (8 * TCP_MSS)
#define TCP_QUEUE_OOSEQ                0

/* ===== Buffers ===== */
#define PBUF_POOL_SIZE                 32

/* ===== Netif callbacks ===== */
#define LWIP_NETIF_STATUS_CALLBACK     1
#define LWIP_NETIF_LINK_CALLBACK       1

/* ===== SO options / timeouts ===== */
#define LWIP_SO_SNDTIMEO               1
#define LWIP_SO_RCVTIMEO               1

/* ===== Stats / debug ===== */
#define LWIP_STATS                     0
#define LWIP_STATS_DISPLAY             0

/* ===== Misc ===== */
#define LWIP_PROVIDE_ERRNO             1

#endif /* LWIPOPTS_H */
