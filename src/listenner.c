#include <stdio.h>
#include <stdlib.h>

#include <errno.h>

#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include "defines.h"
#include "packet.h"
#include "utils.h"

void *listenner(void *bridge) {
    communicator_t *brd = bridge;
    request_t *req = brd->request;
    uint8_t *stop = brd->stop;
    SOCKET client = brd->client;
    size_t seek_count = req->seek_count;
    SOCKET sock;
    uint8_t buffer[5000];
    IP_HEADER *ip_hdr;
    TCP_HEADER *tcp_hdr;
    port_t ports_possible[POSSIBLE_PORTS_SIZE] = {1234, 1235, 1236, 1237, 1238, 1239, 2345, 2346, 
                                         2347, 2348, 3456, 3457, 3458, 3459, 4567, 4568, 
                                         4569, 5678, 5679, 1111, 1212, 1313, 1414, 1515,
                                         1616, 1717, 1818, 1919, 2222, 2323, 2424, 2525};
    uint32_t key[2] = {696969, 262626};
    size_t found = 0;
    struct in_addr tmp;
    int r;
    uint8_t infinite = 0;
    size_t port_count = req->port_count;
    port_t *ports = req->seek_port;
    char *err_buffer = NULL;
    int j = 0;
    time_t end = 0;

    sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock < 0) {
        perror("socket");
        return NULL;
    }

    if (seek_count == 0) {
        infinite = 1;
        printf("Listenner thread start [Infinite]\n");
    } else
        printf("Listenner thread start [SeekCount]\n");

    while (found < seek_count || infinite)
    {
        r = recv(sock, buffer, 5000, 0);
        if (r < 0)
            perror("recv");
        if (r == 0)
            printf("empty request or socket shutdown\n");
        else {
            ip_hdr = (IP_HEADER *)buffer;
            tcp_hdr = (TCP_HEADER *)(buffer + ip_hdr->ihl * 4);
            tmp.s_addr = ip_hdr->saddr;
            for (size_t i = 0; i < port_count; i++) {
                if (tcp_hdr->th_flags == (TH_SYN | TH_ACK) && (ntohs(tcp_hdr->th_dport) == ports_possible[GET_PORT(ip_hdr->saddr, ports[i], key[(time(NULL)/10)%2])])) {
                    char ret[23];
                    int len;
                    int t;
                    memset(ret, 0, 23);
                    snprintf(ret, 23, "%s:%d\n", inet_ntoa(tmp), ntohs(tcp_hdr->th_sport));
                    len = strlen(ret);
                    t = send(client, ret, len, 0);
                    if ((t < 0) && (errno == EAGAIN)) {
                        if (err_buffer == NULL)
                            err_buffer = malloc(len + 1);
                        else
                            err_buffer = realloc(err_buffer, strlen(err_buffer) + len + 1);
                        memcpy(&err_buffer[j], ret, strlen(ret));
                        j += len;
                        err_buffer[j] = 0;
                        printf("In err_buffer\n");
                    }
                    found++;
                }
            }
        }
        if (infinite && stop[0])
            end = req->finished_at + 20;
        if (stop[0] && time(NULL) >= end)
            break;
    }
    stop[0] = 1;
    found = 0;

    send(client, "end\n", 4, 0);

    close(sock);

    printf("End listenner\n");

    return NULL;
}