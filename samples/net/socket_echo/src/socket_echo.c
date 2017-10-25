/*
 * Copyright (c) 2017 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#ifndef __ZEPHYR__

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define init_net()

#else

#include <net/socket.h>
#include <kernel.h>
#include <net_sample_app.h>

void init_net(void)
{
	int ret = net_sample_app_init("socket_echo", NET_SAMPLE_NEED_IPV4,
				      K_SECONDS(3));

	if (ret < 0) {
		printf("Application init failed\n");
		k_panic();
	}
}

#endif

int main(void)
{
	int serv;
	struct sockaddr_in bind_addr;

	init_net();

	serv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	bind_addr.sin_family = AF_INET;
	bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind_addr.sin_port = htons(4242);
	bind(serv, (struct sockaddr *)&bind_addr, sizeof(bind_addr));

	listen(serv, 5);

	while (1) {
		struct sockaddr_in client_addr;
		socklen_t client_addr_len = sizeof(client_addr);
		char addr_str[32];
		int client = accept(serv, (struct sockaddr *)&client_addr,
				    &client_addr_len);
		inet_ntop(client_addr.sin_family, &client_addr.sin_addr,
			  addr_str, sizeof(addr_str));
		printf("Connection from %s\n", addr_str);

		while (1) {
			char buf[128];
			int len = recv(client, buf, sizeof(buf), 0);

			if (len == 0) {
				break;
			}
			send(client, buf, len, 0);
		}

		close(client);
		printf("Connection from %s closed\n", addr_str);
	}
}
