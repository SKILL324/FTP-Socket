#include "Socket.h"

///////////////////////////////////////////////////////////////////////////////
#define DEFAULT_BUFLEN 512

static sockList *_list = NULL;//point to all extern data allocated.

static const char *STR_OK = "OK";
static const char *STR_ABORT = "ABORT";
static const char *STR_FILESIZE = "SIZE";
static const char *STR_SERVER_DIR = "SERVER_FILES/";
static const char *STR_CLIENT_DIR = "CLIENT_FILES/";
static const size_t SIZE_NAMEDIR = 14;

static const size_t SIZE_COMMAND = 4;
static const size_t SIZE_FILETYPE = 3;
static const size_t SIZE_FILENAME = 50;

static const size_t SIZE_LISTEN = 5;
static const size_t SIZE_SOCKINFO = 50;
///////////////////////////////////////////////////////////////////////////////
int WsaCtor()
{
	WSADATA WSAdata;

	if (WSAStartup(MAKEWORD(2, 2), &WSAdata)){return(1);}

	_list = SockListInit();
	if (!_list) { return(1); }

	return(0);
}
int WsaDtor()
{
	SockListClear();
	WSACleanup();
	return(0);
}

static sockList *SockListInit()
{
	sockList *p_socklist;
	p_socklist = calloc(1,sizeof(*p_socklist));
	if (!p_socklist) { return(NULL); }

	return(p_socklist);
}
static sockList **SockListNotNull(void *pp_self)
{
	sockList **p_socklist;
	p_socklist = (sockList **)pp_self;

	if (!*p_socklist) { return(NULL); }
	else if ((*p_socklist)->self == NULL) { return(NULL); }

	return(p_socklist);
}
static void *SockListCtor(void **pp_self,void *data, int type)
{
	sockList **pp_socklist;
	pp_socklist = (sockList **)pp_self;
	if (!pp_socklist) { return(NULL); }
	if (!data) { return(NULL); }

	if (type >= SOCKT_MAX) { return(NULL); }

	(*pp_socklist)->data = data;
	(*pp_socklist)->sock_type = (socklistType)type;

	(*pp_socklist)->next = SockListInit();
	if (!(*pp_socklist)->next) { return(NULL); }

	(*pp_socklist)->next->last = (*pp_socklist);

	(*pp_socklist) = (*pp_socklist)->next;
	return(VALID);
}
static void *SockListDtor(void **pp_self)
{
	sockList **pp_socklist;
	pp_socklist = SockListNotNull(pp_self);

	void *p_data;
	p_data = (*pp_socklist)->data;

	switch ((*pp_socklist)->sock_type)
	{
	case SOCKT_SERVER:
	{
		ServerDtor(p_data);
	}break;

	case SOCKT_CLIENT:
	{
		ClientDtor(p_data);
	}break;

	case SOCKT_FILE:
	{
		SockInfoDtor(p_data);
	}break;
		
	default:
	{
		return(NULL);
	}break;
		
	}//switch-end
	
	(*pp_socklist)->self = NULL;
	free((*pp_socklist));
	(*pp_socklist) = NULL;
	return(VALID);
}
static void *SockListRemove(void *pp_self, void *data)
{
	sockList *p_list;
	p_list = (sockList *)pp_self;

	int found = 0;

	while (p_list)
	{
		if (p_list->data == data)
		{
			found = 1;
			break;
		}
		p_list = p_list->last;	
	}

	if (!found) { return(NULL); }
	
	if (p_list->last)
	{
		p_list->last->next = p_list->next;
	}
	else if (p_list->next)
	{
		p_list->next->last = p_list->last;
	}
	else if (!p_list->next)
	{
		_list = p_list->last;
	}

	SockListDtor(&p_list);
	free(p_list);
	p_list = NULL;

	return(VALID);
}
static void *SockListClear()
{
	if (!_list) { return(NULL); }

	sockList *p_list, *tmp_list;
	p_list = _list;
	tmp_list = _list;

	int found = 0;

	while (p_list)
	{
		tmp_list = p_list->last;
		SockListDtor(&p_list);
		p_list = tmp_list;
	}

	return(VALID);
}

static sockInfo *SockInfoInit()
{
	sockInfo *p_sockinfo;
	p_sockinfo = calloc(1, sizeof(sockInfo));
	if (!p_sockinfo) { return(NULL); }

	p_sockinfo->sock = malloc(sizeof(*(p_sockinfo)->sock));
	if (!p_sockinfo->sock) { return(NULL); }

	p_sockinfo->self = p_sockinfo;

	return(p_sockinfo);
}
static sockInfo *SockInfoNotNull(void *self)
{
	sockInfo *p_sockinfo;
	p_sockinfo = (sockInfo *)self;

	if (!p_sockinfo) { return(NULL); }
	else if (p_sockinfo->self == NULL) { return(NULL); }

	return(p_sockinfo);
}
static void *SockInfoCtor(void *self, void *client_socket)
{
	sockInfo *p_sockinfo;
	p_sockinfo = SockInfoNotNull(self);
	if (!p_sockinfo) { return(NULL); }

	SOCKET *p_socket;
	p_socket = (SOCKET *)client_socket;

	*(p_sockinfo)->sock = *p_socket;

	return(VALID);
}
static void *SockInfoDtor(void *self)
{
	sockInfo *p_sockinfo;
	p_sockinfo = SockInfoNotNull(self);
	if (!p_sockinfo) { return(NULL); }

	p_sockinfo->self = NULL;
	
	free(p_sockinfo->sock);
	p_sockinfo->sock = NULL;

	free(p_sockinfo);
	p_sockinfo = NULL;

	return(VALID);
}
static void *SockInfoSetHost(void *self, void *hostname, size_t namelen)
{
	sockInfo *p_sockinfo;
	p_sockinfo = (sockInfo *)self;
	if (!p_sockinfo) { return(NULL); }

	char *name;
	name = malloc(namelen + 1);
	if (!name) { return(NULL); }

	strcpy_s(name, namelen+1, hostname);

	p_sockinfo->hostname = name;

	return(VALID);
}

