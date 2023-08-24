
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "utils.h"


void clean_exit(){exit(0);}

int main(int argc, char const* argv[])
{
	if (argc < 2)
	{
		printf("  Must supply config json file, e.g.\n    ./client config.json");
		exit(1);
	}

	// graceful exit on ctrl-c ctrl-z
	signal(SIGTERM, clean_exit);
	signal(SIGINT, clean_exit);

	// extract config parameters
	char*** json_parse;
	uint16_t json_n;
	int32_t f_len;
	char* json_raw = slurp_file(argv[1]);
	read_json(&json_raw, &json_parse, &json_n);

	char* d_ipa_s = malloc(strlen(json_parse[0][1])+1);
	strcpy(d_ipa_s, json_parse[0][1]);
	uint16_t port_tcp = (uint16_t) atoi(json_parse[1][1]);
	uint16_t s_port_udp = (uint16_t) atoi(json_parse[2][1]);
	uint16_t d_port_udp = (uint16_t) atoi(json_parse[3][1]);
	uint32_t packet_num = (uint32_t) atoi(json_parse[6][1]);
	uint32_t packet_size = (uint32_t) atoi(json_parse[7][1]);
	uint16_t pause_time = (uint16_t) atoi(json_parse[9][1]);

	if (port_tcp <= 0 || port_tcp > 65535)
	{
		printf("invalid port %d\n", port_tcp);
		exit(1);
	}

	// TCP connect, send config json as string
	int client_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (client_fd < 0)
	{
		perror("socket creation");
		return -1;
	}

	int opt = 1;
	if (setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR | 15, &opt, sizeof(opt)) < 0)
	{
		perror("setsockopt");
		exit(1);
	}

	struct sockaddr_in self_addr, serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port_tcp);
	if (inet_pton(AF_INET, d_ipa_s, &serv_addr.sin_addr) <= 0)
	{
		perror("invalid ipa");
		return -1;
	}

	self_addr.sin_family = AF_INET;
	self_addr.sin_port = htons(port_tcp);
	self_addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(client_fd, (struct sockaddr*)&self_addr, sizeof(self_addr)) < 0)
	{
		perror("bind failed");
		exit(1);
	}

	int status = connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	if (status < 0)
	{
		perror("connect");
		return -1;
	}

	send(client_fd, json_raw, strlen(json_raw), 0);
	sleep(1);
	close(client_fd);
	sleep(1);

	// UDP socket, send non-volatile data
	client_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (client_fd < 0)
	{
		perror("socket creation");
		return -1;
	}

	if (setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR | 15, &opt, sizeof(opt)) < 0)
	{
		perror("setsockopt");
		exit(1);
	}

	opt = IP_PMTUDISC_DO;
	if (setsockopt(client_fd, IPPROTO_IP, IP_MTU_DISCOVER, &opt, sizeof(opt)) < 0)
	{
		perror("setsockopt");
		exit(1);
	}

	serv_addr.sin_port = htons(d_port_udp);
	self_addr.sin_port = htons(s_port_udp);
	if (bind(client_fd, (struct sockaddr*)&self_addr, sizeof(self_addr)) < 0)
	{
		perror("bind failed");
		exit(1);
	}

	if (connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("udp connect");
		exit(1);
	}

	unsigned char* packet_data = malloc(packet_size);
	memset(packet_data, 0, packet_size);

	sleep(2);

	for (uint16_t i = 0; i < packet_num; i++)
	{
		(*packet_data) &= 0;
		(*(packet_data+1)) &= 0;
		(*packet_data) |= i;
		(*(packet_data+1)) |= (i >> 8);
		ssize_t n = sendto(client_fd, packet_data, packet_size, 0, (struct sockaddr*)0, (socklen_t)0);
	}

	// wait
	sleep(pause_time);

	// UDP socket, send volatile data
	FILE* vlt_data_f = fopen("/dev/urandom", "rb");
	fread(packet_data, 1, packet_size, vlt_data_f);
	fclose(vlt_data_f);

	for (uint16_t i = 0; i < packet_num; i++)
	{
		(*packet_data) &= 0;
		(*(packet_data+1)) &= 0;
		(*packet_data) |= i;
		(*(packet_data+1)) |= (i >> 8);
		ssize_t n = sendto(client_fd, packet_data, packet_size, 0, (struct sockaddr*)0, (socklen_t)0);
	}
	close(client_fd);

	sleep(2);

	// TCP connect, get difference in packet delay from server
	client_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (client_fd < 0)
	{
		perror("socket creation");
		return -1;
	}

	opt = 1;
	if (setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR | 15, &opt, sizeof(opt)) < 0)
	{
		perror("setsockopt");
		exit(1);
	}

	self_addr.sin_port = htons(port_tcp+1);
	serv_addr.sin_port = htons(port_tcp+1);
	if (bind(client_fd, (struct sockaddr*)&self_addr, sizeof(self_addr)) < 0)
	{
		perror("bind failed");
		exit(1);
	}

	status = connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	if (status < 0)
	{
		perror("connect");
		return -1;
	}

	char* buf = malloc(1500);
	int n = read(client_fd, buf, 1500);
	if (n < 0)
	{
		perror("socket read");
		exit(1);
	}

	// report existence of network compression
	printf("%s\n", buf);

	sleep(1);
	close(client_fd);
	sleep(1);

	// free mallocs and exit
	free_json(json_parse, json_n, json_raw);
	free(packet_data);
	free(d_ipa_s);

	return 0;
}
