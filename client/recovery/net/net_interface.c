/*
 *  Copyright (C) 2016, Zhang YanMing <jamincheung@126.com>
 *
 *  Linux recovery updater
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <netinet/if_ether.h>
#include <linux/sockios.h>
#include <pthread.h>

#include <utils/log.h>
#include <utils/assert.h>
#include <net/net_interface.h>

#define LOG_TAG "net_interface"

#define ETHTOOL_GLINK           0x0000000a
#define SIOCETHTOOL             0x8946

#define ICMP_MAX_RETRY          5
#define ICMP_MAXPACKET_SIZE     256
#define ICMP_HEADSIZE           8
#define ICMP_DATA_DEF_LEN       56
#define ICMP_MAX_LEN            76
#define IP_MAX_LEN              60

struct ethtool_value
{
    unsigned int cmd;
    unsigned int data;
};

static void msleep(long long msec) {
    struct timespec ts;
    int err;

    ts.tv_sec = (msec / 1000);
    ts.tv_nsec = (msec % 1000) * 1000 * 1000;

    do {
        err = nanosleep(&ts, &ts);
    } while (err < 0 && errno == EINTR);
}

static void init_ifr(struct net_interface* this, struct ifreq* ifr) {
    memset(ifr, 0, sizeof(struct ifreq));
    strncpy(ifr->ifr_name, this->if_name, IFNAMSIZ);
    ifr->ifr_name[IFNAMSIZ - 1] = '\0';
}

static void init_sockaddr_in(struct net_interface* this, struct sockaddr* sa,
        in_addr_t addr) {
    struct sockaddr_in *sin = (struct sockaddr_in*) sa;
    sin->sin_family = AF_INET;
    sin->sin_port = 0;
    sin->sin_addr.s_addr = addr;
}

static int set_flags(struct net_interface* this, unsigned int set,
        unsigned int clr) {
    struct ifreq ifr;
    init_ifr(this, &ifr);

    int error = ioctl(this->socket, SIOCGIFFLAGS, &ifr);
    if (error < 0) {
        LOGE("ioctl(socket, SIOCGIFFLAGS, &ifr) failed: %s\n", strerror(errno));
        return error;
    }

    ifr.ifr_flags = (ifr.ifr_flags & (~clr)) | set;

    error = ioctl(this->socket, SIOCSIFFLAGS, &ifr);
    if (error < 0)
        LOGE("ioctl(socket, SIOCSIFFLAGS, &ifr) failed: %s\n", strerror(errno));

    return error;
}

static int init_socket(struct net_interface* this) {
    if (this->socket == -1) {
        this->socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (this->socket < 0)
            LOGE("Failed to create AF_INET socket: %s\n", strerror(errno));
    }

    if (this->icmp_socket == -1) {
        this->icmp_socket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        if (this->icmp_socket < 0) {
            LOGE("Failed to create AF_INET icmp socket: %s\n", strerror(errno));
            return -1;

        } else {
            int flag = 0;
            if (fcntl(this->icmp_socket, F_GETFL, 0) < 0) {
                LOGE("Failed to get icmp socket flag: %s\n", strerror(errno));
                return -1;
            }

            if (fcntl(this->icmp_socket, F_SETFL, flag | O_NONBLOCK) < 0) {
                LOGE("Failed to set icmp socket flag: %s\n", strerror(errno));
                return -1;
            }

            int sockopt = 1;
            setsockopt(this->icmp_socket, SOL_SOCKET, SO_BROADCAST,
                    (char *) &sockopt, sizeof(sockopt));

            sockopt = 8 * 1024;
            setsockopt(this->icmp_socket, SOL_SOCKET, SO_RCVBUF,
                    (char *) &sockopt, sizeof(sockopt));
        }
    }

    return this->socket;
}

static int cal_chksum(unsigned short *buf, int size) {
    int sum = 0;

    while (size > 1) {
        sum += *buf++;
        size -= sizeof(unsigned short);
    }

    if (size == 1)
        sum += *(unsigned char *) buf;

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);

    return (~sum);
}

static bool icmp_echo(struct net_interface* this, const char* host, int timeout) {
    int n;
    int packet_size;
    int try_count = 0;
    struct iphdr *ip_hdr;
    struct icmp *icmp_pkt;
    unsigned char send_packet[ICMP_DATA_DEF_LEN + ICMP_MINLEN + 4];
    unsigned char recv_packet[ICMP_DATA_DEF_LEN + ICMP_MAX_LEN + IP_MAX_LEN];
    struct sockaddr_in dest;
    pid_t pid = -1;

    assert_die_if(host == NULL, "server address is null\n");
    assert_die_if(timeout <= 0, "timeout is unavailable\n");

    memset(&dest, 0, sizeof(dest));

    init_sockaddr_in(this, (struct sockaddr*) &dest, inet_addr(host));

    pid = getpid() & 0xffff;
    while (try_count < ICMP_MAX_RETRY) {
        icmp_pkt = (struct icmp *) send_packet;

        icmp_pkt->icmp_type = ICMP_ECHO;
        icmp_pkt->icmp_code = 0;
        icmp_pkt->icmp_cksum = 0;
        icmp_pkt->icmp_seq = htons(try_count++);
        icmp_pkt->icmp_id = pid;

        packet_size = ICMP_MINLEN + ICMP_DATA_DEF_LEN;

        icmp_pkt->icmp_cksum = cal_chksum((unsigned short *) send_packet,
                packet_size);

        LOGD("Start sending icmp packet to %s, try %d\n", host, try_count);
        n = sendto(this->icmp_socket, send_packet, packet_size, 0,
                (struct sockaddr *) &dest, sizeof(struct sockaddr_in));
        if (n != packet_size) {
            LOGE("Failed to send icmp packet to %s: %s\n", host, strerror(errno));
            return false;
        }

        while (1) {
            struct sockaddr_in from;
            socklen_t fromlen = (socklen_t) sizeof(from);

            if (timeout > 0) {
                struct pollfd fds;

                fds.fd = this->icmp_socket;
                fds.events = POLLIN;

                do {
                    n = poll(&fds, 1, timeout);
                } while (n < 0 && errno == EINTR);

                if (n < 0)
                    break;
            }

            n = recvfrom(this->icmp_socket, recv_packet, sizeof(recv_packet), 0,
                    (struct sockaddr *) &from, &fromlen);
            if (n < ICMP_DATA_DEF_LEN)
                break;

            ip_hdr = (struct iphdr *) recv_packet;
            icmp_pkt = (struct icmp *) (recv_packet + (ip_hdr->ihl << 2));
            if (icmp_pkt->icmp_id == pid
                    || icmp_pkt->icmp_type == ICMP_ECHOREPLY) {
                LOGD("Sucess to sending icmp packet to %s\n", host);
                return true;
            }
        }
    }

    LOGE("Failed to send icmp packet to %s: %s\n", host, strerror(errno));

    return false;
}

static int get_hwaddr(struct net_interface* this, unsigned char* hwaddr) {
    struct ifreq ifr;
    init_ifr(this, &ifr);

    int error = ioctl(this->socket, SIOCGIFHWADDR, &ifr);
    if (error < 0) {
        LOGE("ioctl(socket, SIOCGIFHWADDR, &ifr) failed: %s\n", strerror(errno));
        return error;
    }

    memcpy(hwaddr, &ifr.ifr_hwaddr.sa_data, ETH_ALEN);

    return error;
}

static int set_hwaddr(struct net_interface* this, const unsigned char* hwaddr) {
    struct ifreq ifr;
    init_ifr(this, &ifr);

    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
    memcpy(&ifr.ifr_hwaddr.sa_data, hwaddr, ETH_ALEN);

    int error = ioctl(this->socket, SIOCSIFHWADDR, &ifr);
    if (error < 0)
        LOGE("ioctl(socket, SIOCSIFHWADDR, &ifr) failed: %s\n", strerror(errno));

    return error;
}

static int get_addr(struct net_interface* this, in_addr_t* addr) {
    struct ifreq ifr;
    init_ifr(this, &ifr);

    int error = ioctl(this->socket, SIOCGIFADDR, &ifr);
    if (error < 0) {
        LOGE("ioctl(socket, SIOCGIFADDR, &ifr) failed: %s\n", strerror(errno));
        return error;
    }

    *addr = ((struct sockaddr_in*) &ifr.ifr_addr)->sin_addr.s_addr;

    return error;
}

static int set_addr(struct net_interface* this, in_addr_t addr) {
    struct ifreq ifr;
    init_ifr(this, &ifr);
    init_sockaddr_in(this, &ifr.ifr_addr, addr);

    int error = ioctl(this->socket, SIOCSIFADDR, &ifr);
    if (error < 0)
        LOGE("ioctl(socket, SIOCSIFADDR, &ifr) failed: %s\n", strerror(errno));

    return error;
}

static int up(struct net_interface* this) {
    return set_flags(this, IFF_UP, 0);
}

static int down(struct net_interface* this) {
    return set_flags(this, 0, IFF_UP);
}

__attribute__((unused)) static cable_state_t cable_detect_ethtool(struct net_interface* this) {
    if (up(this)) {
        LOGE("Failed to turn on interface %s: %s\n", this->if_name,
                strerror(errno));
        return CABLE_STATE_ERR;
    }

    struct ifreq ifr;
    init_ifr(this, &ifr);

    struct ethtool_value edata;
    edata.cmd = ETHTOOL_GLINK;
    ifr.ifr_data = (caddr_t) & edata;

    int err = ioctl(this->socket, SIOCETHTOOL, &ifr);
    if (err) {
        LOGE("ioctl(socket, SIOCETHTOOL, &ifr) failed: %s\n", strerror(errno));
        return CABLE_STATE_ERR;
    }

    return edata.data ? CABLE_STATE_PLUGIN : CABLE_STATE_UNPLUGIN;
}

__attribute__((unused)) static cable_state_t cable_detect_priv(
        struct net_interface* this) {
    if (up(this)) {
        LOGE("Failed to turn on interface %s: %s\n", this->if_name,
                strerror(errno));
        return CABLE_STATE_ERR;
    }

    struct ifreq ifr;
    init_ifr(this, &ifr);

    int err = ioctl(this->socket, SIOCDEVPRIVATE, &ifr);
    if (err) {
        LOGE("ioctl(socket, SIOCDEVPRIVATE, &ifr) failed: %s\n", strerror(errno));
        return CABLE_STATE_ERR;
    }

    ((unsigned short*) &ifr.ifr_data)[1] = 1;
    err = ioctl(this->socket, SIOCDEVPRIVATE + 1, &ifr);
    if (err) {
        LOGE("ioctl(socket, SIOCDEVPRIVATE + 1, &ifr) failed: %s\n",
                strerror(errno));
        return CABLE_STATE_ERR;
    }

    return (((unsigned short*) &ifr.ifr_data)[3] & 0x0004) ?
            CABLE_STATE_PLUGIN : CABLE_STATE_UNPLUGIN;
}

__attribute__((unused)) static cable_state_t cable_detect_iff(
        struct net_interface* this) {
    if (up(this)) {
        LOGE("Failed to turn on interface %s: %s\n", this->if_name,
                strerror(errno));
        return CABLE_STATE_ERR;
    }

    struct ifreq ifr;
    init_ifr(this, &ifr);

    int err = ioctl(this->socket, SIOCGIFFLAGS, &ifr);
    if (err) {
        LOGE("ioctl(socket, SIOCGIFFLAGS, &ifr) failed: %s\n", strerror(errno));
        return CABLE_STATE_ERR;
    }

    return ifr.ifr_flags & IFF_RUNNING ?
            CABLE_STATE_PLUGIN : CABLE_STATE_UNPLUGIN;
}

__attribute__((unused)) static cable_state_t cable_detect_mii(
        struct net_interface* this) {
    if (up(this)) {
        LOGE("Failed to turn on interface %s: %s\n", this->if_name,
                strerror(errno));
        return CABLE_STATE_ERR;
    }

    struct ifreq ifr;
    init_ifr(this, &ifr);

    int err = ioctl(this->socket, SIOCGMIIPHY, &ifr);
    if (err) {
        LOGE("ioctl(socket, SIOCGMIIPHY, &ifr) failed: %s\n", strerror(errno));
        return CABLE_STATE_ERR;
    }

    ((unsigned short*) &ifr.ifr_data)[1] = 1;
    err = ioctl(this->socket, SIOCGMIIREG, &ifr);
    if (err) {
        LOGE("ioctl(socket, SIOCGMIIREG, &ifr) failed: %s\n", strerror(errno));
        return CABLE_STATE_ERR;
    }

    return (((unsigned short*) &ifr.ifr_data)[3] & 0x0004) ?
            CABLE_STATE_PLUGIN : CABLE_STATE_UNPLUGIN;
}

static cable_state_t get_cable_state(struct net_interface* this) {
    return cable_detect_iff(this);
}

static void close_socket(struct net_interface* this) {
    if (this->socket > 0)
        close(this->socket);

    if (this->icmp_socket > 0)
        close(this->icmp_socket);

    this->socket = -1;
    this->icmp_socket = -1;
}

void *detector_loop(void *param) {
    struct net_interface* this = (struct net_interface*) param;

    cable_state_t state;

    for (;;) {
        pthread_mutex_lock(&this->detect_lock);
        while (this->detect_stopped) {
            if (this->detect_exit) {
                pthread_mutex_unlock(&this->detect_lock);
                goto out;
            }
            pthread_cond_wait(&this->detect_cond, &this->detect_lock);
        }
        pthread_mutex_unlock(&this->detect_lock);

        msleep(100);

        state = this->get_cable_state(this);

        if (state != this->cable_status) {
            this->cable_status = state;
            pthread_mutex_lock(&this->detect_lock);
            if (!this->detect_stopped)
                this->detect_listener(this->detect_param,
                        this->cable_status == CABLE_STATE_PLUGIN);
            pthread_mutex_unlock(&this->detect_lock);
        }

    }

out:
    return NULL;
}

static int start_cable_detector(struct net_interface* this,
        detect_listener_t listener, void* param) {
    assert_die_if(listener == NULL, "listener is NULL");

    int retval = 0;
    this->detect_listener = listener;
    this->detect_param = param;

    pthread_t tid;

    pthread_cond_init(&this->detect_cond, NULL);
    pthread_mutex_init(&this->detect_lock, NULL);

    retval = pthread_create(&tid, NULL, detector_loop, (void *) this);
    if (retval) {
        LOGE("pthread_create failed: %s\n", strerror(errno));
        return -1;
    }

    pthread_mutex_lock(&this->detect_lock);
    this->detect_stopped = false;
    pthread_cond_signal(&this->detect_cond);
    pthread_mutex_unlock(&this->detect_lock);

    return 0;
}

static void stop_cable_detector(struct net_interface* this) {
    pthread_mutex_lock(&this->detect_lock);
    this->detect_stopped = true;
    this->detect_exit = true;
    pthread_cond_signal(&this->detect_cond);
    pthread_mutex_unlock(&this->detect_lock);

    pthread_join(this->detect_pid, NULL);

    pthread_mutex_destroy(&this->detect_lock);
    pthread_cond_destroy(&this->detect_cond);
}

void construct_net_interface(struct net_interface* this, const char* if_name) {
    this->icmp_echo = icmp_echo;
    this->init_socket = init_socket;
    this->get_hwaddr = get_hwaddr;
    this->set_hwaddr = set_hwaddr;
    this->get_addr = get_addr;
    this->set_addr = set_addr;
    this->close_socket = close_socket;
    this->up = up;
    this->down = down;
    this->get_cable_state = get_cable_state;
    this->start_cable_detector = start_cable_detector;
    this->stop_cable_detector = stop_cable_detector;

    this->detect_listener = NULL;
    this->detect_param = NULL;
    this->if_name = strdup(if_name);
    this->socket = -1;
    this->icmp_socket = -1;
    this->detect_stopped = true;
    this->detect_exit = false;
    this->cable_status = CABLE_STATE_INIT;
}

void destruct_net_interface(struct net_interface* this) {
    this->icmp_echo = NULL;
    this->init_socket = NULL;
    this->get_hwaddr = NULL;
    this->set_hwaddr = NULL;
    this->get_addr = NULL;
    this->set_addr = NULL;
    this->close_socket = NULL;
    this->up = NULL;
    this->down = NULL;
    this->get_cable_state = NULL;
    this->start_cable_detector = NULL;
    this->stop_cable_detector = NULL;

    this->detect_listener = NULL;
    this->detect_param = NULL;
    this->socket = -1;
    this->icmp_socket = -1;

    if (this->if_name)
        free(this->if_name);
    this->if_name = NULL;
    this->detect_stopped = true;
    this->detect_exit = true;
    this->cable_status = false;
}
