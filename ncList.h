#pragma once

struct ListItem {
    struct ListItem *next;
    int id;
    char name[16];
};
typedef struct ListItem ListItem;

typedef void (*ListItemCallback)(ListItem *item);

struct List {
    void *data;
    ListItem *head;
    int nextId;
    int length;
    int maxLength;
    int capacity;
    int elemSize;
    int dataSize;
    ListItemCallback delItemCallback;
    ListItemCallback addItemCallback;
};
typedef struct List List;

int ncListAllocate(List *list, int count, int size);
void ncListRelease(List *list);
int ncListGetNewID(List *list);
int ncListCheckNameUsed(List *list, const char *name);
ListItem *ncListAdd(List *list, const char *name);
ListItem *ncListFindById(List *list, int id);
int ncListDelete(List *list, ListItem *pDelete);
void ncListEnum(List *list, ListItemCallback callback);
