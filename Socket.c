#include "Socket.h"

///////////////////////////////////////////////////////////////////////////////
#define DEFAULT_BUFLEN 512

static sockList *_list = NULL;//point to all extern data allocated.

static const char *_STR_OK = "OK";
static const char *_STR_ABORT = "ABORT";
static const char *_STR_FILESIZE = "SIZE";
static const char *_SERVER_DIR = "SERVER_FILES/";
static const char *_CLIENT_DIR = "CLIENT_FILES/";

static const size_t SZ_COMMAND = 3;
static const size_t SZ_FILETYPE = 3;
static const size_t SZ_FILENAME = 50;

static const size_t SZ_LISTEN = 5;
static const size_t SZ_SOCKINFO = 50;
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
	p_socklist = (sockList *)calloc(1,sizeof(*p_socklist));
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
	p_sockinfo = (sockInfo *)calloc(1, sizeof(sockInfo));
	if (!p_sockinfo) { return(NULL); }

	p_sockinfo->sock = (SOCKET *)malloc(sizeof(*(p_sockinfo)->sock));
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
	name = (char *)malloc(namelen + 1);
	if (!name) { return(NULL); }

	strcpy_s(name, namelen+1, hostname);

	p_sockinfo->hostname = name;

	return(VALID);
}

sockServer *ServerInit()
{
	sockServer* server;
	server = (sockServer*)calloc(1, sizeof(*server));
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
	_data = (sockData *)calloc(1, sizeof(*_data));
	if (!_data) { return(NULL); }

	_data->sock = (SOCKET *)malloc(sizeof(*_data->sock));
	if (!_data->sock) { return(NULL); }

	_data->self_mutex = (pthread_mutex_t *)calloc(1, sizeof(*_data->self_mutex));
	if (!_data->self_mutex) { return(NULL); }

	pthread_mutex_init(_data->self_mutex, NULL);
	server->data = _data;
	
	void *sucess = SockListCtor(&_list,server, SOCKT_SERVER);
	if (!sucess) { return(NULL); }

	return(server);
}
sockClient *ClientInit()
{
	sockClient* client;
	client = (sockClient*)calloc(1, sizeof(*client));
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
	_data = (sockData *)calloc(1, sizeof(*_data));
	if (!_data) { return(NULL); }

	_data->sock = (SOCKET *)malloc(sizeof(*_data->sock));
	if (!_data->sock) { return(NULL); }

	_data->self_mutex = (pthread_mutex_t *)calloc(1, sizeof(*_data->self_mutex));
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

	if (listen(*(_data)->sock, SZ_LISTEN) < 0) { return(NULL); }

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

	char **tmp_buffer = (char **)malloc(sizeof(*tmp_buffer) * 2);
	if (!tmp_buffer) { return(NULL); }
	else
	{
		for (int i = 0; i < 2; i++)
		{
			tmp_buffer[i] = (char *)malloc(sizeof(**tmp_buffer) * (SZ_SOCKINFO + 1));
			if (!tmp_buffer[i]) { return(NULL); }
			tmp_buffer[i][SZ_SOCKINFO] = '\0';
		}
		
	}
	
	getnameinfo((struct sockaddr*)_data->addr->ai_addr,
				(int)_data->addr->ai_addrlen,
				tmp_buffer[0], SZ_SOCKINFO, 
				tmp_buffer[1], SZ_SOCKINFO,
				NI_NUMERICHOST | NI_NUMERICSERV);

	char* buffer = (char*)malloc(sizeof(*buffer) * (SZ_SOCKINFO + 1));
	if (!buffer) { return(NULL); }
	else
	{
		buffer[SZ_SOCKINFO] = '\0';
	}

	sprintf_s(buffer, SZ_SOCKINFO, "Address:%s\nService:", tmp_buffer[0]);
	strcat_s(buffer, SZ_SOCKINFO, tmp_buffer[1]);

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

	pthread_mutex_init(_data->request_mutex, NULL);

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
					socket_client = (SOCKET *)malloc(sizeof(*socket_client));
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
	SOCKET *p_socket;
	p_socket = (SOCKET*)client_socket;

	if (!p_socket) { return(NULL); }

	char buffer[DEFAULT_BUFLEN] = { 0 };
	int bufflen = DEFAULT_BUFLEN;
	
	int recvbytes;
	recvbytes = recv(*p_socket, buffer, bufflen, 0);
	if (recvbytes <= 0) { return(NULL); }

	char **command, **filepath;
	command = (char **)malloc(sizeof(**command) * SZ_COMMAND + 1);
	filepath = (char **)malloc(sizeof(**filepath) * SZ_FILENAME + 1);
	if (!command || !filepath) { return(NULL); }

	void *result;
	result = DecryptRequest(buffer, command, filepath, _SERVER_DIR);
	if (!result) { return(NULL); }

	if (strcmp(*command, "SET") == 0)
	{
		result = ServerRequestSet(p_socket, *filepath);
	}
	else
	if (strcmp(*command, "GET") == 0)
	{
		result = ServerRequestGet(p_socket, *filepath);
	}

	free(*command);
	free(*filepath);
	free(command);
	free(filepath);
	command = NULL;
	filepath = NULL;

	if (!result)
	{
		*p_socket = INVALID_SOCKET;
		closesocket(*p_socket);
		return(NULL);
	}

	return(VALID);
}
static void *ClientRequest(void *client_socket, void *buffer)
{
	SOCKET *p_socket;
	p_socket = (SOCKET *)client_socket;

	char *p_buffer;
	p_buffer = (char *)buffer;

	if (!p_socket || !p_buffer) { return(NULL); }

	char **command, **filepath;
	command = (char **)malloc(sizeof(**command) * SZ_COMMAND + 1);
	filepath = (char **)malloc(sizeof(**filepath) * SZ_FILENAME + 1);
	if (!command || !filepath) { return(NULL); }

	void *result;
	result = DecryptRequest(p_buffer, command, filepath, _CLIENT_DIR);
	if (!result) { return(NULL); }

	if (strcmp(*command, "SET") == 0)
	{
		result = ClientRequestSet(p_socket, p_buffer, *filepath);
	}
	else
	if (strcmp(*command, "GET") == 0)
	{
		result = ClientRequestGet(p_socket, p_buffer, *filepath);
	}

	free(*command);
	free(*filepath);
	free(command);
	free(filepath);
	command = NULL;
	filepath = NULL;

	if (!result)
	{
		*p_socket = INVALID_SOCKET;
		closesocket(*p_socket);
		return(NULL);
	}

	return(VALID);
}

