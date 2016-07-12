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

#endif

#include "canfestival.h"
#include "TestMasterMicroMod.h"
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
UNS8 slaveNodeId;
UNS8 masterNodeId = 0x17;
//static int init_step = 0;
s_BOARD MasterBoard = {"32", "500K"};
int debugging = 0;
char configLib[MAX_LIBNAME];
char busName[MAX_BUSNAME];
char baudRate[MAX_BUSNAME];
char* LibraryPath="libcanfestival_can_virtual.so";
struct s_obj_dict* currObjDict = NULL;

/*----------------------------------------------------------------------------
*        Local functions
*----------------------------------------------------------------------------*/
static void ConfigureSlaveNode(CO_Data* d, UNS8 nodeId);
UNS8 getNodeId(CO_Data* d);

void CheckWriteInfoPdoMap(CO_Data* d, UNS8 nodeid);
void csn_ObjDict(CO_Data* d, UNS8 nodeId );

/*----------------------------------------------------------------------------
*        Local functions
*----------------------------------------------------------------------------*/

TAILQ_HEAD(nodeq, td_nodeManagment);
struct nodeq nodeManQ;

nodeManagment* lookupNode(uint8_t nodeId)
{
	nodeManagment* ptr;
	TAILQ_FOREACH(ptr,&nodeManQ, tailq)
	{
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
		newNode->configureSlaveNode = configureSlaveNode;
	}
	return(newNode);
}


void testmaster_heartbeatError(CO_Data* d, UNS8 heartbeatID)
{
	eprintf("%s %d 0x%02x\n", __FUNCTION__, heartbeatID, heartbeatID);
}


void testmaster_post_SlaveBootup(CO_Data* d, UNS8 nodeId)
{
	eprintf("%s nodeId: 0x%02x\n", __FUNCTION__, nodeId);
	if(nodeId != 0x20)
	{
		nodeManagment* node = lookupNode( nodeId);
		if(node == NULL)
		{
			eprintf("%s unkndown nodeId: 0x%02x\n", __FUNCTION__, nodeId);

		}
		else
		{
			node->initStep = 0;
			node->configureSlaveNode(d, nodeId);
		}
	}
}


/********************************************************
 * ConfigureSlaveNode is responsible to
 *  - setup master RPDO 1 to receive TPDO 1 from id 0x40
 *  - setup master TPDO 1 to send RPDO 1 to id 0x40
 ********************************************************/
void testmaster_initialisation(CO_Data* d)
{
	UNS32 RPDO1_COBID = 0x0180 + slaveNodeId;
	UNS32 RPDO2_COBID = 0x0280 + slaveNodeId;
	UNS32 TPDO1_COBID = 0x0200 + slaveNodeId;
	UNS32 size = sizeof(UNS32);

	eprintf("%s\n", __FUNCTION__);

	/*****************************************
	 * Define our RPDO to match slave ID TPDO*
	 *****************************************/
	writeLocalDict( &TestMaster_Data, /*CO_Data* d*/
		0x1400, /*UNS16 index*/0x01, /*UNS8 subind*/
		&RPDO1_COBID, /*void * pSourceData,*/
		&size, /* UNS8 * pExpectedSize*/RW);  /* UNS8 checkAccess */

	/*****************************************
	 * Define our RPDO to match slave ID TPDO*
	 *****************************************/
	writeLocalDict( &TestMaster_Data, /*CO_Data* d*/
		0x1401, /*UNS16 index*/0x01, /*UNS8 subind*/
		&RPDO2_COBID, /*void * pSourceData,*/
		&size, /* UNS8 * pExpectedSize*/RW);  /* UNS8 checkAccess */

	/*****************************************
	 * Define our TPDOs to match slave ID RPDO*
	 *****************************************/
	writeLocalDict( &TestMaster_Data, /*CO_Data* d*/
		0x1800, /*UNS16 index*/0x01, /*UNS8 subind*/
		&TPDO1_COBID, /*void * pSourceData,*/
		&size, /* UNS8 * pExpectedSize*/RW);  /* UNS8 checkAccess */
}




