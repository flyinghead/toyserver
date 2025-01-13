time_t TimerRef;

int MaxNbGames = 512;
int MaxNbPlayers = 1024;
int MaxNbChats = 512;

int ncServerUdpSocket = -1;
uint16_t ncServerUdpPort;

int ncNbUdpSent;
int ncNbUdpReceived;
int ncNbUdpLost;
int MaxUdpMsgSize;
int ncNbTcpSent;

char DiscordWebhook[256];

const char *Languages[] = {
	"English",
	"French",
	"German",
	"Spanish",
	"Italian",
};
const char *LangNames[] = { "en", "fr", "ge", "sp", "it" };

const char *TrCars[] = {
	"Pickup    ",
	"Pickup    ",
	"VBL       ",
	"VBL       ",
	"Willys    ",
	"Willys    ",
	"F1        ",
	"F1        ",
	"WRC       ",
	"WRC       ",
	"Buggy     ",
	"Buggy     ",
	"Tiger     ",
	"Tiger     ",
	"Sherman   ",
	"Sherman   ",
	"Bulldozer ",
	"Bulldozer ",
	"Taxi      ",
	"Taxi      ",
	"Combi     ",
	"Combi     ",
	"Transport ",
	"Transport ",
	"ToothMobil",
	"ToothMobil",
	"Trakthor  ",
	"Trakthor  ",
};

const char *StatusName[] = {
	"Available",
	"Disconnecting",
	"Connected",
	"Chating",
	"Game room",
	"Ready",
	"Loading",
	"Waiting",
	"Playing",
	"Ending",
};

const char *GameStatus[] = {
	"Choosing",
	"Loading",
	"Running",
	"Results",
};