static void *DecryptRequest(void *buffer, void **str_command, void **str_filepath, const void *str_dir)
{
	char *p_buffer;
	p_buffer = (char *)buffer;

	char **pp_command;
	pp_command = (char **)str_command;

	char **pp_filepath;
	pp_filepath = (char **)str_filepath;

	char *p_dir;
	p_dir = (char *)str_dir;

	if (!p_buffer || !pp_command || !pp_filepath || !p_dir) { return(NULL); }

	char *cpy_buffer;
	size_t bufferlen = strlen(p_buffer);
	cpy_buffer = (char *)malloc(bufferlen + 1);
	if (!cpy_buffer) { return(NULL); }

	strcpy(cpy_buffer, p_buffer);
	if (!cpy_buffer) { return(NULL); }

	cpy_buffer = strtok(cpy_buffer, " ");
	bufferlen = strlen(cpy_buffer);
	if (bufferlen > SZ_COMMAND) { return(NULL); }

	char *p_command;
	p_command = (char *)malloc(sizeof(*p_command) * bufferlen + 1);
	if (!p_command) { return(NULL); }

	strcpy(p_command, cpy_buffer);
	if (!p_command) { return(NULL); }

	char *filename;
	size_t fnamelen;
	filename = strtok(NULL, ".");
	fnamelen = strlen(filename);
	if (fnamelen > SZ_FILENAME) { return(NULL); }

	char *filetype;
	filetype = strtok(NULL, " ");
	size_t ftypelen = strlen(filetype);
	if (ftypelen > SZ_FILETYPE) { return(NULL); }

	char *filepath;
	size_t filepathlen = strlen(p_dir) + fnamelen + ftypelen + 1;
	filepath = (char *)malloc(filepathlen + 1);
	if (!filepath) { return(NULL); }

	int result;
	result = sprintf(filepath, "%s%s%s%s", p_dir, filename, ".", filetype);
	if (result != filepathlen) { return(NULL); }
	
	*pp_command = p_command;
	*pp_filepath = filepath;

	free(cpy_buffer);
	return(VALID);
}
static void *ServerRequestSet(void *_socket, void *filepath)
{
	SOCKET *p_socket;
	p_socket = (SOCKET *)_socket;

	char *p_filepath;
	p_filepath = (char *)filepath;

	if (!p_socket || !p_filepath) { return(NULL); }

	FILE *file;
	file = fopen(p_filepath, "wb");
	if (!file) { return(NULL); }

	int sendbytes;
	sendbytes = send(*p_socket, _STR_OK, (int)strlen(_STR_OK), 0);
	if (sendbytes <= 0) { return(NULL); }

	char recvbuffer[DEFAULT_BUFLEN] = {0};
	int recvlen = DEFAULT_BUFLEN;

	int filesize;
	filesize = recv(*p_socket, recvbuffer, recvlen, 0);
	if (filesize <= 0) { return(NULL); }
	filesize = ntohl(filesize);
	memset(recvbuffer, 0, recvlen);

	int read = 0;
	while (read < filesize)
	{
		read += recv(*p_socket, recvbuffer, recvlen, 0);
		fwrite(recvbuffer, sizeof(*recvbuffer), recvlen, file);
		memset(recvbuffer, 0, recvlen);
	}

	fclose(file);
	return(VALID);
}
static void *ClientRequestSet(void *_socket, void *buffer, void *filepath)
{
	SOCKET *p_socket;
	p_socket = (SOCKET *)_socket;

	char *p_buffer;
	p_buffer = (char *)buffer;
	size_t bufflen = strlen(p_buffer);

	char *p_filepath;
	p_filepath = (char *)filepath;
	size_t fpathlen = strlen(p_filepath);

	if (!p_socket || !p_buffer || !p_filepath) { return(NULL); }

	FILE *file;
	file = fopen(filepath, "rb");
	if (!file) { return(NULL); }

	size_t filesize;
	fseek(file, 0, SEEK_END);
	filesize = (size_t)ftell(file);
	fseek(file, 0, SEEK_SET);

	int sendbytes;
	sendbytes = send(*p_socket, p_buffer, (int)bufflen, 0);
	if (sendbytes <= 0) { return(NULL); }

	char recvbuffer[DEFAULT_BUFLEN] = { 0 };
	int recvlen = DEFAULT_BUFLEN;

	int recvbytes;
	recvbytes = recv(*p_socket, recvbuffer, recvlen, 0);
	if (recvbytes <= 0) { return(NULL); }

	if (strcmp(recvbuffer, _STR_OK) != 0) { return(NULL); }
	memset(recvbuffer, 0, recvlen);

	size_t l_filesize;
	l_filesize = (size_t)htonl(filesize);

	sendbytes = send(*p_socket, (const char *)&l_filesize, sizeof(l_filesize), 0);
	if (sendbytes < 0) { return(NULL); }

	char sendbuffer[DEFAULT_BUFLEN] = { 0 };
	int sendlen = DEFAULT_BUFLEN;

	size_t read = 0;
	while (!feof(file) || read < filesize)
	{
		read += fread(sendbuffer, sizeof(*sendbuffer), sendlen, file);
		if (send(*p_socket, sendbuffer, sendlen, 0) < 0) { return(NULL); }
		memset(sendbuffer, 0, sendlen);
	}

	fclose(file);

	return(VALID);
}
static void *ServerRequestGet(void *_socket, void *filepath)
{
	SOCKET *p_socket;
	p_socket = (SOCKET *)_socket;

	char *p_filepath;
	p_filepath = (char *)filepath;

	if (!p_socket || !p_filepath) { return(NULL); }

	FILE *file;
	file = fopen(p_filepath, "rb");
	if (!file)
	{
		send(*p_socket, _STR_ABORT, (int)strlen(_STR_ABORT), 0);
		return(NULL);
	}

	int sendbytes;
	sendbytes = send(*p_socket, _STR_OK, (int)strlen(_STR_OK), 0);
	if (sendbytes < 0) { return(NULL); }

	size_t filesize;
	fseek(file, 0, SEEK_END);
	filesize = (size_t)ftell(file);
	fseek(file, 0, SEEK_SET);
	
	int l_filesize;
	l_filesize = htonl(filesize);
	sendbytes = send(*p_socket, (const char *)&l_filesize, sizeof(l_filesize), 0);
	if (sendbytes < 0) { return(NULL); }

	char sendbuffer[DEFAULT_BUFLEN] = { 0 };
	int sendlen = DEFAULT_BUFLEN;
	size_t read = 0;
	while (!feof(file) || read < filesize)
	{
		read += fread(sendbuffer, sizeof(*sendbuffer), sendlen, file);
		if (send(*p_socket, sendbuffer, sendlen, 0) < 0) { return(NULL); }
		memset(sendbuffer, 0, sendlen);
	}	

	fclose(file);
	return(VALID);
}
static void *ClientRequestGet(void *_socket, void *buffer, void *filepath)
{
	SOCKET *p_socket;
	p_socket = (SOCKET *)_socket;

	char *p_buffer;
	p_buffer = (char *)buffer;
	size_t bufflen = strlen(p_buffer);

	char *p_filepath;
	p_filepath = (char *)filepath;
	size_t fpathlen = strlen(p_filepath);

	if (!p_socket || !p_buffer || !p_filepath ) { return(NULL); }

	int sendbytes;
	sendbytes = send(*p_socket, p_buffer, (int)bufflen, 0);
	if (sendbytes <= 0) { return(NULL); }

	char recvbuffer[DEFAULT_BUFLEN] = { 0 };
	int recvlen = DEFAULT_BUFLEN;

	int recvbytes;
	recvbytes = recv(*p_socket, recvbuffer, recvlen, 0);
	if (recvbytes <= 0) { return(NULL); }

	if (strcmp(recvbuffer, _STR_OK) != 0) { return(NULL); }
	memset(recvbuffer, 0, recvlen);

	FILE *file;
	file = fopen(filepath, "wb");
	if (!file) { return(NULL); }

	int filesize = 0;
	recvbytes = recv(*p_socket, (char *)&filesize, sizeof(filesize), 0);
	if (recvbytes <= 0) { return(NULL); }
	filesize = ntohl(filesize);

	
	int read = 0;
	while (read < filesize)
	{
		read += recv(*p_socket, recvbuffer, recvlen, 0);
		fwrite(recvbuffer, sizeof(*recvbuffer), recvlen, file);
		memset(recvbuffer, 0, recvlen);
	}

	fclose(file);
	return(VALID);
}

