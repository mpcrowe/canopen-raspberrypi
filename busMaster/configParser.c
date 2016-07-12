/*
This file based on an example from CanFestival, a library implementing CanOpen Stack.
*/

/*----------------------------------------------------------------------------
*        Headers
*----------------------------------------------------------------------------*/
#include "configParser.h"
nodeManagment* lookupNode(uint8_t nodeId);
nodeManagment* addNodeToManagmentQ(uint8_t nodeId,  void (*configureSlaveNode)(CO_Data* d, UNS8 nodeId));

/*----------------------------------------------------------------------------
*        Local variables
*----------------------------------------------------------------------------*/
char configLib[MAX_LIBNAME];
char busName[MAX_BUSNAME];
char baudRate[MAX_BUSNAME];

/*----------------------------------------------------------------------------
*        Local functions
*----------------------------------------------------------------------------*/
static void ConfigureSlaveNode(CO_Data* d, UNS8 nodeId);
UNS8 getNodeId(CO_Data* d);

// parses the document fragment <can_fest> for canfestival specific parameters
void cp_parseCanFest(xmlTextReaderPtr reader);
// parses the document fragment <can_fest> for canfestival specific parameters
struct s_pdo* cp_parsePdoNode(xmlTextReaderPtr reader);
void cp_dumpSlaveNode(struct s_slaveNode* ssn);

// parses the document fragment <can_fest> for canfestival specific parameters
struct s_slaveNode* cp_parseSlaveNode(xmlTextReaderPtr reader);

// parses the document fragment <can_fest> for canfestival specific parameters
void cp_parseSlaves(xmlTextReaderPtr reader);
struct s_pdo_map_entry* cp_parsePdoMapEntryNode(xmlTextReaderPtr reader);

/*----------------------------------------------------------------------------
*        Local functions
*----------------------------------------------------------------------------*/

xmlChar* cp_getTextInNode(xmlTextReaderPtr reader)
{
	xmlChar* value;
	if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT)
		return(NULL);
	int ret = xmlTextReaderRead(reader);
	while (ret == 1)
	{
		if( xmlTextReaderNodeType(reader)== XML_READER_TYPE_TEXT)
		{
			value = xmlTextReaderValue(reader);
			return(value);
		}
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT)
			return(NULL);
		ret = xmlTextReaderRead(reader);
	}
	return(NULL);
}


// parses the document fragment <can_fest> for canfestival specific parameters
void cp_parseCanFest(xmlTextReaderPtr reader)
{
	while(1)
	{
		xmlTextReaderRead(reader);
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT)
		{
			char* nodeName = (char*)xmlTextReaderName(reader);

			if( xmlStrEqual(nodeName, "can_fest") )
			{
				return;
			}
			continue;
		 }
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_SIGNIFICANT_WHITESPACE)
			continue;
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_COMMENT) continue;


		char* nodeName = (char*)xmlTextReaderName(reader);
		char* str = (char*)cp_getTextInNode(reader);

		if(debugging){ printf("name: (%s) val: (%s)\n",nodeName, str); }
		if( xmlStrEqual(nodeName, "lib") )
		{
			strncpy(configLib, str, MAX_LIBNAME);
			LibraryPath = configLib;
		}
		else if( xmlStrEqual(nodeName, "interface") )
		{
			strncpy(busName, str, MAX_BUSNAME);
			MasterBoard.busname = busName;
		}
		else if( xmlStrEqual(nodeName, "baud") )
		{
			strncpy(baudRate, str, MAX_BUSNAME);
			MasterBoard.baudrate = baudRate;
		}
	}
}


// parses the document fragment <pso> for canfestival specific parameters
struct s_pdo* cp_parsePdoNode(xmlTextReaderPtr reader)
{
	struct s_pdo* newPdo = NULL;
	xmlChar* type = xmlTextReaderGetAttribute(reader, "type");
	xmlChar* num = xmlTextReaderGetAttribute(reader, "num");
	if( (type == NULL) || (num == NULL))
	{
		printf("WARNING: pdo element found without 'type' or 'num' attribute, ignoring\n");
		return(newPdo);
	}
	if(debugging){ printf("pdo attribute type: (%s) num: (%s)\n",type,num); }
	newPdo = (struct s_pdo*)malloc(sizeof(struct s_pdo));
	if( newPdo == NULL)
	{
		exit(-5);
	}
	memset(newPdo,0,sizeof(struct s_pdo));
	newPdo->stype = type;
	newPdo->snum = num;

