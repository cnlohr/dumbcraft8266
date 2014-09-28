#include "mem.h"
#include "c_types.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "espconn.h"
#include "mystuff.h"
#include "dumbcraft.h"

#define PORT 25565
#define SERVER_TIMEOUT 1000
#define MAX_CONNS 5

#define TXRXBUFFER 2048

#define at_procTaskPrio        0
#define at_procTaskQueueLen    1


static struct espconn *pTcpServer;

struct user_client_conn
{
	struct espconn *pespconn;
	int cansend:1;
	int id;
} connections[MAX_CONNS];

os_event_t    at_procTaskQueue[at_procTaskQueueLen];

static void at_procTask(os_event_t *events);


char my_server_name[16];



#define OUTCIRCBUFFSIZE 512



uint8_t sendbuffer[TXRXBUFFER];
uint16_t sendptr;
uint8_t sendplayer;
uint8_t outcirc[OUTCIRCBUFFSIZE];
uint16_t outcirchead = 0;
uint8_t is_in_outcirc;

void StartupBroadcast() { is_in_outcirc = 1; }
void DoneBroadcast() { is_in_outcirc = 0; }
void PushByte( uint8_t byte );

void extSbyte( uint8_t b )
{
	if( is_in_outcirc )
	{
		// printf( "Broadcast: %02x (%c)\n", b, b );
		outcirc[outcirchead & (OUTCIRCBUFFSIZE-1)] = b;
		outcirchead++;
	} else {
		PushByte( b );
	}
}


uint16_t GetRoomLeft()
{
	return 512-sendptr;
}
uint16_t GetCurrentCircHead()
{
	return outcirchead;
}

uint8_t UnloadCircularBufferOnThisClient( uint16_t * whence )
{
	uint16_t i16;
	uint16_t w = *whence;
	i16 = GetRoomLeft();
	while( w != outcirchead && i16 )
	{
		// printf( "." );
		PushByte( outcirc[(w++)&(OUTCIRCBUFFSIZE-1)] );
		i16--;
	}
	*whence = w;
	if( !i16 )
		return 1;
	else
		return 0;
}


void SendData( uint8_t playerno, unsigned char * data, uint16_t packetsize )
{
	struct espconn *pespconn = connections[playerno].pespconn;
	if( pespconn != 0 )
	{
		espconn_sent( pespconn, data, packetsize );
		connections[playerno].cansend = 0;
	}
}


void SendStart( uint8_t playerno )
{
	sendplayer = playerno;
	sendptr = 0;
}
void PushByte( uint8_t byte )
{
	sendbuffer[sendptr++] = byte;
}
void EndSend( )
{
	if( sendptr != 0 )
		SendData( sendplayer, sendbuffer, sendptr );
	//printf( "\n" );
}
uint8_t CanSend( uint8_t playerno )
{
	return connections[playerno].cansend && connections[playerno].pespconn;
}


void ForcePlayerClose( uint8_t playerno, uint8_t reason )
{
	RemovePlayer( playerno );
//	close( playersockets[playerno] );

	//??? ??? ??? XXX TODO: I have no idea how to force a connection closed.

}
uint8_t readbuffer[TXRXBUFFER];
uint16_t readbufferpos = 0;
uint16_t readbuffersize = 0;

uint8_t Rbyte()
{
	uint8_t rb = readbuffer[readbufferpos++];
	// printf( "[%02x] ", rb );
	return rb;
}

uint8_t CanRead()
{
	return readbufferpos < readbuffersize;
}

SetServerName( const char * c )
{
	os_sprintf( my_server_name, "ESP8266Dumb" );
}

void ICACHE_FLASH_ATTR
at_tcpclient_recv(void *arg, char *pdata, unsigned short len)
{
	int i;
	struct espconn *pespconn = (struct espconn *)arg;
	struct user_client_conn *conn = (struct user_client_conn*)pespconn->reverse;

	memcpy( readbuffer, pdata, len );

	readbufferpos = 0;
	readbuffersize = len;

	
	GotData( conn->id );
	
	return;
}