sockServer *ServerInit()
{
	sockServer* server;
	server = calloc(1, sizeof(*server));
	if (!server) { return(NULL); }
	else
	{
		server->ctor = TServerCtor;
		server->dtor = TServerDtor;
		server->info = TSockDataInfo;
		server->start = TServerStart;
		server->get = ThreadGet;
		server->cancel = ThreadCancel;
		server->self = server;
	}

	sockData *_data;
	_data = calloc(1, sizeof(*_data));
	if (!_data) { return(NULL); }

	_data->sock = malloc(sizeof(*_data->sock));
	if (!_data->sock) { return(NULL); }

	_data->self_mutex = calloc(1, sizeof(*_data->self_mutex));
	if (!_data->self_mutex) { return(NULL); }

	_data->request_mutex = calloc(1, sizeof(*_data->request_mutex));
	if (!_data->request_mutex) { return(NULL); }

	pthread_mutex_init(_data->self_mutex, NULL);
	pthread_mutex_init(_data->request_mutex, NULL);

	server->data = _data;
	
	void *sucess = SockListCtor(&_list,server, SOCKT_SERVER);
	if (!sucess) { return(NULL); }

	return(server);
}
sockClient *ClientInit()
{
	sockClient* client;
	client = calloc(1, sizeof(*client));
	if (!client) { return(NULL); }
	else
	{
		client->self = client;
		client->ctor = TClientCtor;
		client->dtor = TClientDtor;
		client->info = TSockDataInfo;
		client->connect = TClientConnect;
		client->request = TClientRequest;
		client->get = ThreadGet;
		client->cancel = ThreadCancel;
	}

	sockData *_data;
	_data = calloc(1, sizeof(*_data));
	if (!_data) { return(NULL); }

	_data->sock = malloc(sizeof(*_data->sock));
	if (!_data->sock) { return(NULL); }

	_data->self_mutex = calloc(1, sizeof(*_data->self_mutex));
	if (!_data->self_mutex) { return(NULL); }
	pthread_mutex_init(_data->self_mutex, NULL);

	client->data = _data;

	void *sucess = SockListCtor(&_list, client, SOCKT_CLIENT);
	if (!sucess) { return(NULL); }

	return(client);
}
static sockServer *ServerNotNull(void *self)
{
	if (!self) { return(NULL); }

	sockServer *server;
	server = (sockServer *)self;

	if (!server->self) { return(NULL); }

	return(server);
}
static sockClient *ClientNotNull(void *self)
{
	if (!self) { return(NULL); }

	sockClient *client;
	client = (sockClient *)self;
	if (!client->self) { return(NULL); }

	return(client);
}
static void *ServerCtor(void *self, void *address, void *service)
{
	sockServer* server;
	server = ServerNotNull(self);
	if (!server) { return(NULL); }

	sockData *_data;
	_data = (sockData *)server->data;

	const char *_address = (const char *)address;
	const char *_service = (const char *)service;

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;															
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_TCP;

	
	if (getaddrinfo(_address, _service, &hints, &_data->addr)) { return(NULL); }

	*(_data)->sock = socket(_data->addr->ai_family,
		_data->addr->ai_socktype,
		_data->addr->ai_protocol);

	if (*(_data)->sock < 0) { return(NULL); }

	if (bind(*(_data)->sock,
		_data->addr->ai_addr,
		(int)_data->addr->ai_addrlen)) {
		return(NULL);
	}

	if (listen(*(_data)->sock, SIZE_LISTEN) < 0) { return(NULL); }

	return(VALID);
}
static void *ClientCtor(void *self, void *host, void *service)
{
	sockClient *client;
	client = ClientNotNull(self);
	if (!client) { return(NULL); }

	sockData *_data;
	_data = (sockData *)client->data;

	const char *_host = (const char *)host;
	const char *_service = (const char *)service;

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo *addr;

	if (getaddrinfo(_host, _service, &hints, &addr)) { return(NULL); }

	*(_data)->sock = socket(addr->ai_family,
		addr->ai_socktype,
		addr->ai_protocol);

	if (*(_data)->sock == INVALID_SOCKET) { return(NULL); }

	_data->addr = addr;

	return(VALID);
}
static void *ServerDtor(void *self)
{
	sockServer *server;
	server = ServerNotNull(self);
	if (!server) { return(NULL); }

	sockData *_data;
	_data = (sockData *)server->data;

	_data->self_mutex = NULL;

	freeaddrinfo(_data->addr);
	free(_data->sockinfo);
	closesocket(*(_data)->sock);
	*(_data)->sock = INVALID_SOCKET;
	free(_data->sock);
	_data->sock = NULL;

	server->self = NULL;
	free(server);
	server = NULL;

	return(VALID);
}
static void *ClientDtor(void *self)
{
	sockClient *client;
	client = ClientNotNull(self);
	if (!client) { return(NULL); }

	sockData *_data;
	_data = (sockData *)client->data;

	_data->self_mutex = NULL;

	freeaddrinfo(_data->addr);
	free(_data->sockinfo);
	closesocket(*(_data)->sock);
	*(_data)->sock = INVALID_SOCKET;
	free(_data->sock);
	_data->sock = NULL;

	client->self = NULL;
	free(client);
	client = NULL;

	return(VALID);
}
static void *SockDataInfo(void* self)
{
	if (!self) { return(NULL); }

	sockData *_data;
	_data = (sockData *)self;

	char **tmp_buffer = malloc(sizeof(*tmp_buffer) * 2);
	if (!tmp_buffer) { return(NULL); }
	else
	{
		for (int i = 0; i < 2; i++)
		{
			tmp_buffer[i] = malloc(sizeof(**tmp_buffer) * (SIZE_SOCKINFO + 1));
			if (!tmp_buffer[i]) { return(NULL); }
			tmp_buffer[i][SIZE_SOCKINFO] = '\0';
		}
		
	}
	
	getnameinfo((struct sockaddr*)_data->addr->ai_addr,
				(int)_data->addr->ai_addrlen,
				tmp_buffer[0], SIZE_SOCKINFO, 
				tmp_buffer[1], SIZE_SOCKINFO,
				NI_NUMERICHOST | NI_NUMERICSERV);

	char* buffer = malloc(sizeof(*buffer) * (SIZE_SOCKINFO + 1));
	if (!buffer) { return(NULL); }
	else
	{
		buffer[SIZE_SOCKINFO] = '\0';
	}

	sprintf_s(buffer, SIZE_SOCKINFO, "Address:%s\nService:", tmp_buffer[0]);
	strcat_s(buffer, SIZE_SOCKINFO, tmp_buffer[1]);

	for (int i = 0; i < 1; i++)
	{
		free(tmp_buffer[i]);
		tmp_buffer[i] = NULL;
	}
	free(tmp_buffer);
	tmp_buffer = NULL;

	_data->sockinfo = buffer;

	return(_data->sockinfo);
}
static void *ServerStart(void* self)
{
	sockServer *server;
	server = ServerNotNull(self);
	if (!server) { return(NULL); }

	sockData *_data;
	_data = (sockData *)server->data;

	sockList *p_socklist;//list to sockinfo
	p_socklist = SockListInit();
	if (!p_socklist) { return(NULL); }

	sockInfo *p_sockinfo;

	p_sockinfo = SockInfoInit();//server
	if (!p_sockinfo) { return(NULL); }
	SockInfoCtor(p_sockinfo, _data->sock);
	SockInfoSetHost(p_sockinfo, "SERVER", 6);
	SockListCtor(&p_socklist, p_sockinfo, SOCKT_FILE);

	fd_set master;
	FD_ZERO(&master);
	FD_SET(*(_data)->sock, &master);

	SOCKET max_socket = *(_data)->sock;
	SOCKET last_client = {0};
	int result = {0};

	char hostname[NI_MAXHOST];
	sockList *p_tmpsocklist;
	sockInfo *p_tmpsockinfo;

	while (1)
	{
		fd_set reads;
		reads = master;

		if (select(0, &reads, 0, 0, 0) < 0){return(NULL);}

		p_tmpsocklist = p_socklist;

		while(p_tmpsocklist->last != NULL)
		{
			p_tmpsockinfo = (sockInfo*)p_tmpsocklist->last->data;

			if (FD_ISSET(*(p_tmpsockinfo)->sock, &reads))
			{
				if (*(p_tmpsockinfo)->sock == *(_data)->sock)
				{
					//Handle new connections
					struct sockaddr_storage client_addr;
					socklen_t client_len;
					client_len = sizeof(client_addr);

					SOCKET *socket_client;
					socket_client = malloc(sizeof(*socket_client));
					if (!socket_client) { return(NULL); }

					*socket_client = accept(*(_data)->sock,
						(struct sockaddr *)&client_addr, &client_len);

					FD_SET(*socket_client, &master);
					
					if (*socket_client > max_socket)
						max_socket = *socket_client;

					getnameinfo((struct sockaddr*)&client_addr, client_len,
						hostname, NI_MAXHOST,
						0, 0, NI_NUMERICHOST);

					size_t hostlen = strlen(hostname);

					p_sockinfo = SockInfoInit();
					if (!p_sockinfo) { return(NULL); }

					SockInfoCtor(p_sockinfo, socket_client);
					SockInfoSetHost(p_sockinfo, hostname, hostlen);
					SockListCtor(&p_socklist, p_sockinfo, SOCKT_FILE);
					free(socket_client);
					socket_client = NULL;
				}
				else
				{
					//Received from client
					FD_CLR(*p_tmpsockinfo->sock, &master);
					TServerRequest(server, p_tmpsockinfo->sock);
					//closesocket?
				}
			}
			p_tmpsocklist = p_tmpsocklist->last;
		}
	}

	return(VALID);
}
static void *ClientConnect(void *self)
{
	sockClient *client;
	client = ClientNotNull(self);
	if (!client) { return(NULL); }

	sockData *_data;
	_data = (sockData *)client->data;

	if (connect(*(_data)->sock,
		_data->addr->ai_addr,
		(int)_data->addr->ai_addrlen)) { return(NULL); }

	return(VALID);
}
static void *ServerRequest(void *client_socket)
{
	void *result = NULL;

	SOCKET *p_socket;
	p_socket = (SOCKET *)client_socket;

	if (!p_socket) { return(NULL); }

	char recvbuffer[DEFAULT_BUFLEN] = { 0 };
	int recvlen = DEFAULT_BUFLEN;

	int recvbytes = 0;
	int sendbytes = 0;
	int n_items = 0;

	recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
	if (recvbytes <= 0 || recvbytes > (int)SIZE_COMMAND) { goto error; }

	char *ftp_command;
	ftp_command = recvbuffer;

	if (strcmp(ftp_command, "PUT") == 0)
	{
		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { goto error; }

		result = ServerRequestPUT(p_socket, 1);
	}
	else
	if (strcmp(ftp_command, "GET") == 0)
	{
		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes < 0) { goto error; }
		result = ServerRequestGET(p_socket, 1);
	}
	else
	if (strcmp(ftp_command, "MPUT") == 0)
	{
		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { goto error; }

		recvbytes = recv_s(*p_socket, (char *)&n_items, sizeof(n_items), 1);
		if (recvbytes < 0) { goto error; }

		n_items = ntohl(n_items);

		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { goto error; }

		result = ServerRequestPUT(p_socket, n_items);
	}
	else
	if (strcmp(ftp_command, "MGET") == 0)
	{
		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { goto error; }

		recvbytes = recv_s(*p_socket, (char *)&n_items, sizeof(n_items), 1);
		if (recvbytes < 0) { goto error; }

		n_items = ntohl(n_items);

		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { goto error; }

		result = ServerRequestGET(p_socket, n_items);
	}

	return(VALID);

	error:;
	closesocket(*p_socket);
	return(NULL);
}
static void *ClientRequest(void *client_socket, void *buffer)
{
	SOCKET *p_socket;
	p_socket = (SOCKET *)client_socket;

	char *p_buffer;
	p_buffer = (char *)buffer;

	if (!p_socket || !p_buffer) { return(NULL); }

	if (strlen(p_buffer) > DEFAULT_BUFLEN) { return(NULL); }
	
	void *result = NULL;
	char ***items = NULL;
	size_t n_items = 0;

	char *cpy_buffer;
	cpy_buffer = malloc(strlen(p_buffer) + 1);
	if (!cpy_buffer) { return(NULL); }

	strcpy(cpy_buffer, p_buffer);
	if (!cpy_buffer) { goto error; }

	char *ftp_command;
	ftp_command = strtok(cpy_buffer, " ");
	if (!ftp_command || strlen(ftp_command) > SIZE_COMMAND) { goto error; }

	char *raw_items;
	raw_items = strtok(NULL, "\0");
	if (!raw_items) { goto error; }

	items = calloc(1, sizeof(*items));
	if (!items) { goto error; }

	result = GetRequestItems(raw_items ,items, &n_items);
	if (!result || n_items == 0) { goto error; }
	result = NULL;

	char recvbuffer[DEFAULT_BUFLEN] = { 0 };
	int recvlen = DEFAULT_BUFLEN;

	char sendbuffer[DEFAULT_BUFLEN] = { 0 };
	int sendlen = DEFAULT_BUFLEN;

	int recvbytes = 0;
	int sendbytes = 0;
	int n_files = 0;
	
	if (strcmp(ftp_command, "PUT") == 0)
	{
		sendbytes = send(*p_socket, "PUT", 3, 0);
		if (sendbytes <= 0) { goto error; }

		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes <= 0) { goto error; }

		if (strcmp(recvbuffer, STR_OK) != 0) { goto error; }

		result = ClientRequestPUT(p_socket, *items, 1);
	}
	else
	if (strcmp(ftp_command, "GET") == 0)
	{
		sendbytes = send(*p_socket, "GET", 3, 0);
		if (sendbytes <= 0) { return(NULL); }

		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes <= 0) { return(NULL); }

		if (strcmp(recvbuffer, STR_OK) != 0) { return(NULL); }

		result = ClientRequestGET(p_socket, *items, 1);
	}
	else
	if (strcmp(ftp_command, "MPUT") == 0)
	{
		sendbytes = send(*p_socket, "MPUT", 4, 0);
		if (sendbytes < 0) { return(NULL); }

		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes < 0) { return(NULL); }
		if (strcmp(STR_OK, recvbuffer) != 0) { return(NULL); }
		memset(recvbuffer, 0, recvlen);

		n_files = htonl((int)n_items);

		sendbytes = send(*p_socket, (const char *)&n_files, sizeof(n_files), 0);
		if (sendbytes < 0) { return(NULL); }

		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes < 0) { return(NULL); }
		if (strcmp(STR_OK, recvbuffer) != 0) { return(NULL); }

		result = ClientRequestPUT(p_socket, *items, n_items);
	}
	else
	if (strcmp(ftp_command, "MGET") == 0)
	{
		
		sendbytes = send(*p_socket, "MGET", 4, 0);
		if (sendbytes < 0) { return(NULL); }

		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes < 0) { return(NULL); }
		if (strcmp(STR_OK, recvbuffer) != 0) { return(NULL); }
		memset(recvbuffer, 0, recvlen);

		n_files = htonl((int)n_items);

		sendbytes = send(*p_socket, (const char *)&n_files, sizeof(n_files), 0);
		if (sendbytes < 0) { return(NULL); }

		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes < 0) { return(NULL); }
		if (strcmp(STR_OK, recvbuffer) != 0) { return(NULL); }

		result = ClientRequestGET(p_socket, *items, n_items);
	}

	error:;
	free(cpy_buffer);
	if (items && *items && **items)
	{
		if (n_items > 0)
		{
			for(size_t i = 0; i < n_items; ++i)
				free((*items)[i]);
		}	
		free(*items);
	}
	
	if (!result)
	{
		closesocket(*p_socket);
		return(NULL);
	}

	return(VALID);
}