static void *ThreadGet(void *thread)
{
	if (!thread) { return(NULL); }

	pthread_t *p_thread;
	p_thread = (pthread_t *)thread;

	void *buffer;
	buffer = (void *)calloc(1, sizeof(buffer));
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
	p_tsocket = (Tsocket *)calloc(1, sizeof(*p_tsocket));
	if (!p_tsocket) { return(NULL); }
	
	return(p_tsocket);
}
static void *TSocketCtor(void *self, void *p_func, void **args, int sz_func, void *mutex)
{
	if (!self) { return(NULL); }

	Tsocket *p_tsocket;
	p_tsocket = (Tsocket *)self;

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
	if (!self) { return(NULL); }

	Tsocket *p_tsocket;
	p_tsocket = (Tsocket *)self;

	p_tsocket->mutex = NULL;

	free(self);
	self = NULL;

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
	else if (p_tsocket->size == 2)
	{
		result = p_tsocket->ptr_func2((p_args)[0], (p_args)[1]);
	}
	else if (p_tsocket->size == 3)
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
	//selfdata
	if (!self_data) { return(NULL); }

	sockData *_data;
	_data = (sockData *)self_data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	int sz_func = 1;

	void *tmp_args[] = { _data, NULL };
	void **args;
	args = (void **)calloc(sz_func, sizeof(*args));
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
	args = (void **)calloc(sz_func, sizeof(*args));
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
	args = (void **)calloc(sz_func, sizeof(*args));
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
	args = (void **)calloc(sz_func, sizeof(*args));
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
	args = (void **)calloc(sz_func, sizeof(*args));
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
	args = (void **)calloc(sz_func, sizeof(*args));
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
	args = (void **)calloc(sz_func, sizeof(*args));
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
	args = (void **)calloc(sz_func, sizeof(*args));
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
		pthread_create(&_data->Trequest, NULL, TSocketBuild, p_tsocket);
	}

	return(&_data->Trequest);
}
static void *TClientRequest(void *self, void *request)
{
	sockClient *client;
	client = ServerNotNull(self);
	if (!client) { return(NULL); }

	sockData *_data;
	_data = (sockData *)client->data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	int sz_func = 2;

	void *tmp_args[] = { (void*)_data->sock, request };/// warning ****
	void **args;
	args = (void **)calloc(sz_func, sizeof(*args));
	if (!args) { return(NULL); }
	else
	{
		for (int i = 0; i < sz_func; i++)
			(args)[i] = tmp_args[i];
	}

	void *sucess;
	sucess = TSocketCtor(p_tsocket, ClientRequest, args, sz_func, NULL);
	if (!sucess) { return(NULL); }
	else
	{
		pthread_create(&_data->Trequest, NULL, TSocketBuild, p_tsocket);
	}

	return(&_data->Trequest);
}

