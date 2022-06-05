#include "Socket.h"

///////////////////////////////////////////////////////////////////////////////
#define DEFAULT_BUFLEN 512

pthread_once_t _once = PTHREAD_ONCE_INIT; 

static sockList *_list = NULL;//point to all extern data allocated.

static const char *STR_OK = "OK";
static const char *STR_ABORT = "ABORT";
static const char *STR_SERVER_DIR = "./SERVER_FILES";
static const char *STR_CLIENT_DIR = "./CLIENT_FILES";
static char *DEFAULT_PORT = "22";

static const size_t SIZE_COMMAND = 5;
static const size_t SIZE_FILETYPE = 3;
static const size_t SIZE_FILENAME = 50;

static const size_t SIZE_LISTEN = 5;
static const size_t SIZE_SOCKINFO = 50;
///////////////////////////////////////////////////////////////////////////////
int WsaCtor()
{
	WSADATA WSAdata = {0};

	if (WSAStartup(MAKEWORD(2, 2), &WSAdata)){return(1);}

	_list = SockListInit();
	if (!_list) { return(1); }

	return(0);
}
int WsaDtor()
{
	SockListClear(_list);
	WSACleanup();

	return(0);
}

static sockList *SockListInit()
{
	sockList *p_socklist = NULL;
	p_socklist = calloc(1,sizeof(*p_socklist));
	if (!p_socklist) { return(NULL); }
	
	p_socklist->self = p_socklist;

	return(p_socklist);
}
static sockList **SockListNotNull(void **pp_self)
{
	sockList **p_socklist;
	p_socklist = (sockList **)pp_self;

	if (!(*p_socklist) || !(*p_socklist)->self) { return(NULL); }

	return(p_socklist);
}
static void *SockListCtor(void **pp_self,void *_data, int _type)
{
	sockList **pp_socklist;
	pp_socklist = SockListNotNull(pp_self);

	if (!pp_socklist || !_data || _type >= SOCKT_MAX) { return(NULL); }

	(*pp_socklist)->data = _data;
	(*pp_socklist)->sock_type = (socklistType)_type;

	(*pp_socklist)->next = SockListInit();
	if (!(*pp_socklist)->next) { return(NULL); }

	(*pp_socklist)->next->last = (*pp_socklist);

	(*pp_socklist) = (*pp_socklist)->next;

	return(VALID);
}
static void *SockListDtor(void **pp_self)
{
	sockList **pp_socklist = NULL;
	pp_socklist = SockListNotNull(pp_self);

	if(!pp_socklist) { return(NULL); }

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

	return(VALID);
}
static void *SockListRemove(void *pp_self, void *_data)
{
	sockList *p_list;
	p_list = (sockList *)pp_self;

	int found = 0;

	while (p_list)
	{
		if (p_list->data == _data)
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
	
	if (p_list->next)
	{
		p_list->next->last = p_list->last;
	}
	else if (!p_list->next)
	{
		_list = p_list->last;
	}

	SockListDtor(&p_list);

	return(VALID);
}
static void *SockListClear(void *_socklist)
{
	sockList *p_socklist = NULL;
	p_socklist = (sockList*)_socklist;

	if (!p_socklist) { return(NULL); }

	sockList *p_list, *tmp_list;
	p_list = p_socklist;
	tmp_list = p_socklist;

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
	sockInfo *p_sockinfo = NULL;
	p_sockinfo = calloc(1, sizeof(sockInfo));
	if (!p_sockinfo) { return(NULL); }

	p_sockinfo->sock = NULL;
	p_sockinfo->current_dir = NULL;

	p_sockinfo->sock = malloc(sizeof(*(p_sockinfo)->sock));
	if (!p_sockinfo->sock) { goto error; }

	p_sockinfo->current_dir = malloc(sizeof(*p_sockinfo->current_dir)* strlen(STR_SERVER_DIR)+1);
	if(!p_sockinfo->current_dir){ goto error; }

	p_sockinfo->last_dir = malloc(sizeof(*p_sockinfo->last_dir) * strlen(STR_SERVER_DIR) + 1);
	if (!p_sockinfo->last_dir) { goto error; }

	strcpy(p_sockinfo->current_dir, STR_SERVER_DIR);
	if(!p_sockinfo->current_dir) { goto error; }

	strcpy(p_sockinfo->last_dir, STR_SERVER_DIR);
	if (!p_sockinfo->last_dir) { goto error; }
	
	p_sockinfo->self = p_sockinfo;

	return(p_sockinfo);

	error:;
	if(p_sockinfo->sock){free(p_sockinfo->sock); }
	if(p_sockinfo->current_dir){free(p_sockinfo->current_dir); }
	return(NULL);
}
static sockInfo *SockInfoNotNull(void *_self)
{
	sockInfo *p_sockinfo;
	p_sockinfo = (sockInfo *)_self;

	if (!p_sockinfo || !p_sockinfo->self) { return(NULL); }

	return(p_sockinfo);
}
static void *SockInfoCtor(void *_self, void *_socket)
{
	sockInfo *p_sockinfo;
	p_sockinfo = SockInfoNotNull(_self);
	if (!p_sockinfo) { return(NULL); }

	SOCKET *p_socket;
	p_socket = (SOCKET *)_socket;

	*(p_sockinfo)->sock = *p_socket;

	return(VALID);
}
static void *SockInfoDtor(void *_self)
{
	sockInfo *p_sockinfo;
	p_sockinfo = SockInfoNotNull(_self);
	if (!p_sockinfo) { return(NULL); }

	p_sockinfo->self = NULL;
	
	closesocket(*p_sockinfo->sock);
	free(p_sockinfo->sock);
	free(p_sockinfo);

	return(VALID);
}
static void *SockInfoSetHost(void *_self, void *_hostname)
{
	sockInfo *p_sockinfo = NULL;
	p_sockinfo = (sockInfo *)_self;
	if (!p_sockinfo) { return(NULL); }

	char *p_hostname = NULL;
	p_hostname = (char*)_hostname;

	size_t _hostlen;
	_hostlen = strlen(_hostname);

	p_sockinfo->hostname = malloc(sizeof(*p_sockinfo->hostname) * (_hostlen + 1));
	if (!p_sockinfo->hostname) { return(NULL); }

	strcpy(p_sockinfo->hostname, p_hostname);

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
	pthread_mutex_init(_data->self_mutex, NULL);

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
static sockServer *ServerNotNull(void *_self)
{
	sockServer *server;
	server = (sockServer *)_self;

	if (!server || !server->self) { return(NULL); }

	return(server);
}
static sockClient *ClientNotNull(void *_self)
{
	sockClient *client;
	client = (sockClient *)_self;
	if (!client || !client->self) { return(NULL); }

	return(client);
}
static void *ServerCtor(void *_self, void *_address, void *_service)
{
	sockServer* server;
	server = ServerNotNull(_self);
	if (!server) { return(NULL); }

	sockData *_data;
	_data = (sockData *)server->data;

	const char *p_address = (const char *)_address;
	const char *p_service = (const char *)_service;

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;															
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_TCP;

	
	if (getaddrinfo(p_address, p_service, &hints, &_data->addr)) { return(NULL); }

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
static void *ClientCtor(void *_self, void *host, void *service)
{
	sockClient *client;
	client = ClientNotNull(_self);
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
static void *ServerDtor(void *_self)
{
	sockServer *server;
	server = ServerNotNull(_self);
	if (!server) { return(NULL); }

	sockData *_data;
	_data = (sockData *)server->data;

	_data->self_mutex = NULL;
	server->self = NULL;

	freeaddrinfo(_data->addr);
	free(_data->sockinfo);
	closesocket(*(_data)->sock);
	free(_data->sock);
	free(server);

	return(VALID);
}
static void *ClientDtor(void *_self)
{
	sockClient *client;
	client = ClientNotNull(_self);
	if (!client) { return(NULL); }

	sockData *_data;
	_data = (sockData *)client->data;

	_data->self_mutex = NULL;
	client->self = NULL;

	freeaddrinfo(_data->addr);
	free(_data->sockinfo);
	closesocket(*(_data)->sock);
	free(_data->sock);
	free(client);

	return(VALID);
}
static void *SockDataInfo(void *_self)
{
	sockData *_data = NULL;
	_data = (sockData *)_self;

	if (!_data) { return(NULL); }

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

	_data->sockinfo = buffer;

	return(_data->sockinfo);
}
static void *ServerStart(void *_self)
{
	sockServer *server = NULL;
	server = ServerNotNull(_self);
	if (!server) { return(NULL); }

	sockData *_data = NULL;
	_data = (sockData *)server->data;

	sockList *p_socklist;//list to sockinfo
	p_socklist = SockListInit();
	if (!p_socklist) { return(NULL); }

	sockInfo *p_sockinfo = NULL;
	p_sockinfo = SockInfoInit();
	if (!p_sockinfo) { return(NULL); }

	SockInfoCtor(p_sockinfo, _data->sock);
	SockInfoSetHost(p_sockinfo, "SERVER");
	SockListCtor(&p_socklist, p_sockinfo, SOCKT_FILE);

	fd_set master = {0};
	FD_ZERO(&master);
	FD_SET(*(_data)->sock, &master);

	SOCKET max_socket = *(_data)->sock;
	SOCKET last_client = 0;
	int result = 0;

	char hostname[NI_MAXHOST];
	sockList *p_tmpsocklist;
	sockInfo *p_tmpsockinfo;

	while (1)
	{
		fd_set reads = {0};
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
					socklen_t client_len = {0};
					client_len = sizeof(client_addr);

					SOCKET *socket_client = {0};
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

					p_sockinfo = SockInfoInit();
					if (!p_sockinfo) { return(NULL); }

					SockInfoCtor(p_sockinfo, socket_client);
					SockInfoSetHost(p_sockinfo, hostname);
					SockListCtor(&p_socklist, p_sockinfo, SOCKT_FILE);
					free(socket_client);
					socket_client = NULL;
				}
				else
				{
					FD_CLR(*p_tmpsockinfo->sock, &master);
					TServerRequest(server, p_tmpsockinfo, p_socklist->self);
					//Received from client
				}
			}
			p_tmpsocklist = p_tmpsocklist->last;
		}
	}

	return(VALID);
}
static void *ClientConnect(void *_self)
{
	sockClient *client;
	client = ClientNotNull(_self);
	if (!client) { return(NULL); }

	sockData *_data;
	_data = (sockData *)client->data;

	if (connect(*(_data)->sock,
		_data->addr->ai_addr,
		(int)_data->addr->ai_addrlen)) { return(NULL); }

	return(VALID);
}
static void *ServerRequest(void *_sockinfo, void *_socklist)
{
	void *result = NULL;

	sockInfo *p_sockinfo;
	p_sockinfo = (sockInfo*)_sockinfo;

	sockList *p_socklist;
	p_socklist = (sockList*)_socklist;

	SOCKET *p_socket;
	p_socket = p_sockinfo->sock;

	if (!p_socket) { return(NULL); }

	char *ftp_command = NULL;

	int failed = 0;
	do
	{
		if(ftp_command) { free(ftp_command); ftp_command = NULL;}

		char recvbuffer[DEFAULT_BUFLEN] = { 0 };
		int recvlen = DEFAULT_BUFLEN-1;

		int recvbytes = 0;
		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes <= 0 || recvbytes > (int)SIZE_COMMAND) { failed++; break; }

		ftp_command = malloc(sizeof(*ftp_command) * strlen(recvbuffer) + 1);
		if (!ftp_command) { failed++; break; }

		strcpy(ftp_command, recvbuffer);
		memset(recvbuffer, 0, recvlen);

		size_t commandlen = strlen(ftp_command);

		if (commandlen == 2)
		{
			if (strcmp(ftp_command, "LS") == 0)
			{
				result = ServerRequestLS(p_sockinfo);
				if (!result) { failed++; break; }
				continue;
			}
			else
			if (strcmp(ftp_command, "CD") == 0)
			{
				result = ServerRequestCD(p_sockinfo);
				if (!result) { failed++; break; }
				continue;
			}
		}

		if (commandlen == 3)
		{
			if (strcmp(ftp_command, "PWD") == 0)
			{
				result = ServerRequestPWD(p_sockinfo);
				if (!result) { failed++; break; }
				continue;
			}
			else
			if (strcmp(ftp_command, "PUT") == 0)
			{
				result = ServerRequestPUT(p_sockinfo, 1);
				if (!result) { failed++; break; }
				continue;
			}
			else
			if (strcmp(ftp_command, "GET") == 0)
			{
				result = ServerRequestGET(p_socket, 1);
				if (!result) { failed++; break; }
				continue;
			}
		}

		if (commandlen == 4)
		{
			if (strcmp(ftp_command, "MPUT") == 0)
			{
				result = ServerRequestMPUT(p_socket);
				if (!result) { failed++; break; }
				continue;
			}
			else
			if (strcmp(ftp_command, "MGET") == 0)
			{
				result = ServerRequestMGET(p_socket);
				if (!result) { failed++; break; }
				continue;
			}
		}

	}while(failed == 0);

	if (failed)
	{
		if(ftp_command) 
		{ 
			free(ftp_command); 
		}
		SockListRemove(p_socklist, p_sockinfo);
		return(NULL);
	}
	
	return(VALID);
}
static void *ClientRequest(void *_self, void *_buffer)
{
	sockClient *client;
	client = (sockClient*)_self;

	sockData *p_selfdata;
	p_selfdata = (sockData*)client->data;
	
	SOCKET *p_socket = NULL;
	p_socket = p_selfdata->sock;

	char *p_buffer = NULL;
	p_buffer = (char *)_buffer;

	if (!p_socket || !p_buffer) { return(NULL); }

	if (strlen(p_buffer) > DEFAULT_BUFLEN) { return(NULL); }

	void *result = NULL;
	char ***items = NULL;
	char *cpy_buffer = NULL;
	u_long n_items = 0;
	
	int failed = 0;
	do
	{
		cpy_buffer = malloc(strlen(p_buffer) + 1);
		if (!cpy_buffer) { failed++; break; }

		strcpy(cpy_buffer, p_buffer);

		char *ftp_command = NULL;
		ftp_command = strtok(cpy_buffer, " ");
		size_t ftpcommand_len = strlen(ftp_command);

		if (!ftp_command || ftpcommand_len > SIZE_COMMAND || ftpcommand_len < 2)
		{ failed++; break; }

		char *raw_items = NULL;
		raw_items = strtok(NULL, "\0");

		if (raw_items)
		{
			items = calloc(1, sizeof(*items));
			if (!items) { failed++; break; }

			result = GetRequestItems(raw_items, items, &n_items);
			if (!result) { failed++; break; }
			result = NULL;
		}


		if (ftpcommand_len == 2)//2
		{
			if (strcmp(ftp_command, "LS") == 0)
			{
				result = ClientRequestLS(p_socket);
				if (!result) { failed++; break; }
				break;
			}
			
			if (strcmp(ftp_command, "CD") == 0)
			{
				if (!items)
				{	
					result = ClientRequestCD(p_socket, NULL, n_items);
					if (!result) { failed++; break; }
				}
				else 
				if(*items)
				{
					result = ClientRequestCD(p_socket, **items, n_items);
					if (!result) { failed++; break; }
				}
				break;
			}
		}


		if (ftpcommand_len == 3)//3
		{
			if (strcmp(ftp_command, "CLS") == 0)
			{
				result = ClientRequestCLS();
				if(!result) { failed++; break; }
				break;
			}

			if (strcmp(ftp_command, "PWD") == 0)
			{
				result = ClientRequestPWD(p_socket);
				if(!result) { failed++; break; }
				break;
			}

			if (strcmp(ftp_command, "PUT") == 0)
			{
				if (!items) { failed++; break; }
				result = ClientRequestPUT(p_socket, *items, 1);
				if (!result) { failed++; break; }
				break;
			}
			
			if (strcmp(ftp_command, "GET") == 0)
			{
				if (!items) { failed++; break; }
				result = ClientRequestGET(p_socket, *items, 1);
				if (!result) { failed++; break; }
				break;
			}
		}


		if (ftpcommand_len == 4)//4
		{
			if (strcmp(ftp_command,"OPEN") == 0)
			{
				if(!items) { failed++; break; }
				result = ClientRequestOPEN(client, **items);
				if(!result) { failed++; break; }
				break;
			}

			if (strcmp(ftp_command,"HELP") == 0)
			{
				result = ClientRequestHELP();
				if(!result) { failed++; break; }
			}

			if (strcmp(ftp_command, "MPUT") == 0)
			{
				if (!items) { failed++; break; }
				result = ClientRequestMPUT(p_socket, *items, n_items);
				if(!result) { failed++; break; }
				break;
			}
		
			if (strcmp(ftp_command, "MGET") == 0)
			{
				if (!items) { failed++; break; }
				result = ClientRequestMGET(p_socket, *items, n_items);
				if (!result) { failed++; break; }
				break;
			}
		}

		if (ftpcommand_len == 5)//5
		{
			if (strcmp(ftp_command,"CLOSE") == 0)
			{
				result = ClientRequestCLOSE(client);
				if(!result) { failed++; break; }
				break;
			}
		}

	}while(0);
	
	if(failed) 
	{ 
		if(cpy_buffer) { free(cpy_buffer); }
		if(items)
		{
			if(*items)
			{
				for(u_long i = 0; i < n_items; ++i)
				{
					free((*items)[i]);
				}
			}
		}
		printf("DISCONECTED\n");
		return(NULL); 
	}

	return(VALID);
}

