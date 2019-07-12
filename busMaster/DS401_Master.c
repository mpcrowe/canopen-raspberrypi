/*
This file based on an example from CanFestival, a library implementing CanOpen Stack.
*/

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

// the following are for socket access
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
// end socket access
#include <ctype.h> // tolower()

#include <pthread.h>	// for thrading the deamon process

#endif

#include "canfestival.h"
#include "DS401_Master.h"
#include "TestMaster.h"
#include "configParser.h"

/*----------------------------------------------------------------------------
*        Internal definitions
*----------------------------------------------------------------------------*/
#define MAX_FILENAME 0x200
#define MAX_LIBNAME 0x200
#define MAX_BUSNAME 0x20


/*----------------------------------------------------------------------------
*        Local variables
*----------------------------------------------------------------------------*/
UNS8 masterNodeId = 0x17;
s_BOARD MasterBoard = {"32", "500K"};
int debugging = 0;
char configLib[MAX_LIBNAME];
char busName[MAX_BUSNAME];
char baudRate[MAX_BUSNAME];
char* LibraryPath="libcanfestival_can_virtual.so";
struct s_obj_dict* currObjDict = NULL;
CO_Data* gCanOpenData = &TestMaster_Data;

//int  pdoInfoStep = 0;
//int pdoIndex = 1;
//UNS32 pdoCobId = 0;
void cfgSlave_TxPdo(CO_Data* d, UNS8 nodeId, int index);

TAILQ_HEAD(nodeq, td_nodeManagment);
struct nodeq nodeManQ;

pthread_t deamon_thread;
int dPortNumber;

/*----------------------------------------------------------------------------
*        Local functions
*----------------------------------------------------------------------------*/
static void ConfigureSlaveNode(CO_Data* d, UNS8 nodeId);
UNS8 getNodeId(CO_Data* d);

void CheckWriteInfoPdoMap(CO_Data* d, UNS8 nodeid);
void cfgSlave_ObjDict(CO_Data* d, UNS8 nodeId );
int ds_LocalObjDictWrite(struct s_obj_dict* pObjDict);

/*----------------------------------------------------------------------------
*        Local functions
*----------------------------------------------------------------------------*/
void* deamonPort(void* pPortNumber)
{
	int listenfd = 0;
	int connfd = 0;
	struct sockaddr_in serv_addr;
	int portNumber = *(int*)pPortNumber;

	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(portNumber);
	bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr));

	if(listen(listenfd, 10) == -1)
	{
		printf("Failed to listen\n");
		return(NULL);
	}
	if(debugging) printf("Listening on localhost, port: %d\n",portNumber);
	while(1)
	{
		int n = 0;
		char recvBuff[1025];
		memset(recvBuff, '0', sizeof(recvBuff));
		connfd = accept(listenfd, (struct sockaddr*)NULL ,NULL); // accept awaiting request
		do
		{
			n = read(connfd, recvBuff, sizeof(recvBuff)-1);
			if(n<=0)
				break;
			recvBuff[n] = 0;
//			printf("msg from client\n%s\n",recvBuff);
			CP_ParseMemory(recvBuff);
			write(connfd,recvBuff,strlen(recvBuff));
		}
		while(n>0);
		close(connfd);
	}
	return(NULL);
}


int startDeamonLoop(int portNumber)
{
	// thread which executes inc_x(&x) */
	dPortNumber = portNumber;
	if(pthread_create(&deamon_thread, NULL, deamonPort, &dPortNumber))
	{
		fprintf(stderr, "Error creating thread\n");
		return(1);
	}
	return(0);
}


nodeManagment* lookupNode(uint8_t nodeId)
{
	nodeManagment* ptr;
	TAILQ_FOREACH(ptr,&nodeManQ, tailq)
	{
//		printf("%s node ID: %x\n", __FUNCTION__ , ptr->nodeId);
		if(ptr->nodeId == nodeId)
		{
			return(ptr);
		}
	}
	return(NULL);
}


nodeManagment* addNodeToManagmentQ(uint8_t nodeId,  void (*configureSlaveNode)(CO_Data* d, UNS8 nodeId))
{
	static uint8_t qNeedsInit = 1;
	nodeManagment* newNode;
	if( qNeedsInit)
	{
		TAILQ_INIT(&nodeManQ);
		qNeedsInit = 0;
	}
//	eprintf("%s size: %d bytes\n",__FUNCTION__, sizeof(nodeManagment));
	newNode = lookupNode(nodeId);
	if(newNode == NULL)
	{
		newNode = malloc(sizeof(nodeManagment));
		if(newNode == NULL)
		{
			printf("malloc error at %s %p\n", __FUNCTION__, newNode);
			exit(-2);
		}
		memset(newNode,0, sizeof(nodeManagment));
		newNode->nodeId = nodeId;
		newNode->configureSlaveNode = configureSlaveNode;
		TAILQ_INSERT_TAIL(&nodeManQ, newNode, tailq);
	}
	else
	{
//		printf("%s found node\n", __FUNCTION__);
		newNode->configureSlaveNode = configureSlaveNode;
	}
	return(newNode);
}


void ds_HeartbeatTimeoutError(CO_Data* d, UNS8 heartbeatID)
{
	eprintf("%s Node id: 0x%02x\n", __FUNCTION__, heartbeatID);
}


// called when a slave boot message is encountered
void ds_ProcessSlaveBootup(CO_Data* d, UNS8 nodeId)
{
	if(debugging>1) eprintf("%s nodeId: 0x%02x\n", __FUNCTION__, nodeId);
	nodeManagment* node = lookupNode( nodeId);
	if(node == NULL)
	{
		eprintf("%s unkndown nodeId: 0x%02x\n", __FUNCTION__, nodeId);
	}
	else
	{
		if(debugging>1) printf("%s found 0x%02x in configuration\n", __FUNCTION__,nodeId);
		node->initStep = 0;
		if(node->configureSlaveNode!= NULL)
		{
			node->configureSlaveNode(d, nodeId);
		}
		if(debugging) printf("%s Configuring Node 0x%02x\n", __FUNCTION__, nodeId);
	}
}


