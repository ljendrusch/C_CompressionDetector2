
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "utils.h"


void clean_exit(){exit(0);}

int main(int argc, char const* argv[])
{
	int port_tcp;
	if (argc < 2 || (port_tcp = atoi(argv[1])) <= 0 || port_tcp > 65535)
	{
		printf("  Must supply port number 0 < n < 65536, e.g.\n    ./server 8787\n");
		exit(1);
	}

	// graceful exit on ctrl-c ctrl-z
	signal(SIGTERM, clean_exit);
	signal(SIGINT, clean_exit);

	// TCP listen
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
	{
		perror("socket");
		exit(1);
	}

	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | 15, &opt, sizeof(opt)) < 0) {
		perror("setsockopt");
		exit(1);
	}

	struct sockaddr_in self_addr, clnt_addr;
	self_addr.sin_family = AF_INET;
	self_addr.sin_port = htons(port_tcp);
	self_addr.sin_addr.s_addr = INADDR_ANY;

	clnt_addr.sin_family = AF_INET;
	clnt_addr.sin_port = htons(port_tcp);
	if (bind(server_fd, (struct sockaddr*)&self_addr, sizeof(self_addr)) < 0)
	{
		perror("bind");
		exit(1);
	}

	if (listen(server_fd, 5) < 0) {
		perror("listen");
		exit(1);
	}

	uint16_t addrlen = sizeof(clnt_addr);
	int client_fd = accept(server_fd, (struct sockaddr*)&clnt_addr, (socklen_t*)&addrlen);
	if (client_fd< 0)
	{
		perror("accept");
		exit(1);
	}

	// extract config parameters
	char* json_raw = malloc(1500);
	int n = read(client_fd, json_raw, 1500);
	if (n < 0)
	{
		perror("socket read");
		exit(1);
	}

	char*** json_parse;
	uint16_t json_n;
	read_json(&json_raw, &json_parse, &json_n);

	uint16_t s_port_udp = (uint16_t) atoi(json_parse[2][1]);
	uint16_t d_port_udp = (uint16_t) atoi(json_parse[3][1]);
	uint32_t packet_size = (uint32_t) atoi(json_parse[7][1]);
	uint32_t compression_threshold = (uint32_t) atoi(json_parse[10][1]);

	close(client_fd);
	sleep(1);
	close(server_fd);

	// UDP listen for first flock of packets
	server_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (server_fd < 0)
	{
		perror("socket");
		exit(1);
	}

	opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | 15, &opt, sizeof(opt)) < 0) {
		perror("setsockopt");
		exit(1);
	}

	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 500000;
	if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
		perror("setsockopt");
		exit(1);
	}

	self_addr.sin_port = htons(d_port_udp);
	clnt_addr.sin_port = htons(s_port_udp);
	if (bind(server_fd, (struct sockaddr*)&self_addr, sizeof(self_addr)) < 0)
	{
		perror("bind");
		exit(1);
	}

	struct timeval lo_start, lo_stop, hi_start, hi_stop;
	unsigned char* buf = malloc(packet_size);
	ssize_t rc;

	// low entropy packet receipt
	uint16_t i = 0;
	while (1)
	{
		rc = recvfrom(server_fd, buf, packet_size, 0, (struct sockaddr*)&clnt_addr, (socklen_t*)&addrlen);
		if (rc >= 0) break;
		else i++;
	}
	gettimeofday(&lo_start, NULL);
	while (1)
	{
		rc = recvfrom(server_fd, buf, packet_size, 0, (struct sockaddr*)&clnt_addr, (socklen_t*)&addrlen);
		if (rc < 0) break;
	}
	gettimeofday(&lo_stop, NULL);
	uint32_t lo_ent_delta_s = lo_stop.tv_sec - lo_start.tv_sec;
	uint32_t lo_ent_delta_ms = (lo_stop.tv_usec - lo_start.tv_usec) / (uint32_t)1000;

	// high entropy packet receipt
	uint16_t j = 0;
	while (1)
	{
		rc = recvfrom(server_fd, buf, packet_size, 0, (struct sockaddr*)&clnt_addr, (socklen_t*)&addrlen);
		if (rc >= 0) break;
		else j++;
	}
	gettimeofday(&hi_start, NULL);
	while (1)
	{
		rc = recvfrom(server_fd, buf, packet_size, 0, (struct sockaddr*)&clnt_addr, (socklen_t*)&addrlen);
		if (rc < 0) break;
	}
	gettimeofday(&hi_stop, NULL);
	uint32_t hi_ent_delta_s = hi_stop.tv_sec - hi_start.tv_sec;
	uint32_t hi_ent_delta_ms = (hi_stop.tv_usec - hi_start.tv_usec) / (uint32_t)1000;

	// calculate difference in burst durations
	uint32_t delta_delta_s = hi_ent_delta_s - lo_ent_delta_s;
	uint32_t delta_delta_ms = hi_ent_delta_ms - lo_ent_delta_ms;

	char* result;
	if (hi_ent_delta_s > lo_ent_delta_s ||
		(hi_ent_delta_s == lo_ent_delta_s &&
			hi_ent_delta_ms > lo_ent_delta_ms &&
			hi_ent_delta_ms - lo_ent_delta_ms >= compression_threshold))
	{
		result = malloc(22);
		strcpy(result, "Compression detected!");
		result[21] = '\0';
	}
	else
	{
		result = malloc(29);
		strcpy(result, "No compression was detected.");
		result[28] = '\0';
	}

	close(server_fd);

	// TCP send result string
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
	{
		perror("socket");
		exit(1);
	}

	opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | 15, &opt, sizeof(opt)) < 0) {
		perror("setsockopt");
		exit(1);
	}

	self_addr.sin_port = htons(port_tcp+1);
	clnt_addr.sin_port = htons(port_tcp+1);
	if (bind(server_fd, (struct sockaddr*)&self_addr, sizeof(self_addr)) < 0)
	{
		perror("bind");
		exit(1);
	}

	if (listen(server_fd, 5) < 0) {
		perror("listen");
		exit(1);
	}

	client_fd = accept(server_fd, (struct sockaddr*)&clnt_addr, (socklen_t*)&addrlen);
	if (client_fd< 0)
	{
		perror("accept");
		exit(1);
	}

	send(client_fd, result, strlen(result)+1, 0);

	sleep(2);
	close(client_fd);
	sleep(1);
	close(server_fd);

	// free mallocs and return
	free(result);
	free(buf);
	free_json(json_parse, json_n, json_raw);

	return 0;
}
