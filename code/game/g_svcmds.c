// Copyright (C) 1999-2000 Id Software, Inc.
//

// this file holds commands that can be executed by the server console, but not remote clients

#include "g_local.h"


/*
==============================================================================

PACKET FILTERING
 

You can add or remove addresses from the filter list with:

addip <ip>
removeip <ip>

The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire class C network with "addip 192.246.40".

Removeip will only remove an address specified exactly the same way.  You cannot addip a subnet, then removeip a single host.

listip
Prints the current list of filters.

g_filterban <0 or 1>

If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game.  This is the default setting.

If 0, then only addresses matching the list will be allowed.  This lets you easily set up a private game, or a game that only allows players from your local network.


==============================================================================
*/

// extern	vmCvar_t	g_banIPs;
// extern	vmCvar_t	g_filterBan;


typedef struct ipFilter_s
{
	unsigned	mask;
	unsigned	compare;
} ipFilter_t;

#define	MAX_IPFILTERS	1024

static ipFilter_t	ipFilters[MAX_IPFILTERS];
static int			numIPFilters;

/*
=================
StringToFilter
=================
*/
static qboolean StringToFilter (char *s, ipFilter_t *f)
{
	char		num[128];
	int			i, j;
	unsigned	compare = 0;
	unsigned	mask = 0;
	byte		*c = (byte *)&compare;
	byte		*m = (byte *)&mask;

	for (i=0 ; i<4 ; i++)
	{
		if (*s < '0' || *s > '9')
		{
			G_Printf( "Bad filter address: %s\n", s );
			return qfalse;
		}

		j = 0;
		while (*s >= '0' && *s <= '9')
		{
			num[j++] = *s++;
		}
		num[j] = 0;
		c[i] = atoi(num);
		if (c[i] != 0)
			m[i] = 255;

		if (!*s)
			break;
		s++;
	}

	f->mask = mask;
	f->compare = compare;

	return qtrue;
}

/*
=================
UpdateIPBans
=================
*/
static void UpdateIPBans (void)
{
	byte	*b;
	int		i;
	char	iplist[MAX_INFO_STRING];

	*iplist = 0;
	for (i = 0 ; i < numIPFilters ; i++)
	{
		if (ipFilters[i].compare == 0xffffffff)
			continue;

		b = (byte *)&ipFilters[i].compare;
		Com_sprintf( iplist + strlen(iplist), sizeof(iplist) - strlen(iplist), 
			"%i.%i.%i.%i ", b[0], b[1], b[2], b[3]);
	}

	trap_Cvar_Set( "g_banIPs", iplist );
}

/*
=================
G_FilterPacket
=================
*/
qboolean G_FilterPacket (char *from)
{
	int			i;
	unsigned	mask = 0;
	byte		*m = (byte *)&mask;
	char		*p;

	i = 0;
	p = from;
	while (*p && i < 4) {
		while (*p >= '0' && *p <= '9') {
			m[i] = m[i]*10 + (*p - '0');
			p++;
		}
		if (!*p || *p == ':')
			break;
		i++, p++;
	}

	for (i=0 ; i<numIPFilters ; i++)
		if ( (mask & ipFilters[i].mask) == ipFilters[i].compare)
			return g_filterBan.integer != 0;

	return g_filterBan.integer == 0;
}

/*
=================
AddIP
=================
*/
static void AddIP( char *str )
{
	int		i;

	for (i = 0 ; i < numIPFilters ; i++)
		if (ipFilters[i].compare == 0xffffffff)
			break;		// free spot
	if (i == numIPFilters)
	{
		if (numIPFilters == MAX_IPFILTERS)
		{
			G_Printf ("IP filter list is full\n");
			return;
		}
		numIPFilters++;
	}
	
	if (!StringToFilter (str, &ipFilters[i]))
		ipFilters[i].compare = 0xffffffffu;

	UpdateIPBans();
}

/*
=================
G_ProcessIPBans
=================
*/
void G_ProcessIPBans(void) 
{
	char *s, *t;
	char		str[MAX_TOKEN_CHARS];

	Q_strncpyz( str, g_banIPs.string, sizeof(str) );

	for (t = s = g_banIPs.string; *t; /* */ ) {
		s = strchr(s, ' ');
		if (!s)
			break;
		while (*s == ' ')
			*s++ = 0;
		if (*t)
			AddIP( t );
		t = s;
	}
}


/*
=================
Svcmd_AddIP_f
=================
*/
void Svcmd_AddIP_f (void)
{
	char		str[MAX_TOKEN_CHARS];

	if ( trap_Argc() < 2 ) {
		G_Printf("Usage:  addip <ip-mask>\n");
		return;
	}

	trap_Argv( 1, str, sizeof( str ) );

	AddIP( str );

}

