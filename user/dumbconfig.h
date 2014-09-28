#ifndef _DUMBCONFIG_H
#define _DUMBCONFIG_H

#define MAX_PLAYERS 3
#define MAX_PLAYERS_STRING "3"

#define MAX_PLAYER_NAME 17

//Overworld
#define WORLDTYPE 0
#define GAMEMODE  0

#define MAPSIZECHUNKS 1

#define RUNSPEED 5
#define WALKSPEED 3

#define SENDBUFFERSIZE 250

//#define STATIC_SERVER_STAT_STRING
#define STATIC_MOTD_NAME
#define MOTD_NAME my_server_name
extern char my_server_name[];



#define MINECRAFT_PORT 25565
#define MINECRAFT_PORT_STRING "25565"

//Optional functionality

#define NEED_PLAYER_CLICK
//#define NEED_PLAYER_BLOCK_ACTION
//#define NEED_SLOT_CHANGE

#endif