static void ICACHE_FLASH_ATTR
at_tcpserver_recon_cb(void *arg, sint8 errType)
{
	struct espconn *pespconn = (struct espconn *)arg;
	struct user_client_conn *conn = (struct user_client_conn*)pespconn->reverse;
	printf( "Repeat Callback\r\n" );
}

static void ICACHE_FLASH_ATTR
at_tcpserver_discon_cb(void *arg)
{
	struct espconn *pespconn = (struct espconn *) arg;
	struct user_client_conn *conn = (struct user_client_conn*)pespconn->reverse;

	conn->pespconn = 0;
	printf("Disconnect.\r\n" );
}

static void ICACHE_FLASH_ATTR
at_tcpclient_sent_cb(void *arg)
{
	struct espconn *pespconn = (struct espconn *) arg;
	struct user_client_conn *conn = (struct user_client_conn*)pespconn->reverse;

	conn->cansend = 1;

	uart0_sendStr("\r\nSEND OK\r\n");
}



LOCAL void ICACHE_FLASH_ATTR
at_tcpserver_listen(void *arg)
{
	struct espconn *pespconn = (struct espconn *)arg;
	uint8_t i;
	printf("get tcpClient:\r\n");
	for( i = 0; i < MAX_CONNS; i++ )
	{
		if( connections[i].pespconn == 0 )
		{
			break;
		}
	}
	if( i == MAX_CONNS )
	{
		return;
	}

	connections[i].pespconn = pespconn;
	connections[i].cansend = 1;
	connections[i].id = i;

	pespconn->reverse = (void*)&connections[i];
	espconn_regist_recvcb(pespconn, at_tcpclient_recv);
	espconn_regist_reconcb(pespconn, at_tcpserver_recon_cb);
	espconn_regist_disconcb(pespconn, at_tcpserver_discon_cb);
	espconn_regist_sentcb(pespconn, at_tcpclient_sent_cb);
	uart0_sendStr("Link\r\n");
	AddPlayer( i );
}

static void ICACHE_FLASH_ATTR
at_procTask(os_event_t *events)
{
	static int updates_without_tick = 0;
//	uart0_sendStr("ATPRoc");
	system_os_post(at_procTaskPrio, 0, 0 );
	if( events->sig == 0 && events->par == 0 )
	{
		//Idle Event.
		if( connections[0].pespconn && connections[0].cansend )
		{
			UpdateServer();
			if( updates_without_tick++ > 100 ) 
			{
				TickServer();
			}
		    //espconn_sent( connections[0].pespconn, "hello\r\n", 7 );
		}
	}
}


void at_recvTask()
{
	//Start popping stuff off fifo. (UART)
}

void user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	int at_wifiMode = wifi_get_opmode();

	uart0_sendStr("\r\nCustom Server\r\n");

	wifi_set_opmode( 2 ); //We broadcast our ESSID, wait for peopel to join.

	pTcpServer = (struct espconn *)os_zalloc(sizeof(struct espconn));
	pTcpServer->type = ESPCONN_TCP;
	pTcpServer->state = ESPCONN_NONE;
	pTcpServer->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
	pTcpServer->proto.tcp->local_port = PORT;
	espconn_regist_connectcb(pTcpServer, at_tcpserver_listen);
	espconn_accept(pTcpServer);
	espconn_regist_time(pTcpServer, SERVER_TIMEOUT, 0);

	printf("Hello, world.  Starting server.\n" );
	InitDumbcraft();
	
	os_sprintf( my_server_name, "ESP8266Dumb" );

	system_os_task(at_procTask, at_procTaskPrio, at_procTaskQueue, at_procTaskQueueLen);

	uart0_sendStr("\r\nCustom Server\r\n");
	system_os_post(at_procTaskPrio, 0, 0 );
}
