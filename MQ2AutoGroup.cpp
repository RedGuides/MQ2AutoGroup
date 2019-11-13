// MQ2AutoGroup.cpp : Defines the entry point for the DLL application.
//

// PLUGIN_API is only to be used for callbacks.  All existing callbacks at this time
// are shown below. Remove the ones your plugin does not use.  Always use Initialize
// and Shutdown for setup and cleanup, do NOT do it in DllMain.
// v1.00 :: Plure - 2017-07-15 Initial commit
// v1.01 :: Plure - 2017-11-09 Changed how we handle mercs
// v1.02 :: Plure - 2019-10-08 Added support for MQ2Dannet

#define PLUGIN_NAME					"MQ2AutoGroup"                // Plugin Name
#define VERSION						1.02
#define	PLUGIN_MSG					"\ag[MQ2AutoGroup]\ax "     
#define MAINTANK					1
#define MAINASSIST					2
#define PULLER						3
#define MARKNPC						4
#define MASTERLOOTER				5

#ifndef PLUGIN_API
#include "../MQ2Plugin.h"
using namespace std;
PreSetup(PLUGIN_NAME);
PLUGIN_VERSION(VERSION);
#endif PLUGIN_API

#include <vector>

bool					bLeader = false;
bool					bUseMerc = false;
bool					bSummonMerc = true;
bool					bSummonedMerc = false;
bool					bSuspendMerc = false;
bool					bUseStartCommand = false; 
bool					bGroupComplete = false;
bool					bInvitingPlayer = false; // Used to limit the number of threads for inviting eqbc/dannet spots in the group to 1 at a time
int						iHandleMerc = 0;
unsigned int			iGroupNumber = 0;
unsigned int			iNumberOfGroups = 0;
char					szMainTank[MAX_STRING] = "NoEntry";
char					szMainAssist[MAX_STRING] = "NoEntry";
char					szPuller[MAX_STRING] = "NoEntry";
char					szMarkNPC[MAX_STRING] = "NoEntry";
char					szMasterLooter[MAX_STRING] = "NoEntry";
char					szStartCommand[MAX_STRING]; 
char					szCommand[MAX_STRING];
vector <string>			vGroupNames;
vector <string>			vInviteNames;
SEARCHSPAWN				ssPCSearchCondition;


#pragma region Inlines
// Returns TRUE if character is in game and has valid character data structures
inline bool InGameOK()
{
	return(GetGameState() == GAMESTATE_INGAME && GetCharInfo() && GetCharInfo()->pSpawn && GetCharInfo2());
}

// Returns TRUE if the specified UI window is visible
inline bool WinState(CXWnd *Wnd)
{
	return (Wnd && ((PCSIDLWND)Wnd)->IsVisible());
}
#pragma endregion Inlines

// This is the region for connecting to other plugins
PMQPLUGIN Plugin(char* PluginName)
{
	long Length = strlen(PluginName) + 1;
	PMQPLUGIN pLook = pPlugins;
	while (pLook && _strnicmp(PluginName, pLook->szFilename, Length)) pLook = pLook->pNext;
	return pLook;
}