static void *GetRequestItems(void *rawitems, void ***items, size_t *n_items)
{
	char *raw_items, ***ppp_items;
	raw_items = (char *)rawitems;
	ppp_items = (char ***)items;

	if (!raw_items || !ppp_items) { return(NULL); }

	char **pp_items = NULL;

	int i = 0;
	char *item_name = NULL;
	item_name = strtok(raw_items, " ");
	while (item_name != NULL)
	{
		size_t item_namelen = strlen(item_name);
		if (item_namelen > SIZE_FILENAME) { return(NULL); }

		char *item;
		item = malloc(item_namelen + 1);
		if (!item) { return(NULL); }

		strcpy(item, item_name);
		if (!item) { return(NULL); }

		char **tmp_items;
		tmp_items = realloc(pp_items, sizeof(*pp_items) * (i + 1));
		if (!tmp_items) { return(NULL); }

		pp_items = tmp_items;
		pp_items[i++] = item;

		item_name = strtok(NULL, " ");
	} 

	if (i == 0) { return(NULL); }
	
	*ppp_items = pp_items;
	*n_items = (size_t)(i);

	return(VALID);
}
static void *ServerRequestPUT(void *_socket, size_t n_items)
{
	SOCKET *p_socket;
	p_socket = (SOCKET *)_socket;

	if (!p_socket) { return(NULL); }

	for (int i = 0; i < (int)n_items; ++i)
	{ 
		FILE *file = NULL;

		char recvbuffer[DEFAULT_BUFLEN] = { 0 };
		int recvlen = DEFAULT_BUFLEN;

		int recvbytes = 0;
		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes < 0){ return(NULL); }

		char *filepath = NULL;
		size_t filenamelen = strlen(recvbuffer) + strlen(STR_SERVER_DIR);
		filepath = (char *)malloc(filenamelen + 1);
		if (!filepath) { return(NULL); }

		int printed;
		printed = sprintf(filepath, "%s%s", STR_SERVER_DIR, recvbuffer);
		if (printed < (int)filenamelen) { goto error; }
	
		file = fopen(filepath, "wb");
		if (!file) { goto error; }

		int sendbytes = 0;
		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { return(NULL); }

		int filesize = 0;
		recvbytes = recv_s(*p_socket, (char *)&filesize, sizeof(filesize), 1);
		if (recvbytes <= 0) { goto error; }
		filesize = ntohl(filesize);
		memset(recvbuffer, 0, recvlen);

		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { return(NULL); }

		int read = 0;
		while (read < filesize)
		{
			read += recv_s(*p_socket, recvbuffer, recvlen, 0);///
			fwrite(recvbuffer, sizeof(*recvbuffer), recvlen, file);
			memset(recvbuffer, 0, recvlen);
		}

		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { return(NULL); }

		fclose(file);
		free(filepath);
		continue;

		error:
		if (file) { fclose(file); remove(filepath);}
		free(filepath);
		return(NULL);
	}

	return(VALID);
}
static void *ClientRequestPUT(void *_socket, void **item, size_t n_items)
{
	SOCKET *p_socket;
	p_socket = (SOCKET *)_socket;

	char **p_item;
	p_item = (char **)item;

	if (!p_socket || !p_item) { return(NULL); }
	
	for (int i = 0; i < (int)n_items; ++i)
	{
		FILE *file = NULL;

		char *filepath = NULL;
		size_t filepathlen = strlen(STR_CLIENT_DIR) + strlen(p_item[i]);
		filepath = calloc(filepathlen + 1, sizeof(*filepath));
		if (!filepath) { return(NULL); }

		int printed = 0;
		printed = sprintf(filepath, "%s%s", STR_CLIENT_DIR, p_item[i]);
		if (printed != filepathlen) { goto error; }

		file = fopen(filepath, "rb");
		if (!file) { goto error; }

		fseek(file, 0, SEEK_END);
		int filesize = 0;
		filesize = ftell(file);
		fseek(file, 0, SEEK_SET);

		int sendbytes = 0;
		sendbytes = send(*p_socket, (const char*)p_item[i], (int)strlen(p_item[i]), 0);
		if (sendbytes <= 0) { goto error; }

		char recvbuffer[DEFAULT_BUFLEN] = { 0 };
		int recvlen = DEFAULT_BUFLEN;

		int recvbytes = 0;
		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes <= 0) { goto error; }

		if (strcmp(recvbuffer, STR_OK) != 0) { goto error; }
		memset(recvbuffer, 0, recvlen);

		int l_filesize = 0;
		l_filesize = htonl(filesize);

		sendbytes = send(*p_socket, (const char *)&l_filesize, sizeof(l_filesize), 0);
		if (sendbytes < 0) { goto error; }

		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes <= 0) { goto error; }

		if (strcmp(recvbuffer, STR_OK) != 0) { goto error; }
		memset(recvbuffer, 0, recvlen);

		size_t read = 0;
		while (!feof(file) || (int)read < filesize)
		{
			char sendbuffer[DEFAULT_BUFLEN] = { 0 };
			int sendlen = DEFAULT_BUFLEN;

			read += fread(sendbuffer, sizeof(*sendbuffer), sendlen, file);
			if(ferror(file) != 0) { goto error; }

			sendbytes = send(*p_socket, sendbuffer, sendlen, 0);
			if(sendbytes < 0) { goto error; }
		}

		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes <= 0) { goto error; }

		if (strcmp(recvbuffer, STR_OK) != 0) { goto error; }

		fclose(file);
		free(filepath);
		continue;

		error:;
		if (file) { fclose(file); }
		free(filepath);
		return(NULL);
	}

	return(VALID);
}
static void *ServerRequestGET(void *_socket, size_t n_items)
{
	SOCKET *p_socket;
	p_socket = (SOCKET *)_socket;

	if (!p_socket) { return(NULL); }

	for(int i = 0; i < (int)n_items; ++i)
	{
		FILE *file = NULL;

		char recvbuffer[DEFAULT_BUFLEN] = { 0 };
		int recvlen = DEFAULT_BUFLEN;

		int recvbytes = 0;
		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);// nao ta recebendo na segunda rodada
		if (recvbytes < 0) { return(NULL); }

		char *filepath = NULL;
		size_t filenamelen = strlen(recvbuffer) + strlen(STR_SERVER_DIR);
		filepath = malloc(filenamelen + 1);
		if (!filepath) { return(NULL); }

		int printed = 0;
		printed = sprintf(filepath, "%s%s", STR_SERVER_DIR, recvbuffer);
		if (printed < (int)filenamelen) { goto error; }
		memset(recvbuffer, 0, recvlen);

		file = fopen(filepath, "rb");
		if (!file) { goto error; }

		size_t filesize = 0;
		fseek(file, 0, SEEK_END);
		filesize = (size_t)ftell(file);
		fseek(file, 0, SEEK_SET);
		int l_filesize = htonl(filesize);

		int sendbytes = 0;
		sendbytes = send(*p_socket, (const char *)&l_filesize, sizeof(l_filesize), 0);
		if (sendbytes < 0) { goto error; }

		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes < 0) { return(NULL); }

		if (strcmp(STR_OK, recvbuffer) != 0) { return(NULL); }

		size_t read = 0;
		while (!feof(file) || read < filesize)
		{
			char sendbuffer[DEFAULT_BUFLEN] = { 0 };
			int sendlen = DEFAULT_BUFLEN;

			read += fread(sendbuffer, sizeof(*sendbuffer), sendlen, file);
			if(ferror(file) != 0){ goto error; }

			sendbytes = send(*p_socket, sendbuffer, sendlen, 0);
			if (sendbytes < 0) { goto error; }
		}	

		fclose(file);
		free(filepath);
		continue;

		error:;
		if (file) { fclose(file); }
		free(filepath);
		return(NULL);
	}

	return(VALID);
}
static void *ClientRequestGET(void *_socket, void **item, size_t n_items)
{
	SOCKET *p_socket;
	p_socket = (SOCKET *)_socket;

	char **pp_item;
	pp_item = (char **)item;

	if (!p_socket || !pp_item) { return(NULL); }

	for(int i = 0; i < (int)n_items; ++i)
	{
		FILE *file = NULL;

		char *filepath = NULL;
		size_t filepathlen = strlen(STR_CLIENT_DIR) + strlen(pp_item[i]);
		filepath = calloc(filepathlen + 1, sizeof(*filepath));
		if (!filepath) { return(NULL); }

		int printed = 0;
		printed = sprintf(filepath, "%s%s", STR_CLIENT_DIR, pp_item[i]);
		if (printed != filepathlen) { goto error; }

		file = fopen(filepath, "wb");
		if (!file) { goto error; }

		int sendbytes = 0;
		sendbytes = send(*p_socket, (const char*)pp_item[i], (int)strlen(pp_item[i]), 0);
		if (sendbytes <= 0) { return(NULL); }

		int filesize = 0;
		int recvbytes;
		recvbytes = recv_s(*p_socket, (char *)&filesize, sizeof(filesize), 1);
		if (recvbytes <= 0) { goto error; }
		filesize = ntohl(filesize);

		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { return(NULL); }

		int read = 0;
		while (read < filesize)
		{
			char recvbuffer[DEFAULT_BUFLEN] = { 0 };
			int recvlen = DEFAULT_BUFLEN;

			recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
			if (recvbytes < 0) { goto error; }

			read += recvbytes;
			fwrite(recvbuffer, sizeof(*recvbuffer), recvlen, file);
		}

		fclose(file);
		free(filepath);
		continue;

	    error:;
		if (file) { fclose(file), remove(filepath); }
		free(filepath);
		return(NULL);
	}

	return(VALID);
}

