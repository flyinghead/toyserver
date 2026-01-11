#include "toyserver.h"
#include <dcserver/discord.h>
#include <dcserver/status.h>
#include <stdlib.h>
#include <unistd.h>

extern char DCNetGameId[];

static void clientList(Client *client, void *arg)
{
	char *list = (char *)arg;
	char *name = discordEscape(client->listItem.name);
	strcat(list, name);
	free(name);
	strcat(list, "\n");
}

void discordGameCreated(GameRoom *gameRoom)
{
	char content[128];
	char *name = discordEscape(gameRoom->players[0]->listItem.name);
	sprintf(content, "Player **%s** created a game on track **TRACK %d**", name,
			(gameRoom->gameParams[3] & 0x7f) + 1);
	free(name);
	char list[1024];
	list[0] = '\0';
	ncServerEnumClients(clientList, list);

	discordNotif("toyracer", content, "Connected Players", list);
}

void discordRaceStart(GameRoom *gameRoom)
{
	if (gameRoom->playerCount <= 1)
		return;
	char title[128];
	sprintf(title, "TRACK %d: Race Start", (gameRoom->gameParams[3] & 0x7f) + 1);
	char text[1024];
	text[0] = '\0';
	for (unsigned i = 0; i < gameRoom->playerCount; i++)
	{
		char *name = discordEscape(gameRoom->players[i]->listItem.name);
		strcat(text, name);
		free(name);
		strcat(text, " (");
		strcat(text, TrCars[gameRoom->playerParams[i]]);
		char *p = text + strlen(text) - 1;
		while (*p == ' ')
			*p-- = '\0';
		strcat(text, ")\n");
	}

	discordNotif("toyracer", "", title, text);
}

static void counter(void *unused, void *counter) {
	*(int *)counter += 1;
}

void updateStatus()
{
	static time_t lastUpdate;
	// Update every 5 min (default)
	if (TimerRef - lastUpdate < statusGetInterval() * 1000)
		return;
	lastUpdate = TimerRef;

	int playerCount = 0;
	ncServerEnumClients((void (*)(Client *, void *))counter, &playerCount);
	int gameCount = 0;
	ncServerEnumGameRooms((void (*)(GameRoom *, void *))counter, &gameCount);
	statusUpdate(DCNetGameId, playerCount, gameCount);
	statusCommit(DCNetGameId);
}