static void *FindDir(void *_sockinfo, void *_dir, void *_find)
{
	sockInfo *p_sockinfo;
	p_sockinfo = (sockInfo*)_sockinfo;

	char *p_dir;
	p_dir = (char*)_dir;
	size_t p_dir_len = strlen(p_dir);

	int *p_find;
	p_find = (int*)_find;

	wchar_t *wcs_dir = NULL;

	int failed = 0;
	do
	{
		
		wcs_dir = calloc(p_dir_len + 1, sizeof(*wcs_dir));
		if (!wcs_dir) { failed++; break; }

		mbstowcs(wcs_dir, p_dir, p_dir_len);

		tinydir_dir dir;
		tinydir_open(&dir, (TCHAR *)wcs_dir);

		while (dir.has_next)
		{
			tinydir_file file;
			tinydir_readfile(&dir, &file);
			
			if (file.is_dir)
			{
				if (wcscmp(wcs_dir, file.name))
				{
					*p_find = 1;
					break;
				}
			}

			tinydir_next(&dir);
		}

		tinydir_close(&dir);
		free(wcs_dir);

	}while(0);
	
	if (failed)
	{
		if(wcs_dir) { free(wcs_dir); }
	}

	return(VALID);
}
static void *FileNamesOnDir(void *_dir, void ***_filenames, u_long *n_files)
{
	char ***ppp_filenames = NULL;
	ppp_filenames = (char***)_filenames;

	char *p_dir = NULL;
	p_dir = (char *)_dir;
	int p_dirlen = strlen(p_dir);

	wchar_t *wcs_dir = NULL;
	char **pp_filenames = NULL;
	u_long i = 0;
	
	int failed = 0;
	do
	{
		wcs_dir = calloc(p_dirlen + 1, sizeof(*wcs_dir));
		if(!wcs_dir) { failed++; break; }

		mbstowcs(wcs_dir, p_dir, p_dirlen);

		tinydir_dir dir = {0};
		tinydir_open(&dir, (TCHAR*)wcs_dir);

		while (dir.has_next)
		{
			tinydir_file file = {0};
			tinydir_readfile(&dir, &file);

			char **tmp_filenames = NULL;
			tmp_filenames = realloc(pp_filenames, sizeof(*pp_filenames) * (i + 1));
			if (!tmp_filenames) { failed++; break; }

			pp_filenames = tmp_filenames;
	 
			pp_filenames[i] = calloc(wcslen(file.name) + 1, sizeof((*pp_filenames)[i]));
			if (!pp_filenames[i]) { failed++; break; }

			wcstombs(pp_filenames[i], file.name, wcslen(file.name));

			i++;

			tinydir_next(&dir);
		}

		if(failed) { break; }

		tinydir_close(&dir);
		free(wcs_dir);

		*n_files = i;
		*ppp_filenames = pp_filenames;


	}while(0);

	if(failed) 
	{ 
		if (wcs_dir) { free(wcs_dir); }

		if (pp_filenames)
		{
			for (u_long j = 0; j < i; ++j)
			{
				free(pp_filenames[j]);
			}
			free(pp_filenames);
		}
		return(NULL);
	}
	
	return(VALID);
}
static void *GetRequestItems(void *rawitems, void ***_items, u_long *n_items)
{
	char *raw_items, ***ppp_items;
	raw_items = (char *)rawitems;
	ppp_items = (char ***)_items;

	int failed = 0;
	do
	{
		char **pp_items = NULL;
		char *item_name = NULL;
		item_name = strtok(raw_items, " ");

		int i = 0;
		while (item_name != NULL)
		{
			size_t item_namelen = strlen(item_name);
			if (item_namelen > SIZE_FILENAME) { failed++; break; }

			char **tmp_items = NULL;
			tmp_items = realloc(pp_items, sizeof(*pp_items) * (i + 1));
			if (!tmp_items) { failed++; break; }

			pp_items = tmp_items;

			pp_items[i] = malloc(item_namelen + 1);
			if (!pp_items[i]) { failed++; break; }

			strcpy(pp_items[i], item_name);

			i++;

			item_name = strtok(NULL, " ");
		}

		if(failed) 
		{
			if (pp_items)
			{
				for (int j = 0; j < i; ++j)
				{
					free(pp_items[i]);
				}
			}
			break; 
		}

		*ppp_items = pp_items;
		*n_items = (u_long)(i);

	}while(0);
	
	if(failed) { return(NULL); }
	
	return(VALID);
}