static int recv_s(SOCKET _socket, char *buf, int len, int flags)
{
	int trigger = 0;
	int recvbytes = 0;
	
	for(int trigger = 0; trigger < 2; ++trigger)
	{
		recvbytes = recv(_socket, buf, len, 0);
		if (recvbytes == len && strlen(buf) == 0 && !flags) { continue; }
		else { break; }
	}

	if (trigger >= 2){ return(0); }

	return(recvbytes);
}
static void *ThreadGet(void *thread)
{
	if (!thread) { return(NULL); }

	pthread_t *p_thread;
	p_thread = (pthread_t *)thread;

	void *buffer;
	buffer = calloc(1, sizeof(buffer));
	if (!buffer) { return(NULL); }

	if (pthread_join(*p_thread, &buffer)) { return(NULL); }
	
	return(buffer);
}
static void *ThreadCancel(void *thread)
{
	if (!thread) { return(NULL); }

	pthread_t *p_thread;
	p_thread = (pthread_t *)thread;
	
	if (pthread_cancel(*p_thread)) { return(NULL); }
	else
	{
		int result = pthread_join(*p_thread, NULL);
		if (!result) { return(NULL); }
	}

	return(VALID);
}

static Tsocket *TSocketInit()
{
	Tsocket *p_tsocket;
	p_tsocket = calloc(1, sizeof(*p_tsocket));
	if (!p_tsocket) { return(NULL); }
	
	return(p_tsocket);
}
static void *TSocketCtor(void *self, void *p_func, void **args, int sz_func, void *mutex)
{
	Tsocket *p_tsocket;
	p_tsocket = (Tsocket *)self;

	if (!p_tsocket) { return(NULL); }

	p_tsocket->mutex = (pthread_mutex_t*)mutex;

	p_tsocket->size = sz_func;
	p_tsocket->pp_void = args;
	
	if (p_tsocket->size == 1)
	{
		p_tsocket->ptr_func1 = p_func;
	}
	else if (p_tsocket->size == 2)
	{
		p_tsocket->ptr_func2 = p_func;
	}
	else if (p_tsocket->size == 3)
	{
		p_tsocket->ptr_func3 = p_func;
	}

	return(VALID);
}
static void *TSocketDtor(void *self)
{
	Tsocket *p_tsocket;
	p_tsocket = (Tsocket *)self;

	if (!p_tsocket) { return(NULL); }

	p_tsocket->mutex = NULL;

	free(p_tsocket->pp_void);
	free(p_tsocket);

	return(VALID);
}
static void *TSocketBuild(void *self)
{
	if (!self) { return(NULL); }

	Tsocket *p_tsocket;
	p_tsocket = (Tsocket *)self;

	void **p_args = p_tsocket->pp_void;

	void *result = NULL;

	if (p_tsocket->size == 1)
	{
		result = p_tsocket->ptr_func1((p_args)[0]);
	}
	else 
	if (p_tsocket->size == 2)
	{
		result = p_tsocket->ptr_func2((p_args)[0], (p_args)[1]);
	}
	else
	if (p_tsocket->size == 3)
	{
		result = p_tsocket->ptr_func3((p_args)[0], (p_args)[1], (p_args)[2]);
	}

	if(p_tsocket->mutex)
		pthread_mutex_unlock(p_tsocket->mutex);

	TSocketDtor(p_tsocket);
	return(result);
}