/*
=================
Svcmd_RemoveIP_f
=================
*/
void Svcmd_RemoveIP_f (void)
{
	ipFilter_t	f;
	int			i;
	char		str[MAX_TOKEN_CHARS];

	if ( trap_Argc() < 2 ) {
		G_Printf("Usage:  sv removeip <ip-mask>\n");
		return;
	}

	trap_Argv( 1, str, sizeof( str ) );

	if (!StringToFilter (str, &f))
		return;

	for (i=0 ; i<numIPFilters ; i++) {
		if (ipFilters[i].mask == f.mask	&&
			ipFilters[i].compare == f.compare) {
			ipFilters[i].compare = 0xffffffffu;
			G_Printf ("Removed.\n");

			UpdateIPBans();
			return;
		}
	}

	G_Printf ( "Didn't find %s.\n", str );
}

void Svcmd_ListIP_f( void ) 
{
	trap_SendConsoleCommand(EXEC_NOW, "g_banIPs\n");
}

/*
===================
Svcmd_EntityList_f
===================
*/
void	Svcmd_EntityList_f (void) {
	int			e;
	gentity_t		*check;

	check = g_entities+1;
	for (e = 0; e < level.num_entities ; e++, check++) {
		if ( !check->inuse ) {
			continue;
		}
		G_Printf("%3i:", e);
		switch ( check->s.eType ) {
		case ET_GENERAL:
			G_Printf("ET_GENERAL          ");
			break;
		case ET_PLAYER:
			G_Printf("ET_PLAYER           ");
			break;
		case ET_ITEM:
			G_Printf("ET_ITEM             ");
			break;
		case ET_MISSILE:
			G_Printf("ET_MISSILE          ");
			break;
		case ET_MOVER:
			G_Printf("ET_MOVER            ");
			break;
		case ET_BEAM:
			G_Printf("ET_BEAM             ");
			break;
		case ET_PORTAL:
			G_Printf("ET_PORTAL           ");
			break;
		case ET_SPEAKER:
			G_Printf("ET_SPEAKER          ");
			break;
		case ET_PUSH_TRIGGER:
			G_Printf("ET_PUSH_TRIGGER     ");
			break;
		case ET_TELEPORT_TRIGGER:
			G_Printf("ET_TELEPORT_TRIGGER ");
			break;
		case ET_INVISIBLE:
			G_Printf("ET_INVISIBLE        ");
			break;
		case ET_GRAPPLE:
			G_Printf("ET_GRAPPLE          ");
			break;
		default:
			G_Printf("%3i                 ", check->s.eType);
			break;
		}

		if ( check->classname ) {
			G_Printf("%s", check->classname);
		}
		G_Printf("\n");
	}
}

gclient_t	*ClientForString( const char *s ) {
	gclient_t	*cl;
	int			i;
	int			idnum;

	// numeric values are just slot numbers
	if ( s[0] >= '0' && s[0] <= '9' ) {
		idnum = atoi( s );
		if ( idnum < 0 || idnum >= level.maxclients ) {
			Com_Printf( "Bad client slot: %i\n", idnum );
			return NULL;
		}

		cl = &level.clients[idnum];
		if ( cl->pers.connected == CON_DISCONNECTED ) {
			G_Printf( "Client %i is not connected\n", idnum );
			return NULL;
		}
		return cl;
	}

	// check for a name match
	for ( i=0 ; i < level.maxclients ; i++ ) {
		cl = &level.clients[i];
		if ( cl->pers.connected == CON_DISCONNECTED ) {
			continue;
		}
		if ( !Q_stricmp( cl->pers.netname, s ) ) {
			return cl;
		}
	}

	G_Printf( "User %s is not on the server\n", s );

	return NULL;
}

/*
===================
Svcmd_ForceTeam_f

forceteam <player> <team>
===================
*/
void	Svcmd_ForceTeam_f( void ) {
	gclient_t	*cl;
	char		str[MAX_TOKEN_CHARS];

	// find the player
	trap_Argv( 1, str, sizeof( str ) );
	cl = ClientForString( str );
	if ( !cl ) {
		return;
	}

	// set the team
	trap_Argv( 2, str, sizeof( str ) );
	SetTeam( &g_entities[cl - level.clients], str );
}

