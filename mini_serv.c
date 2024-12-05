/* From main.c */
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* New Includes */
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>

int id[3000000], last_fd = 0, total_clients = 0;
char *msg[3000000], read_buffer[3000000], write_buffer[3000000];
fd_set read_fd, write_fd, active_fd;

/* From main.c */
int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

/* New Includes */

void send_to_all(int exception, char *msg)
{
	for (int fd = 0; fd <= last_fd; fd++)
	{
		if (FD_ISSET(fd, &write_fd) && fd != exception)
			write(fd, msg, strlen(msg));
	}
}

void send_msg(int fd)
{
	char *message;

	bzero(&write_buffer, sizeof(write_buffer));
	while (extract_message(&msg[fd], &message))
	{
		sprintf(write_buffer, "client %d: ", id[fd]);
		send_to_all(fd, write_buffer);
		send_to_all(fd, message);
		free(message);
	}
}

void add_new_client(int fd)
{
	if (fd > last_fd)
		last_fd = fd;
	id[fd] = total_clients++;
	msg[fd] = NULL;
	FD_SET(fd, &active_fd);
	sprintf(write_buffer, "server: client %d just arrived\n", id[fd]);
	send_to_all(fd, write_buffer);
}

void remove_client(int fd)
{
	bzero(&write_buffer, sizeof(write_buffer));
	sprintf(write_buffer, "server: client %d just left\n", id[fd]);
	send_to_all(fd, write_buffer);
	FD_CLR(fd, &active_fd);
	id[fd] = -1;
	free(msg[fd]);
	close(fd);
}

void fatal_error()
{
	write(2, "Fatal error\n", 12);
	exit(1);
}

/* From main.c with edits */

int main(int argc, char **argv)
{
	int sockfd;
	struct sockaddr_in servaddr, cli; 

	if (argc != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}
	
	FD_ZERO(&active_fd);

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1)
		fatal_error();
	
	bzero(&servaddr, sizeof(servaddr)); 
	bzero(&cli, sizeof(cli));

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1])); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal_error();

	if (listen(sockfd, SOMAXCONN) != 0) 
		fatal_error();

	/* New Includes */
	
	last_fd = sockfd;
	FD_SET(sockfd, &active_fd);

	while (1)
	{
		read_fd = write_fd = active_fd;

		if (select(last_fd + 1, &read_fd, &write_fd, NULL, NULL) < 0)
		{
			for (int fd = 0; fd <= last_fd; fd++)
			{
				if (msg[fd])
					free(msg[fd]);
				close(fd);
			}
		}

		for (int fd = 0; fd <= last_fd; fd++)
		{
			if (!FD_ISSET(fd, &read_fd))
				continue;

			if (fd == sockfd)
			{
				int len = sizeof(cli);
				int client_fd = accept(sockfd, (struct sockaddr *)&cli, &len);
				if (client_fd >= 0)
				{
					add_new_client(client_fd);
					break ;
				}
			}
			else
			{
				bzero(&read_buffer, sizeof(read_buffer));
				int bytes_read = recv(fd, read_buffer, sizeof(read_buffer) - 1, 0);
				if (bytes_read <= 0)
				{
					remove_client(fd);
					break ;
				}
				else
				{
					read_buffer[bytes_read] = '\0';
					msg[fd] = str_join(msg[fd], read_buffer);
					send_msg(fd);
				}
			}
		}
	}
	return (0);
}