#ifndef _CONFIG_PARSER
#define _CONFIG_PARSER
/*----------------------------------------------------------------------------
*        Headers
*----------------------------------------------------------------------------*/
#if defined(WIN32) && !defined(__CYGWIN__)
#include <windows.h>
#include "getopt.h"
void pause(void)
{
	system("PAUSE");
}
#else
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdint.h>
#include <sys/queue.h>
#include <libxml/xmlreader.h>
#endif

#include "canfestival.h"


/*----------------------------------------------------------------------------
*        definitions
*----------------------------------------------------------------------------*/
#define MAX_FILENAME 0x200
#define MAX_LIBNAME 0x200
#define MAX_BUSNAME 0x20

struct s_pdo
{
	xmlChar* stype;
	xmlChar* snum;
	xmlChar* scobid;
	xmlChar* stransmission_type;
	xmlChar* sinhibit_time;
	xmlChar* sevent_timer;
};

struct s_pdo_map_entry
{
	xmlChar* sindex;
	xmlChar* ssub;
	xmlChar* snumBits;
	struct s_pdo_map_entry* next;
};

struct s_pdo_map
{
	xmlChar* stype;
	xmlChar* snum;
	struct s_pdo_map_entry* mapping;
	struct s_pdo_map* nextPdoMap;
};

SLIST_HEAD(pdomapq, s_pdo_map);

struct s_obj_dict
{
	xmlChar* sindex;
	xmlChar* ssub;
	xmlChar* stype;
	xmlChar* sval;
	struct s_obj_dict* nextObjDict;
};

struct s_slaveNode
{
	xmlChar* sid;
	xmlChar* sname;
	xmlChar* snodeId;
	xmlChar* sheartbeatTime;
	struct s_pdo* rxPdo[4];
	struct s_pdo* txPdo[4];
	struct s_pdo_map* pdoMap;
	struct s_obj_dict* objDict;
};

//struct pdomapq nodeManQ;
//struct td_nodeManagment* nextNode = NULL;


typedef struct td_nodeManagment
{
	uint8_t nodeId;
	uint16_t initStep;
	void (*configureSlaveNode)(CO_Data* d, UNS8 nodeId);
	struct s_slaveNode* configInfo;
	TAILQ_ENTRY(td_nodeManagment) tailq;
} nodeManagment;


/*----------------------------------------------------------------------------
*        Local variables
*----------------------------------------------------------------------------*/
extern UNS8 masterNodeId;
extern s_BOARD MasterBoard;
extern int debugging;
extern char configLib[];
extern char busName[];
extern char baudRate[];
extern char* LibraryPath;

// opens and parses a config file for two specific documenet fragments
// <can_fest> and <slave_nodes>
int CP_parseConfigFile(char *filename);


#endif