int  pdoInfoStep = 0;
int pdoIndex = 1;
UNS32 pdoCobId = 0;
void csn_TxPdo(CO_Data* d, UNS8 nodeId, int index);


void CheckReadInfoSDOTxPdo(CO_Data* d, UNS8 nodeid)
{
	UNS32 abortCode;
	UNS32 size=sizeof(pdoCobId);

	{
		/* Display data received */
		switch(pdoInfoStep)
		{
		case 1:
			if(getReadResultNetworkDict(d, nodeid, &pdoCobId, &size, &abortCode) != SDO_FINISHED)
			{
				printf("Master : Failed in getting information for slave %2.2x, AbortCode :%4.4x \n", nodeid, abortCode);
				closeSDOtransfer(d, nodeid, SDO_CLIENT);
				return;
			}
			printf("\ntxpdo%d pdoCobId: 0x%x\n", pdoIndex, pdoCobId);
		break;
		default:
//			printf("txpdo%d completed step %d\n",pdoIndex,pdoInfoStep);
		break;
		}
	}
	/* Finalize last SDO transfer with this node */
	closeSDOtransfer(d, nodeid, SDO_CLIENT);
	csn_TxPdo(d, nodeid, pdoIndex );
}

void csn_RxPdo(CO_Data* d, UNS8 nodeId, int index);

