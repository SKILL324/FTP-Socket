#include "Resource02.h"
#include "Socket.h"
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int main()
{
	if (WsaCtor()) { return(1); }

	void *result;
	char *sendbuffer = "sendfrom client1 to hostclient";
	int sendsize = strlen(sendbuffer);

	sockServer *server;
	server = ServerInit();
	if (!server) { return(1); }

	server->ctor(server, "127.0.0.1", "22");
	result = server->start(server);
	if (!result) { return(1); }
	
	printf("Servidor Online\n\n");

	sockClient *client1;
	client1 = ClientInit();
	if (!client1) { return(1); }

	client1->ctor(client1, "127.0.0.1", "22");
	result = client1->connect(client1);
	result = client1->get(result);
	if(result)
		printf("Client1 Connected\n\n");

	/*result = client1->request(client1, "GET aa.png");
	result = client1->get(result);
	if(result)
		printf("GET SUCESS\n");
	else
		printf("GET FAILED\n");*/
	result = client1->request(client1, "SET aa.png");
	result = client1->get(result);
	if (result)
		printf("SET SUCESS\n");
	else
		printf("SET FAILED\n");
	//Sleep(10000000000000);
	return(0);
}