	while(1)
	{
		xmlTextReaderRead(reader);
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT)
		{
			char* nodeName = (char*)xmlTextReaderName(reader);

			if( xmlStrEqual(nodeName, "pdo") )
			{
				return(newPdo);
			}
			continue;
		 }
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_SIGNIFICANT_WHITESPACE)
			continue;
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_COMMENT)
			continue;


		xmlChar* nodeName = xmlTextReaderName(reader);
		xmlChar* str = cp_getTextInNode(reader);

		if(debugging){ printf("pdo info name: (%s) val: (%s)\n",nodeName, str); }
		if( xmlStrEqual(nodeName, "cobid") )
		{
			newPdo->scobid = str;
		}
		else if( xmlStrEqual(nodeName, "transmission_type") )
		{
			newPdo->stransmission_type = str;
		}
		else if( xmlStrEqual(nodeName, "inhibit_time") )
		{
			newPdo->sinhibit_time = str;
		}
		else if( xmlStrEqual(nodeName, "event_timer") )
		{
			newPdo->sevent_timer = str;
		}
	}
}


// parses the document fragment <pdo_map> for canfestival specific parameters
struct s_pdo_map* cp_parsePdoMapNode(xmlTextReaderPtr reader)
{
	struct s_pdo_map* newPdoMap = NULL;
	xmlChar* type = xmlTextReaderGetAttribute(reader, "type");
	xmlChar* num = xmlTextReaderGetAttribute(reader, "num");
	if( (type == NULL) || (num == NULL))
	{
		printf("WARNING: pdo element found without 'type' or 'num' attribute, ignoring\n");
		return(newPdoMap);
	}
	if(debugging){ printf("pdo map attribute type: (%s) num: (%s)\n",type,num); }
	newPdoMap = (struct s_pdo_map*)malloc(sizeof(struct s_pdo_map));
	if( newPdoMap == NULL)
	{
		exit(-5);
	}
	memset(newPdoMap,0,sizeof(struct s_pdo_map));
	newPdoMap->stype = type;
	newPdoMap->snum = num;
	struct s_pdo_map_entry**next = &(newPdoMap->mapping);

	while(1)
	{
		xmlTextReaderRead(reader);
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT)
		{
			char* nodeName = (char*)xmlTextReaderName(reader);

			if( xmlStrEqual(nodeName, "pdo_map") )
			{
				return(newPdoMap);
			}
			continue;
		 }
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_SIGNIFICANT_WHITESPACE)
			continue;
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_COMMENT)
			continue;

		xmlChar* nodeName = xmlTextReaderName(reader);

		if(debugging){ printf("pdo map info name: (%s)\n",nodeName); }
		if( xmlStrEqual(nodeName, "map") )
		{
			struct s_pdo_map_entry* newEntry = cp_parsePdoMapEntryNode(reader);
			if(newEntry)
			{
				*next = newEntry;
				next = &(newEntry->next);
			}
		}
	}
}


// parses the document fragment <map> for pdo mapping specific parameters
struct s_pdo_map_entry* cp_parsePdoMapEntryNode(xmlTextReaderPtr reader)
{
	struct s_pdo_map_entry* newPdoMapEntry = NULL;
	newPdoMapEntry = (struct s_pdo_map_entry*)malloc(sizeof(struct s_pdo_map_entry));
	if( newPdoMapEntry == NULL)
	{
		exit(-5);
	}
	memset(newPdoMapEntry,0,sizeof(struct s_pdo_map_entry));

	while(1)
	{
		xmlTextReaderRead(reader);
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT)
		{
			char* nodeName = (char*)xmlTextReaderName(reader);

			if( xmlStrEqual(nodeName, "map") )
			{
				return(newPdoMapEntry);
			}
			continue;
		 }
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_SIGNIFICANT_WHITESPACE)
			continue;
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_COMMENT)
			continue;


		xmlChar* nodeName = xmlTextReaderName(reader);
		xmlChar* str = cp_getTextInNode(reader);

		if(debugging){ printf("pdo map info name: (%s) val: (%s)\n",nodeName, str); }
		if( xmlStrEqual(nodeName, "index") )
		{
			newPdoMapEntry->sindex = str;
		}
		else if( xmlStrEqual(nodeName, "subindex") )
		{
			newPdoMapEntry->ssub = str;
		}
		else if( xmlStrEqual(nodeName, "num_bits") )
		{
			newPdoMapEntry->snumBits = str;
		}
	}
}

#define NEW_INSTANCE(type, name) struct type* name = (struct type*)malloc(sizeof(struct type));\
        if( name == NULL) exit(-5); \
        memset(name,0,sizeof(struct type));
                                