static void *ServerRequestLS(void *_sockinfo)
{
	sockInfo *p_sockinfo = NULL;
	p_sockinfo = (sockInfo *)_sockinfo;

	SOCKET *p_socket = NULL;
	p_socket = p_sockinfo->sock;

	char ***filenames = NULL;
	u_long n_files = 0;

	int failed = 0;
	do
	{
		filenames = calloc(1, sizeof(*filenames));
		if (!filenames) { failed++; break; }

		void *result = NULL;
		result = FileNamesOnDir(p_sockinfo->current_dir, filenames, &n_files);
		if (!result) { failed++; break; }

		u_long ul_nfiles = htonl(n_files);

		int sendbytes = 0;
		sendbytes = send(*p_socket, (const char *)&ul_nfiles, sizeof(ul_nfiles), 0);
		if (sendbytes <= 0) { failed++; break; }

		char recvbuffer[DEFAULT_BUFLEN] = { 0 };
		int recvlen = DEFAULT_BUFLEN-1;

		int recvbytes = 0;
		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes <= 0) { failed++; break; }

		if (strcmp(recvbuffer, STR_OK) != 0) { failed++; break; }
		memset(recvbuffer, 0, recvlen);

		u_long i = 0;
		for (i = 0; i < n_files; ++i)
		{
			sendbytes = send(*p_socket, (*filenames)[i], strlen((*filenames)[i]), 0);
			if (sendbytes <= 0) { failed++; break; }

			free((*filenames)[i]);
			(*filenames)[i] = NULL;

			recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
			if (recvbytes <= 0) { failed++; break; }

			if (strcmp(recvbuffer, STR_OK) != 0) { failed++; break; }
			memset(recvbuffer, 0, recvlen);
		}

		if (failed)
		{
			for (u_long j = n_files; j > i; )
			{
				if ((*filenames)[i]) //i think is safe, but have warning;
					free((*filenames)[i]);
			}
			free(*filenames);
			free(filenames);
			break;
		}

		free(*filenames);
		free(filenames);

	}while (0);

	if (failed) { return(NULL); }

	return(VALID);
}
static void *ClientRequestLS(void *_socket)
{
	SOCKET *p_socket = NULL;
	p_socket = (SOCKET *)_socket;

	int failed = 0;
	do
	{
		int sendbytes = 0;
		sendbytes = send(*p_socket, "LS", 2, 0);
		if (sendbytes <= 0) { failed++; break; }

		u_long n_files = 0;
		int recvbytes = 0;
		recvbytes = recv_s(*p_socket, (char *)&n_files, sizeof(n_files), 1);
		if (recvbytes <= 0) { failed++; break; }

		n_files = ntohl(n_files);

		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { failed++; break; }

		char recvbuffer[DEFAULT_BUFLEN] = { 0 };
		int recvlen = DEFAULT_BUFLEN-1;

		for (u_long i = 0; i < n_files; ++i)
		{
			memset(recvbuffer, 0, recvlen);
			recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
			if (recvbytes <= 0) { failed++; break; }

			printf("%s\n", recvbuffer);

			sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
			if (sendbytes <= 0) { failed++; break; }
		}

		if (failed) { break; }

	} while (0);

	if (failed) { return(NULL); }

	return(VALID);
}
static void *ServerRequestCD(void *_sockinfo)
{
	sockInfo *p_sockinfo = NULL;
	p_sockinfo = (sockInfo*)_sockinfo;

	SOCKET *p_socket = NULL;
	p_socket = p_sockinfo->sock;

	char **current_dir = &p_sockinfo->current_dir;
	char *tmp_str = NULL;
	char *tmp_alloc = NULL;

	int failed = 0;
	do
	{
		int sendbytes = 0;
		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { failed++; break; }

		u_long n_items = 0;
		int recvbytes = 0;
		recvbytes = recv_s(*p_socket, (char*)&n_items, sizeof(n_items), 1);
		if(recvbytes <= 0) { failed++; break; }
		
		n_items = ntohl(n_items);

		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { failed++; break; }

		if (n_items == 1)
		{
			char recvbuffer[DEFAULT_BUFLEN] = {0};
			int recvlen = DEFAULT_BUFLEN-1;

			recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
			if (recvbytes <= 0) { failed++; break; }

			tmp_alloc = malloc(sizeof(*tmp_alloc) * (strlen(recvbuffer) + strlen(*current_dir) + 1));
			if(!tmp_alloc) { failed++; break; }
			size_t tmpalloc_len = strlen(tmp_alloc);

			sprintf(tmp_alloc, "%s/%s", (char*)(*current_dir), recvbuffer);

			void *result = NULL;
			int find = 0;
			result = FindDir(p_sockinfo, tmp_alloc, &find);
			if (!result) { failed++; break; }

			if (find)
			{
	
				tmp_str = realloc(*current_dir, tmpalloc_len + 1);
				if(!tmp_str) { failed++; break; }

				*current_dir = tmp_str;
				tmp_str = NULL;

				strcpy(*current_dir, tmp_alloc);
				tmp_alloc = NULL;

				sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
				if (sendbytes <= 0) { failed++; break; }
			}
			else
			{
				sendbytes = send(*p_socket, STR_ABORT, (int)strlen(STR_ABORT), 0);
				if (sendbytes <= 0) { failed++; break; }
			}
		}
		else
		if (n_items == 0)
		{
			if (strcmp((*current_dir), STR_SERVER_DIR) != 0)
			{
				size_t i = 0;
				for (i = strlen((*current_dir)); i > strlen(STR_SERVER_DIR); --i)
				{
					if ((*current_dir)[i] == '/') { break; }
				}
					
				
				tmp_str = realloc((*current_dir), sizeof(**current_dir) * (i+1));
				if(!tmp_str) { failed++; break; }

				(*current_dir) = tmp_str;
				tmp_str = NULL;

				(*current_dir)[i] = '\0';

				sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
				if (sendbytes <= 0) { failed++; break; }
			}
			else
			{
				sendbytes = send(*p_socket, STR_ABORT, (int)strlen(STR_ABORT), 0);
				if (sendbytes <= 0) { failed++; break; }
			}
		}	

	}while(0);

	if (failed)
	{
		if(tmp_str) { free(tmp_str); }
		if(tmp_alloc) { free(tmp_alloc); }
		return(NULL);
	}
	
	return(VALID);
}
static void *ClientRequestCD(void *_socket, void *_items, u_long n_items)
{
	SOCKET *p_socket = NULL;
	p_socket = (SOCKET*)_socket;

	char *p_items = NULL;
	p_items = (char*)_items;

	int failed = 0;
	do
	{
		int sendbytes = 0;
		sendbytes = send(*p_socket, "CD", 2, 0);
		if(sendbytes <= 0) { failed++; break; }

		char recvbuffer[DEFAULT_BUFLEN] = {0};
		int recvlen = DEFAULT_BUFLEN-1;

		int recvbytes = 0;
		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if(recvbytes <= 0) { failed++; break; }

		if(strcmp(recvbuffer, STR_OK) != 0) { failed++; break; }
		memset(recvbuffer, 0, recvlen);

		u_long ln_items = htonl(n_items);
		sendbytes = send(*p_socket, (const char*)&ln_items, sizeof(ln_items), 0);
		if(sendbytes <= 0) { failed++; break; }

		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if(recvbytes <= 0) { failed++; break; }

		if(strcmp(recvbuffer, STR_OK) != 0) { failed++; break; }
		memset(recvbuffer, 0, recvlen);

		if (n_items == 1)
		{
			sendbytes = send(*p_socket, p_items, (int)strlen(p_items), 0);
			if(sendbytes <= 0) { failed++; break; }
		}

		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if(recvbytes <= 0) { failed++; break; }

		if (strcmp(recvbuffer, STR_OK) != 0) { failed++; break; }
		memset(recvbuffer, 0, recvlen);

	}while(0);

	if(failed) { return(NULL); }


	return(VALID);
}
static void *ClientRequestCLS()
{
	system("cls");
	return(VALID);
}
static void *ServerRequestPWD(void *_sockinfo)
{
	sockInfo *p_sockinfo = NULL;
	p_sockinfo = (sockInfo*)_sockinfo;

	char *current_dir = NULL;
	current_dir = p_sockinfo->current_dir;

	SOCKET *p_socket = NULL;
	p_socket = p_sockinfo->sock;

	int failed = 0;
	do
	{
		int sendbytes = 0;
		sendbytes = send(*p_socket, current_dir, strlen(current_dir), 0);
		if (sendbytes <= 0) { failed++; break; }

	}while(0);
	
	if(failed) { return(NULL); }

	return(VALID);
}
static void *ClientRequestPWD(void *_socket)
{
	SOCKET *p_socket = NULL;
	p_socket = (SOCKET*)_socket;

	int failed = 0;
	do
	{
		int sendbytes = 0;
		sendbytes = send(*p_socket, "PWD", 3, 0);
		if (sendbytes <= 0) { failed++; break; }

		char recvbuffer[DEFAULT_BUFLEN] = { 0 };
		int recvlen = DEFAULT_BUFLEN-1;

		int recvbytes = 0;
		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes <= 0) { failed++; break; }

		printf("Current Folder:%s\n", recvbuffer);

	}while(0);
	
	if(failed) { return(NULL); }

	return(VALID);
}
static void *ServerRequestPUT(void *_sockinfo, u_long n_items)
{
	sockInfo *p_sockinfo = NULL;
	p_sockinfo = (sockInfo*)_sockinfo;

	SOCKET *p_socket = NULL;
	p_socket = p_sockinfo->sock;

	char *p_current_dir = NULL;
	p_current_dir = (char*)p_sockinfo->current_dir;

	char *filepath = NULL;
	FILE *file = NULL;

	int failed = 0;
	do
	{
		int sendbytes;
		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { failed++; break; }

		for (int i = 0; i < (int)n_items; ++i)
		{
			char recvbuffer[DEFAULT_BUFLEN] = { 0 };
			int recvlen = DEFAULT_BUFLEN-1;

			int recvbytes;
			recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
			if (recvbytes <= 0) { failed++; break; }
			
			size_t filenamelen = strlen(recvbuffer) + strlen(p_current_dir);
			filepath = (char *)malloc(filenamelen + 2);
			if (!filepath) { failed++; break; }

			int printed;
			printed = sprintf(filepath, "%s/%s", p_current_dir, recvbuffer);
			if (printed != (int)filenamelen + 1) { failed++; break; }
			memset(recvbuffer, 0, recvlen);

			file = fopen(filepath, "wb");
			if (!file) { failed++; break; }

			sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
			if (sendbytes <= 0) { failed++; break; }

			int filesize;
			recvbytes = recv_s(*p_socket, (char *)&filesize, sizeof(filesize), 1);
			if (recvbytes <= 0) { failed++; break; }
			filesize = ntohl(filesize);

			sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
			if (sendbytes <= 0) { failed++; break; }

			int read = 0;
			size_t write = 0;
			while (read < filesize)
			{
				recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
				if(recvbytes <= 0) { failed++; break; }

				read += recvbytes;
				
				write = fwrite(recvbuffer, sizeof(*recvbuffer), recvlen, file);
				if(write <= 0) { failed++; break; }

				memset(recvbuffer, 0, recvlen);
			}

			if(failed) { break; }

			fclose(file);
			file = NULL;
			free(filepath);
			filepath = NULL;
		}

	}while(0);
	
	if(failed) 
		{ 
			if(file) { fclose(file); remove(filepath); }
			if(filepath) { free(filepath); }
			return(NULL); 
		}

	return(VALID);
}
static void *ClientRequestPUT(void *_socket, void **_items, u_long n_items)
{
	SOCKET *p_socket;
	p_socket = (SOCKET *)_socket;

	char **p_item;
	p_item = (char **)_items;

	char *filepath = NULL;
	FILE *file = NULL;

	int failed = 0;
	for (int i = 0; i < (int)n_items; ++i)
	{
		size_t filepathlen = strlen(STR_CLIENT_DIR) + strlen(p_item[i]);
		filepath = calloc(filepathlen + 2, sizeof(*filepath));
		if (!filepath) { failed++; break; }

		int printed;
		printed = sprintf(filepath, "%s/%s", STR_CLIENT_DIR, p_item[i]);
		if (printed != filepathlen + 1) { failed++; break; }

		file = fopen(filepath, "rb");
		if (!file) { failed++; break; }

		fseek(file, 0, SEEK_END);
		u_long filesize = 0;
		filesize = ftell(file);
		fseek(file, 0, SEEK_SET);

		int sendbytes;
		sendbytes = send(*p_socket, "PUT", 3, 0);
		if (sendbytes <= 0) { failed++; break; }

		char recvbuffer[DEFAULT_BUFLEN] = { 0 };
		int recvlen = DEFAULT_BUFLEN-1;

		int recvbytes;
		recvbytes = recv(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes <= 0) { failed++; break; }

		if (strcmp(recvbuffer, STR_OK) != 0) { failed++; break; }
		memset(recvbuffer, 0, recvlen);

		sendbytes = send(*p_socket, (const char*)p_item[i], (int)strlen(p_item[i]), 0);
		if (sendbytes <= 0) { failed++; break; }

		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes <= 0) { failed++; break; }

		if (strcmp(recvbuffer, STR_OK) != 0) { failed++; break; }
		memset(recvbuffer, 0, recvlen);

		u_long l_filesize;
		l_filesize = htonl(filesize);

		sendbytes = send(*p_socket, (const char *)&l_filesize, sizeof(l_filesize), 0);
		if (sendbytes < 0) { failed++; break; }

		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes <= 0) { failed++; break; }
		
		if (strcmp(recvbuffer, STR_OK) != 0) { failed++; break; }
		memset(recvbuffer, 0, recvlen);

		char sendbuffer[DEFAULT_BUFLEN] = { 0 };
		int sendlen = DEFAULT_BUFLEN-1;

		u_long read = 0;
		while (!feof(file) || read < filesize)
		{
			read += fread(sendbuffer, sizeof(*sendbuffer), sendlen, file);
			if(ferror(file) != 0) { failed++; break; }

			sendbytes = send(*p_socket, sendbuffer, sendlen, 0);
			if(sendbytes < 0) { failed++; break; }

			memset(sendbuffer, 0, sendlen);
		}

		if(failed) { break; }

		fclose(file);
		file = NULL;
		free(filepath);
		filepath = NULL;
	}

	if(failed)
	{
		if(file) { fclose(file); }
		if(filepath)free(filepath);
		return(NULL);
	}

	return(VALID);
}
static void *ServerRequestGET(void *_socket, u_long n_items)
{
	SOCKET *p_socket;
	p_socket = (SOCKET *)_socket;

	char *filepath = NULL;
	FILE *file = NULL;

	int failed = 0;
	do
	{
		int sendbytes;
		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { failed++; break; }

		for (u_long i = 0; i < n_items; ++i)
		{
			char recvbuffer[DEFAULT_BUFLEN] = {0};
			int recvlen = DEFAULT_BUFLEN-1;

			int recvbytes;
			recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
			if (recvbytes < 0) { failed++; break; }

			size_t filenamelen = strlen(recvbuffer) + strlen(STR_SERVER_DIR);
			filepath = malloc(filenamelen + 2);
			if (!filepath) { failed++; break; }

			int printed = 0;
			printed = sprintf(filepath, "%s/%s", STR_SERVER_DIR, recvbuffer);
			if (printed != (int)filenamelen + 1) { failed++; break; }
			memset(recvbuffer, 0, recvlen);

			file = fopen(filepath, "rb");
			if (!file) { failed++; break; }

			u_long filesize = 0;
			fseek(file, 0, SEEK_END);
			filesize = ftell(file);
			fseek(file, 0, SEEK_SET);
			u_long l_filesize = htonl(filesize);

			sendbytes = send(*p_socket, (const char *)&l_filesize, sizeof(l_filesize), 0);
			if (sendbytes <= 0) { failed++; break; }

			recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
			if (recvbytes <= 0) { failed++; break; }

			if (strcmp(STR_OK, recvbuffer) != 0) { failed++; break; }

			char sendbuffer[DEFAULT_BUFLEN] = { 0 };
			int sendlen = DEFAULT_BUFLEN -1;

			size_t read = 0;
			while (!feof(file) || read < filesize)
			{
				read += fread(sendbuffer, sizeof(*sendbuffer), sendlen, file);
				if (ferror(file) != 0) { failed++; break; }

				sendbytes = send(*p_socket, sendbuffer, sendlen, 0);
				if (sendbytes <= 0) { failed++; break; }

				memset(sendbuffer, 0, sendlen);
			}

			if(failed) { break; }

			fclose(file);
			file = NULL;
			free(filepath);
			filepath = NULL;
		}

	}while(0);

	if(failed) 
	{
		if(file) { fclose(file); }
		if(filepath) { free(filepath); }
		return(NULL); 
	}

	return(VALID);
}
static void *ClientRequestGET(void *_socket, void **_items, u_long n_items)
{
	SOCKET *p_socket;
	p_socket = (SOCKET *)_socket;

	char **pp_item;
	pp_item = (char **)_items;

	char *filepath = NULL;
	FILE *file = NULL;

	int failed = 0;
	do
	{
		char recvbuffer[DEFAULT_BUFLEN] = {0};
		int recvlen = DEFAULT_BUFLEN-1;

		int sendbytes;
		sendbytes = send(*p_socket, "GET", 3, 0);
		if (sendbytes <= 0) { failed++; break; }

		int recvbytes;
		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes <= 0) { failed++; break; }

		if (strcmp(recvbuffer, STR_OK) != 0) { failed++; break; }

		for (int i = 0; i < (int)n_items; ++i)
		{
			size_t filepathlen = strlen(STR_CLIENT_DIR) + strlen(pp_item[i]);
			filepath = calloc(filepathlen + 2, sizeof(*filepath));
			if (!filepath) { failed++; break; }

			int printed;
			printed = sprintf(filepath, "%s/%s", STR_CLIENT_DIR, pp_item[i]);
			if (printed != filepathlen + 1) { failed++; break; }

			file = fopen(filepath, "wb");
			if (!file) { failed++; break; }

			sendbytes = send(*p_socket, (const char *)pp_item[i], (int)strlen(pp_item[i]), 0);
			if (sendbytes <= 0) { failed++; break; }

			int filesize;
			recvbytes = recv_s(*p_socket, (char *)&filesize, sizeof(filesize), 1);
			if (recvbytes <= 0) { failed++; break; }
			filesize = ntohl(filesize);
			
			sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
			if (sendbytes <= 0) { failed++; break; }

			int read = 0;
			size_t write = 0;
			while (read < filesize)
			{
				memset(recvbuffer, 0, recvlen);
				recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
				if (recvbytes < 0) { failed++; break; }

				read += recvbytes;
				
				write = fwrite(recvbuffer, sizeof(*recvbuffer), recvlen, file);
				if(write != recvbytes) { failed++; break; }
			}

			if(failed) { break; }

			fclose(file);
			file = NULL;
			free(filepath);
			filepath = NULL;
		}

	}while(0);

	if(failed) 
	{
		if(file) { fclose(file); remove(filepath); }
		if(filepath) { free(filepath); }
		return(NULL); 
	}

	return(VALID);
}
static void *ServerRequestMPUT(void *_sockinfo)
{
	sockInfo *p_sockinfo;
	p_sockinfo = (sockInfo*)_sockinfo;

	SOCKET *p_socket;
	p_socket = p_sockinfo->sock;

	int failed = 0;
	do
	{
		int sendbytes = 0;
		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { failed++; break; }

		u_long n_items;
		int recvbytes = 0;
		recvbytes = recv_s(*p_socket, (char *)&n_items, sizeof(n_items), 1);
		if (recvbytes <= 0) { failed++; break; }

		n_items = ntohl(n_items);

		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { failed++; break; }

		void *result;
		result = ServerRequestPUT(p_sockinfo, n_items);
		if(!result) { failed++; break; }

	}while(0);

	if(failed) { return(NULL); }

	return(VALID);
}
static void *ClientRequestMPUT(void *_socket, void **_items, u_long n_items)
{
	SOCKET *p_socket;
	p_socket = (SOCKET*)_socket;

	char **pp_items;
	pp_items = (char**)_items;

	int failed = 0;
	do
	{
		int sendbytes;
		sendbytes = send(*p_socket, "MPUT", 4, 0);
		if (sendbytes < 0) { failed++; break; }

		char recvbuffer[DEFAULT_BUFLEN] = {0};
		int recvlen = DEFAULT_BUFLEN;

		int recvbytes;
		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes < 0) { failed++; break; }

		if (strcmp(STR_OK, recvbuffer) != 0) { failed++; break; }
		memset(recvbuffer, 0, recvlen);

		u_long n_files;
		n_files = htonl(n_items);

		sendbytes = send(*p_socket, (const char *)&n_files, sizeof(n_files), 0);
		if (sendbytes < 0) { failed++; break; }

		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes < 0) { failed++; break; }
		if (strcmp(STR_OK, recvbuffer) != 0) { failed++; break; }

		void *result;
		result = ClientRequestPUT(p_socket, pp_items, n_items);
		if(!result) { failed++; break; }

	}while(0);

	if(failed) { return(NULL); }

	return(VALID);
}
static void *ServerRequestMGET(void *_socket)
{
	SOCKET *p_socket;
	p_socket = (SOCKET*)_socket;

	int failed = 0;
	do
	{
		int sendbytes = 0;
		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { failed++; break; }

		u_long n_items = 0;
		int recvbytes = 0;
		recvbytes = recv_s(*p_socket, (char *)&n_items, sizeof(n_items), 1);
		if (recvbytes <= 0) { failed++; break; }

		n_items = ntohl(n_items);

		sendbytes = send(*p_socket, STR_OK, (int)strlen(STR_OK), 0);
		if (sendbytes <= 0) { failed++; break; }

		void *result;
		result = ServerRequestGET(p_socket, n_items);
		if(!result) { failed++; break; }

	}while(0);

	if(failed) { return(NULL); }

	return(VALID);
}
static void *ClientRequestMGET(void *_socket, void **_items, u_long n_items)
{
	SOCKET *p_socket;
	p_socket = (SOCKET*)_socket;

	char **pp_items;
	pp_items = (char**)_items;

	int failed = 0;
	do
	{
		int sendbytes;
		sendbytes = send(*p_socket, "MGET", 4, 0);
		if (sendbytes <= 0) { failed++; break; }

		char recvbuffer[DEFAULT_BUFLEN] = { 0 };
		int recvlen = DEFAULT_BUFLEN;

		int recvbytes;
		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes <= 0) { failed++; break; }

		if (strcmp(STR_OK, recvbuffer) != 0) { failed++; break; }
		memset(recvbuffer, 0, recvlen);

		u_long n_files;
		n_files = htonl(n_items);

		sendbytes = send(*p_socket, (const char *)&n_files, sizeof(n_files), 0);
		if (sendbytes <= 0) { failed++; break; }

		recvbytes = recv_s(*p_socket, recvbuffer, recvlen, 0);
		if (recvbytes <= 0) { failed++; break; }

		if (strcmp(STR_OK, recvbuffer) != 0) { failed++; break; }

		void *result;
		result = ClientRequestGET(p_socket, pp_items, n_items);

	}while(0);
	
	if(failed) { return(NULL); }

	return(VALID);
}
static void *ClientRequestOPEN(void *_self, void *_items)
{
	sockClient *client;
	client = (sockClient*)_self;

	sockData *p_sockdata;
	p_sockdata = (sockData*)client->data;

	char *p_items;
	p_items = (char*)_items;

	void *Tresult;
	void *result;

	Tresult = client->ctor(client, p_items, DEFAULT_PORT);
	result = client->get(Tresult);
	if(!result) { return(NULL); }

	Tresult = client->connect(client);
	result = client->get(Tresult);
	if(!result) { return(NULL); }

	return(VALID);
}
static void *ClientRequestCLOSE(void *_self)
{
    sockClient *client;
	client = (sockClient*)_self;

	sockData *p_sockdata;
	p_sockdata = (sockData*)client->data;

	if (closesocket(*p_sockdata->sock)) { return(NULL); }

	printf("DISCONECTED\n");
	
	return(VALID);
}
static void *ClientRequestHELP()
{	
	printf("LS    [empty = list files on Server Dir]\n");
	printf("CD    [dir_name/empty to rollback]\n");
	printf("CLS   [clean terminal]\n");
	printf("PWD   [show the current client dir from server]\n");
	printf("PUT   [filename = upload one file to server]\n");
	printf("MPUT  [filenames = upload many files to server]\n");
	printf("GET   [filename = download one file from server]\n");
	printf("MGET  [filenames = download many file from server]\n");
	printf("OPEN  [server address = connect to server]\n");
	printf("CLOSE [empty = close connection]\n");
	return(VALID);
}