void ds_Init(CO_Data* d)
{
	UNS8 masterNodeId = getNodeId(&TestMaster_Data);
	nodeManagment* node = lookupNode( masterNodeId);
	if(node == NULL)
	{
		eprintf("%s master node id: 0x%02x not found\n",__FUNCTION__,masterNodeId);
		return;
	}
	struct s_slaveNode* ci = node->configInfo;
	if(ci ==NULL)
	{
		printf("%s nothing to configure, master node\n", __FUNCTION__);
		return;
	}

	struct s_obj_dict* pObjDict = ci->objDict;
	while(pObjDict != NULL)
	{
		ds_LocalObjDictWrite(pObjDict);
		pObjDict = pObjDict->nextObjDict;
	}
}


void CheckReadInfoSDOTxPdo(CO_Data* d, UNS8 nodeId)
{
	UNS32 abortCode;

	nodeManagment* node = lookupNode( nodeId);
	if(node == NULL)
	{
		eprintf("%s node 0x%02x not found\n",__FUNCTION__,nodeId);
		masterSendNMTstateChange (d, nodeId, NMT_Start_Node);
		eprintf("Slave to operational mode \n");
		return;
	}
	struct s_slaveNode* ci = node->configInfo;
	if(ci ==NULL)
	{
		printf("%s nothing to configure, slave node\n", __FUNCTION__);
		masterSendNMTstateChange (d, nodeId, NMT_Start_Node);
		eprintf("Slave to operational mode \n");
		return;
	}
	UNS32 size=sizeof(ci->pdoCobId);

	switch(ci->pdoInfoStep)
	{
	case 1:
		if(getReadResultNetworkDict(d, nodeId, &ci->pdoCobId, &size, &abortCode) != SDO_FINISHED)
		{
			printf("Master : Failed in getting information for slave %2.2x, AbortCode :%4.4x \n", nodeId, abortCode);
			closeSDOtransfer(d, nodeId, SDO_CLIENT);
			return;
		}
		if(debugging>1)eprintf("\ntxpdo%d pdoCobId: 0x%x\n", ci->pdoIndex, ci->pdoCobId);
	break;
	default:
//			printf("txpdo%d completed step %d\n",ci->pdoIndex, ci->pdoInfoStep);
	break;
	}
	// Finalize last SDO transfer with this node
	closeSDOtransfer(d, nodeId, SDO_CLIENT);
	cfgSlave_TxPdo(d, nodeId, ci->pdoIndex );
}


void cfgSlave_RxPdo(CO_Data* d, UNS8 nodeId, int index);

void cfgSlave_TxPdo(CO_Data* d, UNS8 nodeId, int index)
{
	UNS8 res;
	struct s_pdo* spdo;

	nodeManagment* node = lookupNode( nodeId);
	if(node == NULL)
	{
		eprintf("%s node 0x%02x not found\n",__FUNCTION__,nodeId);
		masterSendNMTstateChange (d, nodeId, NMT_Start_Node);
		eprintf("Slave to operational mode \n");
		return;
	}
	struct s_slaveNode* ci = node->configInfo;
	if(ci ==NULL)
	{
		printf("%s nothing to configure, slave node\n", __FUNCTION__);
		masterSendNMTstateChange (d, nodeId, NMT_Start_Node);
		eprintf("Slave to operational mode \n");
		return;
	}

	do
	{
		ci->pdoIndex = index;
		if(index > 4)
			break;
		spdo = ci->txPdo[index-1];
		if(spdo == NULL)
		{
			index++;
			if(index >4)
				break;
		}
	}
	while( spdo == NULL);

	if(spdo ==NULL)
	{
		if(debugging>1)printf("%s no remaining tx PDOs to configure\n", __FUNCTION__);
		ci->pdoInfoStep = 0;
		cfgSlave_RxPdo( d, nodeId, 1);

		return;
	}


	while(1)
	{
		switch(++ci->pdoInfoStep)
		{
		case 1:	// read existing cobid
			readNetworkDictCallback(d, nodeId, 0x1800+index-1, 0x01, 0, CheckReadInfoSDOTxPdo, 0);
			return;
		break;
		case 2:	// disable tpdo in cobid
		{
			UNS32 TPDO_COBId = 0x80000000 | ci->pdoCobId;
			if(debugging>1) eprintf("%s: disable slave %2.2x TPDO %d %lx\n", __FUNCTION__, nodeId, index, TPDO_COBId);
			res = writeNetworkDictCallBack (d, /*CO_Data* d*/
				/**TestSlave_Data.bDeviceNodeId, UNS8 nodeId*/
				nodeId, /*UNS8 nodeId*/0x1800+index-1, /*UNS16 index*/0x01, /*UNS8 subindex*/
				4, /*UNS8 count*/0, /*UNS8 dataType*/
				&TPDO_COBId,/*void *data*/
				CheckReadInfoSDOTxPdo, /*SDOCallback_t Callback*/0); /* use block mode */
			return;
		}
		case 3:	//setup Slave's TPDO transmission type
		{
			if( spdo->stransmission_type == NULL)
				continue;
			UNS8 transType = strtol( spdo->stransmission_type, NULL, 0);
			if(debugging>1) eprintf("%s: set slave %2.2x TPDO %d transmit type to 0x%02x\n", __FUNCTION__, nodeId, index, transType);
			res = writeNetworkDictCallBack (d, /*CO_Data* d*/
				/**TestSlave_Data.bDeviceNodeId, UNS8 nodeId*/
				nodeId, /*UNS8 nodeId*/0x1800+index-1, /*UNS16 index*/0x02, /*UNS8 subindex*/
				1, /*UNS8 count*/0, /*UNS8 dataType*/
				&transType,/*void *data*/
				CheckReadInfoSDOTxPdo, /*SDOCallback_t Callback*/0); /* use block mode */
			return;
		}
		break;
		case 4:	//setup Slave's TPDO inhibit time
		{
			if( spdo->sinhibit_time == NULL)
				continue;
			UNS16 inhibitTime = strtol( spdo->sinhibit_time, NULL, 0);
			if(debugging>1) eprintf("%s: set slave %2.2x TPDO %d inhibit time to 0x%04x (%d)\n", __FUNCTION__, nodeId, index, inhibitTime, inhibitTime);
			res = writeNetworkDictCallBack (d, /*CO_Data* d*/
				/**TestSlave_Data.bDeviceNodeId, UNS8 nodeId*/
				nodeId, /*UNS8 nodeId*/0x1800+index-1, /*UNS16 index*/0x03, /*UNS8 subindex*/
				2, /*UNS8 count*/0, /*UNS8 dataType*/
				&inhibitTime,/*void *data*/
				CheckReadInfoSDOTxPdo, /*SDOCallback_t Callback*/0); /* use block mode */
			return;
		}
		break;
		case 5:	//setup Slave's TPDO event timer
		{
			if( spdo->sevent_timer == NULL)
				continue;
			UNS16 eventTimer = strtol( spdo->sevent_timer, NULL, 0);
			if(debugging>1) eprintf("%s: set slave %2.2x TPDO %d event timer to 0x%04x (%d)\n", __FUNCTION__, nodeId, index, eventTimer, eventTimer);
			res = writeNetworkDictCallBack (d, /*CO_Data* d*/
				/**TestSlave_Data.bDeviceNodeId, UNS8 nodeId*/
				nodeId, /*UNS8 nodeId*/0x1800+index-1, /*UNS16 index*/0x05, /*UNS8 subindex*/
				2, /*UNS8 count*/0, /*UNS8 dataType*/
				&eventTimer,/*void *data*/
				CheckReadInfoSDOTxPdo, /*SDOCallback_t Callback*/0); /* use block mode */
			return;
		}
		break;
		case 6:	//setup Slave's TPDO cobid and re-enable
		{
			UNS32 cobid = ci->pdoCobId;
			if( spdo->scobid != NULL)
			{
				cobid = strtoll( spdo->scobid, NULL, 0);
			}
			if(debugging>1) eprintf("%s: set slave %2.2x TPDO %d cobid to 0x%08x \n", __FUNCTION__, nodeId, index, cobid);
			res = writeNetworkDictCallBack (d, /*CO_Data* d*/
				/**TestSlave_Data.bDeviceNodeId, UNS8 nodeId*/
				nodeId, /*UNS8 nodeId*/0x1800+index-1, /*UNS16 index*/0x01, /*UNS8 subindex*/
				4, /*UNS8 count*/0, /*UNS8 dataType*/
				&cobid,/*void *data*/
				CheckReadInfoSDOTxPdo, /*SDOCallback_t Callback*/0); /* use block mode */
			return;
		}
		break;
		case 7: // this Slave TPDO has been configured, move on to the next on
		{
			ci->pdoInfoStep = 0;
			cfgSlave_TxPdo(d, nodeId, ci->pdoIndex+1 );
			return;
		}
		default:
			return;
		}
	}
}