// parses the document fragment <obj_dict> for pdo mapping specific parameters
struct s_obj_dict* cp_parseObjDictNode(xmlTextReaderPtr reader)
{
//	struct s_obj_dict* newObjDict = (struct s_obj_dict*)malloc(sizeof(struct s_obj_dict));
//	if( newObjDict == NULL)
//		exit(-5);
//	memset(newObjDict,0,sizeof(struct s_obj_dict));
	NEW_INSTANCE(s_obj_dict, newObjDict);

	while(1)
	{
		xmlTextReaderRead(reader);
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT)
		{
			char* nodeName = (char*)xmlTextReaderName(reader);
			if( xmlStrEqual(nodeName, "obj_dict") )
			{
				return(newObjDict);
			}
			continue;
		 }
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_SIGNIFICANT_WHITESPACE)
			continue;
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_COMMENT)
			continue;


		xmlChar* nodeName = xmlTextReaderName(reader);
		xmlChar* str = cp_getTextInNode(reader);

		if(debugging){ printf("obj_dict info name: (%s) val: (%s)\n",nodeName, str); }

		if( xmlStrEqual(nodeName, "index") )
		{
			newObjDict->sindex = str;
		}
		else if( xmlStrEqual(nodeName, "subindex") )
		{
			newObjDict->ssub = str;
		}
		else if( xmlStrEqual(nodeName, "type") )
		{
			newObjDict->stype = str;
		}
		else if( xmlStrEqual(nodeName, "val") )
		{
			newObjDict->sval = str;
		}
	}
}


void dumpSlaveNode(struct s_slaveNode* ssn)
{
	int i;
	printf("slaveNode id(%s) name(%s)", ssn->sid, ssn->sname);
	printf("  nodeid(%s) hb(%s) \n", ssn->snodeId, ssn->sheartbeatTime);
	for(i=0; i<4; i++)
	{
		if(ssn->txPdo[i] != NULL)
		{
			printf("txpdo%d (%s) (%s) (%s)", i+1, ssn->txPdo[i]->stype,ssn->txPdo[i]->snum,ssn->txPdo[i]->scobid );
			printf("  (%s) (%s) (%s)\n", ssn->txPdo[i]->stransmission_type, ssn->txPdo[i]->sinhibit_time, ssn->txPdo[i]->sevent_timer );
		}
	}
	for(i=0; i<4; i++)
	{
		if(ssn->rxPdo[i] != NULL)
		{
			printf("rxpdo%d (%s) (%s) (%s)", i+1, ssn->txPdo[i]->stype,ssn->txPdo[i]->snum,ssn->txPdo[i]->scobid );
			printf("  (%s) (%s) (%s)\n", ssn->txPdo[i]->stransmission_type, ssn->txPdo[i]->sinhibit_time, ssn->txPdo[i]->sevent_timer );
		}
	}
	printf("\n");
}