static int recv_s(SOCKET _socket, char *buf, int len, int flags)
{
	// flag = 0 (char*)   flag = 1 (randomtype*)
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
	pthread_t *p_thread;
	p_thread = (pthread_t *)thread;

	if (!p_thread) { return(NULL); }

	void *buffer;
	buffer = calloc(1, sizeof(buffer));
	if (!buffer) { return(NULL); }

	if (pthread_join(*p_thread, &buffer)) 
	{ 
		free(buffer);
		return(NULL); 
	}
	
	return(buffer);
}
static void *ThreadCancel(void *thread)
{
	pthread_t *p_thread;
	p_thread = (pthread_t *)thread;

	if (!p_thread) { return(NULL); }
	
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
	sockData *p_data;
	p_data = (sockData*)self_data;

	if (!p_data) { return(NULL); }

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	int size_func = 1;

	void *tmp_args[] = { p_data, NULL };
	void **args = NULL;

	int failed = 0;
	do
	{
		args = calloc(size_func, sizeof(*args));
		if (!args) { failed++; break; }
		else
		{
			for (int i = 0; i < size_func; i++)
				(args)[i] = tmp_args[i];
		}

		void *sucess;
		sucess = TSocketCtor(p_tsocket, SockDataInfo, args, size_func, p_data->self_mutex);
		if (!sucess) { failed++; break; }
		else
		{
			if(pthread_mutex_lock(p_tsocket->mutex)) 
			{ failed++; break; }
			if(pthread_create(&p_data->Tinfo, NULL, TSocketBuild, p_tsocket)) 
			{ failed++; break; }
		}

	}while(0);
	
	if(failed)
	{
		if(args) { free(args); }
	}

	return(&p_data->Tinfo);
}
static void *TServerCtor(void *_self, void *arg1, void *arg2)
{
	sockServer *server;
	server = ServerNotNull(_self);
	if (!server || !arg1 || !arg2) { return(NULL); }

	sockData *_data;
	_data = (sockData *)server->data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	int sz_func = 3;

	void *tmp_args[] = { _self, arg1, arg2 };
	void **args = NULL;

	int failed = 0;
	do
	{
		args = calloc(sz_func, sizeof(*args));
		if (!args) { failed++; break; }
		else
		{
			for (int i = 0; i < sz_func; i++)
				(args)[i] = tmp_args[i];
		}

		void *sucess;
		sucess = TSocketCtor(p_tsocket, ServerCtor, args, sz_func, _data->self_mutex);
		if (!sucess) { failed++; break; }
		else
		{
			if(pthread_mutex_lock(p_tsocket->mutex))
			{ failed++; break; }
			if(pthread_create(&_data->Tctor, NULL, TSocketBuild, p_tsocket))
			{ failed++; break; }
		}

	}while(0);
	
	if (failed)
	{
		if(args) { free(args); }
	}

	return(&_data->Tctor);
}
static void *TClientCtor(void *_self, void *arg1, void *arg2)
{
	sockClient *client;
	client = ClientNotNull(_self);
	if (!client || !arg1 || !arg2) { return(NULL); }

	sockData *_data;
	_data = (sockData *)client->data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	int sz_func = 3;

	void *tmp_args[] = { _self, arg1, arg2 };
	void **args =  NULL;

	int failed = 0;
	do
	{
		args = calloc(sz_func, sizeof(*args));
		if (!args) { failed++; break; }
		else
		{
			for (int i = 0; i < sz_func; i++)
				(args)[i] = tmp_args[i];
		}

		void *sucess;
		sucess = TSocketCtor(p_tsocket, ClientCtor, args, sz_func, _data->self_mutex);
		if (!sucess) { failed++; break; }
		else
		{
			if(pthread_mutex_lock(p_tsocket->mutex))
			{ failed++; break; }
			if(pthread_create(&_data->Tctor, NULL, TSocketBuild, p_tsocket))
			{ failed++; break; }
		}

	}while(0);
	
	if (failed)
	{
		if(args) { free(args); }
	}

	return(&_data->Tctor);
}
static void *TServerDtor(void *_self)
{
	sockServer *server;
	server = ServerNotNull(_self);
	if (!server) { return(NULL); }

	sockData *_data;
	_data = (sockData *)server->data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	int sz_func = 1;

	void *tmp_args[] = { _self, NULL };
	void **args = NULL;

	int failed = 0;
	do
	{
		args = calloc(sz_func, sizeof(*args));
		if (!args) { failed++; break; }
		else
		{
			for (int i = 0; i < sz_func; i++)
				(args)[i] = tmp_args[i];
		}

		void *sucess;
		sucess = TSocketCtor(p_tsocket, ServerDtor, args, sz_func, _data->self_mutex);
		if (!sucess) { failed++; break; }
		else
		{
			if(pthread_mutex_lock(p_tsocket->mutex))
			{ failed++; break; }
			if(pthread_create(&_data->Tdtor, NULL, TSocketBuild, p_tsocket))
			{ failed++; break; }
		}	

	}while(0);
	
	if (failed)
	{
		if(args) { free(args); }
	}

	return(&_data->Tdtor);
}
static void *TClientDtor(void *_self)
{
	sockClient *client;
	client = ClientNotNull(_self);
	if (!client) { return(NULL); }

	sockData *_data;
	_data = (sockData *)client->data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	int sz_func = 1;

	void *tmp_args[] = { _self, NULL };
	void **args = NULL;

	int failed = 0;
	do
	{
		args = calloc(sz_func, sizeof(*args));
		if (!args) { failed++; break; }
		else
		{
			for (int i = 0; i < sz_func; i++)
				(args)[i] = tmp_args[i];
		}

		void *sucess;
		sucess = TSocketCtor(p_tsocket, ClientDtor, args, sz_func, _data->self_mutex);
		if (!sucess) { failed++; break; }
		else
		{
			if(pthread_mutex_lock(p_tsocket->mutex))
			{ failed++; break; }
			if(pthread_create(&_data->Tdtor, NULL, TSocketBuild, p_tsocket))
			{ failed++; break; }
		}

	}while(0);
	
	if (failed)
	{
		if(args) { free(args); }
	}

	return(&_data->Tdtor);
}
static void *TServerStart(void *_self)
{
	sockServer *server;
	server = ServerNotNull(_self);
	if (!server) { return(NULL); }

	sockData *_data;
	_data = (sockData *)server->data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }
	
	int size_func = 1;

	void *tmp_args[] = { _self, NULL };
	void **args = NULL;

	int failed = 0;
	do
	{
		args = calloc(size_func, sizeof(*args));
		if (!args) { failed++; break; }
		else
		{
			for (int i = 0; i < size_func; i++)
				(args)[i] = tmp_args[i];
		}

		void *sucess;
		sucess = TSocketCtor(p_tsocket, ServerStart, args, size_func, _data->self_mutex);
		if (!sucess) { failed++; break; }
		else
		{
			if(pthread_mutex_lock(p_tsocket->mutex))
			{ failed++; break; }
			if(pthread_create(&_data->Tstart, NULL, TSocketBuild, p_tsocket))
			{ failed++; break; }
		}

	}while(0);
	
	if (failed)
	{
		if(args) { free(args); }
	}

	return(&_data->Tstart);
}
static void *TClientConnect(void *_self)
{
	sockClient *client;
	client = ClientNotNull(_self);
	if (!client) { return(NULL); }

	sockData *_data;
	_data = (sockData *)client->data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	int sz_func = 1;

	void *tmp_args[] = { _self, NULL };
	void **args = NULL;

	int failed = 0;
	do
	{
		args = calloc(sz_func, sizeof(*args));
		if (!args) { failed++; break; }
		else
		{
			for (int i = 0; i < sz_func; i++)
				(args)[i] = tmp_args[i];
		}

		void *sucess;
		sucess = TSocketCtor(p_tsocket, ClientConnect, args, sz_func, _data->self_mutex);
		if (!sucess) { failed++; break; }
		else
		{
			if(pthread_mutex_lock(p_tsocket->mutex))
			{ failed++; break; }
			if(pthread_create(&_data->Tstart, NULL, TSocketBuild, p_tsocket))
			{ failed++; break; }
		}

	}while(0);
	
	if (failed)
	{
		if(args) { free(args); }
	}

	return(&_data->Tstart);
}
static void *TServerRequest(void *_self, void *_sockinfo, void *_socklist)
{
	sockServer *server;
	server = ServerNotNull(_self);
	if (!server) { return(NULL); }

	sockData *_data;
	_data = (sockData*)server->data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	int size_func = 2;

	void *tmp_args[] = { _sockinfo, _socklist };
	void **args = NULL;

	int failed = 0;
	do
	{
		args = calloc(size_func, sizeof(*args));
		if (!args) { failed++; break; }
		else
		{
			for (int i = 0; i < size_func; i++)
				(args)[i] = tmp_args[i];
		}

		void *sucess = NULL;
		sucess = TSocketCtor(p_tsocket, ServerRequest, args, size_func, NULL);
		if (!sucess) { failed++; break; }
		else
		{
			if(pthread_once(&_once, (void (_cdecl*)(void))ServerRequest))
			{ failed++; break; }
			if(pthread_create(&_data->Trequest, NULL, TSocketBuild, p_tsocket))
			{ failed++; break; }
		}
	}while(0);
	
	if (failed)
	{
		if(args) { free(args); }
	}

	return(&_data->Trequest);
}
static void *TClientRequest(void *_self, void *_request)
{
	sockClient *client;
	client = ClientNotNull(_self);
	if (!client) { return(NULL); }

	sockData *_data;
	_data = (sockData *)client->data;

	Tsocket *p_tsocket;
	p_tsocket = TSocketInit();
	if (!p_tsocket) { return(NULL); }

	int size_func = 2;

	void *tmp_args[] = { client, _request };
	void **args = NULL;

	int failed = 0;
	do
	{
		args = calloc(size_func, sizeof(*args));
		if (!args) { failed++; break; }
		else
		{
			for (int i = 0; i < size_func; i++)
				(args)[i] = tmp_args[i];
		}

		void *sucess;
		sucess = TSocketCtor(p_tsocket, ClientRequest, args, size_func, NULL);
		if (!sucess) { failed++; break; }
		else
		{
			if(pthread_once(&_once, (void (_cdecl*)(void))ClientRequest))
			{ failed++; break; }
			if(pthread_create(&_data->Trequest, NULL, TSocketBuild, p_tsocket))
			{ failed++; break; }
		}

	}while(0);
	
	if (failed)
	{
		if(args) { free(args); }
	}

	return(&_data->Trequest);
}

void *QuickServerFTP(void **_server, void *_address)
{
	sockServer **pp_server;
	pp_server = (sockServer **)_server;

	char *p_address;
	p_address = (char *)_address;

	void *result = NULL;
	void *Tresult = NULL;

	int failed = 0;
	do
	{
		(*pp_server) = ServerInit();
		if (!*pp_server) { failed++; break; }

		Tresult = (*pp_server)->ctor((*pp_server), p_address, DEFAULT_PORT);
		result = (*pp_server)->get(Tresult);
		if(!result) { failed++; break; }

		(*pp_server)->start((*pp_server));

		printf("Server Online\n");
		
	}while(0);

	if(failed) { return(NULL); }

	return(VALID);
}