bool CheckEQBC(PCHAR szName)
{
	if (PMQPLUGIN pLook = Plugin("mq2eqbc"))
	{
		if (unsigned short(*fisConnected)() = (unsigned short(*)())GetProcAddress(pLook->hModule, "isConnected"))
		{
			if (fisConnected())
			{
				if (bool(*fAreTheyConnected)(char* szName) = (bool(*)(char* szName))GetProcAddress(pLook->hModule, "AreTheyConnected"))
				{
					if (fAreTheyConnected(szName))
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool CheckDanNet(PCHAR szName)
{
	if (PMQPLUGIN pLook = Plugin("mq2dannet"))
	{
		if (bool(*f_peer_connected)(const std::string& name) = (bool(*)(const std::string& name))GetProcAddress(pLook->hModule, "peer_connected"))
		{
			char szTemp[MAX_STRING];
			sprintf_s(szTemp, "%s_%s", EQADDR_SERVERNAME, szName);
			if (f_peer_connected(szTemp))
			{
				return true;
			}
		}
	}
	return false;
}

unsigned int FindCreateGroupIndex(void)
{
	char szTemp1[MAX_STRING];
	char szTemp2[MAX_STRING];
	char szTemp3[MAX_STRING];
	unsigned int a;
	for (a = 1; a < iNumberOfGroups + 1; a++)
	{
		sprintf_s(szTemp1, "Group%i", a);
		for (unsigned int b = 1; b < 7; b++)
		{
			sprintf_s(szTemp2, "Member%i", b);
			GetPrivateProfileString(szTemp1, szTemp2, "NoEntry", szTemp3, MAX_STRING, INIFileName);
			if (_stricmp(szTemp3, ""))
			{
				break;
			}
			if (b == 6)
			{
				return a;
			}
		}
	}
	return a;
}

unsigned int FindGroupNumber(CHAR* pszGroupEntry)
{
	char szTemp1[MAX_STRING];
	char szTemp2[MAX_STRING];
	char szTemp3[MAX_STRING];
	iNumberOfGroups = GetPrivateProfileInt("Settings", "NumberOfGroups", 0, INIFileName);
	unsigned int iGroupIndex = 0;
	for (unsigned int a = 1; a < iNumberOfGroups + 1; a++)
	{
		sprintf_s(szTemp1, "Group%i", a);
		if (GetPrivateProfileString(szTemp1, "Server", 0, szTemp2, MAX_STRING, INIFileName) != 0)
		{
			if (!_stricmp(szTemp2, EQADDR_SERVERNAME))
			{
				for (unsigned int b = 1; b < 7; b++)
				{
					sprintf_s(szTemp2, "Member%i", b);
					if (GetPrivateProfileString(szTemp1, szTemp2, 0, szTemp3, MAX_STRING, INIFileName) != 0)
					{
						if (strstr(szTemp3, pszGroupEntry))
						{
							iGroupIndex = a;
						}
					}
				}
			}
		}
	}
	return iGroupIndex;
}

void WriteGroupEntry(CHAR* pszGroupEntry, unsigned int iGroupIndex)
{
	char szTemp1[MAX_STRING];
	char szTemp2[MAX_STRING];
	char szTemp3[MAX_STRING];
	sprintf_s(szTemp1, "Group%i", iGroupIndex);
	for (unsigned int a = 1; a < 7; a++)
	{
		sprintf_s(szTemp2, "Member%i", a);
		GetPrivateProfileString(szTemp1, szTemp2, "NoEntry", szTemp3, MAX_STRING, INIFileName);
		if (!_stricmp(szTemp3, ""))
		{
			WritePrivateProfileString(szTemp1, szTemp2, pszGroupEntry, INIFileName);
			WriteChatf("%s:: \ag%s\ax has been added to group %i.", PLUGIN_MSG, pszGroupEntry, iGroupNumber);
			return;
		}
	}
	WriteChatf("%s:: Group %i doesn't have room for\ar%s\ax.", PLUGIN_MSG, iGroupNumber, pszGroupEntry);
	return;
}

void RemoveGroupEntry(CHAR* pszGroupEntry, unsigned int iGroupIndex)
{
	char szTemp1[MAX_STRING];
	char szTemp2[MAX_STRING];
	char szTemp3[MAX_STRING];
	sprintf_s(szTemp1, "Group%i", iGroupIndex);
	for (unsigned int a = 1; a < 7; a++)
	{
		sprintf_s(szTemp2, "Member%i", a);
		GetPrivateProfileString(szTemp1, szTemp2, "NoEntry", szTemp3, MAX_STRING, INIFileName);
		if (!_stricmp(szTemp3, pszGroupEntry))
		{
			WritePrivateProfileString(szTemp1, szTemp2, "", INIFileName);
			WriteChatf("%s:: \ar%s\ax has been removed from group %i.", PLUGIN_MSG, pszGroupEntry, iGroupNumber);
			return;
		}
	}
	return;
}

void RemoveGroupRole(CHAR* pszGrouprole, unsigned int iGroupIndex)
{
	char szTemp1[MAX_STRING];
	char szTemp2[MAX_STRING];
	char szTemp3[MAX_STRING];
	char szTemp4[MAX_STRING];
	sprintf_s(szTemp1, "Group%i", iGroupIndex);
	for (unsigned int a = 1; a < 7; a++)
	{
		sprintf_s(szTemp2, "Member%i", a);
		if (GetPrivateProfileString(szTemp1, szTemp2, 0, szTemp3, MAX_STRING, INIFileName))
		{
			CHAR *pParsedToken = NULL;
			CHAR *pParsedValue = strtok_s(szTemp3, "|", &pParsedToken);
			if (_stricmp(pParsedValue, pszGrouprole))
			{
				sprintf_s(szTemp4, "%s", pParsedValue);
			}
			pParsedValue = strtok_s(NULL, "|", &pParsedToken);
			while (pParsedValue != NULL)
			{
				if (_stricmp(pParsedValue, pszGrouprole))
				{
					sprintf_s(szTemp4, "%s|%s", szTemp4, pParsedValue);
				}
				pParsedValue = strtok_s(NULL, "|", &pParsedToken);
			}
			WritePrivateProfileString(szTemp1, szTemp2, szTemp4, INIFileName);
		}
	}
	return;
}

LONG SetBOOL(long Cur, PCHAR Val, PCHAR Sec, PCHAR Key, PCHAR INI)
{
	long result = 0;
	if (!_strnicmp(Val, "false", 5) || !_strnicmp(Val, "off", 3) || !_strnicmp(Val, "0", 1))
	{
		result = 0;
	}
	else if (!_strnicmp(Val, "true", 4) || !_strnicmp(Val, "on", 2) || !_strnicmp(Val, "1", 1))
	{
		result = 1;
	}
	else
	{
		result = (!Cur) & 1;
	}
	if (Sec[0] && Key[0])
	{
		WritePrivateProfileString(Sec, Key, result ? "1" : "0", INI);
	}
	return result;
}

void AutoGroupCommand(PSPAWNINFO pCHAR, PCHAR zLine)
{
	if (!InGameOK()) return;
	bool NeedHelp = false;
	char szTemp1[MAX_STRING];
	char szTemp2[MAX_STRING];
	char szTemp3[MAX_STRING];
	char szTemp4[MAX_STRING];
	char Parm1[MAX_STRING];
	char Parm2[MAX_STRING];
	char Parm3[MAX_STRING];
	GetArg(Parm1, zLine, 1);
	GetArg(Parm2, zLine, 2);
	GetArg(Parm3, zLine, 3);
	PCHARINFO pChar = GetCharInfo();

	if (!_stricmp(Parm1, "help"))
	{
		NeedHelp = true;
	}
	else if (!_stricmp(Parm1, "handlemerc"))
	{
		if (!_stricmp(Parm2, "on")) iHandleMerc = SetBOOL(iHandleMerc, Parm2, "Settings", "HandleMerc", INIFileName);
		if (!_stricmp(Parm2, "off")) iHandleMerc = SetBOOL(iHandleMerc, Parm2, "Settings", "HandleMerc", INIFileName);
		WriteChatf("%s:: Summoning/suspending merc's is turned %s", PLUGIN_MSG, iHandleMerc ? "\agON\ax" : "\arOFF\ax");
	}
	else if (!_stricmp(Parm1, "create"))
	{
		iGroupNumber = FindGroupNumber(((PCHARINFO)pCharData)->Name);
		if (iGroupNumber == 0)
		{
			iGroupNumber = FindCreateGroupIndex();
			sprintf_s(szTemp1, "Group%i", iGroupNumber);
			WritePrivateProfileString(szTemp1, "Server", EQADDR_SERVERNAME, INIFileName);
			WritePrivateProfileString(szTemp1, "Member1", ((PCHARINFO)pCharData)->Name, INIFileName);
			WritePrivateProfileString(szTemp1, "Member2", "", INIFileName);
			WritePrivateProfileString(szTemp1, "Member3", "", INIFileName);
			WritePrivateProfileString(szTemp1, "Member4", "", INIFileName);
			WritePrivateProfileString(szTemp1, "Member5", "", INIFileName);
			WritePrivateProfileString(szTemp1, "Member6", "", INIFileName);
			if (iGroupNumber == iNumberOfGroups + 1)
			{
				sprintf_s(szTemp1, "%i", iGroupNumber);
				WritePrivateProfileString("Settings", "NumberOfGroups", szTemp1, INIFileName);
			}
			WriteChatf("%s:: \agSuccessfully created group: number %i.\ax", PLUGIN_MSG, iGroupNumber);
		}
		else
		{
			WriteChatf("%s:: \agYou are in a group, you cannot be in two groups are the same time.\ax", PLUGIN_MSG);
		}
	}
	else if (!_stricmp(Parm1, "delete"))
	{
		iGroupNumber = FindGroupNumber(((PCHARINFO)pCharData)->Name);
		if (iGroupNumber > 0)
		{
			sprintf_s(szTemp1, "Group%i", iGroupNumber);
			WritePrivateProfileString(szTemp1, "Server", "", INIFileName);
			WritePrivateProfileString(szTemp1, "Member1", "", INIFileName);
			WritePrivateProfileString(szTemp1, "Member2", "", INIFileName);
			WritePrivateProfileString(szTemp1, "Member3", "", INIFileName);
			WritePrivateProfileString(szTemp1, "Member4", "", INIFileName);
			WritePrivateProfileString(szTemp1, "Member5", "", INIFileName);
			WritePrivateProfileString(szTemp1, "Member6", "", INIFileName);
			WriteChatf("%s:: \arSuccessfully deleted group: number %i.\ax", PLUGIN_MSG, iGroupNumber);
		}
		else
		{
			WriteChatf("%s:: \arYou are not in a group, you need to be in a group before you can destoy it.\ax", PLUGIN_MSG);
		}
	}
	else if (!_stricmp(Parm1, "startcommand"))
	{
		sprintf_s(szTemp1, "%s_%s", EQADDR_SERVERNAME, ((PCHARINFO)pCharData)->Name);
		WritePrivateProfileString("StartCommand", szTemp1, Parm2, INIFileName);
		WriteChatf("%s:: Setting the start command to: \ag%s\ax", PLUGIN_MSG, Parm2);
	}
	else if (!_stricmp(Parm1, "add"))
	{
		iGroupNumber = FindGroupNumber(((PCHARINFO)pCharData)->Name);
		if (iGroupNumber >= 0)
		{
			WriteChatf("%s:: \arYou are not in a group, you need to create a group or be added to a group before you can add someone to it.\ax", PLUGIN_MSG);
			return;
		}
		if (!_stricmp(Parm2, "eqbc"))
		{
			WriteGroupEntry("EQBC", iGroupNumber);
		}
		else if (!_stricmp(Parm2, "dannet"))
		{
			WriteGroupEntry("DANNET", iGroupNumber);
		}
		else if (!_stricmp(Parm2, "player") || !_stricmp(Parm2, "merc"))
		{
			if (PSPAWNINFO psTarget = (PSPAWNINFO)pTarget)
			{
				if (psTarget->Type == SPAWN_PLAYER)
				{
					if (!_stricmp(Parm2, "player"))
					{
						if (FindGroupNumber(psTarget->Name) == 0)
						{
							WriteGroupEntry(psTarget->Name, iGroupNumber);
						}
						else
						{
							WriteChatf("%s:: \ar%s\ax is already in a group, please remove them first before adding them to this group", PLUGIN_MSG, psTarget->Name);
						}
					}
					else if(!_stricmp(Parm2, "merc"))
					{
						if (FindGroupNumber(psTarget->Name) > 0)
						{
							sprintf_s(szTemp4, "Merc|%s", psTarget->Name);
							if (FindGroupNumber(szTemp4) == 0)
							{
								WriteGroupEntry(szTemp4, iGroupNumber);
								WriteChatf("%s:: A mercenary has been added to group %i for \ag%s\ax", PLUGIN_MSG, iGroupNumber, psTarget->Name);
							}
							else
							{
								WriteChatf("%s:: \ar%s\ax already has a mercenary in the group", PLUGIN_MSG, psTarget->Name);
							}
						}
						else
						{
							WriteChatf("%s:: \ar%s\ax is not in a group, please add them first before adding their mercenary", PLUGIN_MSG, psTarget->Name);
						}
					}
				}
				else
				{
					WriteChatf("%s:: You need to target a player to add them to your group", PLUGIN_MSG);
				}
			}
			else
			{
				WriteChatf("%s:: You need to target a player to add them to your group", PLUGIN_MSG);
			}
		}
		else
		{
			WriteChatf("%s:: \ar%s\ax is an invalid entry, please use /AutoGroup add [player|merc|eqbc|dannet]", PLUGIN_MSG, Parm2);
		}
	}
	else if (!_stricmp(Parm1, "set"))
	{
		if (!_stricmp(Parm2, "maintank") || !_stricmp(Parm2, "mainassist") || !_stricmp(Parm2, "puller") || !_stricmp(Parm2, "marknpc") || !_stricmp(Parm2, "masterlooter"))
		{
			iGroupNumber = 0;
			if (PSPAWNINFO psTarget = (PSPAWNINFO)pTarget)
			{
				if (psTarget && psTarget->Type == SPAWN_PLAYER)
				{
					sprintf_s(szTemp3, "%s", psTarget->Name); // szTemp3 = Name of target
					iGroupNumber = FindGroupNumber(szTemp3);
				}
				else if (psTarget && psTarget->Mercenary)
				{
					bool bInGroup = false;
					for (unsigned int a = 0; a < 6; a++)
					{
						if (pChar->pGroupInfo->pMember[a] && pChar->pGroupInfo->pMember[a]->Mercenary && pChar->pGroupInfo->pMember[a]->pSpawn->SpawnID == psTarget->SpawnID)
						{
							GetCXStr(pChar->pGroupInfo->pMember[a]->pOwner, szTemp4, MAX_STRING);  // szTemp4 = Owner name of the merc you are targeted
							bInGroup = true;
						}
					}
					if (bInGroup)
					{
						sprintf_s(szTemp3, "Merc|%s", szTemp4);  // szTemp3 = Merc|Owner Name
						iGroupNumber = FindGroupNumber(szTemp3);
					}
					else
					{
						WriteChatf("%s:: You need to be grouped with a mercenary before you can assign it a group role", PLUGIN_MSG);
						return;
					}
				}
				if (iGroupNumber > 0)
				{
					sprintf_s(szTemp1, "Group%i", iGroupNumber);
					for (unsigned int a = 1; a < 7; a++)
					{
						sprintf_s(szTemp2, "Member%i", a);
						GetPrivateProfileString(szTemp1, szTemp2, "NoEntry", szTemp4, MAX_STRING, INIFileName);
						if (strstr(szTemp4, szTemp3))
						{
							if (!_stricmp(Parm2, "maintank"))
							{
								WriteChatf("%s:: \ag%s\ax has been made \agMain Tank\ax for group %i.", PLUGIN_MSG, szTemp3, iGroupNumber);
								RemoveGroupRole("Main Tank", iGroupNumber);
								sprintf_s(szTemp3, "%s|Main Tank", szTemp4);
							}
							else if (!_stricmp(Parm2, "mainassist"))
							{
								WriteChatf("%s:: \ag%s\ax has been made \agMain Assist\ax for group %i.", PLUGIN_MSG, szTemp3, iGroupNumber);
								RemoveGroupRole("Main Assist", iGroupNumber);
								sprintf_s(szTemp3, "%s|Main Assist", szTemp4);
							}
							else if (!_stricmp(Parm2, "puller"))
							{
								WriteChatf("%s:: \ag%s\ax has been made \agPuller\ax for group %i.", PLUGIN_MSG, szTemp3, iGroupNumber);
								RemoveGroupRole("Puller", iGroupNumber);
								sprintf_s(szTemp3, "%s|Puller", szTemp4);
							}
							else if (!_stricmp(Parm2, "marknpc"))
							{
								WriteChatf("%s:: \ag%s\ax has been made \agMark NPC\ax for group %i.", PLUGIN_MSG, szTemp3, iGroupNumber);
								RemoveGroupRole("Mark NPC", iGroupNumber);
								sprintf_s(szTemp3, "%s|Mark NPC", szTemp4);
							}
							else if (!_stricmp(Parm2, "masterlooter"))
							{
								WriteChatf("%s:: \ag%s\ax has been made \agMaster Looter\ax for group %i.", PLUGIN_MSG, szTemp3, iGroupNumber);
								RemoveGroupRole("Master Looter", iGroupNumber);
								sprintf_s(szTemp3, "%s|Master Looter", szTemp4);
							}
							WritePrivateProfileString(szTemp1, szTemp2, szTemp3, INIFileName);
							return;
						}
					}
				}
				else
				{
					WriteChatf("%s:: Your target needs to be in a group to give them a role", PLUGIN_MSG);
				}
			}
			else
			{
				WriteChatf("%s:: You need to target someone to give them a role", PLUGIN_MSG);
			}
		}
		else
		{
			WriteChatf("%s:: \ar%s\ax is an invalid entry, please use /AutoGroup set [maintank|mainassist|puller|marknpc|masterlooter]", PLUGIN_MSG, Parm2);
		}
	}
	else if (!_stricmp(Parm1, "remove"))
	{
		if (!_stricmp(Parm2, "eqbc"))
		{
			iGroupNumber = FindGroupNumber(((PCHARINFO)pCharData)->Name);
			if (iGroupNumber > 0)
			{
				RemoveGroupEntry("EQBC", iGroupNumber);
			}
		}
		else if (!_stricmp(Parm2, "dannet"))
		{
			iGroupNumber = FindGroupNumber(((PCHARINFO)pCharData)->Name);
			if (iGroupNumber > 0)
			{
				RemoveGroupEntry("DANNET", iGroupNumber);
			}
		}
		else if (!_stricmp(Parm2, "player") || !_stricmp(Parm2, "merc") || !_stricmp(Parm2, "maintank") || !_stricmp(Parm2, "mainassist") || !_stricmp(Parm2, "puller") || !_stricmp(Parm2, "marknpc") || !_stricmp(Parm2, "masterlooter"))
		{
			if (PSPAWNINFO psTarget = (PSPAWNINFO)pTarget)
			{
				if (psTarget && psTarget->Type == SPAWN_PLAYER)
				{
					iGroupNumber = 0;
					if (!_stricmp(Parm2, "player"))
					{
						iGroupNumber = FindGroupNumber(psTarget->Name);
						if (iGroupNumber > 0)
						{
							RemoveGroupEntry(psTarget->Name, iGroupNumber);
						}
						else
						{
							WriteChatf("%s:: \ar%s cannot be removed from a group because they are not in a group.\ax", PLUGIN_MSG, psTarget->Name);
						}
					}
					else if (!_stricmp(Parm2, "merc"))
					{
						sprintf_s(szTemp4, "Merc|%s", psTarget->Name);
						iGroupNumber = FindGroupNumber(szTemp4);
						if (iGroupNumber > 0)
						{
							RemoveGroupEntry(szTemp4, iGroupNumber);
						}
						else
						{
							WriteChatf("%s:: \arYou cannot remove %s's mercenary because they don't have a mercenary.\ax", PLUGIN_MSG, psTarget->Name);
						}
					}
					else if (!_stricmp(Parm2, "maintank"))
					{
						sprintf_s(szTemp3, "%s", psTarget->Name); // szTemp3 = Name of target
						iGroupNumber = FindGroupNumber(szTemp3);
						if (iGroupNumber > 0)
						{
							WriteChatf("%s:: Removing \aRMain Tank\ax from group %i.", PLUGIN_MSG, iGroupNumber);
							RemoveGroupRole("Main Tank", iGroupNumber);
						}
						else
						{
							WriteChatf("%s:: Your target needs to be in a group to make them Main Tank", PLUGIN_MSG);
						}
					}
					else if (!_stricmp(Parm2, "mainassist"))
					{
						sprintf_s(szTemp3, "%s", psTarget->Name); // szTemp3 = Name of target
						iGroupNumber = FindGroupNumber(szTemp3);
						if (iGroupNumber > 0)
						{
							WriteChatf("%s:: Removing \aRMain Assist\ax from group %i.", PLUGIN_MSG, iGroupNumber);
							RemoveGroupRole("Main Assist", iGroupNumber);
						}
						else
						{
							WriteChatf("%s:: You need to target someone to make them Main Assist", PLUGIN_MSG);
						}
					}
					else if (!_stricmp(Parm2, "puller"))
					{
						sprintf_s(szTemp3, "%s", psTarget->Name); // szTemp3 = Name of target
						iGroupNumber = FindGroupNumber(szTemp3);
						if (iGroupNumber > 0)
						{
							WriteChatf("%s:: Removing \aRPuller\ax from group %i.", PLUGIN_MSG, iGroupNumber);
							RemoveGroupRole("Puller", iGroupNumber);
						}
						else
						{
							WriteChatf("%s:: Your target needs to be in a group to make them Puller", PLUGIN_MSG);
						}
					}
					else if (!_stricmp(Parm2, "marknpc"))
					{
						sprintf_s(szTemp3, "%s", psTarget->Name); // szTemp3 = Name of target
						iGroupNumber = FindGroupNumber(szTemp3);
						if (iGroupNumber > 0)
						{
							WriteChatf("%s:: Removing \aRMark NPC\ax from group %i.", PLUGIN_MSG, iGroupNumber);
							RemoveGroupRole("Mark NPC", iGroupNumber);
						}
						else
						{
							WriteChatf("%s:: Your target needs to be in a group to make them Mark NPC", PLUGIN_MSG);
						}
					}
					else if (!_stricmp(Parm2, "masterlooter"))
					{
						sprintf_s(szTemp3, "%s", psTarget->Name); // szTemp3 = Name of target
						iGroupNumber = FindGroupNumber(szTemp3);
						if (iGroupNumber > 0)
						{
							WriteChatf("%s:: Removing \aRMaster Looter\ax from group %i.", PLUGIN_MSG, iGroupNumber);
							RemoveGroupRole("Master Looter", iGroupNumber);
						}
						else
						{
							WriteChatf("%s:: Your target needs to be in a group to make them Master Looter", PLUGIN_MSG);
						}
					}
				}
				else if (psTarget && psTarget->Mercenary)
				{
					bool bInGroup = false;
					for (unsigned int a = 0; a < 6; a++)
					{
						if (pChar->pGroupInfo->pMember[a] && pChar->pGroupInfo->pMember[a]->Mercenary && pChar->pGroupInfo->pMember[a]->pSpawn->SpawnID == psTarget->SpawnID)
						{
							GetCXStr(pChar->pGroupInfo->pMember[a]->pOwner, szTemp4, MAX_STRING);  // szTemp4 = Owner name of the merc you are targeted
							bInGroup = true;
						}
					}
					if (bInGroup)
					{
						sprintf_s(szTemp3, "Merc|%s", szTemp4);  // szTemp3 = Merc|Owner Name
						iGroupNumber = FindGroupNumber(szTemp3);
						if (iGroupNumber > 0)
						{
							if (!_stricmp(Parm2, "maintank"))
							{
								WriteChatf("%s:: Removing \aRMain Tank\ax from group %i.", PLUGIN_MSG, iGroupNumber);
								RemoveGroupRole("Main Tank", iGroupNumber);
							}
							else if (!_stricmp(Parm2, "mainassist"))
							{
								WriteChatf("%s:: Removing \aRMain Assist\ax from group %i.", PLUGIN_MSG, iGroupNumber);
								RemoveGroupRole("Main Assist", iGroupNumber);
							}
							else if (!_stricmp(Parm2, "puller"))
							{
								WriteChatf("%s:: Removing \aRPuller\ax from group %i.", PLUGIN_MSG, iGroupNumber);
								RemoveGroupRole("Puller", iGroupNumber);
							}
							else if (!_stricmp(Parm2, "marknpc"))
							{
								WriteChatf("%s:: Removing \aRMark NPC\ax from group %i.", PLUGIN_MSG, iGroupNumber);
								RemoveGroupRole("Mark NPC", iGroupNumber);
							}
							else if (!_stricmp(Parm2, "masterlooter"))
							{
								WriteChatf("%s:: Removing \aRMaster Looter\ax from group %i.", PLUGIN_MSG, iGroupNumber);
								RemoveGroupRole("Master Looter", iGroupNumber);
							}
						}
						else
						{
							WriteChatf("%s:: Your target needs to be in a group to remove a role", PLUGIN_MSG);
						}
					}
					else
					{
						WriteChatf("%s:: You need to be grouped with a mercenary before you can assign it a group role", PLUGIN_MSG);
						return;
					}
				}
				else
				{
					WriteChatf("%s:: You need to target a player or mercenary", PLUGIN_MSG);
				}
			}
			else
			{
				WriteChatf("%s:: You need to target a player or mercenary", PLUGIN_MSG);
			}
		}
		else
		{
			WriteChatf("%s:: \ar%s\ax is an invalid entry, please use /AutoGroup remove [player|merc|eqbc|dannet|maintank|mainassist|puller|marknpc|masterlooter]", PLUGIN_MSG, Parm2);
		}
	}
	else if (!_stricmp(Parm1, "status"))
	{
		sprintf_s(szTemp1, "%s_%s", EQADDR_SERVERNAME, ((PCHARINFO)pCharData)->Name);
		GetPrivateProfileString("StartCommand", szTemp1, "NoEntry", szTemp2, MAX_STRING, INIFileName);
		WriteChatf("%s:: My command to run once the group is formed is: \ag%s\ax", PLUGIN_MSG,szTemp2);
		iGroupNumber = FindGroupNumber(((PCHARINFO)pCharData)->Name);
		if (iGroupNumber > 0)
		{
			sprintf_s(szTemp1, "Group%i", iGroupNumber);
			GetPrivateProfileString(szTemp1, "Member1", "NoEntry", szTemp2, MAX_STRING, INIFileName);
			WriteChatf("%s:: Group member 1 is: \ag%s\ax", PLUGIN_MSG, szTemp2);
			GetPrivateProfileString(szTemp1, "Member2", "NoEntry", szTemp2, MAX_STRING, INIFileName);
			WriteChatf("%s:: Group member 2 is: \ag%s\ax", PLUGIN_MSG, szTemp2);
			GetPrivateProfileString(szTemp1, "Member3", "NoEntry", szTemp2, MAX_STRING, INIFileName);
			WriteChatf("%s:: Group member 3 is: \ag%s\ax", PLUGIN_MSG, szTemp2);
			GetPrivateProfileString(szTemp1, "Member4", "NoEntry", szTemp2, MAX_STRING, INIFileName);
			WriteChatf("%s:: Group member 4 is: \ag%s\ax", PLUGIN_MSG, szTemp2);
			GetPrivateProfileString(szTemp1, "Member5", "NoEntry", szTemp2, MAX_STRING, INIFileName);
			WriteChatf("%s:: Group member 5 is: \ag%s\ax", PLUGIN_MSG, szTemp2);
			GetPrivateProfileString(szTemp1, "Member6", "NoEntry", szTemp2, MAX_STRING, INIFileName);
			WriteChatf("%s:: Group member 6 is: \ag%s\ax", PLUGIN_MSG, szTemp2);
		}
		else
		{
			WriteChatf("%s:: You are not a member of a group!", PLUGIN_MSG);
		}
	}
	else if (!_stricmp(Parm1, "test"))
	{
		WriteChatf("%s:: GetCharInfo()->pSpawn->MercID = \ag%i\ax", PLUGIN_MSG, GetCharInfo()->pSpawn->MercID);
	}
	else
	{
		NeedHelp = true;
	}
	if (NeedHelp) {
		WriteChatColor("Usage:");
		WriteChatColor("/AutoGroup handlemerc [on|off] -> Toggle suspending/summoning merc's if your in a group");
		WriteChatColor("/AutoGroup create -> Create a new group, this player will be the leader.");
		WriteChatColor("/AutoGroup delete -> Will delete the group this player was in.");
		WriteChatColor("/AutoGroup startcommand \"Command to be executed\" -> Will execute the command once your group is formed.");
		WriteChatColor("/AutoGroup set [maintank|mainassist|puller|marknpc|masterlooter] -> Will set the player/merc targeted to that group role.");
		WriteChatColor("/AutoGroup remove [maintank|mainassist|puller|marknpc|masterlooter] -> Will remove that group role from your group.");
		WriteChatColor("/AutoGroup add [player|merc] -> Add the player/merc of the player targeted to your group.");
		WriteChatColor("/AutoGroup remove [player|merc] -> Remove the player/merc of the player targeted from their group.");
		WriteChatColor("/AutoGroup add [eqbc|dannet] -> Will give the option to invite characters connected to eqbc/dannet to your group.");
		WriteChatColor("/AutoGroup remove [eqbc|dannet] -> Will remove the the option to invite characters connected to eqbc/dannet.");
		WriteChatColor("/AutoGroup status -> Displays your settings and group.");
		WriteChatColor("/AutoGroup help");
	}
}

DWORD __stdcall InviteEQBCorDanNetToons(PVOID pData)
{
	CHAR szTemp1[MAX_STRING];
	CHAR szTemp2[MAX_STRING];
	sprintf_s(szTemp1, "%s", (PCHAR)pData);
	Sleep(10000); // 10 seconds
	sprintf_s(szCommand, "/invite %s", szTemp1);
	DoCommand(GetCharInfo()->pSpawn, szCommand);
	Sleep(5000); // 5 seconds to let them get the group invite
	if (CheckEQBC(szTemp1))
	{
		sprintf_s(szCommand, "/bct %s //invite", szTemp1);
	}
	else if (CheckDanNet(szTemp1))
	{
		sprintf_s(szCommand, "/dexecute %s /invite", szTemp1);
	}
	DoCommand(GetCharInfo()->pSpawn, szCommand);
	Sleep(5000); // 5 seconds wait 5 seconds for them to join the group so we don't try and invite them again
	if (vGroupNames.size())
	{
		for (register unsigned int a = 0; a < vGroupNames.size(); a++)
		{
			strcpy_s(szTemp2, vGroupNames[a].c_str());
			if (!_stricmp(szTemp2, "EQBC") || !_stricmp(szTemp2, "DANNET"))
			{
				vGroupNames.erase(vGroupNames.begin() + a);
				bInvitingPlayer = false;
				return 0;
			}
		}
	}
	bInvitingPlayer = false;
	return 0;
}

DWORD __stdcall InviteToons(PVOID pData)
{
	CHAR szTemp1[MAX_STRING];
	sprintf_s(szTemp1, "%s", (PCHAR)pData);
	Sleep(10000); // 10 seconds
	sprintf_s(szCommand, "/invite %s", szTemp1);
	DoCommand(GetCharInfo()->pSpawn, szCommand);
	return 0;
}

// Called once, when the plugin is to initialize
PLUGIN_API VOID InitializePlugin(VOID)
{
	// commands
	AddCommand("/autogroup", AutoGroupCommand);
}

// Called once, when the plugin is to shutdown
PLUGIN_API VOID ShutdownPlugin(VOID)
{
	// remove commands
	RemoveCommand("/autogroup");
}

// Called once directly after initialization, and then every time the gamestate changes
PLUGIN_API VOID SetGameState(DWORD GameState)
{
	if (!InGameOK()) return;
	char szTemp1[MAX_STRING];
	char szTemp2[MAX_STRING];
	char szTemp3[MAX_STRING];
	char szTemp4[MAX_STRING];
	vGroupNames.clear();
	vInviteNames.clear();
	ClearSearchSpawn(&ssPCSearchCondition);
	ssPCSearchCondition.SpawnType = PC;

	sprintf_s(INIFileName, "%s\\%s.ini", gszINIPath, PLUGIN_NAME);
	if (GetPrivateProfileInt("Settings", "Version", 0, INIFileName) != VERSION)
	{
		char Version[MAX_STRING] = { 0 };
		sprintf_s(Version, "%1.2f", VERSION);
		WritePrivateProfileString("Settings", "Version", Version, INIFileName);
	}
	iHandleMerc = GetPrivateProfileInt("Settings", "HandleMerc", -1, INIFileName);
	if (iHandleMerc == -1)
	{
		iHandleMerc = 1;
		WritePrivateProfileString("Settings", "HandleMerc", "1", INIFileName);
	}
	iNumberOfGroups = GetPrivateProfileInt("Settings", "NumberOfGroups", 0, INIFileName);
	if (iNumberOfGroups == 0)
	{
		WritePrivateProfileString("Settings", "NumberOfGroups", "0", INIFileName);
	}

	sprintf_s(szTemp1, "%s_%s", EQADDR_SERVERNAME, ((PCHARINFO)pCharData)->Name);
	if (GetPrivateProfileString("StartCommand", szTemp1, 0, szStartCommand, MAX_STRING, INIFileName) != 0)
	{
		bUseStartCommand = true;
		WriteChatf("%s:: I want to use the start command: \ag%s\ax", PLUGIN_MSG, szStartCommand);
	}

	iGroupNumber = 0;
	for (unsigned int a = 1; a < iNumberOfGroups + 1; a++)
	{
		sprintf_s(szTemp1, "Group%i", a);
		if (GetPrivateProfileString(szTemp1, "Server", 0, szTemp2, MAX_STRING, INIFileName) != 0)
		{
			if (!_stricmp(szTemp2, EQADDR_SERVERNAME))
			{
				for (unsigned int b = 1; b < 7; b++)
				{
					sprintf_s(szTemp2, "Member%i", b);
					if (GetPrivateProfileString(szTemp1, szTemp2, 0, szTemp3, MAX_STRING, INIFileName) != 0)
					{
						if (strstr(szTemp3, "Merc") && strstr(szTemp3, ((PCHARINFO)pCharData)->Name))
						{
							bUseMerc = true;
							WriteChatf("%s:: \arI am to use a mercenary\ax", PLUGIN_MSG);
						}
						else if (strstr(szTemp3, ((PCHARINFO)pCharData)->Name))
						{
							iGroupNumber = a;
							bGroupComplete = false;
							if (b == 1)
							{
								bLeader = true;
							}
						}
					}
				}
			}
		}
	}

	if (iGroupNumber > 0)
	{
		sprintf_s(szTemp1, "Group%i", iGroupNumber);
		for (unsigned int b = 1; b < 7; b++)
		{
			sprintf_s(szTemp2, "Member%i", b);
			if (GetPrivateProfileString(szTemp1, szTemp2, 0, szTemp3, MAX_STRING, INIFileName) != 0)
			{
				CHAR *pParsedToken = NULL;
				CHAR *pParsedValue = strtok_s(szTemp3, "|", &pParsedToken);
				if (!_stricmp(pParsedValue, "Merc"))
				{
					sprintf_s(szTemp4, "%s", pParsedValue);
					pParsedValue = strtok_s(NULL, "|", &pParsedToken);
					if (pParsedValue == NULL)
					{
						WriteChatf("%s:: Whoa friend, your group has a merc without an owner.  Please edit MQ2AutoGroup.ini to give it an owner.", PLUGIN_MSG);
					}
					else
					{
						sprintf_s(szTemp4, "%s|%s", szTemp4, pParsedValue);
					}
				}
				else
				{
					sprintf_s(szTemp4, "%s", szTemp3);
				}
				if (strstr(szTemp4, "Merc"))
				{
					vGroupNames.push_back(szTemp4);
				}
				else 
				{
					vGroupNames.push_back(szTemp4);
					vInviteNames.push_back(szTemp4);
				}
				pParsedValue = strtok_s(NULL, "|", &pParsedToken);
				while (pParsedValue != NULL)
				{
					if (!_stricmp(pParsedValue, "Main Tank"))
					{
						sprintf_s(szMainTank, "%s", szTemp4);
						WriteChatf("%s:: The main tank is \ag%s\ax", PLUGIN_MSG, szTemp4);
					}
					else if (!_stricmp(pParsedValue, "Main Assist"))
					{
						sprintf_s(szMainAssist, "%s", szTemp4);
						WriteChatf("%s:: The main assist is \ag%s\ax", PLUGIN_MSG, szTemp4);
					}
					else if (!_stricmp(pParsedValue, "Puller"))
					{
						sprintf_s(szPuller, "%s", szTemp4);
						WriteChatf("%s:: The puller is \ag%s\ax", PLUGIN_MSG, szTemp4);
					}
					else if (!_stricmp(pParsedValue, "Mark NPC"))
					{
						sprintf_s(szMarkNPC, "%s", szTemp4);
						WriteChatf("%s:: The mark npc is \ag%s\ax", PLUGIN_MSG, szTemp4);
					}
					else if (!_stricmp(pParsedValue, "Master Looter"))
					{
						sprintf_s(szMasterLooter, "%s", szTemp4);
						WriteChatf("%s:: The master looter is \ag%s\ax", PLUGIN_MSG, szTemp4);
					}
					pParsedValue = strtok_s(NULL, "|", &pParsedToken);
				}
			}
		}
	}

	// Ok so you are in full group, and you aren't going to use a merc.  I'm going to turn on merc suspension
	if (vGroupNames.size() == 6 && !bUseMerc)
	{
		bSuspendMerc = true;
	}
}

// Will click the confirmation box for joining groups when you have a merc up it
bool CheckConfirmationWindow(void)
{
	if (CXWnd *pWnd = (CXWnd *)FindMQ2Window("ConfirmationDialogBox"))
	{
		if (((PCSIDLWND)(pWnd))->IsVisible())
		{
			if (CStmlWnd*Child = (CStmlWnd*)pWnd->GetChildItem("CD_TextOutput"))
			{
				char ConfirmationText[MAX_STRING];
				GetCXStr(Child->STMLText, ConfirmationText, sizeof(ConfirmationText));
				if (strstr(ConfirmationText, "If you accept"))
				{
					if (strstr(ConfirmationText, "you and your group members will lose"))
					{
						if (CXWnd *pWndButton = pWnd->GetChildItem("CD_Yes_Button"))
						{
							SendWndClick2(pWndButton, "leftmouseup");
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

// If you want the plugin to suspend/summon your merc this is called to setup your group 
bool HandleMercs(PCHARINFO pChar)
{
	if (iHandleMerc)
	{
		// Ok I'm going to summon a merc if you are suppose to use one
		if (!bSummonedMerc)
		{
			if (bUseMerc)
			{
				if (pChar->pSpawn->MercID == 0)
				{
					if (CXWnd *pWnd = FindMQ2Window("MMGW_ManageWnd"))
					{
						if (CXWnd *pWndButton = pWnd->GetChildItem("MMGW_SuspendButton"))
						{
							if (pWndButton->IsEnabled())
							{
								bSummonedMerc = true;
								WriteChatf("%s:: Summoning mercenary.", PLUGIN_MSG);
								SendWndClick2(pWndButton, "leftmouseup");
								return true;
							}
						}
					}
					else if (bSummonMerc)
					{
						DoCommand(GetCharInfo()->pSpawn, "/merc");
						bSummonMerc = false;
					}
				}
			}
		}
		// Ok your not suppose to use a merc, i'm going to suspend them
		if (bSuspendMerc)
		{
			if (GetCharInfo()->pSpawn->MercID > 0)
			{
				if (CXWnd *pWnd = FindMQ2Window("MMGW_ManageWnd"))
				{
					if (CXWnd *pWndButton = pWnd->GetChildItem("MMGW_SuspendButton"))
					{
						if (pWndButton->IsEnabled())
						{
							bSuspendMerc = false;
							WriteChatf("%s:: Dismissing mercenary.", PLUGIN_MSG);
							SendWndClick2(pWndButton, "leftmouseup");
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

// The leader will invite people and give them roles based on the plugin's ini file
bool SetupGroup(PCHARINFO pChar)
{
	if (bLeader)
	{
		if (pChar->pGroupInfo && pChar->pGroupInfo->pMember && pChar->pGroupInfo->pMember[0])
		{
			if (pChar->pGroupInfo->pLeader && pChar->pGroupInfo->pLeader->pSpawn && pChar->pGroupInfo->pLeader->pSpawn->SpawnID)
			{
				if (pChar->pGroupInfo->pLeader->pSpawn->SpawnID != pChar->pSpawn->SpawnID)
				{
					bLeader = false; // Hey you aren't the leader, I am turning off leader stuff
					return true;
				}
			}
			else
			{
				bLeader = false; // Hey you aren't the leader, I am turning off leader stuff
				return true;
			}
		}
		char szTemp1[MAX_STRING];
		char szTemp2[MAX_STRING];
		if (pChar->pGroupInfo && pChar->pGroupInfo->pMember && pChar->pGroupInfo->pMember[0])
		{
			if (pChar->pGroupInfo->pLeader->pSpawn->SpawnID == pChar->pSpawn->SpawnID)
			{
				for (LONG k = 0; k < 6; k++)
				{
					if (pChar->pGroupInfo->pMember[k] && pChar->pGroupInfo->pMember[k]->Mercenary)
					{
						GetCXStr(pChar->pGroupInfo->pMember[k]->pOwner, szTemp2, MAX_STRING);
						sprintf_s(szTemp1, "Merc|%s", szTemp2);
						GetCXStr(pChar->pGroupInfo->pMember[k]->pName, szTemp2, MAX_STRING);
					}
					else if (pChar->pGroupInfo->pMember[k])
					{
						GetCXStr(pChar->pGroupInfo->pMember[k]->pName, szTemp1, MAX_STRING);
						GetCXStr(pChar->pGroupInfo->pMember[k]->pName, szTemp2, MAX_STRING);
					}
					if (!_stricmp(szMainTank, szTemp1))
					{
						sprintf_s(szCommand, "/grouproles set %s 1", szTemp2);
						WriteChatf("%s:: Setting \ag%s\ax to be main tank", PLUGIN_MSG, szTemp2);
						DoCommand(GetCharInfo()->pSpawn, szCommand);
						sprintf_s(szMainTank, "Done");
					}
					else if (!_stricmp(szMainAssist, szTemp1))
					{
						sprintf_s(szCommand, "/grouproles set %s 2", szTemp2);
						WriteChatf("%s:: Setting \ag%s\ax to be main assist", PLUGIN_MSG, szTemp2);
						DoCommand(GetCharInfo()->pSpawn, szCommand);
						sprintf_s(szMainAssist, "Done");
					}
					else if (!_stricmp(szPuller, szTemp1))
					{
						sprintf_s(szCommand, "/grouproles set %s 3", szTemp2);
						WriteChatf("%s:: Setting \ag%s\ax to be puller", PLUGIN_MSG, szTemp2);
						DoCommand(GetCharInfo()->pSpawn, szCommand);
						sprintf_s(szPuller, "Done");
					}
					else if (!_stricmp(szMarkNPC, szTemp1))
					{
						sprintf_s(szCommand, "/grouproles set %s 4", szTemp2);
						WriteChatf("%s:: Setting \ag%s\ax to be mark npc", PLUGIN_MSG, szTemp2);
						DoCommand(GetCharInfo()->pSpawn, szCommand);
						sprintf_s(szMarkNPC, "Done");
					}
					else if (!_stricmp(szMasterLooter, szTemp1))
					{
						sprintf_s(szCommand, "/grouproles set %s 5", szTemp2);
						WriteChatf("%s:: Setting \ag%s\ax to be master looter", PLUGIN_MSG, szTemp2);
						DoCommand(GetCharInfo()->pSpawn, szCommand);
						sprintf_s(szMasterLooter, "Done");
					}
				}
			}
		}
		if (vInviteNames.size())
		{
			int	iGroupMembers = 0;
			for (register unsigned int a = 0; a < 6; a++)
			{
				if (pChar->pGroupInfo && pChar->pGroupInfo->pMember && pChar->pGroupInfo->pMember[a])
				{
					iGroupMembers++;
				}
			}
			if (iGroupMembers < 6)
			{
				for (register unsigned int a = 0; a < vInviteNames.size(); a++)
				{
					strcpy_s(szTemp1, vInviteNames[a].c_str());
					if (!_stricmp(szTemp1, ((PCHARINFO)pCharData)->Name))
					{
						vInviteNames.erase(vInviteNames.begin() + a);
						return true;
					}
					else if (!_stricmp(szTemp1, "EQBC") || !_stricmp(szTemp1, "DANNET"))
					{
						if (!bInvitingPlayer)
						{
							unsigned int iSpawnCount = CountMatchingSpawns(&ssPCSearchCondition, GetCharInfo()->pSpawn, false);
							if (iSpawnCount)
							{
								for (unsigned int s = 1; s <= iSpawnCount; s++)
								{
									if (PSPAWNINFO pNewSpawn = NthNearestSpawn(&ssPCSearchCondition, s, GetCharInfo()->pSpawn, false))
									{
										sprintf_s(szTemp2, "%s", pNewSpawn->Name);
										if (CheckEQBC(szTemp2) || CheckDanNet(szTemp2))
										{
											bool bInGroup = false;
											for (register unsigned int b = 1; b < 6; b++)
											{
												if (pChar->pGroupInfo && pChar->pGroupInfo->pMember && pChar->pGroupInfo->pMember[b] && pChar->pGroupInfo->pMember[b]->pSpawn && pChar->pGroupInfo->pMember[b]->pSpawn->SpawnID)
												{
													if (pNewSpawn->SpawnID == pChar->pGroupInfo->pMember[b]->pSpawn->SpawnID)
													{
														bInGroup = true;
													}
												}
											}
											if (!bInGroup)
											{
												bInvitingPlayer = true;
												DWORD nThreadID = 0;
												CreateThread(NULL, NULL, InviteEQBCorDanNetToons, _strdup(szTemp2), 0, &nThreadID);
												vInviteNames.erase(vInviteNames.begin() + a);
												return true;
											}
										}
									}
								}
							}
						}
					}
					else if (PSPAWNINFO pNewSpawn = (PSPAWNINFO)GetSpawnByName(szTemp1))
					{
						DWORD nThreadID = 0;
						CreateThread(NULL, NULL, InviteToons, _strdup(szTemp1), 0, &nThreadID);
						vInviteNames.erase(vInviteNames.begin() + a);
						return true;
					}
				}
			}
		}
	}
	return false;
}

// Lets check when the group is complete so this plugin can run the start command and then stop this plugin from doing stuff
void CheckGroup(PCHARINFO pChar)
{
	char szTemp1[MAX_STRING];
	char szTemp2[MAX_STRING];
	if (pChar->pGroupInfo && pChar->pGroupInfo->pMember && pChar->pGroupInfo->pMember[0])
	{
		int	iGroupMembers = 0;
		for (register unsigned int a = 0; a < 6; a++)
		{
			if (pChar->pGroupInfo && pChar->pGroupInfo->pMember && pChar->pGroupInfo->pMember[a])
			{
				iGroupMembers++;
			}
		}
		if (vGroupNames.size() && iGroupMembers < 6)
		{
			for (register unsigned int a = 0; a < vGroupNames.size(); a++)
			{
				strcpy_s(szTemp1, vGroupNames[a].c_str());
				CHAR *pParsedToken = NULL;
				CHAR *pParsedValue = strtok_s(szTemp1, "|", &pParsedToken);
				if (!_stricmp(pParsedValue, "Merc"))
				{
					pParsedValue = strtok_s(NULL, "|", &pParsedToken);
					if (pParsedValue == NULL)
					{
						WriteChatf("%s:: Whoa friend, your group has a merc without an owner.  Please edit MQ2AutoGroup.ini to give it an owner.", PLUGIN_MSG);
						vGroupNames.erase(vGroupNames.begin() + a);
					}
					else
					{
						for (LONG k = 0; k < 6; k++)
						{
							if (pChar->pGroupInfo->pMember[k] && pChar->pGroupInfo->pMember[k]->Mercenary)
							{
								GetCXStr(pChar->pGroupInfo->pMember[k]->pOwner, szTemp2, MAX_STRING);
								if (!_stricmp(pParsedValue, szTemp2))
								{
									WriteChatf("%s:: The mercenary owned by \ag%s\ax is in the group!", PLUGIN_MSG, szTemp2);
									vGroupNames.erase(vGroupNames.begin() + a);
								}
							}
						}
					}
				}
				else
				{
					for (LONG k = 0; k < 6; k++)
					{
						if (pChar->pGroupInfo->pMember[k] && pChar->pGroupInfo->pMember[k]->pName)
						{
							GetCXStr(pChar->pGroupInfo->pMember[k]->pName, szTemp2, MAX_STRING);
							if (!_stricmp(szTemp1, szTemp2))
							{
								WriteChatf("%s:: \ag%s\ax is in the group!", PLUGIN_MSG, szTemp2);
								vGroupNames.erase(vGroupNames.begin() + a);
							}
						}
					}
				}
			}
		}
		else if (bUseStartCommand)
		{
			WriteChatf("%s:: Group is formed! Running: \ag%s\ax", PLUGIN_MSG, szStartCommand);
			DoCommand(GetCharInfo()->pSpawn, szStartCommand);
			bUseStartCommand = false;
		}
		else
		{
			bGroupComplete = true;
		}
	}
}

// This is called every time MQ pulses
PLUGIN_API VOID OnPulse(VOID)
{
	if (!InGameOK()) return;
	if (bGroupComplete) return;
	PCHARINFO pChar = GetCharInfo();
	// Will click the confirmation box for joining groups when you have a merc up it
	if (CheckConfirmationWindow())  // Ok we clicked the confirmation box, lets exit this pulse
	{
		return;
	}
	// If you want the plugin to suspend/summon your merc this is called to setup your group
	if (HandleMercs(pChar)) // Ok we did some merc action, lets exit this pulse
	{
		return;
	}
	// The leader will invite people and give them roles based on the plugin's ini file
	if (SetupGroup(pChar)) // Ok the leader made some action, lets exit this pulse
	{
		return;
	}
	// Lets check when the group is complete so this plugin can run the start command and then stop this plugin from doing stuff
	CheckGroup(pChar);
}

// This is called every time EQ shows a line of chat with CEverQuest::dsp_chat,
// but after MQ filters and chat events are taken care of.
PLUGIN_API DWORD OnIncomingChat(PCHAR Line, DWORD Color)
{
	if (!InGameOK()) return 0;
	CHAR szName[MAX_STRING];
	if (strstr(Line, "invites you to join a group.")) 
	{
		GetArg(szName, Line, 1);
		for (unsigned int a = 0; a < vGroupNames.size(); a++) 
		{
			string& vRef = vGroupNames[a];
			if (!_strcmpi(szName, vRef.c_str())) 
			{
				DoCommand(GetCharInfo()->pSpawn, "/timed 50 /invite");
				WriteChatf("%s:: Joining group with \ag%s\ax", PLUGIN_MSG, szName);
			}
		}
	}
    return 0;
}