// parses the document fragment <node> for canfestival specific parameters
struct s_slaveNode* cp_parseSlaveNode(xmlTextReaderPtr reader)
{
	struct s_slaveNode* newSN = (struct s_slaveNode*)malloc(sizeof(struct s_slaveNode));
	if( newSN == NULL)
	{
		exit(-5);
	}
	memset(newSN,0,sizeof(struct s_slaveNode));
	newSN->sid = xmlTextReaderGetAttribute(reader, "id");
	newSN->pdoMap = NULL;
	while(1)
	{

		xmlTextReaderRead(reader);
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT)
		{
			char* nodeName = (char*)xmlTextReaderName(reader);

			if( xmlStrEqual(nodeName, "node") )
			{
				return(newSN);
			}
			continue;
		 }
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_SIGNIFICANT_WHITESPACE)
			continue;
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_COMMENT)
			continue;

		xmlChar* nodeName = xmlTextReaderName(reader);
		if( xmlStrEqual(nodeName, "pdo"))
		{
			struct s_pdo* spdo = cp_parsePdoNode(reader);
			if(spdo)
			{
				int num = strtol(spdo->snum, NULL, 0);
				if(num < 1 || num > 4)
				{
					printf("WARNING: pdo 'num' out of range %d ignoring\n",num);
					continue;
				}
				if(xmlStrcasecmp(spdo->stype, "rx") == 0)
				{
					newSN->rxPdo[num-1] = spdo;
				}
				else if(xmlStrcasecmp(spdo->stype, "tx") == 0)
				{
					newSN->txPdo[num-1] = spdo;
				}
				else
				{
					printf("WARNING: pdo 'type' out of range %x ignoring\n",spdo->stype);
					continue;
				}
			}

		}
		else if( xmlStrEqual(nodeName, "pdo_map"))
		{
			struct s_pdo_map* spdoMap = cp_parsePdoMapNode(reader);
			if(spdoMap)
			{
				int num = strtol(spdoMap->snum, NULL, 0);
				if(num < 1 || num > 4)
				{
					printf("WARNING: pdo 'num' out of range %d ignoring\n",num);
					continue;
				}
				if(xmlStrcasecmp(spdoMap->stype, "rx") == 0)
				{
					spdoMap->nextPdoMap = newSN->pdoMap;
					newSN->pdoMap = spdoMap;
				}
				else if(xmlStrcasecmp(spdoMap->stype, "tx") == 0)
				{
					spdoMap->nextPdoMap = newSN->pdoMap;
					newSN->pdoMap = spdoMap;
				}
				else
				{
					printf("WARNING: pdo 'type' out of range %x ignoring\n",spdoMap->stype);
					continue;
				}
			}

		}
		else if( xmlStrEqual(nodeName, "obj_dict"))
		{
			struct s_obj_dict* objDict = cp_parseObjDictNode(reader);
			if(objDict)
			{
				objDict->nextObjDict = newSN->objDict;
				newSN->objDict = objDict;
			}

		}
		else
		{
			char* str = (char*)cp_getTextInNode(reader);

			if(debugging){ printf("sn name: (%s)  val: (%s)\n",nodeName, str); }
			if( xmlStrEqual(nodeName, "name") )
			{
				newSN->sname = str;
			}
			else if( xmlStrEqual(nodeName, "nodeid") )
			{
				newSN->snodeId = str;
			}
			else if( xmlStrEqual(nodeName, "heartbeat_time") )
			{
				newSN->sheartbeatTime = str;
			}
		}
	}
}


// parses the document fragment <slave_nodes> for canfestival specific parameters
void cp_parseSlaves(xmlTextReaderPtr reader)
{
	while(1)
	{
		xmlTextReaderRead(reader);
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT)
		{
			char* nodeName = (char*)xmlTextReaderName(reader);

			if( xmlStrEqual(nodeName, "slave_nodes") )
			{
				return;
			}
			continue;
		 }
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_SIGNIFICANT_WHITESPACE)
			continue;
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_COMMENT)
			continue;


		if(debugging){ printf("%s\n", (char*)xmlTextReaderName(reader)); }
		if(xmlStrEqual(xmlTextReaderName(reader), "node") )
		{
			struct s_slaveNode* sn = cp_parseSlaveNode(reader);
			if(debugging)
				dumpSlaveNode(sn);

			if(sn->snodeId == NULL)
			{
				printf("WARNING: node config did not contain a nodeid, skiping\n");
				continue;
			}
			int nodeid = strtol(sn->snodeId, NULL, 0);
			nodeManagment* nmp = lookupNode(nodeid);
			if(nmp == NULL)
			{
				if(debugging){ printf("node not found in queue, adding for 0x%02x\n",nodeid);}

				nmp = addNodeToManagmentQ(nodeid, NULL);
			}
			nmp->configInfo = sn;

		}

	}
}


/*----------------------------------------------------------------------------
*        Exported functions
*----------------------------------------------------------------------------*/

// opens and parses a config file for two specific documenet fragments
// <can_fest> and <slave_nodes>
int CP_parseConfigFile(char *filename)
{ //this method parses the xml document and populates a linked list called boundsValueList
	int ret = 1;
	struct valueNode* lastNode;
	xmlTextReaderPtr reader = xmlNewTextReaderFilename(filename);
	if(reader == NULL)
	{
		printf("ERROR: Unable to open <%s>\n", filename);
		return(-1);
	}

	while(ret == 1)
	{
		ret = xmlTextReaderRead(reader);
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_SIGNIFICANT_WHITESPACE) continue;
		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_COMMENT) continue;

		if(xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT)
		{
			if(xmlStrEqual(xmlTextReaderName(reader), "config") )
			{
				ret = 0;
				break;
			}
			continue;
		}
		if(debugging){ printf("%s\n", (char*)xmlTextReaderName(reader)); }
		if(xmlStrEqual(xmlTextReaderName(reader), "can_fest") )
		{
			cp_parseCanFest(reader);
		}
		if(xmlStrEqual(xmlTextReaderName(reader), "slave_nodes") )
		{
			cp_parseSlaves(reader);
		}
	}  // end while
	xmlFreeTextReader(reader);
	if (ret == -1)
	{
		printf("ERROR %s : failed to parse %d\n", filename, ret);
	}
	return(ret);
}

