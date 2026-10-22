#include "bridge.h"
#include "events.h"
#include "vehicle.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
static Bridge *bridge_ref;
static void reply(int fd,const char *payload){unsigned char h[10]; size_t n=strlen(payload); h[0]=0x81; if(n<126){h[1]=(unsigned char)n; send(fd,h,2,0);} else {h[1]=126; h[2]=(n>>8)&255; h[3]=n&255; send(fd,h,4,0);} send(fd,payload,n,0);}
void websocket_server_run(Bridge *bridge){bridge_ref=bridge; int s=socket(AF_INET,SOCK_STREAM,0), opt=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt)); struct sockaddr_in a={.sin_family=AF_INET,.sin_port=htons(8080),.sin_addr.s_addr=htonl(INADDR_ANY)}; bind(s,(struct sockaddr*)&a,sizeof(a)); listen(s,4); printf("WebSocket: ws://localhost:8080\n"); for(;;){int c=accept(s,0,0); if(c<0)continue; char buf[2048]={0}; int n=recv(c,buf,sizeof(buf)-1,0); if(n>0 && strstr(buf,"Upgrade: websocket")){const char *key=strstr(buf,"Sec-WebSocket-Key:"); char response[512]; (void)key; snprintf(response,sizeof(response),"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: invalid-demo\r\n\r\n"); send(c,response,strlen(response),0); reply(c,"{\"type\":\"SNAPSHOT\",\"weight\":0,\"message\":\"Backend connecté\"}"); unsigned char frame[2048]; while(recv(c,frame,sizeof(frame),0)>0){char *cmd=(char*)frame; if(strstr(cmd,"CAR"))vehicle_start(bridge_ref,VEHICLE_CAR); else if(strstr(cmd,"TRUCK"))vehicle_start(bridge_ref,VEHICLE_TRUCK); else if(strstr(cmd,"RESET")){events_clear(); reply(c,"{\"type\":\"SIMULATION_RESET\"}");} char event[512]; while(events_pop(event,sizeof(event)))reply(c,event);} } close(c);} }