UNS32 spdoMap2Val(struct s_pdo_map_entry* map)
{
	UNS32 retval = 0;
//	printf("%s  idx:%s sub:%s, num:%s\n", __FUNCTION__, map->sindex, map->ssub, map->snumBits);
	if(map->sindex)
	{
		UNS32 temp = strtol( map->sindex, NULL, 0);
		retval |= temp<<16;
	}
	if(map->ssub)
	{
		UNS32 temp = strtol( map->ssub, NULL, 0);
		retval |= temp<<8;
	}
	if(map->snumBits)
	{
		UNS32 temp = strtol( map->snumBits, NULL, 0);
		retval |=  temp;
	}
	return(retval);
}


// writes to the local dictionary based on the parameters in a s_obj_dict instance (from a config.xml doc)
int ds_LocalObjDictWrite(struct s_obj_dict* pObjDict)
{
	UNS32 count;
	UNS8 bVal;
	UNS16 wVal;
	void* pSource;

	UNS16 wIndex = strtol(pObjDict->sindex, NULL, 0);
	UNS8 bSubindex = strtol(pObjDict->ssub, NULL, 0);
	UNS32 val = strtol(pObjDict->sval, NULL, 0);

	// convert type string to lower case
	char* p = pObjDict->stype;
	for ( ; *p; ++p) *p = tolower(*p);
	if(strstr(pObjDict->stype, "8") != NULL )
	{
		count = 1;
		bVal = val;
		pSource = &bVal;
	}
	else if(strstr(pObjDict->stype, "16") != NULL )
	{
		count = 2;
		wVal = val;
		pSource = &wVal;
	}
	else if(strstr(pObjDict->stype, "32") != NULL)
	{
		count = 4;
		pSource = &val;
	}
	else if(strstr(pObjDict->stype, "string") != NULL)
	{
		count = strlen(pObjDict->sval);
		eprintf("%s type:%s index:0x%04x/%x  val:(%s) len:%d\n", __FUNCTION__, pObjDict->stype, wIndex, bSubindex, pObjDict->sval, count);

		int res = writeLocalDict(gCanOpenData, /*CO_Data* d*/
			wIndex, 	/*UNS16 index*/
			bSubindex, 	/*UNS8 subindex*/
			pObjDict->sval,	/*void *data*/
			&count,
			0);
		return(res);
	}

	if(debugging >1) printf("Local dict write to index:0x%04x/%02x, len:%d val:%d (0x%08lx)\n",wIndex,bSubindex,count,val,val);
	int retval = writeLocalDict(gCanOpenData, wIndex, bSubindex, pSource, &count, 0);
	return(retval);
}


// writes to the dictionary of a node all available objdict entries from a config.xml instance
// s_slaveNode is generated by an entry in a config.xml file
int TMMM_ObjDictWrite( struct s_slaveNode* ci)
{
	if(ci ==NULL)
	{
		printf("%s nothing to configure, slave node\n", __FUNCTION__);
		return(-1);
	}
	if(ci->snodeId == NULL)
	{
		printf("%s no slave node id\n", __FUNCTION__);
		return(-2);
	}

	UNS8 nodeId = strtol(ci->snodeId, NULL, 0);

	if(nodeId == 0)
	{
		currObjDict = ci->objDict;
		while(currObjDict)
		{
			int retval = ds_LocalObjDictWrite(currObjDict);
			if(retval != OD_SUCCESSFUL)
			{
				printf("error with local dict write 0x%x\n\n",retval);
			}
			currObjDict = currObjDict->nextObjDict;
		}
		return(0);
	}

	currObjDict = ci->objDict;
	cfgSlave_ObjDict(gCanOpenData, nodeId);
	return(0);
}


