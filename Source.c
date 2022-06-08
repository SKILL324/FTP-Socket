#include "Resource02.h"
#include "Socket.h"
///////////////////////////////////////////////////////////////////////////////
#define DEFAULT_BUFLEN 512
///////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
	if (WsaCtor()) { return(NULL); }

	sockServer *server = NULL;
	sockClient *client = NULL;
	
	void *result = NULL;
	void *Tresult = NULL;

	char inputbuff[DEFAULT_BUFLEN] = {0};
	int inputbuff_len = DEFAULT_BUFLEN;

	int value = 0;
	do
	{
		printf("CLIENT = 0 | SERVER = 1\n-->");
		if (fgets(inputbuff, inputbuff_len, stdin) == NULL)
		{
			continue;
		}

		if(inputbuff[0] == '1') { value++; break; }
		if(inputbuff[0] == '0') { break; }

	}while(1);

	if (value)
	{
		do
		{
			printf("\nSERVER ADDRESS: ");
			fflush(stdin);
			if (fgets(inputbuff, inputbuff_len, stdin) == NULL)
			{
				continue;
			}
			else
			{
				inputbuff[strlen(inputbuff)-1] = '\0';
				result = QuickServerFTP(&server, inputbuff);
				if(!result) 
				{ 
					printf("DISCONECTED\n");
					continue; 
				}

				break;
			}


		}while(1);

	}
	
	client = ClientInit();
	if(!client) { return(NULL); }

	printf("\nClient Created!\n");

	result = NULL;
	int failed = 0;
	do
	{
		printf("\n-->");
		fflush(stdin);
		if (fgets(inputbuff, inputbuff_len, stdin) == NULL)
		{
			continue;
		}
		
		inputbuff[strlen(inputbuff)-1] = '\0';
		printf("\n");

		Tresult = client->request(client, inputbuff);
		result = client->get(Tresult);
		if(!result) { printf("ERROR\n"); }
		else { printf("SUCCESS\n"); }
		memset(inputbuff, 0, inputbuff_len);

	}while(!failed);
	//failed++; 
	return(0);
}
