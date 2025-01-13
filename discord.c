#include "toyserver.h"
#include <stdlib.h>
#include <curl/curl.h>
#include <pthread.h>

struct Notif
{
	char *content;
	char *embedTitle;
	char *embedText;
};
typedef struct Notif Notif;

extern char DiscordWebhook[256];

static int writeJsonString(char *json, const char *s)
{
	if (s == NULL)
		return sprintf(json, "null");

	char *j = json;
	*j++ = '"';
	for (; *s != '\0'; s++) {
		switch (*s)
		{
		case '"':
			*j++ = '\\';
			*j++ = '"';
			break;
		case '\n':
			*j++ = '\\';
			*j++ = 'n';
			break;
		default:
			*j++ = *s;
		}
	}
	*j++ = '"';
	*j++ = '\0';

	return j - json - 1;
}

static void freeNotif(Notif *notif)
{
	free(notif->content);
	free(notif->embedTitle);
	free(notif->embedText);
	free(notif);
}

static void *postWebhookThread(void *arg)
{
	Notif *notif = (Notif *)arg;
	CURL *curl = curl_easy_init();
	if (curl == NULL) {
		ncLogPrintf(2, "Can't create curl handle");
		freeNotif(notif);
		return NULL;
	}
	CURLcode res;
	curl_easy_setopt(curl, CURLOPT_URL, DiscordWebhook);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "DCNet-DiscordWebhook");
	struct curl_slist *headers = curl_slist_append(NULL, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	char msg[1024];
	int n;
	n = sprintf(msg, "{ \"content\": ");
	n += writeJsonString(msg + n, notif->content);
	msg[n++] = ',';
	msg[n++] = ' ';

	n += sprintf(msg + n, "\"embeds\": [ "
			"{ \"author\": { \"name\": \"Toy Racer\", "
				"\"icon_url\": \"https://cdn.thegamesdb.net/images/thumb/boxart/front/19336-1.jpg\" }, "
			  "\"title\": ");
	n += writeJsonString(msg + n, notif->embedTitle);
	n += sprintf(msg + n, ", \"description\": ");
	n += writeJsonString(msg + n, notif->embedText);
	n += sprintf(msg + n, ", \"color\": 9118205 } ] }");
	//printf("%s\n", msg);

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, msg);

	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		ncLogPrintf(2, "curl error: %d", res);
	}
	else
	{
		long code;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
		if (code < 200 || code >= 300)
			ncLogPrintf(2, "Discord error: %d", code);
	}
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	freeNotif(notif);
	return NULL;
}

static void postWebhook(Notif *notif)
{
	if (DiscordWebhook[0] == '\0')
		return;
	pthread_attr_t threadAttr;
	if (pthread_attr_init(&threadAttr)) {
		ncLogPrintf(2, "pthread_attr_init() error");
		freeNotif(notif);
		return;
	}
	if (pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED)) {
		ncLogPrintf(2, "pthread_attr_setdetachstate() error");
		freeNotif(notif);
		return;
	}
    pthread_t thread;
    if (pthread_create(&thread, &threadAttr, postWebhookThread, notif)) {
    	ncLogPrintf(2, "pthread_create() error");
		freeNotif(notif);
    	return;
    }
}

static void clientList(Client *client, void *arg)
{
	char *list = (char *)arg;
	strcat(list, client->listItem.name);
	strcat(list, "\n");
}

void discordGameCreated(GameRoom *gameRoom)
{
	Notif *notif = (Notif *)calloc(1, sizeof(Notif));
	char content[128];
	sprintf(content, "Player **%s** created a game on track **TRACK %d**", gameRoom->players[0]->listItem.name,
			(gameRoom->gameParams[3] & 0x7f) + 1);
	notif->content = strdup(content);
	notif->embedTitle = strdup("Connected Players");
	char list[1024];
	list[0] = '\0';
	ncServerEnumClients(clientList, list);
	notif->embedText = strdup(list);

	postWebhook(notif);
}

void discordRaceStart(GameRoom *gameRoom)
{
	Notif *notif = (Notif *)calloc(1, sizeof(Notif));
	notif->content = strdup("");
	char title[128];
	sprintf(title, "TRACK %d: Race Start", (gameRoom->gameParams[3] & 0x7f) + 1);
	notif->embedTitle = strdup(title);
	char text[1024];
	text[0] = '\0';
	for (unsigned i = 0; i < gameRoom->playerCount; i++)
	{
		strcat(text, gameRoom->players[i]->listItem.name);
		strcat(text, " (");
		strcat(text, TrCars[gameRoom->playerParams[i]]);
		char *p = text + strlen(text) - 1;
		while (*p == ' ')
			*p-- = '\0';
		strcat(text, ")\n");
	}
	notif->embedText = strdup(text);

	postWebhook(notif);
}
