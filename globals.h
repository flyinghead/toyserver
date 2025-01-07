ChatInfo tmpInfoChat;

Stats TotalStats;
Stats DailyStats;

HighScores TrHighScores;
BestLap TrBestLaps[14][8];

int TimerRef;
uint32_t PrecTimer_lo;
uint32_t PrecTimer_hi;
int FirstTimer;

uint16_t Tcp_Port = 2048;
uint16_t Udp_Port = 2049;
int ProgramTerminated;
int MaxNbGames = 512;
int MaxNbPlayers = 1024;
int MaxNbChats = 512;

short NbCommandPaquetsPerSec = 8;
short NbDynamixPaquetsPerSec = 6;
int UdpTimeout = 5000;
int ClientUdpTimeout = 7000;

int ToyServerFPS;
unsigned NbFrames;
unsigned NbFrames_hi;
int MinFps = 99999999;
int MaxFps;
int Nb;

int ncServerUdpSocket = -1;
uint16_t ncServerUdpPort;

int ncNbUdpSent;
int ncNbUdpReceived;
int ncNbUdpLost;
int MaxUdpMsgSize;
int ncNbTcpSent;

const char *Languages[] = {
	"English",
	"French",
	"German",
	"Spanish",
	"Italian",
};
const char *LangNames[] = { "en", "fr", "ge", "sp", "it" };

int LastDailyDay;

int chatRoomCount;

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