static void *TSockDataInfo(void *self_data)
{
	if (!self_data) { return(NULL); }

	sockData *_data;
	_data = (sockData *)self_data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	int sz_func = 1;

	void *tmp_args[] = { _data, NULL };
	void **args;
	args = calloc(sz_func, sizeof(*args));
	if (!args) { return(NULL); }
	else
	{
		for (int i = 0; i < sz_func; i++)
			(args)[i] = tmp_args[i];
	}

	void *sucess;
	sucess = TSocketCtor(p_tsocket, SockDataInfo, args, sz_func, _data->self_mutex);
	if (!sucess) { return(NULL); }
	else
	{
		pthread_mutex_lock(p_tsocket->mutex);
		pthread_create(&_data->Tinfo, NULL, TSocketBuild, p_tsocket);
	}

	return(&_data->Tinfo);
}
static void *TServerCtor(void *self, void *arg1, void *arg2)
{
	sockServer *server;
	server = ServerNotNull(self);
	if (!server || !arg1 || !arg2) { return(NULL); }

	sockData *_data;
	_data = (sockData *)server->data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	int sz_func = 3;

	void *tmp_args[] = { self, arg1, arg2 };
	void **args;
	args = calloc(sz_func, sizeof(*args));
	if (!args) { return(NULL); }
	else
	{
		for (int i = 0; i < sz_func; i++)
			(args)[i] = tmp_args[i];
	}

	void *sucess;
	sucess = TSocketCtor(p_tsocket, ServerCtor, args, sz_func, _data->self_mutex);
	if (!sucess) { return(NULL); }
	else
	{
		pthread_mutex_lock(p_tsocket->mutex);
		pthread_create(&_data->Tctor, NULL, TSocketBuild, p_tsocket);
	}

	return(&_data->Tctor);
}
static void *TClientCtor(void *self, void *arg1, void *arg2)
{
	sockClient *client;
	client = ClientNotNull(self);
	if (!client || !arg1 || !arg2) { return(NULL); }

	sockData *_data;
	_data = (sockData *)client->data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	int sz_func = 3;

	void *tmp_args[] = { self, arg1, arg2 };
	void **args;
	args = calloc(sz_func, sizeof(*args));
	if (!args) { return(NULL); }
	else
	{
		for (int i = 0; i < sz_func; i++)
			(args)[i] = tmp_args[i];
	}

	void *sucess;
	sucess = TSocketCtor(p_tsocket, ClientCtor, args, sz_func, _data->self_mutex);
	if (!sucess) { return(NULL); }
	else
	{
		pthread_mutex_lock(p_tsocket->mutex);
		pthread_create(&_data->Tctor, NULL, TSocketBuild, p_tsocket);
	}

	return(&_data->Tctor);
}
static void *TServerDtor(void *self)
{
	sockServer *server;
	server = ServerNotNull(self);
	if (!server) { return(NULL); }

	sockData *_data;
	_data = (sockData *)server->data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	int sz_func = 1;

	void *tmp_args[] = { self, NULL };
	void **args;
	args = calloc(sz_func, sizeof(*args));
	if (!args) { return(NULL); }
	else
	{
		for (int i = 0; i < sz_func; i++)
			(args)[i] = tmp_args[i];
	}

	void *sucess;
	sucess = TSocketCtor(p_tsocket, ServerDtor, args, sz_func, _data->self_mutex);
	if (!sucess) { return(NULL); }
	else
	{
		pthread_cancel(_data->Tstart);//////////////////
		pthread_mutex_unlock(p_tsocket->mutex);
		pthread_mutex_lock(p_tsocket->mutex);
		pthread_create(&_data->Tdtor, NULL, TSocketBuild, p_tsocket);
	}

	return(&_data->Tdtor);
}
static void *TClientDtor(void *self)
{
	sockClient *client;
	client = ClientNotNull(self);
	if (!client) { return(NULL); }

	sockData *_data;
	_data = (sockData *)client->data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	int sz_func = 1;

	void *tmp_args[] = { self, NULL };
	void **args;
	args = calloc(sz_func, sizeof(*args));
	if (!args) { return(NULL); }
	else
	{
		for (int i = 0; i < sz_func; i++)
			(args)[i] = tmp_args[i];
	}

	void *sucess;
	sucess = TSocketCtor(p_tsocket, ClientDtor, args, sz_func, _data->self_mutex);
	if (!sucess) { return(NULL); }
	else
	{
		pthread_cancel(_data->Tstart);//////////////////
		pthread_mutex_unlock(p_tsocket->mutex);
		pthread_mutex_lock(p_tsocket->mutex);
		pthread_create(&_data->Tdtor, NULL, TSocketBuild, p_tsocket);
	}

	return(&_data->Tdtor);
}
static void *TServerStart(void *self)
{
	sockServer *server;
	server = ServerNotNull(self);
	if (!server) { return(NULL); }

	sockData *_data;
	_data = (sockData *)server->data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }
	
	int sz_func = 1;

	void *tmp_args[] = { self, NULL };
	void **args;
	args = calloc(sz_func, sizeof(*args));
	if (!args) { return(NULL); }
	else
	{
		for (int i = 0; i < sz_func; i++)
			(args)[i] = tmp_args[i];
	}

	void *sucess;
	sucess = TSocketCtor(p_tsocket, ServerStart, args, sz_func, _data->self_mutex);
	if (!sucess) { return(NULL); }
	else
	{
		pthread_mutex_lock(p_tsocket->mutex);
		pthread_create(&_data->Tstart, NULL, TSocketBuild, p_tsocket);
	}

	return(&_data->Tstart);
}
static void *TClientConnect(void *self)
{
	sockClient *client;
	client = ClientNotNull(self);
	if (!client) { return(NULL); }

	sockData *_data;
	_data = (sockData *)client->data;

	//pthread_join(_data->Tctor,NULL);

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	int sz_func = 1;

	void *tmp_args[] = { self, NULL };
	void **args;
	args = calloc(sz_func, sizeof(*args));
	if (!args) { return(NULL); }
	else
	{
		for (int i = 0; i < sz_func; i++)
			(args)[i] = tmp_args[i];
	}

	void *sucess;
	sucess = TSocketCtor(p_tsocket, ClientConnect, args, sz_func, _data->self_mutex);
	if (!sucess) { return(NULL); }
	else
	{
		pthread_mutex_lock(p_tsocket->mutex);
		pthread_create(&_data->Tstart, NULL, TSocketBuild, p_tsocket);
	}

	return(&_data->Tstart);
}
static void *TServerRequest(void *self, void *client_socket)
{
	sockServer *server;
	server = ServerNotNull(self);
	if (!server) { return(NULL); }

	sockData *_data;
	_data = (sockData*)server->data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	int sz_func = 1;

	void *tmp_args[] = { client_socket, NULL};
	void **args;
	args = calloc(sz_func, sizeof(*args));
	if (!args) { return(NULL); }
	else
	{
		for (int i = 0; i < sz_func; i++)
			(args)[i] = tmp_args[i];
	}

	void *sucess;
	sucess = TSocketCtor(p_tsocket, ServerRequest, args, sz_func, _data->request_mutex);
	if (!sucess) { return(NULL); }
	else
	{
		pthread_mutex_lock(p_tsocket->mutex);
		pthread_create(&_data->Trequest, NULL, TSocketBuild, p_tsocket);
	}

	return(&_data->Trequest);
}
static void *TClientRequest(void *self, void *request)
{
	sockClient *client;
	client = ClientNotNull(self);
	if (!client) { return(NULL); }

	sockData *_data;
	_data = (sockData *)client->data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	void *_sock;
	_sock = (void *)_data->sock;

	int sz_func = 2;

	void *tmp_args[] = { _sock, request };/// warning ****
	void **args;
	args = calloc(sz_func, sizeof(*args));
	if (!args) { return(NULL); }
	else
	{
		for (int i = 0; i < sz_func; i++)
			(args)[i] = tmp_args[i];
	}

	void *sucess;
	sucess = TSocketCtor(p_tsocket, ClientRequest, args, sz_func, _data->self_mutex);
	if (!sucess) { return(NULL); }
	else
	{
		pthread_mutex_lock(p_tsocket->mutex);
		pthread_create(&_data->Trequest, NULL, TSocketBuild, p_tsocket);
	}

	return(&_data->Trequest);
}