void cfgSlave_PdoMapping(CO_Data* d, UNS8 nodeId )
{
	UNS8 res;
	struct s_pdo* spdo;

	nodeManagment* node = lookupNode( nodeId);
	if(node == NULL)
	{
		eprintf("%s node 0x%02x not found\n",__FUNCTION__,nodeId);
		masterSendNMTstateChange (d, nodeId, NMT_Start_Node);
		eprintf("Slave to operational mode \n");
		return;
	}
	struct s_slaveNode* ci = node->configInfo;
	if(ci ==NULL)
	{
		printf("%s nothing to configure, slave node\n", __FUNCTION__);
		masterSendNMTstateChange (d, nodeId, NMT_Start_Node);
		eprintf("Slave to operational mode \n");
		return;
	}
	
	if(ci->currPdoMap == NULL)
	{
		if(debugging>1) eprintf("%s no remaining PDOs maps to configure\n", __FUNCTION__);

		/* Ask slave node to go in operational mode */
		currObjDict = ci->objDict;
		cfgSlave_ObjDict(d, nodeId);
		return;
	}

	if(ci->currPdoMapEntry == NULL)
	{
		ci->currPdoMapEntry = ci->currPdoMap->mapping;
		ci->mapEntryIndex = 1;
	}

	UNS32 pdoMappingVal = spdoMap2Val(ci->currPdoMapEntry);	// produces the 32 bit index,subindex,val number
	UNS16 index = strtol(ci->currPdoMap->snum, NULL, 0) - 1;
	if(xmlStrcasecmp(ci->currPdoMap->stype, "rx") == 0)
	{
		index |= 0x1600;
	}
	else if(xmlStrcasecmp(ci->currPdoMap->stype, "tx") == 0)
	{
		index |= 0x1A00;
	}

	eprintf("%s type:%s node:%2.2x index:0x%04x/%x  map:0x%lx\n", __FUNCTION__, ci->currPdoMap->stype, nodeId, index, ci->mapEntryIndex, pdoMappingVal);

	res = writeNetworkDictCallBack (d, /*CO_Data* d*/
		nodeId, 	/*UNS8 nodeId*/
		index, 		/*UNS16 index*/
		ci->mapEntryIndex, 	/*UNS8 subindex*/
		4, 		/*UNS8 count*/
		0, 		/*UNS8 dataType*/
		&pdoMappingVal,	/*void *data*/
		CheckWriteInfoPdoMap, /*SDOCallback_t Callback*/0); /* use block mode */
	if(res)
	{
		eprintf("%s ERROR, writeNetworkDictCallback %d 0x%x",__FUNCTION__, res, res);
		exit(-6);
	}

	ci->currPdoMapEntry = ci->currPdoMapEntry->next;
	ci->mapEntryIndex++;

	while(ci->currPdoMapEntry == NULL)
	{ // go through maps until we find one with a non-null mapping
		ci->currPdoMap = ci->currPdoMap->nextPdoMap;
		if(ci->currPdoMap == NULL)
		{ // if there are no more maps, we are done with pdo mappings
			ci->currPdoMapEntry = NULL;
			return;

		}
		ci->currPdoMapEntry = ci->currPdoMap->mapping;
		ci->mapEntryIndex = 1;
	}
}


void CheckWriteInfoPdoMap(CO_Data* d, UNS8 nodeId)
{
	UNS32 abortCode;
	nodeManagment* node = lookupNode(nodeId);
	if(node == NULL)
	{
		eprintf("%s node 0x%02x not found in table\n",__FUNCTION__, nodeId);
		closeSDOtransfer(&TestMaster_Data, nodeId, SDO_CLIENT);
		return;
	}

	if(getWriteResultNetworkDict (d, nodeId, &abortCode) != SDO_FINISHED)
	{
		eprintf("%s: Failed setting PDO map for slave %2.2x, AbortCode :%4.4x \n", __FUNCTION__, nodeId, abortCode);
		exit(-7);
	}

	closeSDOtransfer(d, nodeId, SDO_CLIENT);	// Finalise last SDO transfer with this node
	cfgSlave_PdoMapping(d, nodeId);
}


void CheckWriteInfoObjDict(CO_Data* d, UNS8 nodeId)
{
	UNS32 abortCode;
	nodeManagment* node = lookupNode(nodeId);
	if(node == NULL)
	{
		eprintf("%s node 0x%02x not found in table\n",__FUNCTION__, nodeId);
		closeSDOtransfer(&TestMaster_Data, nodeId, SDO_CLIENT);
		return;
	}

	if(getWriteResultNetworkDict (d, nodeId, &abortCode) != SDO_FINISHED)
	{
		eprintf("%s: Failed setting Object Dictionary for slave %2.2x, AbortCode :%4.4x \n", __FUNCTION__, nodeId, abortCode);
		closeSDOtransfer(d, nodeId, SDO_CLIENT);
		return;		
//		exit(-7);
	}

	/* Finalise last SDO transfer with this node */
	closeSDOtransfer(d, nodeId, SDO_CLIENT);

	cfgSlave_ObjDict(d, nodeId);
}


void cfgSlave_ObjDict(CO_Data* d, UNS8 nodeId )
{
	UNS8 res;
	struct s_pdo* spdo;
	UNS8 count = 1;
	if(currObjDict == NULL)
	{
		if(debugging>1) eprintf("%s no remaining Object Dictionary entries to configure\n", __FUNCTION__);
		/* Ask slave node to go in operational mode */
		masterSendNMTstateChange (d, nodeId, NMT_Start_Node);
		if(debugging) eprintf("Node 0x%02x set to operational mode.\n",nodeId);
		return;
	}


	UNS16 index = strtol(currObjDict->sindex, NULL, 0);
	UNS8 subindex = strtol(currObjDict->ssub, NULL, 0);
	UNS32 val = strtol(currObjDict->sval, NULL, 0);

	// convert type string to lower case
	char* p = currObjDict->stype;
	for ( ; *p; ++p) *p = tolower(*p);
	if(strstr(currObjDict->stype, "8") != NULL )
	{
		count = 1;
	}
	else if(strstr(currObjDict->stype, "16") != NULL )
	{
		count = 2;
	}
	else if(strstr(currObjDict->stype, "32") != NULL)
	{
		count = 4;
	}
	else if(strstr(currObjDict->stype, "string") != NULL)
	{
		count = strlen(currObjDict->sval);
		eprintf("%s type:%s node:%2.2x index:0x%04x/%x  val:(%s) len:%d\n", __FUNCTION__, currObjDict->stype, nodeId, index, subindex, currObjDict->sval, count);

		res = writeNetworkDictCallBack (d, /*CO_Data* d*/
			nodeId, 	/*UNS8 nodeId*/
			index, 		/*UNS16 index*/
			subindex, 	/*UNS8 subindex*/
			count, 		/*UNS8 count*/
			visible_string, /*UNS8 dataType*/
			currObjDict->sval,	/*void *data*/
			CheckWriteInfoObjDict, /*SDOCallback_t Callback*/0); /* use block mode */
			if(res)
			{
				eprintf("%s ERROR, writeNetworkDictCallback %d 0x%x",__FUNCTION__, res, res);
				exit(-6);
			}

			currObjDict = currObjDict->nextObjDict;
			return;
	}


	eprintf("%s type:%s node:%2.2x index:0x%04x/%x  val:0x%lx len:%d\n", __FUNCTION__, currObjDict->stype, nodeId, index, subindex, val, count);

	res = writeNetworkDictCallBack (d, /*CO_Data* d*/
		nodeId, 	/*UNS8 nodeId*/
		index, 		/*UNS16 index*/
		subindex, 	/*UNS8 subindex*/
		count, 		/*UNS8 count*/
		0, 		/*UNS8 dataType*/
		&val,	/*void *data*/
		CheckWriteInfoObjDict, /*SDOCallback_t Callback*/0); /* use block mode */
	if(res)
	{
		eprintf("%s ERROR, writeNetworkDictCallback %d 0x%x",__FUNCTION__, res, res);
		exit(-6);
	}

	currObjDict = currObjDict->nextObjDict;
}