void csn_TxPdo(CO_Data* d, UNS8 nodeId, int index)
{
	UNS8 res;
	struct s_pdo* spdo;

	nodeManagment* node = lookupNode( nodeId);
	if(node == NULL)
	{
		eprintf("%s node 0x%02x not found\n",__FUNCTION__,nodeId);
		return;

	}

	struct s_slaveNode* ci = node->configInfo;
	if(ci ==NULL)
	{
		printf("%s nothing to configure, slave node\n", __FUNCTION__);
		return;
	}

	do
	{
		pdoIndex = index;
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
		printf("%s no remaining tx PDOs to configure\n", __FUNCTION__);
		pdoInfoStep = 0;
		csn_RxPdo( d, nodeId, 1);

		return;
	}


	while(1)
	{
		switch(++pdoInfoStep)
		{
		case 1:	// read existing cobid
			readNetworkDictCallback(d, nodeId, 0x1800+index-1, 0x01, 0, CheckReadInfoSDOTxPdo, 0);
			return;
		break;
		case 2:	// disable tpdo in cobid
		{
			UNS32 TPDO_COBId = 0x80000000 | pdoCobId;
			eprintf("%s: disable slave %2.2x TPDO %d %lx\n", __FUNCTION__, nodeId, index, TPDO_COBId);
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
			eprintf("%s: set slave %2.2x TPDO %d transmit type to 0x%02x\n", __FUNCTION__, nodeId, index, transType);
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
			eprintf("%s: set slave %2.2x TPDO %d inhibit time to 0x%04x (%d)\n", __FUNCTION__, nodeId, index, inhibitTime, inhibitTime);
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
			eprintf("%s: set slave %2.2x TPDO %d event timer to 0x%04x (%d)\n", __FUNCTION__, nodeId, index, eventTimer, eventTimer);
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
			UNS32 cobid = pdoCobId;
			if( spdo->scobid != NULL)
			{
				cobid = strtoll( spdo->scobid, NULL, 0);
			}
			eprintf("%s: set slave %2.2x TPDO %d cobid to 0x%08x \n", __FUNCTION__, nodeId, index, cobid);
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
			pdoInfoStep = 0;
			csn_TxPdo(d, nodeId, pdoIndex+1 );
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


struct s_pdo_map* currPdoMap = NULL;
void csn_PdoMapping(CO_Data* d, UNS8 nodeId )
{
	static struct s_pdo_map_entry* currPdoMapEntry = NULL;
	static UNS8 mapEntryIndex = 0;
	UNS8 res;
	struct s_pdo* spdo;

	if(currPdoMap == NULL)
	{
		printf("%s no remaining PDOs maps to configure\n", __FUNCTION__);
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

		/* Ask slave node to go in operational mode */
		currObjDict = ci->objDict;
		csn_ObjDict(d, nodeId);
		return;
	}

	while(currPdoMapEntry == NULL)
	{
		currPdoMapEntry = currPdoMap->mapping;
		mapEntryIndex = 1;
	}

	UNS32 pdoMappingVal = spdoMap2Val(currPdoMapEntry);
	UNS16 index = strtol(currPdoMap->snum, NULL, 0) - 1;
	if(xmlStrcasecmp(currPdoMap->stype, "rx") == 0)
	{
		index |= 0x1600;
	}
	else if(xmlStrcasecmp(currPdoMap->stype, "tx") == 0)
	{
		index |= 0x1A00;
	}

	eprintf("%s type:%s node:%2.2x index:0x%04x/%x  map:0x%lx\n", __FUNCTION__, currPdoMap->stype, nodeId, index, mapEntryIndex, pdoMappingVal);

	res = writeNetworkDictCallBack (d, /*CO_Data* d*/
		nodeId, 	/*UNS8 nodeId*/
		index, 		/*UNS16 index*/
		mapEntryIndex, 	/*UNS8 subindex*/
		4, 		/*UNS8 count*/
		0, 		/*UNS8 dataType*/
		&pdoMappingVal,	/*void *data*/
		CheckWriteInfoPdoMap, /*SDOCallback_t Callback*/0); /* use block mode */
	if(res)
	{
		eprintf("%s ERROR, writeNetworkDictCallback %d 0x%x",__FUNCTION__, res, res);
		exit(-6);
	}

	currPdoMapEntry = currPdoMapEntry->next;
	mapEntryIndex++;

	while(currPdoMapEntry == NULL)
	{ // go through maps until we find one with a non-null mapping
		currPdoMap = currPdoMap->nextPdoMap;
		if(currPdoMap == NULL)
		{ // if there are no more maps, we are done with pdo mappings
			currPdoMapEntry = NULL;
			return;

		}
		currPdoMapEntry = currPdoMap->mapping;
		mapEntryIndex = 1;
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

	/* Finalise last SDO transfer with this node */
	closeSDOtransfer(d, nodeId, SDO_CLIENT);

	csn_PdoMapping(d, nodeId);
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
		exit(-7);
	}

	/* Finalise last SDO transfer with this node */
	closeSDOtransfer(d, nodeId, SDO_CLIENT);

	csn_ObjDict(d, nodeId);
}

void csn_ObjDict(CO_Data* d, UNS8 nodeId )
{
	static UNS8 mapEntryIndex = 0;
	UNS8 res;
	struct s_pdo* spdo;
	UNS8 count = 1;
	if(currObjDict == NULL)
	{
		printf("%s no remaining Object Dictionary entries to configure\n", __FUNCTION__);
		/* Ask slave node to go in operational mode */
		masterSendNMTstateChange (d, nodeId, NMT_Start_Node);
		eprintf("Slave to operational mode \n");
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


void CheckReadInfoSDORxPdo(CO_Data* d, UNS8 nodeid)
{
	UNS32 abortCode;
	UNS32 size=sizeof(pdoCobId);

	{
		/* Display data received */
		switch(pdoInfoStep)
		{
		case 1:
			if(getReadResultNetworkDict(d, nodeid, &pdoCobId, &size, &abortCode) != SDO_FINISHED)
			{
				printf("%s: Failed in getting information for slave %2.2x, AbortCode :%4.4x \n", __FUNCTION__, nodeid, abortCode);
				closeSDOtransfer(d, nodeid, SDO_CLIENT);
				return;
			}
			printf("\nrxpdo%d pdoCobId    : 0x%x\n", pdoIndex, pdoCobId);
		break;
		default:
			if(getWriteResultNetworkDict(d, nodeid, &abortCode) != SDO_FINISHED)
			{
				printf("%s: WARNING failed to set info for slave %2.2x, AbortCode :%4.4x \n", __FUNCTION__, nodeid, abortCode);
			}
		break;
		}
	}
	/* Finalize last SDO transfer with this node */
	closeSDOtransfer(d, nodeid, SDO_CLIENT);
	csn_RxPdo(d, nodeid, pdoIndex );
}


void csn_RxPdo(CO_Data* d, UNS8 nodeId, int index)
{
	UNS8 res;
	struct s_pdo* spdo;

	nodeManagment* node = lookupNode( nodeId);
	if(node == NULL)
	{
		eprintf("%s node 0x%02x not found\n",__FUNCTION__,nodeId);
		return;

	}

	struct s_slaveNode* ci = node->configInfo;
	if(ci ==NULL)
	{
		printf("%s nothing to configure, slave node\n", __FUNCTION__);
		return;
	}

	do
	{
		pdoIndex = index;
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
		printf("%s no remaining rx PDOs to configure\n", __FUNCTION__);
		currPdoMap = ci->pdoMap;
		csn_PdoMapping(d,nodeId);
		return;
	}


	while(1)
	{
		switch(++pdoInfoStep)
		{
		case 1:	// read existing cobid
			readNetworkDictCallback(d, nodeId, 0x1400+index-1, 0x01, 0, CheckReadInfoSDORxPdo, 0);
			return;
		break;
		case 2:	// disable tpdo in cobid
		{
			UNS32 RPDO_COBId = 0x80000000 | pdoCobId;
			eprintf("sdoRxPdo: disable slave %2.2x RPDO %d  %lx\n", nodeId, index, RPDO_COBId);
			res = writeNetworkDictCallBack (d, /*CO_Data* d*/
				/**TestSlave_Data.bDeviceNodeId, UNS8 nodeId*/
				nodeId, /*UNS8 nodeId*/0x1400+index-1, /*UNS16 index*/0x01, /*UNS8 subindex*/
				4, /*UNS8 count*/0, /*UNS8 dataType*/
				&RPDO_COBId,/*void *data*/
				CheckReadInfoSDORxPdo, /*SDOCallback_t Callback*/0); /* use block mode */
			return;
		}
		case 3:	//setup Slave's RPDO transmission type
		{
			if( spdo->stransmission_type == NULL)
				continue;
			UNS8 transType = strtol( spdo->stransmission_type, NULL, 0);
			eprintf("sdoRxPdo: set slave %2.2x RPDO %d transmit type to 0x%02x\n", nodeId, index, transType);
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
			eprintf("sdoRxPdo: set slave %2.2x RPDO %d inhibit time to 0x%04x (%d)\n", nodeId, index, inhibitTime, inhibitTime);
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
			eprintf("sdoRxPdo: set slave %2.2x RPDO %d event timer to 0x%04x (%d)\n", nodeId, index, eventTimer, eventTimer);
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
			UNS32 cobid = pdoCobId;
			if( spdo->scobid != NULL)
			{
				cobid = strtol( spdo->scobid, NULL, 0);
			}
			eprintf("sdoRxPdo: set slave %2.2x RPDO %d cobid to 0x%08x \n", nodeId, index, cobid);
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
			pdoInfoStep = 0;
			csn_RxPdo(d, nodeId, pdoIndex+1 );
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


/********************************************************
 * ConfigureSlaveNode is responsible to
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
 * This is called first by testmaster_preOperational
 * then it called again each time a SDO exchange is
 * finished.
 ********************************************************/
static void ConfigureSlaveNode(CO_Data* d, UNS8 nodeId)
{
	UNS8 res;
	nodeManagment* node = lookupNode( nodeId);
	if(node == NULL)
	{
		eprintf("%s node 0x%02x not found\n",__FUNCTION__,nodeId);
		return;

	}

	while(1)
	{
		switch(++(node->initStep))
		{
		case 10:
		{	// heartbeat
			UNS16 heartbeatProducerTime = 1000;  // time, in milliseconds
			if( node->configInfo != NULL)
			{
				if( node->configInfo->sheartbeatTime != NULL)
					heartbeatProducerTime = strtol(node->configInfo->sheartbeatTime, NULL, 0);
			}
			eprintf("\n%s: set slave %2.2x heartbeat producer time to %d (0x%04x)\n", __FUNCTION__, nodeId, heartbeatProducerTime, heartbeatProducerTime);
			res = writeNetworkDictCallBack (d, /*CO_Data* d*/
				/**TestSlave_Data.bDeviceNodeId, UNS8 nodeId*/
					nodeId, /*UNS8 nodeId*/0x1017, /*UNS16 index*/0x00, /*UNS8 subindex*/
					2, /*UNS8 count*/0, /*UNS8 dataType*/
					&heartbeatProducerTime,/*void *data*/
					CheckSDOAndContinue, /*SDOCallback_t Callback*/0); /* use block mode */
			if(res)
			{
				eprintf("%s: Error 0x%02x returned at step %d\r\n", __FUNCTION__, res, node->initStep);
				exit(-1);
			}
			return;
		}
		break;
		case 13:
			csn_TxPdo(d,nodeId,1);  	// configure the slave node
			setState(d, Operational);		// Put the master in operational mode
			return;
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
 * This is called first by testmaster_preOperational
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
//	printf("nodeId slave=%x\n",nodeId);
	switch(++(node->initStep))
	{
// heartbeat
	case 1:
	{
		UNS16 heartbeatProducerTime = 1000;  // time, in milliseconds
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


void testmaster_preOperational(CO_Data* d)
{
	eprintf("%s primary slave node:0x%02x\n", __FUNCTION__, slaveNodeId);
	masterSendNMTstateChange(&TestMaster_Data, 0x00, NMT_Reset_Node);
//        masterSendNMTstateChange(&TestMaster_Data, slaveNodeId, NMT_Reset_Node);

}


void testmaster_operational(CO_Data* d)
{
	eprintf("%s\n", __FUNCTION__);
}


void testmaster_stopped(CO_Data* d)
{
	eprintf("%s\n", __FUNCTION__);
}


void testmaster_post_sync(CO_Data* d)
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
//	eprintf("MicroMod Digital Out: %2.2x\n",DO);
//	eprintf("MicroMod Analogue Out1: %d\n",AO1);
//	eprintf("MicroMod Analogue Out2: %d\n",AO2);
//	eprintf("MicroMod Analogue Out3: %d\n",AO3);
//	eprintf("MicroMod Analogue Out4: %d\n",AO4);
//	eprintf("MicroMod Digital In (by bit): DI1: %2.2x DI2: %2.2x DI3: %2.2x DI4: %2.2x DI5: %2.2x DI6: %2.2x DI7: %2.2x DI8: %2.2x\n",DI1,DI2,DI3,DI4,DI5,DI6,DI7,DI8);
	if(lastAI1 != AI1)
	{
		eprintf("System Voltage: %d (mV) curr: %d (mA) %3.1f (degC) \n", AI1, AI2, AI3/10.0f);
		lastAI1 = AI1;
	}
//	eprintf("MicroMod Analogue In2: %d\n", AI2);
//	eprintf("MicroMod Analogue In3: %d\n", AI3);
//	eprintf("MicroMod Analogue In4: %d\n", AI4);
//	eprintf("MicroMod Analogue In5: %d\n", AI5);
//	eprintf("MicroMod Analogue In6: %d\n", AI6);
//	eprintf("MicroMod Analogue In7: %d\n", AI7);
//	eprintf("MicroMod Analogue In8: %d\n", AI8);
}


void testmaster_post_TPDO(CO_Data* d)
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
  printf("*  TestMasterMicroMod                                        *\n");
  printf("*                                                            *\n");
  printf("*  A simple example for PC.                                  *\n");
  printf("*  A CanOpen master that control a MicroMod module:          *\n");
  printf("*  - setup module TPDO 1 transmit type                       *\n");
  printf("*  - setup module RPDO 1 transmit type                       *\n");
  printf("*  - setup module hearbeatbeat period                        *\n");
  printf("*  - disable others TPDOs                                    *\n");
  printf("*  - set state to operational                                *\n");
  printf("*  - send periodic SYNC                                      *\n");
  printf("*  - send periodic RPDO 1 to Micromod (digital output)       *\n");
  printf("*  - listen Micromod's TPDO 1 (digital input)                *\n");
  printf("*  - Mapping RPDO 1 bit per bit (digital input)              *\n");
  printf("*                                                            *\n");
  printf("*   Usage:                                                   *\n");
  printf("*   ./TestMasterMicroMod  [OPTIONS]                          *\n");
  printf("*                                                            *\n");
  printf("*   OPTIONS:                                                 *\n");
  printf("*     -l : Can library [\"libcanfestival_can_virtual.so\"]     *\n");
  printf("*                                                            *\n");
  printf("*    Slave:                                                  *\n");
  printf("*     -i : Slave Node id format [0x01 , 0x7F]                *\n");
  printf("*                                                            *\n");
  printf("*    Master:                                                 *\n");
  printf("*     -m : bus name [\"1\"]                                    *\n");
  printf("*     -M : 1M,500K,250K,125K,100K,50K,20K,10K                *\n");
  printf("*                                                            *\n");
  printf("**************************************************************\n");
}


/***************************  INIT  *****************************************/
void InitNodes(CO_Data* d, UNS32 id)
{
	/****************************** INITIALISATION MASTER *******************************/
	if(MasterBoard.baudrate){
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
	extern char *optarg;
	char *snodeid;
	char xmlFileName[MAX_FILENAME];

	while ((c = getopt(argc, argv, "-m:s:M:S:l:i:x:d:")) != EOF)
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
			sscanf(snodeid,"%x",&slaveNodeId);
		break;
		default:
			help();
			exit(1);
		}
	}
	*TestMaster_Data.bDeviceNodeId=(UNS8)0xFF;	// set to invalid
	setNodeId(&TestMaster_Data, masterNodeId);
	eprintf("Nodeid 0x%02x\n",getNodeId(&TestMaster_Data));

#if !defined(WIN32) || defined(__CYGWIN__)
	/* install signal handler for manual break */
	signal(SIGTERM, catch_signal);
	signal(SIGINT, catch_signal);
	TimerInit();
#endif

	if(debugging){ printf("\n\n\nReading and parsing <%s>\n",xmlFileName); }
	//read in test limits
	if(CP_parseConfigFile(xmlFileName))
	{
		fprintf(stderr,"ERROR unable to parse bounds file\n");
		exit(3);
	}



#ifndef NOT_USE_DYNAMIC_LOADING
	LoadCanDriver(LibraryPath);
#endif

	TestMaster_Data.heartbeatError = testmaster_heartbeatError;
	TestMaster_Data.initialisation = testmaster_initialisation;
	TestMaster_Data.preOperational = testmaster_preOperational;
	TestMaster_Data.operational = testmaster_operational;
	TestMaster_Data.stopped = testmaster_stopped;
	TestMaster_Data.post_sync = testmaster_post_sync;
	TestMaster_Data.post_TPDO = testmaster_post_TPDO;

	TestMaster_Data.post_SlaveBootup=testmaster_post_SlaveBootup;
	eprintf("Master Nodeid 0x%02x\n",getNodeId(&TestMaster_Data));

	addNodeToManagmentQ(0x66, ConfigureSlaveNode);	// built in node on some raspberry pi's
	addNodeToManagmentQ(0x41, ConfigureSlaveNode);
	addNodeToManagmentQ(0x33, ConfigureSlaveNode);
	addNodeToManagmentQ(0x22, ConfigureDisplayNode); // display device #1
	addNodeToManagmentQ(0x23, ConfigureSlaveNode); // display device #2
//	addNodeToManagmentQ(0x06, ConfigureSlaveNode);
//	addNodeToManagmentQ(0x07, ConfigureSlaveNode);


	if(!canOpen(&MasterBoard,&TestMaster_Data))
	{
		eprintf("Cannot open Master Board\n");
		goto fail_master;
	}

	// Start timer thread
	StartTimerLoop(&InitNodes);

	// wait Ctrl-C
	pause();
	eprintf("Finishing.\n");

	// Reset the slave node for next use (will stop emitting heartbeat)
	masterSendNMTstateChange (&TestMaster_Data, slaveNodeId, NMT_Reset_Node);

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

