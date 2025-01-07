#include "ncList.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int ncListAllocate(List *list, int count, int size)
{
	memset(list, 0, sizeof(List));
	list->capacity = count;
	list->elemSize = size;
	list->dataSize = list->elemSize * list->capacity;
	list->nextId = 1;
	list->data = malloc(list->dataSize);
	if (list->data == NULL)
		return 0;
	memset(list->data, 0, list->dataSize);
	return list->dataSize;
}

void ncListRelease(List *list)
{
	if (list->data != NULL)
		free(list->data);
	memset(list, 0, sizeof(List));
}

int ncListGetNewID(List *list) {
	return list->nextId++;
}

static ListItem *ncListGetFreeRecord(List *list)
{
	if (list != NULL && list->data != NULL && list->length < list->capacity)
	{
		ListItem *pItem = (ListItem *)list->data;
		for (int i = 0; i < list->capacity; i++)
		{
			if (pItem->id == 0)
				return pItem;
			pItem = (ListItem *)((uint8_t *)pItem + list->elemSize);
		}
	}
	return NULL;
}

int ncListCheckNameUsed(List *list, const char *name)
{
	ListItem *pItem = (ListItem *)list->head;
	for (;;)
	{
		if (pItem == NULL)
			return 0;
		if (strcasecmp(name, pItem->name) == 0)
			return 1;
		pItem = pItem->next;
	}
}

ListItem *ncListAdd(List *list, const char *name)
{
	ListItem *pItem = ncListGetFreeRecord(list);
	if (pItem != NULL)
	{
		memset(pItem, 0, list->elemSize);
		pItem->id = ncListGetNewID(list);
		strcpy(pItem->name, name);
		pItem->next = (ListItem *)list->head;
		list->head = pItem;
		list->length++;
		if (list->maxLength < list->length)
			list->maxLength = list->length;
		if (list->addItemCallback != NULL)
			list->addItemCallback(pItem);
	}
	return pItem;
}

ListItem * ncListFindById(List *list, int id)
{
	ListItem *pItem;
	for (pItem = list->head; pItem != NULL && pItem->id != id;
			pItem = pItem->next) {
	}
	return pItem;
}

int ncListDelete(List *list, ListItem *pDelete)
{
	int found = 0;

	if (list->head == pDelete) {
		list->head = pDelete->next;
		found = 1;
	}
	else
	{
		for (ListItem *pItem = list->head; pItem != NULL && found == 0; pItem = pItem->next)
		{
			if (pItem->next == pDelete) {
				pItem->next = pDelete->next;
				found = 1;
			}
		}
	}
	if (found == 1)
	{
		if (list->delItemCallback != NULL)
			list->delItemCallback(pDelete);
		list->length--;
		memset(pDelete, 0, list->elemSize);
	}
	return found;
}

void ncListEnum(List *list, void (*callback)(ListItem *item))
{
	if (callback == NULL)
		return;
	for (ListItem *pItem = (ListItem *)list->head; pItem != NULL; pItem = pItem->next)
		callback(pItem);
}
