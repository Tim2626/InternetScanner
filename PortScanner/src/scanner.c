#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <time.h>

#include <pthread.h>

#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "defines.h"
#include "packet.h"
#include "utils.h"

#include "listenner.h"

#define DEBIT_OpS 1000000
#define ONE_SECONDE 1000000

static void send_to_ip_mask(SOCKET sock4, request_t *req, port_t *ports_possible, ipv4_t source_ip, SOCKADDRV4 dest_addr4, uint32_t *key, pthread_mutex_t *mutex) {
	uint32_t index = req->curr_addr;

	if (req->finished_at != 0)
		return;
	if (req->addresses[index].type == 4) {
		uint32_t bit_mask = ~((1 << (32 - req->addresses[index].CDIR)) - 1);
		ipv4_t first_ip = ntohl(req->addresses[index].addr.v4);
		ipv4_t dest_ip = (first_ip & bit_mask) + (req->scan_count / req->port_count);
		port_t dest_port = req->seek_port[req->scan_count % req->port_count];
		port_t source_port = htons(ports_possible[GET_PORT(dest_ip, dest_port, key[(time(NULL)/10)%2])]);
		packet_t packet;

		dest_addr4.sin_port = dest_port;
		create_packet4(&packet, source_ip, htonl(dest_ip), dest_port, source_port);
		pthread_mutex_unlock(mutex);
		if (sendto(sock4, &packet, sizeof(packet_t), 0, (struct sockaddr *)&dest_addr4, sizeof(SOCKADDRV4)) < 0) {
			struct in_addr tmp;
			tmp.s_addr = dest_ip;
			perror("sendto");
			printf("On %s:%d from %d\n", inet_ntoa(tmp), ntohs(dest_port), ntohs(source_port));
		}
		pthread_mutex_lock(mutex);
		req->scan_count++;
		if (req->scan_count >= (req->port_count * (1 << (32 - req->addresses[index].CDIR)))) {
			req->curr_addr++;
			req->scan_count = 0;
		}
		if (req->curr_addr == req->addr_count)
			req->finished_at = time(NULL);
	} else if (req->addresses[index].type == 6) {

	} else {
		// ERREUR inconnue
		// impossible de rentrer ivi en theorie
	}
}

uint32_t get_index(uint32_t rand, request_t *req) {
	for (size_t i = 0; i < req->addr_count; i++) {
		if (rand - req->addresses[i].ratio <= 0)
			return i;
	}
	return req->addr_count - 1;
}

ipv4_t get_random_ipv4(ipv4_t addr, uint8_t mask) {
	uint32_t bit_mask = ~((1 << (32 - mask)) - 1);
	ipv4_t ip = addr & (bit_mask >> (32 - mask)); 
	uint64_t rand = RANDOM() % (uint64_t)((uint64_t) 1 << (32 - mask));

	ip = ntohl(ip);
	ip += (uint32_t)rand;
	ip = htonl(ip);

	return ip;
}

ipv6_t get_random_ipv6(ipv6_t addr, uint8_t mask) {
	uint128_t bit_mask = ~((1 << (128 - mask)) - 1);
	ipv6_t ip;
	uint128_t rand = RANDOM();
	ipv6_t tmp;

	rand |= RANDOM() << 63;
	rand = rand % (uint128_t)((uint128_t) 1 << (128 - mask));

	for (int i = 0; i < 16; i++) {
		ip.__in6_u.__u6_addr8[i] = addr.__in6_u.__u6_addr8[i] & (bit_mask >> (128 - mask));
		tmp.__in6_u.__u6_addr8[i] = rand << 8 * i;
	}

	for (int i = 0; i < 16; i++)
		ip.__in6_u.__u6_addr8[i] += tmp.__in6_u.__u6_addr8[i];

	return ip;
}

port_t get_random_port(port_t *ports, size_t nb) {
	uint32_t rand = RANDOM() % nb;
	return *(ports + rand);
}