void Svcmd_JK2gameplay_f( void )
{
	char arg1[MAX_TOKEN_CHARS];

	trap_Argv(1, arg1, sizeof(arg1));

	switch (atoi(arg1))
	{
		case VERSION_1_02:
			MV_SetGamePlay(VERSION_1_02);
			trap_SendServerCommand(-1, "print \"Gameplay changed to 1.02\n\"");
			break;
		case VERSION_1_03:
			MV_SetGamePlay(VERSION_1_03);
			trap_SendServerCommand(-1, "print \"Gameplay changed to 1.03\n\"");
			break;
		default:
		case VERSION_1_04:
			MV_SetGamePlay(VERSION_1_04);
			trap_SendServerCommand(-1, "print \"Gameplay changed to 1.04\n\"");
			break;
	}
}

char *ConcatArgs( int start );

void Svcmd_Say_f( void )
{
	if (g_dedicated.integer)
	{
		trap_SendServerCommand(-1, va("print \"%s[%sServer%s]%s %s\n\"", LM_SYMBOL_COLOR, LM_TEXT_COLOR, LM_SYMBOL_COLOR, LM_TEXT_COLOR, ConcatArgs(1)));
	}
}

//[Attano] - ConsoleCommand was an ugly mess before. More tidy now.
static const lmRconCommands_t rconCommand[] = {
	{ "addbot",		 1, "<name> <skill> <team> <delay> <altname>", "Add a bot onto the server",										 Svcmd_AddBot_f		 },
	{ "addip",		 1, "<ip-mask>",							   "Blacklist an IP from the server",								 Svcmd_AddIP_f		 },
	{ "botlist",	 0, "",										   "Prints a list of all bots installed on the server",				 Svcmd_BotList_f	 },
	{ "entitylist",  0, "",										   "Prints a list of all entities on the server and their types",	 Svcmd_EntityList_f  },
	{ "forceteam",	 1, "<client>",								   "Force a player onto another team",								 Svcmd_ForceTeam_f	 },
	{ "say",		 1, "<message>",							   "Broadcasts a message into the console of all players.",			 Svcmd_Say_f		 },
	{ "game_memory", 0, "",										   "Prints how much game memory is allocated out of the total pool", Svcmd_GameMem_f	 },
	{ "listip",		 0, "",										   "Prints the entire IP blacklist",								 Svcmd_ListIP_f		 },
	{ "removeip",	 1, "<ip-mask>",							   "Remove a banned IP from the blacklist",							 Svcmd_RemoveIP_f	 },
	
	// Going to assume it is set to debug-only for a reason.
	#if _DEBUG
	{ "jk2gameplay", 1, "<version>",							   "Prints how much game memory is allocated out of the total pool", Svcmd_JK2gameplay_f },
	#endif
};

rconCommandSize = sizeof(rconCommand) / sizeof(rconCommand[0]);
//[/Attano]

/*
=================
ConsoleCommand

=================
*/
qboolean ConsoleCommand( void )
{
	int		i;
	char	cmd[MAX_TOKEN_CHARS];
	const	lmRconCommands_t *command = NULL;

	trap_Argv( 0, cmd, sizeof( cmd ) );

	//[Attano] - Loop through the commands.
	for (i = 0; i < rconCommandSize; i++)
	{
		// Found a match.
		if (!Q_stricmp(rconCommand[i].cmd, cmd))
		{
			command = &rconCommand[i];
			break;
		}
	}

	if (command != NULL)
	{
		if (trap_Argc() >= command->minArgs + 1)
		{
			// Sufficient arguments supplied. Run the function.
			command->function();
			return qtrue;
		}
		else
		{
			G_Printf("%s[%sError%s]%s Insufficient arguments%s.\n", LM_SYMBOL_COLOR, LM_ERROR_COLOR, LM_SYMBOL_COLOR, LM_TEXT_COLOR, LM_SYMBOL_COLOR);

			// If guidance on usage exists ...
			if (strlen(command->description))
			{
				G_Printf("%sDescription%s: %s%s%s.\n", LM_TEXT_COLOR, LM_SYMBOL_COLOR, LM_TEXT_COLOR, command->description, LM_SYMBOL_COLOR);
			}

			if (strlen(command->usage))
			{
				G_Printf("%sUsage%s: /%s%s %s%s.\n", LM_TEXT_COLOR, LM_SYMBOL_COLOR, LM_TEXT_COLOR, command->cmd, command->usage, LM_SYMBOL_COLOR);
			}

			// No guidance found.
			if (!strlen(command->description) && !strlen(command->usage))
			{
				G_Printf("%sNo description and usage guidance exists for this command at this time%s.\n", LM_TEXT_COLOR, LM_SYMBOL_COLOR);
			}

			return qtrue;
		}
	}
	//[/Attano]

	G_Printf("%sUnknown command %s'%s%s%s'.\n", LM_TEXT_COLOR, LM_SYMBOL_COLOR, LM_TEXT_COLOR, cmd, LM_SYMBOL_COLOR);
	return qfalse;
}

