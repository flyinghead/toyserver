#pragma once
#include <stdint.h>

struct NetMsg {
    short msgType;
    uint16_t size;
    int msgId;
    uint32_t crc;
};
typedef struct NetMsg NetMsg;

// Login
struct NetMsgT1 {
    struct NetMsg head;
    char userName[16];
    int unk1;
    uint16_t udpPort;
    uint8_t language;
    uint8_t cli_unk5;
    int udpTimeout;
    short dynPackPerSec;
    short cmdPackPerSec;
};
typedef struct NetMsgT1 NetMsgT1;

// Ping
struct NetMsgT2 {
    struct NetMsg head;
    int timer;
};
typedef struct NetMsgT2 NetMsgT2;

// High scores
struct NetMsgT7 {
    struct NetMsg head;
    int highScores[8];
    char racerNames[8][16];
    char racerCars[8];
    uint8_t unk[168];
    char racerCountries[8][10];
};
typedef struct NetMsgT7 NetMsgT7;

// Create chat room
struct NetMsgT8 {
    struct NetMsg head;
    char roomName[16];
};
typedef struct NetMsgT8 NetMsgT8;

// Chat room created
struct NetMsgT9 {
    struct NetMsg head;
    char roomName[16];
    uint16_t language;
    uint16_t unk;
};
typedef struct NetMsgT9 NetMsgT9;

// ChatInfo line
struct NetMsgT14 {
    struct NetMsg head;
    char message[256];
};
typedef struct NetMsgT14 NetMsgT14;

// Chat room info
struct NetMsgT15 {
    struct NetMsg head;
    uint16_t userCount;
    uint16_t language;
    char userNames[16][16];
    int userIds[16];
};
typedef struct NetMsgT15 NetMsgT15;

// Chat room list
struct NetMsgT16 {
    struct NetMsg head;
    int count;
    char chatNames[8][16];
    int chatIds[8];
    uint16_t languages[8];
    uint8_t userCount[8];
};
typedef struct NetMsgT16 NetMsgT16;

// Create game
struct NetMsgT18 {
    struct NetMsg head;
    char gameName[16];
    uint8_t unk1;
    uint8_t maxPlayers;
    uint8_t unk2;
    uint8_t unk3;
    uint8_t gameParams[4];
};
typedef struct NetMsgT18 NetMsgT18;

// Game created
struct NetMsgT19 {
    struct NetMsg head;
    char gameName[16];
    uint8_t language;
    uint8_t maxPlayers;
    uint16_t unk5_3;
    int gameParams;
};
typedef struct NetMsgT19 NetMsgT19;

// Player joined game
struct NetMsgT20 {
    struct NetMsg head;
    char playerName[16];
};
typedef struct NetMsgT20 NetMsgT20;

// Game info
struct NetMsgT22Player {
    int clientId;
    char name[16];
    uint8_t results[16];
    uint8_t playerParam;
    uint8_t playerReady;
    uint16_t unk;
};
struct NetMsgT22 {
    struct NetMsg head;
    uint8_t language;
    uint8_t maxPlayers;
    uint8_t playerCount;
    uint8_t gameStatus;
    uint8_t gameParams[4];
    struct NetMsgT22Player players[4];
};
typedef struct NetMsgT22 NetMsgT22;

// Game list
struct NetMsgT26 {
    struct NetMsg head;
    int count;
    char gameNames[8][16];
    int gameIds[8];
    uint16_t maxPlayers_gameStatus[8];
    uint8_t playerCounts[8];
    uint8_t gameParams[8][4];
};
typedef struct NetMsgT26 NetMsgT26;

// Set game params
struct NetMsgT27 {
    struct NetMsg head;
    uint8_t gameParams[4];
    uint8_t maxPlayers;
    uint8_t unk[3];
};
typedef struct NetMsgT27 NetMsgT27;

// Set game player params
struct NetMsgT28 {
    struct NetMsg head;
    uint8_t playerParams;
};
typedef struct NetMsgT28 NetMsgT28;

// Game loading
struct NetMsgT30 {
    struct NetMsg head;
    short unk1;
    uint8_t nplayers;
    uint8_t unk2;
    int gameParams;
};
typedef struct NetMsgT30 NetMsgT30;

// Game status changed
struct NetMsgT33 {
    struct NetMsg head;
    uint8_t status;
    uint8_t padding[3];
};
typedef struct NetMsgT33 NetMsgT33;

// Game player results
struct NetMsgT34 {
    struct NetMsg head;
    int unk;
    uint8_t results[16];
};
typedef struct NetMsgT34 NetMsgT34;

// In game UDP update
struct NetMsgT36 {
    struct NetMsg head;
    int raceId;
    int udpSequence;
    // data...
};
typedef struct NetMsgT36 NetMsgT36;