void CheckReadInfoSDORxPdo(CO_Data* d, UNS8 nodeId)
{
	UNS32 abortCode;
	
	nodeManagment* node = lookupNode( nodeId);
	if(node == NULL)
	{
		eprintf("%s node 0x%02x not found\n",__FUNCTION__,nodeId);
		masterSendNMTstateChange (d, nodeId, NMT_Start_Node);
		eprintf("Slave to operational mode \n");
		return;
	}
	struct s_slaveNode* ci = node->configInfo;
	if(ci ==NULL)
	{
		printf("%s nothing to configure, slave node\n", __FUNCTION__);
		masterSendNMTstateChange (d, nodeId, NMT_Start_Node);
		eprintf("Slave to operational mode \n");
		return;
	}
	UNS32 size=sizeof(ci->pdoCobId);


	{
		/* Display data received */
		switch(ci->pdoInfoStep)
		{
		case 1:
			if(getReadResultNetworkDict(d, nodeId, &ci->pdoCobId, &size, &abortCode) != SDO_FINISHED)
			{
				printf("%s: Failed in getting information for slave %2.2x, AbortCode :%4.4x \n", __FUNCTION__, nodeId, abortCode);
				closeSDOtransfer(d, nodeId, SDO_CLIENT);
				return;
			}
			printf("\nrxpdo%d pdoCobId    : 0x%x\n", ci->pdoIndex, ci->pdoCobId);
		break;
		default:
			if(getWriteResultNetworkDict(d, nodeId, &abortCode) != SDO_FINISHED)
			{
				printf("%s: WARNING failed to set info for slave %2.2x, AbortCode :%4.4x \n", __FUNCTION__, nodeId, abortCode);
			}
		break;
		}
	}
	/* Finalize last SDO transfer with this node */
	closeSDOtransfer(d, nodeId, SDO_CLIENT);
	cfgSlave_RxPdo(d, nodeId, ci->pdoIndex );
}


void cfgSlave_RxPdo(CO_Data* d, UNS8 nodeId, int index)
{
	UNS8 res;
	struct s_pdo* spdo;

	nodeManagment* node = lookupNode( nodeId);
	if(node == NULL)
	{
		eprintf("%s node 0x%02x not found\n",__FUNCTION__,nodeId);
		masterSendNMTstateChange (d, nodeId, NMT_Start_Node);
		eprintf("Slave to operational mode \n");
		return;
	}
	struct s_slaveNode* ci = node->configInfo;
	if(ci ==NULL)
	{
		printf("%s nothing to configure, slave node\n", __FUNCTION__);
		masterSendNMTstateChange (d, nodeId, NMT_Start_Node);
		eprintf("Slave to operational mode \n");
		return;
	}

	do
	{
		ci->pdoIndex = index;
		spdo = ci->rxPdo[index-1];
		if(spdo == NULL)
		{
			index++;
			if(index >4)
				break;
		}
	}
	while( spdo == NULL);

	if(spdo ==NULL)
	{
		if(debugging>1) printf("%s no remaining rx PDOs to configure\n", __FUNCTION__);
		ci->currPdoMap = ci->pdoMap;
		cfgSlave_PdoMapping(d,nodeId);
		return;
	}


	while(1)
	{
		switch(++ci->pdoInfoStep)
		{
		case 1:	// read existing cobid
			readNetworkDictCallback(d, nodeId, 0x1400+index-1, 0x01, 0, CheckReadInfoSDORxPdo, 0);
			return;
		break;
		case 2:	// disable tpdo in cobid
		{
			UNS32 RPDO_COBId = 0x80000000 | ci->pdoCobId;
			if(debugging>1) eprintf("sdoRxPdo: disable slave %2.2x RPDO %d  %lx\n", nodeId, index, RPDO_COBId);
			res = writeNetworkDictCallBack (
				d, /*CO_Data* d*//**TestSlave_Data.bDeviceNodeId, UNS8 nodeId*/
				nodeId, /*UNS8 nodeId*/
				0x1400+index-1, /*UNS16 index*/
				0x01, /*UNS8 subindex*/
				4, /*UNS8 count*/
				0, /*UNS8 dataType*/
				&RPDO_COBId,/*void *data*/
				CheckReadInfoSDORxPdo, /*SDOCallback_t Callback*/0); /* use block mode */
			return;
		}
		case 3:	//setup Slave's RPDO transmission type
		{
			if( spdo->stransmission_type == NULL)
				continue;
			UNS8 transType = strtol( spdo->stransmission_type, NULL, 0);
			if(debugging>1) eprintf("sdoRxPdo: set slave %2.2x RPDO %d transmit type to 0x%02x\n", nodeId, index, transType);
			res = writeNetworkDictCallBack (d, /*CO_Data* d*/
				/**TestSlave_Data.bDeviceNodeId, UNS8 nodeId*/
				nodeId, /*UNS8 nodeId*/0x1400+index-1, /*UNS16 index*/0x02, /*UNS8 subindex*/
				1, /*UNS8 count*/0, /*UNS8 dataType*/
				&transType,/*void *data*/
				CheckReadInfoSDORxPdo, /*SDOCallback_t Callback*/0); /* use block mode */
			return;
		}
		break;
		case 4:	//setup Slave's RPDO inhibit time
		{
			if( spdo->sinhibit_time == NULL)
				continue;
			UNS16 inhibitTime = strtol( spdo->sinhibit_time, NULL, 0);
			if(debugging>1) eprintf("sdoRxPdo: set slave %2.2x RPDO %d inhibit time to 0x%04x (%d)\n", nodeId, index, inhibitTime, inhibitTime);
			res = writeNetworkDictCallBack (d, /*CO_Data* d*/
				/**TestSlave_Data.bDeviceNodeId, UNS8 nodeId*/
				nodeId, /*UNS8 nodeId*/0x1400+index-1, /*UNS16 index*/0x03, /*UNS8 subindex*/
				2, /*UNS8 count*/0, /*UNS8 dataType*/
				&inhibitTime,/*void *data*/
				CheckReadInfoSDORxPdo, /*SDOCallback_t Callback*/0); /* use block mode */
			return;
		}
		break;
		case 5:	//setup Slave's RPDO event timer
		{
			if( spdo->sevent_timer == NULL)
				continue;
			UNS16 eventTimer = strtol( spdo->sevent_timer, NULL, 0);
			if(debugging>1) eprintf("sdoRxPdo: set slave %2.2x RPDO %d event timer to 0x%04x (%d)\n", nodeId, index, eventTimer, eventTimer);
			res = writeNetworkDictCallBack (d, /*CO_Data* d*/
				/**TestSlave_Data.bDeviceNodeId, UNS8 nodeId*/
				nodeId, /*UNS8 nodeId*/0x1400+index-1, /*UNS16 index*/0x05, /*UNS8 subindex*/
				2, /*UNS8 count*/0, /*UNS8 dataType*/
				&eventTimer,/*void *data*/
				CheckReadInfoSDORxPdo, /*SDOCallback_t Callback*/0); /* use block mode */
			return;
		}
		break;
		case 6:	//setup Slave's RPDO cobid and re-enable
		{
			UNS32 cobid = ci->pdoCobId;
			if( spdo->scobid != NULL)
			{
				cobid = strtol( spdo->scobid, NULL, 0);
			}
			if(debugging>1) eprintf("sdoRxPdo: set slave %2.2x RPDO %d cobid to 0x%08x \n", nodeId, index, cobid);
			res = writeNetworkDictCallBack (d, /*CO_Data* d*/
				/**TestSlave_Data.bDeviceNodeId, UNS8 nodeId*/
				nodeId, /*UNS8 nodeId*/0x1400+index-1, /*UNS16 index*/0x01, /*UNS8 subindex*/
				4, /*UNS8 count*/0, /*UNS8 dataType*/
				&cobid,/*void *data*/
				CheckReadInfoSDORxPdo, /*SDOCallback_t Callback*/0); /* use block mode */
			return;
		}
		break;
		case 7: // this Slave RPDO has been configured, move on to the next on
		{
			ci->pdoInfoStep = 0;
			cfgSlave_RxPdo(d, nodeId, ci->pdoIndex+1 );
			return;
		}
		default:
			return;
		}
	}
}


