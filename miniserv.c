#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>

typedef struct s_client 
{
	int fd;
	int id;
	struct s_client *next;
} t_client;

t_client *g_clients = NULL;
int serv_fd, g_id = 0;
fd_set all_fds, read_cpy, write_cpy;
char status_msg[42];
char recv_buffer[42 * 4096];
char temp_buffer[42 * 4096];
char final_buffer[42 + 42 * 4096];

void fatal()
{
	write(2,"Fatal error\n", strlen("Fatal error\n"));
	close(serv_fd);
	exit(1);
}

int	get_id(int fd)
{
	t_client *temp;

	temp = g_clients;
	while (temp)
	{
		if (temp->fd == fd)
			return temp->id;
		temp = temp->next;
	}
	return -1;
}

int get_max_fd()
{
	int max;
	t_client *temp;

	max = serv_fd;
	temp = g_clients;
	while (temp)
	{
		if (temp->fd > max)
			max = temp->fd;
		temp = temp->next;
	}
	return max;
}

void	send_all(int fd, char *str)
{
	t_client *temp;

	temp = g_clients;
	while (temp)
	{
		if (temp->fd != fd && FD_ISSET(temp->fd, &write_cpy))
			if ((send(temp->fd, str, strlen(str), 0)) < 0) fatal();
		temp = temp->next;
	}
}

int	add_client_to_list(int fd)
{
	t_client *temp;
	t_client *new;

	if (!(new = calloc(1, sizeof(t_client)))) fatal();
	new->fd = fd;
	new->id = g_id++;
	new->next = NULL;
	if (!g_clients)
		g_clients = new;
	else
	{
		temp = g_clients;
		while (temp && temp->next)
			temp = temp->next;
		temp->next = new;
	}
	return new->id;
}

void	add_client()
{
	struct sockaddr_in	clientaddr;
	socklen_t			addrlen;
	int					client_fd;

	addrlen = sizeof(clientaddr);
	if ((client_fd = accept(serv_fd, (struct sockaddr *) &clientaddr, &addrlen)) < 0) fatal();
	sprintf(status_msg, "server: client %d just arrived\n", add_client_to_list(client_fd));
	send_all(client_fd, status_msg);
	FD_SET(client_fd, &all_fds);
}

int rm_client(int fd)
{
	t_client *temp;
	t_client *del;
	int		  id;

	id = get_id(fd);
	if (g_clients->fd == fd)
	{
		del = g_clients;
		g_clients = g_clients->next;
		free(del);
	}
	else
	{
		temp = g_clients;
		while(temp && temp->next)
		{
			if (temp->next->fd == fd)
			{
				del = temp->next;
				temp->next = temp->next->next;
				free(del);
			}
			temp = temp->next;
		}
	}
	return id;
}

void	ex_msg(int fd)
{
	int i;
	int j;

	i = 0;
	j = 0;
	while (recv_buffer[i])
	{
		temp_buffer[j] = recv_buffer[i];
		j++;
		if (recv_buffer[i] == '\n')
		{
			j = 0;
			sprintf(final_buffer, "client %d: %s",  get_id(fd), temp_buffer);
			send_all(fd, final_buffer);
			bzero(&final_buffer, strlen(final_buffer));
			bzero(&temp_buffer, strlen(temp_buffer));
		}
		i++;
	}
	bzero(&recv_buffer, strlen(recv_buffer));
}

int main(int argc, char **argv)
{
	struct sockaddr_in	servaddr;
	socklen_t		 	addrlen;
	uint16_t			port;
	int					recv_ret;

	if (argc != 2)
	{
		write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		return 1;
	}
	
	port = atoi(argv[1]);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433);
	servaddr.sin_port = htons(port);
	addrlen = sizeof(servaddr);

	if ((serv_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) fatal();
	if ((bind(serv_fd, (struct sockaddr *) &servaddr, addrlen)) < 0) fatal();
	if (listen(serv_fd, 0) < 0 ) fatal();

	FD_ZERO(&all_fds);
	FD_SET(serv_fd, &all_fds);
	bzero(&recv_buffer, sizeof(recv_buffer));
	bzero(&temp_buffer, sizeof(temp_buffer));
	bzero(&final_buffer, sizeof(final_buffer));

	while (1)
	{
		write_cpy = read_cpy = all_fds;
		
		if ((select(get_max_fd() + 1, &read_cpy, &write_cpy, 0, 0)) < 0) continue;

		//check all fds
		for (int fd = 0; fd <= get_max_fd(); ++fd)
		{
			//fd is ready to read
			if (FD_ISSET(fd, &read_cpy))
			{
				//new connection
				if (fd == serv_fd)
				{
					bzero(&status_msg, sizeof(status_msg));
					add_client();
					break ;
				}
				//read from client
				else
				{
					recv_ret = 1000;
					while (recv_ret == 1000 || recv_buffer[strlen(recv_buffer) - 1] != '\n')
						if ((recv_ret = recv(fd, recv_buffer + strlen(recv_buffer), 1000, 0)) <= 0) break;
					
					//client send dc
					if (recv_ret <= 0)
					{
						bzero(&status_msg, sizeof(status_msg));
						sprintf(status_msg, "server: client %d just left\n", rm_client(fd));
						send_all(fd, status_msg);
						FD_CLR(fd, &all_fds);
						close(fd);
						break;
					}
					//client send text
					else
						ex_msg(fd);	
				}
			}
		}
	}
	return 1;
}