//VOID* RecData(void* arg)
//{	
//	struct sockaddr_storage client_address;
//	socklen_t client_len = sizeof(client_address);
//	SOCKET client = accept(server, (struct sockaddr*)&client_address, &client_len);
//	if (client < 0 || client == INVALID_SOCKET)/////// INVALID SOCKET// alerta de gambiarra relacionado a threads
//	{
//		return(NULL);
//	}
//
//	while (1)
//	{
//		char ReadBuffer[101];
//		int bytes_received = recv(client, ReadBuffer, 100, 0);
//		if (bytes_received <= 0)
//			break;
//		else if (bytes_received > 0)
//		{
//			ReadBuffer[bytes_received] = '\0';
//			vParsedComma(ReadBuffer);
//			int bytes_sended = send(client, ReadBuffer, strlen(ReadBuffer), 0);
//			if (bytes_sended < 0)
//				return(NULL);
//		}
//	}
//	return(NULL);
//}
//
//static VOID vParsedComma(char *vComma)
//{
//	char* token;
//	int i = 0;
//
//	token = strtok(vComma, ",");
//
//	while (token != NULL)
//	{
//		*(vtoken + i++) = atoi(token);
//		token = strtok(NULL, ",");
//	}
//}
//int* vToken()
//{
//	return(vtoken);
//}