/**/
static void CheckSDOAndContinue(CO_Data* d, UNS8 nodeId)
{
	UNS32 abortCode;

	nodeManagment* node = lookupNode(nodeId);
	if(node == NULL)
	{
		eprintf("%s node 0x%02x not found in table\n",__FUNCTION__,nodeId);
		closeSDOtransfer(&TestMaster_Data, nodeId, SDO_CLIENT);
		return;
	}

	if(getWriteResultNetworkDict (d, nodeId, &abortCode) != SDO_FINISHED)
	{
		eprintf("%s: Failed in initializing slave %2.2x, step %d, AbortCode :%4.4x \n", __FUNCTION__, nodeId, node->initStep, abortCode);
		exit(-6);
	}
	/* Finalise last SDO transfer with this node */
	closeSDOtransfer(&TestMaster_Data, nodeId, SDO_CLIENT);

	node->configureSlaveNode(d,nodeId);
}


//********************************************************
// ConfigureSlaveNode
//********************************************************
static void ConfigureSlaveNode(CO_Data* d, UNS8 nodeId)
{
	UNS8 res;
	nodeManagment* node = lookupNode( nodeId);
	if(node == NULL)
	{
		eprintf("%s node 0x%02x not found\n",__FUNCTION__,nodeId);
		return;

	}
	else
		printf("%s 0x%x\n",__FUNCTION__,nodeId);

	while(1)
	{
		switch(++(node->initStep))
		{
		case 1:
		{	// heartbeat
			UNS16 heartbeatProducerTime = 5000;  // time, in milliseconds
			if( node->configInfo != NULL)
			{
				if( node->configInfo->sheartbeatTime != NULL)
					heartbeatProducerTime = strtol(node->configInfo->sheartbeatTime, NULL, 0);
			}
			if(debugging>1) eprintf("\n%s: set slave %2.2x heartbeat producer time to %d (0x%04x)\n", __FUNCTION__, nodeId, heartbeatProducerTime, heartbeatProducerTime);

			// set the heartbeat producer time of the slave device
			res = writeNetworkDictCallBack(d, /*CO_Data* d*/
				/**TestSlave_Data.bDeviceNodeId, UNS8 nodeId*/
					nodeId, /*UNS8 nodeId*/0x1017, /*UNS16 index*/0x00, /*UNS8 subindex*/
					2, /*UNS8 count*/0, /*UNS8 dataType*/
					&heartbeatProducerTime,/*void *data*/
					CheckSDOAndContinue, /*SDOCallback_t Callback*/0); /* use block mode */
			if(res)
			{
				eprintf("%s: Error 0x%02x returned at step %d for node %x\r\n", __FUNCTION__, res, node->initStep,nodeId);
				exit(-1);
			}
			return;
		}
		break;
		case 2:
		{
			struct s_slaveNode* ci = node->configInfo;
			if(ci ==NULL)
			{
				printf("%s nothing to configure, slave node\n", __FUNCTION__);
				masterSendNMTstateChange (d, nodeId, NMT_Start_Node);
				eprintf("Slave to operational mode \n");
				return;
			}

			ci->currPdoMap = NULL;
			ci->currPdoMapEntry = NULL;
			ci->mapEntryIndex = 0;

			ci->pdoInfoStep = 0;
			ci->pdoIndex = 1;
			ci->pdoCobId = 0;
			
printf("%s starting TxPdo: 0x%x\n",__FUNCTION__, nodeId);
			cfgSlave_TxPdo(d,nodeId,1);  	// configure the slave node txpdo's
			setState(d, Operational);	// Put the master in operational mode
			return;
		}
		}
	}	
}


