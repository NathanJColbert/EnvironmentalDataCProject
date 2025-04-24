#ifndef DATA_LIST_H
#define DATA_LIST_H
#include <mysql/mysql.h>

struct dataValue {
	double temperature;
	double humidity;
	MYSQL_TIME time;
};
typedef struct dataValue DataValue;
typedef struct DataNode {
    DataValue data;
    struct DataNode *next;
} DataNode;

DataNode* createDataNode(DataValue *data) {
    DataNode *newNode = (DataNode*)malloc(sizeof(DataNode));
    if (newNode == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }
    newNode->data = *data;
    newNode->next = NULL;
    return newNode;
}
void appendDataNode(DataNode **head, DataValue *data) {
    DataNode *newNode = createDataNode(data);
    if (newNode == NULL) return;

    if (*head == NULL) {
        *head = newNode;
    } else {
        DataNode *temp = *head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = newNode;
    }
}
void freeDataList(DataNode *head) {
    DataNode *current = head;
    DataNode *nextNode;

    while (current != NULL) {
        nextNode = current->next;
        free(current);
        current = nextNode;
    }
}

int compareTimestamps(MYSQL_TIME time1, MYSQL_TIME time2) {
    if (time1.year != time2.year) return time1.year - time2.year;
    if (time1.month != time2.month) return time1.month - time2.month;
    if (time1.day != time2.day) return time1.day - time2.day;
    if (time1.hour != time2.hour) return time1.hour - time2.hour;
    if (time1.minute != time2.minute) return time1.minute - time2.minute;
    if (time1.second != time2.second) return time1.second - time2.second;
    return 0;
}

void sortedInsert(DataNode **sorted, DataNode *newNode) {
    if (*sorted == NULL || compareTimestamps(newNode->data.time, (*sorted)->data.time) <= 0) {
        newNode->next = *sorted;
        *sorted = newNode;
    } else {
        DataNode *current = *sorted;
        while (current->next != NULL && compareTimestamps(newNode->data.time, current->next->data.time) > 0) {
            current = current->next;
        }
        newNode->next = current->next;
        current->next = newNode;
    }
}

DataNode* sortDataByTimestamp(DataNode *dataList) {
    if (dataList == NULL || dataList->next == NULL) return dataList;

    DataNode *sorted = NULL;
    DataNode *current = dataList;

    while (current != NULL) {
        DataNode *next = current->next;
        current->next = NULL;
        sortedInsert(&sorted, current);
        current = next;
    }

    return sorted;
}

void getMinMaxTemperature(DataNode *dataList, double *min, double *max, double buffer) {
    if (dataList == NULL || min == NULL || max == NULL) return;
    *min = INFINITY;
    *max = -INFINITY;
    DataNode *current = dataList;
    while (current != NULL) {
        if (current->data.temperature < *min) *min = current->data.temperature;
        if (current->data.temperature > *max) *max = current->data.temperature;
        current = current->next;
    }
    if (*min == *max) {
        *min -= buffer;
        *max += buffer;
    }
}

void getMinMaxValue(DataNode *dataList, double *min, double *max, double buffer) {
    if (dataList == NULL || min == NULL || max == NULL) return;
    *min = INFINITY;
    *max = -INFINITY;
    DataNode *current = dataList;
    while (current != NULL) {
        if (current->data.temperature < *min) *min = current->data.temperature;
	if (current->data.humidity < *min) *min = current->data.humidity;
        if (current->data.temperature > *max) *max = current->data.temperature;
	if (current->data.humidity > *max) *max = current->data.humidity;
        current = current->next;
    }
    if (*min == *max) {
        *min -= buffer;
        *max += buffer;
    }
}

#endif