// TODO: rajouter socket pour ipv6 si faisable
static void send_to_ramdom_ip(SOCKET sock4, request_t *req, ipv4_t source_ip, port_t *ports_possible, uint32_t *key, SOCKADDRV4 dest_addr4, pthread_mutex_t *mutex) {
	uint32_t index;

	index = get_index(RANDOM() % req->somme_ratio, req);
	if (req->addresses[index].type == 4) {
		ipv4_t dest_ip = get_random_ipv4(req->addresses[index].addr.v4, req->addresses[index].CDIR);
		port_t dest_port = get_random_port(req->seek_port, req->port_count);
		port_t source_port = htons(ports_possible[GET_PORT(dest_ip, dest_port, key[(time(NULL)/10)%2])]);
		packet_t packet;

		dest_addr4.sin_port = dest_port;
		create_packet4(&packet, source_ip, dest_ip, dest_port, source_port);
		pthread_mutex_unlock(mutex);
		if (sendto(sock4, &packet, sizeof(packet_t), 0, (struct sockaddr *)&dest_addr4, sizeof(SOCKADDRV4)) < 0) {
			struct in_addr tmp;
			tmp.s_addr = dest_ip;
			perror("sendto");
			printf("On %s:%d from %d\n", inet_ntoa(tmp), ntohs(dest_port), ntohs(source_port));
		}
		pthread_mutex_lock(mutex);
	} else if (req->addresses[index].type == 6) {
		// TODO faire ipv6;
	} else {
		// ERREUR INCONNUE
		// impossible de rentrer ici normalement
	}

}

void *scanner(void *data) {
	reqlist_t *reqlist = (reqlist_t *)data;
	ipv4_t source_ip4 = inet_addr("10.18.205.61");
	//ipv6_t source_ip6;
	uint32_t key[2] = {696969, 262626};
	SOCKET sock4;
	//SOCKET sock6;
	SOCKADDRV4 dest_addr4;
	//SOCKADDRV6 dest_addr6;
	int dummy = 1;
	port_t ports_possible[POSSIBLE_PORTS_SIZE] = {1234, 1235, 1236, 1237, 1238, 1239, 2345, 2346, 
												  2347, 2348, 3456, 3457, 3458, 3459, 4567, 4568};
   // uint64_t wait_time = ONE_SECONDE / DEBIT_OpS / sizeof(packet_t);

	sock4 = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
	if (sock4 < 0) {
		perror("socket");
		return NULL;
	}
/*
	sock6 = socket(AF_INET6, SOCK_RAW, IPPROTO_TCP);
	if (sock6 < 0) {
		perror("socket");
		return NULL;
	}
*/
	// Header deja fait
	//  changer les option de la socket (kernel y touche pas)
	if (setsockopt(sock4, IPPROTO_IP, IP_HDRINCL, &dummy, sizeof(dummy)) < 0) {
		perror("setsockopt");
		return NULL;
	}
/*
	if (setsockopt(sock6, IPPROTO_IPV6, IPV6_HDRINCL, &dummy, sizeof(dummy)) < 0) {
		perror("setsockopt");
		return NULL;
	}
*/
	memset(&dest_addr4, 0, sizeof(SOCKADDRV4));
	//memset(&dest_addr6, 0, sizeof(SOCKADDRV6));

	dest_addr4.sin_addr.s_addr = inet_addr("10.51.0.7");
	dest_addr4.sin_family = AF_INET;
/*
	inet_pton(AF_INET6, "2a01:cb1d:22c:1b00:7117:5cee:2860:2b42", &source_ip6); // mon ipv6 local wlo1
	dest_addr6.sin6_family = AF_INET6;
	dest_addr6.sin6_port = 80;
	inet_pton(AF_INET6, "2001:4860:4860::8888", &dest_addr6.sin6_addr); // = 8.8.8.8
*/

	printf("Thread Scanner started\n");
	while (1)
	{
		pthread_mutex_lock(&reqlist->mutex);
		for (size_t i = 0; i < reqlist->len; i++) {
			// On itere requete par requete
			if (reqlist->ptr[i].request.seek_count == 0)
				send_to_ip_mask(sock4, &(reqlist->ptr[i].request), ports_possible, source_ip4, dest_addr4, key, &(reqlist->mutex));
			else if (reqlist->ptr[i].request.scan_count < reqlist->ptr[i].request.seek_count)
				send_to_ramdom_ip(sock4, &(reqlist->ptr[i].request), source_ip4, ports_possible, key, dest_addr4, &(reqlist->mutex));
			pthread_mutex_unlock(&reqlist->mutex);
			usleep(1);
			pthread_mutex_lock(&reqlist->mutex);
		}
		pthread_mutex_unlock(&reqlist->mutex);
	}

	close(sock4);
	//close(sock6);

	printf("End scanner\n");

	return NULL;
}
