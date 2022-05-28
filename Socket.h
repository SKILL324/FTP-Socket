// WSA == WSADATA (winsock lib)
// socklist == LIFO list 
// tsocket == threaded behavior;
// init == alloc and zero out;
// ctor == fill with data;			 ---|
// dtor == destroy and free			 ---|	return
// info == all info about the socket ---|	Handle to thread
// start == do ya thang              ---|
// get == get data returned from thread
// cancel == cancel a thread
///////////////////////////////////////////////////////////////////////////////
#ifndef SOCKET_H
#define SOCKET_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#pragma comment(lib, "Ws2_32.lib")
#include <time.h>
#include <conio.h>
#include "pthread.h"

#ifndef VALID
#define VALID ((void *)1)
#endif
///////////////////////////////////////////////////////////////////////////////
struct sockdata;
struct sockbase;
union  ptrfuncx;
struct tsock;
struct sockserver;
struct sockclient;
struct socklist;
enum   socklisttype;
struct sockinfo;

typedef struct sockdata			sockData;
typedef struct sockserver		sockServer;
typedef struct sockclient		sockClient;
typedef struct socklist			sockList;
typedef struct tsock			Tsocket;
typedef enum   socklisttype		socklistType;
typedef struct sockinfo			sockInfo;
///////////////////////////////////////////////////////////////////////////////
struct sockdata
{
	SOCKET				*sock;
	char				*sockinfo;
	struct addrinfo		        *addr;
	pthread_mutex_t		        *self_mutex;
	pthread_mutex_t		        *request_mutex;
	pthread_t			Tctor, Tdtor, Tinfo, Tstart;
	pthread_t			Trequest;
};
struct sockbase
{
	void *(*ctor)(void *self, void *address, void *service);
	void *(*dtor)(void *self);
	void *(*info)(void *self_data);
	void *(*get)(void *thread);
	void *(*cancel)(void *thread);
	void *data;
};
union ptrfuncx
{
	void *(*ptr_func3)(void *, void *, void *);
	void *(*ptr_func2)(void *, void *);
	void *(*ptr_func1)(void *);
};
struct tsock
{
	int			size;
	void			**pp_void;
	pthread_mutex_t		*mutex;

	union ptrfuncx;
};
struct sockserver
{
	sockServer	*self;
	void		*(*start)(void *self);

	struct sockbase;
};
struct sockclient
{
	sockClient	*self;
	void		*(*connect)(void *self);
	void		*(*request)(void *self, void *str_request);

	struct sockbase;
};
struct socklist
{
	sockList		*self;
	void			*data;
	sockList		*last;
	sockList		*next;
	socklistType	sock_type;
};

struct sockinfo
{
	sockInfo	*self;
	SOCKET		*sock; 
	pthread_t	Tsock;
	char		*hostname;
};
enum socklisttype { SOCKT_NONE, SOCKT_SERVER, SOCKT_CLIENT, SOCKT_FILE, 
				SOCKT_MAX};
///////////////////////////////////////////////////////////////////////////////
int WsaCtor();
int WsaDtor();

static sockList *SockListInit();
static sockList **SockListNotNull(void *);
static void *SockListCtor(void **,void *, int);
static void *SockListDtor(void **);
static void *SockListRemove(void *, void *);
static void *SockListClear();

static sockInfo *SockInfoInit();
static sockInfo *SockInfoNotNull(void *);
static void *SockInfoCtor(void *, void *);
static void *SockInfoDtor(void *);
static void *SockInfoSetHost(void *, void *, size_t);

sockServer *ServerInit();
sockClient *ClientInit();
static sockServer *ServerNotNull(void *);
static sockClient *ClientNotNull(void *);
static void *ServerCtor(void *, void *,void *);
static void *ClientCtor(void *, void *, void *);
static void *ServerDtor(void *);
static void *ClientDtor(void *);
static void *ServerStart(void *);
static void *ClientConnect(void *);
static void *SockDataInfo(void *);
static void *ServerRequest(void *);
static void *ClientRequest(void *, void *);

static void *DecryptRequest(void *, void **, void **, const void*);
static void *ServerRequestSet(void *, void *);
static void *ClientRequestSet(void *, void *, void *);
static void *ServerRequestGet(void *, void *);
static void *ClientRequestGet(void *, void *, void *);

static void *ThreadGet(void *);
static void *ThreadCancel(void *);

static Tsocket *TSocketInit();
static void *TSocketCtor(void *, void *, void **, int, void *);
static void *TSocketDtor(void *);
static void *TSocketBuild(void *);

static void *TSockDataInfo(void *);
static void *TServerCtor(void *, void *, void *);
static void *TClientCtor(void *, void *, void *);
static void *TServerDtor(void *);
static void *TClientDtor(void *);
static void *TServerStart(void *);
static void *TClientConnect(void *);
static void *TServerRequest(void *, void *);
static void *TClientRequest(void *, void *);


//VOID* RecData(void* arg);
//
//static VOID vParsedComma(char* vComma);
//int* vToken();
#endif