/********************************************************
 * ConfigureDisplayNode is responsible to
 *  - setup slave TPDO 1 transmit time
 *  - setup slave TPDO 2 transmit time
 *  - setup slave Heartbeat Producer time
 *  - switch to operational mode
 *  - send NMT to slave
 ********************************************************
 * This an example of :
 * Network Dictionary Access (SDO) with Callback
 * Slave node state change request (NMT)
 ********************************************************
 * This is called first by ds_PreOperational
 * then it called again each time a SDO exchange is
 * finished.
 ********************************************************/
static void ConfigureDisplayNode(CO_Data* d, UNS8 nodeId)
{
	UNS8 res;
	nodeManagment* node = lookupNode( nodeId);
	if(node == NULL)
	{
		eprintf("%s node 0x%02x not found\n",__FUNCTION__,nodeId);
		return;

	}
//	eprintf("Master : ConfigureSlaveNode %2.2x\n", nodeId);
	printf("display node nodeId slave=%x\n",nodeId);
	switch(++(node->initStep))
	{
// heartbeat
	case 1:
	{
		UNS16 heartbeatProducerTime = 10000;  // time, in milliseconds
		eprintf("\nMaster : set slave %2.2x heartbeat producer time to %d\n", nodeId, heartbeatProducerTime);
		res = writeNetworkDictCallBack (d, /*CO_Data* d*/
			/**TestSlave_Data.bDeviceNodeId, UNS8 nodeId*/
				nodeId, /*UNS8 nodeId*/0x1017, /*UNS16 index*/0x00, /*UNS8 subindex*/
				2, /*UNS8 count*/0, /*UNS8 dataType*/
				&heartbeatProducerTime,/*void *data*/
				CheckSDOAndContinue, /*SDOCallback_t Callback*/0); /* use block mode */
	}
	break;


	case 12:
		/* Put the slave in operational mode */
		setState(d, Operational);

		/* Ask slave node to go in operational mode */
		masterSendNMTstateChange (d, nodeId, NMT_Start_Node);
		eprintf("Master and  slave to operational mode \n");

	}

	if(res)
	{
		eprintf("Master: Error 0x%02x returned at step %d\r\n", res, node->initStep);
		exit(-1);
	}
}


void ds_PreOperational(CO_Data* d)
{
	if(debugging) eprintf("%s master node:0x%02x\n", __FUNCTION__, masterNodeId);
	masterSendNMTstateChange(&TestMaster_Data, 0x00, NMT_Reset_Node);
//        masterSendNMTstateChange(&TestMaster_Data, masterNodeId, NMT_Reset_Node);
}


void ds_Operational(CO_Data* d)
{
	if(debugging) eprintf("%s\n", __FUNCTION__);
}


void ds_Stopped(CO_Data* d)
{
	if(debugging) eprintf("%s\n", __FUNCTION__);
}


void ds_PostSync(CO_Data* d)
{
	DO++;
	static int lastAI1 = -1;

//	AO1 = AI1 / 2;
//	AO2 = AI2 / 2;
//	AO3 = AI3 / 2;
//	AO4 = AI4 / 2;
	AO1 = 0;
	AO2 = 0;
	AO3 = 0;
	AO4 = 0;
// 	 eprintf("DS401_Master Digital Out: %2.2x\n",DO);
//	 eprintf("DS401_Master Analogue Out1: %d\n",AO1);
//	 eprintf("DS401_Master Analogue Out2: %d\n",AO2);
//	 eprintf("DS401_Master Analogue Out3: %d\n",AO3);
//	 eprintf("DS401_Master Analogue Out4: %d\n",AO4);
//	 eprintf("DS401_Master Digital In (by bit): DI1: %2.2x DI2: %2.2x DI3: %2.2x DI4: %2.2x DI5: %2.2x DI6: %2.2x DI7: %2.2x DI8: %2.2x\n",DI1,DI2,DI3,DI4,DI5,DI6,DI7,DI8);
//	if(lastAI1 != AI1)
//	{
//		eprintf("System Voltage: %d (mV) curr: %d (mA) %3.1f (degC) \n", AI1, AI2, AI3/10.0f);
//		lastAI1 = AI1;
//	}
//	 eprintf("DS401_Master Analogue In2: %d\n", AI2);
//	 eprintf("DS401_Master Analogue In3: %d\n", AI3);
//	 eprintf("DS401_Master Analogue In4: %d\n", AI4);
//	 eprintf("DS401_Master Analogue In5: %d\n", AI5);
//	 eprintf("DS401_Master Analogue In6: %d\n", AI6);
//	 eprintf("DS401_Master Analogue In7: %d\n", AI7);
// 	 eprintf("DS401_Master Analogue In8: %d\n", AI8);
}


void ds_PostTpdo(CO_Data* d)
{
//	eprintf("%s\n", __FUNCTION__);
}


#if !defined(WIN32) || defined(__CYGWIN__)
void catch_signal(int sig)
{
  signal(SIGTERM, catch_signal);
  signal(SIGINT, catch_signal);

  eprintf("Got Signal %d\n",sig);
}
#endif


void help(void)
{
	printf("**************************************************************\n");
	printf("*  DS401_Master                                              *\n");
	printf("*                                                            *\n");
	printf("*  A CanOpen master that controls a group of slaves:         *\n");
	printf("*  - set state to operational                                *\n");
	printf("*  - send periodic SYNC                                      *\n");
	printf("*  - send periodic RPDO 1 to a slave (digital output)        *\n");
	printf("*  - listen to a slave TPDO 1 (digital input)                *\n");
	printf("*  - Mapping RPDO 1 bit per bit (digital input)              *\n");
	printf("*                                                            *\n");
	printf("*   Usage:                                                   *\n");
	printf("*   ./DS401_Master  [OPTIONS]                                *\n");
	printf("*                                                            *\n");
	printf("*   OPTIONS:                                                 *\n");
	printf("*     -l : Can library [\"libcanfestival_can_virtual.so\"]     *\n");
	printf("*     -x : config file [\"config.xml\"]                        *\n");
	printf("*     -d : increase debugging verbosity                      *\n");
	printf("*                                                            *\n");
	printf("*    Master:                                                 *\n");
	printf("*     -m : bus name [\"1\"]                                    *\n");
	printf("*     -M : 1M,500K,250K,125K,100K,50K,20K,10K                *\n");
	printf("*     -i : Node id format [0x01 , 0x7F]                      *\n");
	printf("*     -p : port number to listen to for cgi-bin requests     *\n");
	printf("*                                                            *\n");
	printf("**************************************************************\n");
}


/***************************  INIT  *****************************************/
void InitNodes(CO_Data* d, UNS32 id)
{
	/****************************** INITIALISATION MASTER *******************************/
	if(MasterBoard.baudrate)
	{
		/* Defining the node Id */
		//setNodeId(&TestMaster_Data, 0x01);

		/* init */
		setState(&TestMaster_Data, Initialisation);
	}
}


/***************************  EXIT  *****************************************/
void Exit(CO_Data* d, UNS32 id)
{
	masterSendNMTstateChange(&TestMaster_Data, 0x00, NMT_Reset_Node);

	//Stop master
	setState(&TestMaster_Data, Stopped);
}


/****************************************************************************/
/***************************  MAIN  *****************************************/
/****************************************************************************/
int main(int argc,char **argv)
{
	int c;
	int i;
	extern char *optarg;
	char *snodeid;
	char xmlFileName[MAX_FILENAME];
	int portNum = 7332;
	char* sPortNum;

	while ((c = getopt(argc, argv, "-m:M:l:i:p:x:d")) != EOF)
	{
		switch(c)
		{
		case 'd':
			//printf("copying testfile");
			debugging++;
		break;
		case 'x':
			//printf("copying testfile");
			strncpy(xmlFileName, optarg, MAX_FILENAME);
		break;
		case 'm' :
			if (optarg[0] == 0)
			{
				help();
				exit(1);
			}
			MasterBoard.busname = optarg;
		break;
		case 'M' :
			if (optarg[0] == 0)
			{
				help();
				exit(1);
			}
			MasterBoard.baudrate = optarg;
		break;
		case 'l' :
			if (optarg[0] == 0)
			{
				help();
				exit(1);
			}
			LibraryPath = optarg;
		break;
		case 'i' :
			if (optarg[0] == 0)
			{
				help();
				exit(1);
			}
			snodeid = optarg;
			sscanf(snodeid,"%x",&masterNodeId);
		break;
		case 'p' :
			if (optarg[0] == 0)
			{
				help();
				exit(1);
			}
			sPortNum = optarg;
			sscanf(sPortNum,"%d",&portNum);
		break;
		default:
			help();
			exit(1);
		}
	}
	if(debugging)
	{
		eprintf("\n");
		for(i=0;i<argc;i++)
		{
			eprintf("%s ",argv[i]);
		}
		eprintf("\n");
	}

	*TestMaster_Data.bDeviceNodeId=(UNS8)0xFF;	// set to invalid
	setNodeId(&TestMaster_Data, masterNodeId);	//This breaks it
//	eprintf("Nodeid 0x%02x\n",getNodeId(&TestMaster_Data));

#if !defined(WIN32) || defined(__CYGWIN__)
	/* install signal handler for manual break */
	signal(SIGTERM, catch_signal);
	signal(SIGINT, catch_signal);
	TimerInit();
#endif

	if(debugging){ printf("Reading and parsing <%s>\n",xmlFileName); }
	//read in test limits
	if(CP_parseConfigFile(xmlFileName))
	{
		fprintf(stderr,"ERROR unable to parse bounds file\n");
		exit(3);
	}

#ifndef NOT_USE_DYNAMIC_LOADING
	LoadCanDriver(LibraryPath);
#endif

	TestMaster_Data.heartbeatError = ds_HeartbeatTimeoutError;
	TestMaster_Data.initialisation = ds_Init;
	TestMaster_Data.preOperational = ds_PreOperational;
	TestMaster_Data.operational = ds_Operational;
	TestMaster_Data.stopped = ds_Stopped;
	TestMaster_Data.post_sync = ds_PostSync;
	TestMaster_Data.post_TPDO = ds_PostTpdo;

	TestMaster_Data.post_SlaveBootup=ds_ProcessSlaveBootup;
	if(debugging) eprintf("Master Node Id: 0x%02x\n",getNodeId(&TestMaster_Data));

	addNodeToManagmentQ(0x66, ConfigureSlaveNode);	// built in node on some raspberry pi's
	addNodeToManagmentQ(0x41, ConfigureSlaveNode);
	addNodeToManagmentQ(0x33, ConfigureSlaveNode);
	addNodeToManagmentQ(0x22, ConfigureDisplayNode); // display device #1
	addNodeToManagmentQ(0x23, ConfigureSlaveNode); // display device #2

	addNodeToManagmentQ(0x01, ConfigureSlaveNode);
	addNodeToManagmentQ(0x02, ConfigureSlaveNode);
	addNodeToManagmentQ(0x03, ConfigureSlaveNode);
	addNodeToManagmentQ(0x04, ConfigureSlaveNode);
	addNodeToManagmentQ(0x05, ConfigureSlaveNode);
	addNodeToManagmentQ(0x06, ConfigureSlaveNode);
	addNodeToManagmentQ(0x07, ConfigureSlaveNode);
	addNodeToManagmentQ(0x08, ConfigureSlaveNode);
	addNodeToManagmentQ(0x09, ConfigureSlaveNode);
	addNodeToManagmentQ(0x0A, ConfigureSlaveNode);
	addNodeToManagmentQ(0x0B, ConfigureSlaveNode);
	addNodeToManagmentQ(0x0C, ConfigureSlaveNode);
	addNodeToManagmentQ(0x0D, ConfigureSlaveNode);
	addNodeToManagmentQ(0x0E, ConfigureSlaveNode);
	addNodeToManagmentQ(0x0F, ConfigureSlaveNode);

	if(!canOpen(&MasterBoard,&TestMaster_Data))
	{
		eprintf("Cannot open Master Board\n");
		goto fail_master;
	}

	// Start timer thread
	StartTimerLoop(&InitNodes);

	startDeamonLoop(portNum);

	// wait Ctrl-C
	pause();
	eprintf("Finishing.\n");

	// Reset the slave node for next use (will stop emitting heartbeat)
//	masterSendNMTstateChange (&TestMaster_Data, slaveNodeId, NMT_Reset_Node);

	// Stop master
	setState(&TestMaster_Data, Stopped);

	// Stop timer thread
	StopTimerLoop(&Exit);

fail_master:
	if(MasterBoard.baudrate)
		canClose(&TestMaster_Data);

	TimerCleanup();
	return 0;
}

