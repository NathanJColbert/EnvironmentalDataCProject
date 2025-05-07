#include <stdio.h>
#include <stdlib.h>
#include <mysql/mysql.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <float.h>
#include "DHT11Control.h"
#include "LCDControl.h"
#include "commandLineControl.h"
#include "dataList.h"

int LCD_ADDRESS = 0x27;
int DHT11_PIN = 7;
size_t RATE_SECONDS = 600;
size_t MAX_READ_TRIES = 100;
size_t MAX_STORE_TRIES = 5;

char *buildStoreQuery(int data[], const char *tableName) {
    // insert into tableName values (x, y, z, ... );
    char *output = malloc(sizeof(char) * 256);
    if (output == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    sprintf(output, "INSERT INTO %s (HumLHS, HumRHS, TempLHS, TempRHS) VALUES (%d, %d, %d, %d)", 
        tableName, data[0], data[1], data[2], data[3]);
    return output;
}

struct sqlSetup {
    char *server;
    char *user;
    char *password;
    char *database;
    char *table;
};
typedef struct sqlSetup SQLSetup;

void initSetup(SQLSetup *setup) {
    setup->server = NULL;
    setup->user = NULL;
    setup->password = NULL;
    setup->database = NULL;
    setup->table = NULL;
}
void freeSetup(SQLSetup *setup) {
    if (setup->server != NULL) free(setup->server);
    if (setup->user != NULL) free(setup->user);
    if (setup->password != NULL) free(setup->password);
    if (setup->database != NULL) free(setup->database);
    if (setup->table != NULL) free(setup->table);
}

int testConnection(SQLSetup *setup) {
    if (setup == NULL) return 0;
    MYSQL *conn;
    MYSQL_RES *res;
    
    conn = mysql_init(NULL);
    if (conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        return 0;
    }
    if (!mysql_real_connect(conn, setup->server, setup->user, setup->password, setup->database, 0, NULL, 0)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        return 0;
    }
    
    char query[256];
    snprintf(query, sizeof(query), "SHOW TABLES LIKE '%s'", setup->table);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "SHOW TABLES LIKE query failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 0;
    }

    res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "Failed to store result: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 0;
    }
        
    int result = 0;
    if (mysql_num_rows(res) > 0)
        result = 1;
       
    mysql_free_result(res);
    mysql_close(conn);
    return result;
}

MYSQL *buildConnection(SQLSetup *setup) {
    MYSQL *conn;
    conn = mysql_init(NULL);
    if (conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        return NULL;
    }
    if (!mysql_real_connect(conn, setup->server, setup->user, setup->password, setup->database, 0, NULL, 0)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        return NULL;
    }
    return conn;
}

int storeData(int data[], SQLSetup *setup) {
    MYSQL *conn = buildConnection(setup);
    if (conn == NULL) return 0;
        
    char *query = buildStoreQuery(data, setup->table);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        free(query);
        return 0;
    }
    free(query);

    mysql_close(conn);
    return 1;
}

struct timeValue {
    unsigned int year;
    unsigned int month;
    unsigned int day;
    unsigned int hour;
};
typedef struct timeValue TimeValue;

int copyTimeValue(TimeValue *dest, TimeValue *src) {
    if (dest == NULL || src == NULL) return 0;
    dest->year = src->year;
    dest->month = src->month;
    dest->day = src->day;
    dest->hour = src->hour;
    return 1;
}

int timeDifference(TimeValue *lhs, TimeValue *rhs) {
    if (lhs->year != rhs->year) return lhs->year - rhs->year;
    if (lhs->month != rhs->month) return lhs->month - rhs->month;
    if (lhs->day != rhs->day) return lhs->day - rhs->day;
    if (lhs->hour != rhs->hour) return lhs->hour - rhs->hour;
    return 0;
}

int getDataInRange(SQLSetup *setup, TimeValue *start, TimeValue *end, DataNode **dataList) {
    if (start == NULL || end == NULL) {
        fprintf(stderr, "Null time range passed.\n");
        return 0;
    }
        
    MYSQL *conn = buildConnection(setup);
    if (conn == NULL) return 0;
        
    MYSQL_BIND bind[2];
    MYSQL_TIME sql_start, sql_end;
    memset(bind, 0, sizeof(bind));
    memset(&sql_start, 0, sizeof(sql_start));
    memset(&sql_end, 0, sizeof(sql_end));
    
    sql_start.year = start->year;
    sql_start.month = start->month;
    sql_start.day = start->day;
    sql_start.hour = start->hour;

    sql_end.year = end->year;
    sql_end.month = end->month;
    sql_end.day = end->day;
    sql_end.hour = end->hour;
    // Just in case you are checking a single hour
    sql_end.minute = 59;
    sql_end.second = 59;
    
    bind[0].buffer_type = MYSQL_TYPE_TIMESTAMP;
    bind[0].buffer = (void*)&sql_start;
    bind[0].is_null = 0;
    bind[1].buffer_type = MYSQL_TYPE_TIMESTAMP;
    bind[1].buffer = (void *)&sql_end;
    bind[1].is_null = 0;
    
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) {
        fprintf(stderr, "mysql_stmt_init() failed\n");
        mysql_close(conn);
        return 1;
    }

    if (mysql_stmt_prepare(stmt, "SELECT * FROM data_main WHERE time BETWEEN ? AND ?", -1)) {
        fprintf(stderr, "mysql_stmt_prepare() failed: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return 1;
    }

    if (mysql_stmt_bind_param(stmt, bind)) {
        fprintf(stderr, "mysql_stmt_bind_param() failed: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return 1;
    }

    if (mysql_stmt_execute(stmt)) {
        fprintf(stderr, "mysql_stmt_execute() failed: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return 1;
    }
        
    // FETCH
    MYSQL_BIND resultBind[5];
    memset(resultBind, 0, sizeof(resultBind));
        
    int dataValues[5];
    MYSQL_TIME ts;

    resultBind[0].buffer_type = MYSQL_TYPE_LONG;
    resultBind[0].buffer = &dataValues[0];
    resultBind[1].buffer_type = MYSQL_TYPE_LONG;
    resultBind[1].buffer = &dataValues[1];
    resultBind[2].buffer_type = MYSQL_TYPE_LONG;
    resultBind[2].buffer = &dataValues[2];
    resultBind[3].buffer_type = MYSQL_TYPE_LONG;
    resultBind[3].buffer = &dataValues[3];
    resultBind[4].buffer_type = MYSQL_TYPE_TIMESTAMP;
    resultBind[4].buffer = &ts;

    if (mysql_stmt_bind_result(stmt, resultBind)) {
        fprintf(stderr, "Result bind failed: %s\n", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        mysql_close(conn);
        return 0;
    }

    while (mysql_stmt_fetch(stmt) == 0) {
        DataValue *data = malloc(sizeof(DataValue));
        data->time = ts;
        convertData(dataValues, &data->temperature, &data->humidity);
        data->f_temperature = (data->temperature * (9.0/5.0)) + 32;
        appendDataNode(dataList, data);
    }
        
    mysql_stmt_close(stmt);
    mysql_close(conn);
    return 1;
}

char *promptString(const char *prompt) {
    printf("%s", prompt);
    char buffer[256];
    fgets(buffer, sizeof(buffer), stdin);
        
    buffer[strcspn(buffer, "\r\n\t")] = '\0';
        
    char *result = malloc(strlen(buffer) + 1);
    if (!result) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    strcpy(result, buffer);
    return result;
}

void processData(SQLSetup *setup) {
    int data[5];
    for (size_t i = 0; i < MAX_READ_TRIES; i++) {
        if (read_dht11_dat(data)) {
            for (size_t j = 0; j < MAX_STORE_TRIES; j++) {
                if (storeData(data, setup)) continue;
                sleep(1);
            }
            double temp, hum;
            convertData(data, &hum, &temp);
            writeData(temp, hum, time(NULL));
            return;
        }
    }
    writeRegister(0, 0, "WRITE ISSUE");
}

volatile int stopMainQueryThread = 0;
void *mainQuery(void *arg) {
    SQLSetup *setup = (SQLSetup*)arg;
    // Wait the time before processing. So I can test run the program without adding
    // unnecessary data.
    for (size_t i = 0; i < RATE_SECONDS; i++) {
        if (stopMainQueryThread) break;
        sleep(1);
    }
    while (!stopMainQueryThread) {
        processData(setup);
        for (size_t i = 0; i < RATE_SECONDS; i++) {
            if (stopMainQueryThread) break;
            sleep(1);
        }
    }
    return NULL;
}

int testInput(char *input, const char *ref, int allowFirstChar) {
    if (input == NULL || ref == NULL)
    return 0;
    if (allowFirstChar && strlen(input) == 1 && 
        tolower((unsigned char)input[0]) == tolower((unsigned char)ref[0])) 
        return 1;
    while (*input && *ref) {
        if (tolower((unsigned char)*input) != tolower((unsigned char)*ref))
            return 0;
        input++;
        ref++;
    }
    return *input == '\0' && *ref == '\0';
}

void printCommands() {
    printf("%5s%40s\n", "Help / H", "Show all commands.");
    printf("%5s%40s\n", "Quit / Q", "Quit the program.");
    printf("%5s%40s\n", "Test / T", "Test the SQL connection.");
    printf("%5s%40s\n", "Data / D", "Open the tool to check the database.");
    printf("%5s%40s\n", "Show / S", "Show the current global settings.");
}

void enterToContinue() {
    puts("Enter to continue.");
    char *temp = promptString("");
    free(temp);
}

void initTime(TimeValue *start, TimeValue *end) {
    time_t now = time(NULL);
    // static buffer (dont need to free)
    struct tm *now_tm = localtime(&now);
    
    // Years since 1900
    end->year = now_tm->tm_year + 1900;
    // [0, 11]
    end->month = now_tm->tm_mon + 1;
    end->day = now_tm->tm_mday;
    end->hour = now_tm->tm_hour;

    // current - 24 hours
    time_t yesterday = now - 24 * 60 * 60;
    struct tm *yesterday_tm = localtime(&yesterday);

    start->year = yesterday_tm->tm_year + 1900;
    start->month = yesterday_tm->tm_mon + 1;
    start->day = yesterday_tm->tm_mday;
    start->hour = yesterday_tm->tm_hour;
}

void setTimeRelative(TimeValue *value, unsigned int hours) {
    time_t now = time(NULL);
    time_t time = now - hours * 60 * 60;
    struct tm *tm_relative = localtime(&time);

    value->year = tm_relative->tm_year + 1900;
    value->month = tm_relative->tm_mon + 1;
    value->day = tm_relative->tm_mday;
    value->hour = tm_relative->tm_hour;
}

void printTimeRange(TimeValue *start, TimeValue *end) {
    if (start == NULL || end == NULL) return;
    printf("%04d-%02d-%02d %02d to %04d-%02d-%02d %02d\n",
        start->year, start->month, start->day, start->hour,
        end->year, end->month, end->day, end->hour);
}
void printTime(TimeValue *value) {
    if (value == NULL) return;
    printf("%04d-%02d-%02d %02d\n",
        value->year, value->month, value->day, value->hour);
}

enum PlotType { BOTH = 0, TEMPERATURE = 1, HUMIDITY = 2 };

void plotData(DataNode *dataList, TimeValue *start, TimeValue *end, enum PlotType type, int fahrenheit) {
    if (dataList == NULL) return;
    dataList = sortDataByTimestamp(dataList);
        
    double buffer = 2.0;
    double min, max;
    switch (type) {
        case BOTH:
            getMinMaxValue(dataList, &min, &max, buffer, fahrenheit);
            break;
        case TEMPERATURE:
            getMinMaxTemperature(dataList, &min, &max, buffer, fahrenheit);
            break;
        case HUMIDITY:
            getMinMaxHumidity(dataList, &min, &max, buffer);
            break;
        default: return;
    }
        
    FILE *gnuplot = popen("gnuplot -persistent", "w");
    if (gnuplot == NULL) {
        perror("Failed to open gnuplot");
        return;
    }
        
    fprintf(gnuplot, "set terminal wxt\n");

    fprintf(gnuplot, "set xdata time\n");
    fprintf(gnuplot, "set timefmt '%%Y-%%m-%%d%%H:%%M:%%S'\n");
    fprintf(gnuplot, "set format x '%%H:%%M'\n");
    fprintf(gnuplot, "set xlabel 'Time'\n");
        
    fprintf(gnuplot, "set xrange ['%04d-%02d-%02d%02d:%02d:%02d' to '%04d-%02d-%02d%02d:%02d:%02d']\n",
        start->year, start->month, start->day, start->hour, 0, 0,
        end->year, end->month, end->day, end->hour, 59, 59);
    fprintf(gnuplot, "set yrange [%lf:%lf]\n", min - buffer, max + buffer);
    
    switch (type) {
        case BOTH:
            fprintf(gnuplot, "set ylabel 'Temperature (%s) / Humidity'\n", (fahrenheit ? "F" : "C"));
            fprintf(gnuplot, "plot '-' using 1:2 title 'Temperature' with linespoints pt 7 ps 1.5, "
                "'-' using 1:2 title 'Humidity' with linespoints pt 7 ps 1.5\n");
            break;
        case HUMIDITY:
            fprintf(gnuplot, "set ylabel 'Humidity'\n");
            fprintf(gnuplot, "plot '-' using 1:2 title 'Humidity' with linespoints pt 7 ps 1.5\n");
            break;
        case TEMPERATURE:
            fprintf(gnuplot, "set ylabel 'Temperature (%s)'\n", (fahrenheit ? "F" : "C"));
            fprintf(gnuplot, "plot '-' using 1:2 title 'Temperature' with linespoints pt 7 ps 1.5\n");
            break;
        }
        
        DataNode *current = NULL;
        
    switch (type) {
        case BOTH:
            current = dataList;
            while (current != NULL) {
                char timestamp[64];
                snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d%02d:%02d:%02d",
                    current->data.time.year, current->data.time.month, current->data.time.day,
                    current->data.time.hour, current->data.time.minute, current->data.time.second);
                    fprintf(gnuplot, "%s %.2lf\n", timestamp, (fahrenheit ? current->data.f_temperature : current->data.temperature));
                    current = current->next;
            }
            fprintf(gnuplot, "e\n");
            __attribute__((fallthrough));
        case HUMIDITY:
            current = dataList;
            while (current != NULL) {
                char timestamp[64];
                snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d%02d:%02d:%02d",
                    current->data.time.year, current->data.time.month, current->data.time.day,
                    current->data.time.hour, current->data.time.minute, current->data.time.second);
                fprintf(gnuplot, "%s %.2lf\n", timestamp, current->data.humidity);
                current = current->next;
            }
            fprintf(gnuplot, "e\n");
            break;
        case TEMPERATURE:
            current = dataList;
            while (current != NULL) {
                char timestamp[64];
                snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d%02d:%02d:%02d",
                    current->data.time.year, current->data.time.month, current->data.time.day,
                    current->data.time.hour, current->data.time.minute, current->data.time.second);
                fprintf(gnuplot, "%s %.2lf\n", timestamp, (fahrenheit ? current->data.f_temperature : current->data.temperature));
                current = current->next;
            }
            fprintf(gnuplot, "e\n");
            break;
    }
    
    fflush(gnuplot);
    pclose(gnuplot);
}

void listData(SQLSetup *setup, TimeValue *start, TimeValue *end, int fahrenheit) {
    DataNode *dataList = NULL;
    if (!getDataInRange(setup, start, end, &dataList)) return;
    DataNode *current = dataList;
    int totalDataBlocks = 0;
    
    char tempChar = (fahrenheit ? 'F' : 'C');
    
    double averageTemp = 0;
    double averageHum = 0;
    
    double maxTemp = DBL_MIN;
    double maxHum = DBL_MIN;
    double minTemp = DBL_MAX;
    double minHum = DBL_MAX;
    
    double currentTemp = 0;
    while (current != NULL) {
        currentTemp = (fahrenheit ? current->data.f_temperature : current->data.temperature);
        printf("Temperature: %.3lf%c | Humidity: %.3lf | Time: %04d-%02d-%02d %02d:%02d:%02d\n",
            currentTemp, tempChar, current->data.humidity,
            current->data.time.year, current->data.time.month, current->data.time.day,
            current->data.time.hour, current->data.time.minute, current->data.time.second);
        averageTemp += currentTemp;
        averageHum += current->data.humidity;
        
        if (currentTemp > maxTemp) maxTemp = currentTemp;
        if (current->data.humidity > maxHum) maxHum = current->data.humidity;
        if (currentTemp < minTemp) minTemp = currentTemp;
        if (current->data.humidity < minHum) minHum = current->data.humidity;
        
        current = current->next;
        totalDataBlocks++;
    }
    averageTemp /= totalDataBlocks;
    averageHum /= totalDataBlocks;
    
    printf("\nAverage temperature: %.3lf%c | Average humidity: %.3lf\n", averageTemp, tempChar, averageHum);
    printf("Max temperature: %.3lf%c | Max humidity: %.3lf\n", maxTemp, tempChar, maxHum);
    printf("Min temperature: %.3lf%c | Min humidity: %.3lf\n", minTemp, tempChar, minHum);
    
    printf("Total values in set: %d\n", totalDataBlocks);
    freeDataList(dataList);
}

void printGraphingType(enum PlotType plotType) {
    switch (plotType) {
        case BOTH: puts("Graph BOTH"); break;
        case TEMPERATURE: puts("Graph TEMPERATURE"); break;
        case HUMIDITY: puts("Graph HUMIDITY"); break;
        default: puts("Invalid graphing type. Please change it.");
    }
}

void printEvaluationCommands() {
    printf("%5s%40s\n", "Help  / H", "Show all commands.");
    printf("%5s%40s\n", "List  / L", "List data in time range.");
    printf("%5s%40s\n", "Graph / G", "Graph the data in the time range.");
    printf("%5s%40s\n", "Type  / T", "Change graphing type.");
    printf("%5s%40s\n", "Range / R", "Set the time range for data retrieval.");
    printf("%5s%40s\n", "Back  / B", "Back to main control.");
}

void rangeHelp() {
    printf("%5s%40s\n", "Help  / H", "Show all commands.");
    printf("%5s%40s\n", "Start / S", "Edit the start time.");
    printf("%5s%40s\n", "End   / E", "Edit the end time.");
    printf("%5s%40s\n", "Back  / B", "Back to main control.");
}

void rangeValueHelp() {
    printf("%5s%45s\n", "Help        ", "Show all commands.");
    printf("%5s%45s\n", "Year     / Y", "Set the year.");
    printf("%5s%45s\n", "Month    / M", "Set the month (1 - 12).");
    printf("%5s%45s\n", "Day      / D", "Set the day (1 - 31).");
    printf("%5s%45s\n", "Hour     / H", "Set the hour (0 [12am] - 23 [12pm]).");
    printf("%5s%45s\n", "Current  / C", "Set the time to current time.");
    printf("%5s%45s\n", "Subtract / S", "Set the time to current - hours input.");
    printf("%5s%45s\n", "Back     / B", "Back to range settings (saves current work).");
}

void changeTimeValue(TimeValue *value) {
    char *input = NULL;
    while (1) {
        if (input != NULL) free(input);
        puts("CURRENT TIME");
        printTime(value);
        input = promptString("> ");
        if (testInput(input, "help", 0)) {
            clearScreen();
            rangeValueHelp();
            enterToContinue();
        }
        else if (testInput(input, "year", 1)) {
            clearScreen();
            puts("Enter new year: ");
            scanf("%u", &value->year);
        }
        else if (testInput(input, "month", 1)) {
            clearScreen();
            puts("Enter new month (1 - 12) : ");
            unsigned int tempMonth = value->month;
            scanf("%u", &value->month);
            if (value->month == 0 || value->month > 12) {
                value->month = tempMonth;
                puts("Invalid month.");
                enterToContinue();
            }
        }
        else if (testInput(input, "day", 1)) {
            clearScreen();
            puts("Enter new day (1 - 31) : ");
            unsigned int tempDay = value->day;
            scanf("%u", &value->day);
            if (value->day == 0 || value->day > 31) {
                value->day = tempDay;
                puts("Invalid day.");
                enterToContinue();
            }
        }
        else if (testInput(input, "hour", 1)) {
            clearScreen();
            puts("Enter new hour (0 [12am] - 23 [12pm]) : ");
            unsigned int tempHour = value->hour;
            scanf("%u", &value->hour);
            if (value->hour > 23) {
                value->hour = tempHour;
                puts("Invalid hour.");
                enterToContinue();
            }
        }
        else if (testInput(input, "current", 1)) {
            setTimeRelative(value, 0);
        }
        else if (testInput(input, "subtract", 1)) {
            clearScreen();
            puts("Enter hours back from current (NOW) time: ");
            unsigned int hoursBack = 0;
            scanf("%u", &hoursBack);
            setTimeRelative(value, hoursBack);
        }
        else if (testInput(input, "back", 1)) {
            break;
        }
        clearScreen();
    }
    if (input != NULL) free(input);
}

void setRange(TimeValue *start, TimeValue *end) {
    char *input = NULL;
    while (1) {
        if (input != NULL) free(input);
        puts("CHANGE TIME RANGE");
        printTimeRange(start, end);
        input = promptString("> ");
        if (testInput(input, "help", 1)) {
            clearScreen();
            rangeHelp();
            enterToContinue();
        }
        if (testInput(input, "start", 1)) {
            clearScreen();
            TimeValue temp = {0};
            copyTimeValue(&temp, start);
            changeTimeValue(start);
            if (timeDifference(start, end) > 0) {
                copyTimeValue(start, &temp);
                puts("Start time exceeds end time. Reverting.");
                enterToContinue();
            }
        }
        else if (testInput(input, "end", 1)) {
            clearScreen();
            TimeValue temp2 = {0};
            copyTimeValue(&temp2, end);
            changeTimeValue(end);
            if (timeDifference(start, end) > 0) {
                copyTimeValue(end, &temp2);
                puts("End time is less than the start time. Reverting.");
                enterToContinue();
            }
        }
        else if (testInput(input, "back", 1)) {
            break;
        }
        clearScreen();
    }
    if (input != NULL) free(input);
}

void databaseMenu(SQLSetup *setup) {
    char *input = NULL;
    TimeValue start;
    TimeValue end;
    enum PlotType plotType = BOTH;
    int fahrenheit = 0;
    initTime(&start, &end);
    clearScreen();
    while (1) {
        if (input != NULL) free(input);
        puts("DATA EVALUATION");
        printTimeRange(&start, &end);
        printGraphingType(plotType);
        printf("Temperature type: %s\n", (fahrenheit ? "Fahrenheit" : "Celsius"));
        input = promptString("> ");
        if (testInput(input, "help", 1)) {
            clearScreen();
            printEvaluationCommands();
            enterToContinue();
        }
        else if (testInput(input, "list", 1)) {
            clearScreen();
            listData(setup, &start, &end, fahrenheit);
            enterToContinue();
        }
        else if (testInput(input, "graph", 1)) {
            clearScreen();
            DataNode *dataList = NULL;
            if (getDataInRange(setup, &start, &end, &dataList)) {
                plotData(dataList, &start, &end, plotType, fahrenheit);
                enterToContinue();
            }
            freeDataList(dataList);
        }
        else if (testInput(input, "fahrenheit", 1)) {
            fahrenheit = 1;
        }
        else if (testInput(input, "celsius", 1)) {
            fahrenheit = 0;
        }
        else if (testInput(input, "type", 1)) {
            clearScreen();
            char *tempInput = promptString("Enter a graphing type (BOTH / B, TEMPERATURE / T, HUMIDITY / H)\n> ");
            if (testInput(tempInput, "both", 1))
                plotType = BOTH;
            else if (testInput(tempInput, "temperature", 1))
                plotType = TEMPERATURE;
            else if (testInput(tempInput, "humidity", 1))
                plotType = HUMIDITY;
            if (tempInput != NULL) free(tempInput);
        }
        else if (testInput(input, "range", 1)) {
            clearScreen();
            setRange(&start, &end);
        }
        else if (testInput(input, "back", 1))
            break;
        clearScreen();
    }
    if (input != NULL) free(input);
}

void menuInput(SQLSetup *setup) {
    char *input = NULL;
    printf("%5s%40s\n", "Help / H", "Show all commands.");
    while (1) {
        if (input != NULL) free(input);
        input = promptString("> ");
        if (testInput(input, "help", 1)) {
            clearScreen();
            printCommands();
            enterToContinue();
        }
        else if (testInput(input, "quit", 1)) {
            clearScreen();
            stopMainQueryThread = 1;
            puts("Quitting application...");
            break;
        }
        else if (testInput(input, "test", 1)) {
            clearScreen();
            printf("%s\n", (testConnection(setup) ? "Connection is valid." : "Connection is NOT valid."));
            enterToContinue();
        }
        else if (testInput(input, "data", 1)) {
            databaseMenu(setup);
        }
        else if (testInput(input, "show", 1)) {
            clearScreen();
            printf("Current settings\n");
            printf("\tLCD_ADDRESS = 0x%X\n", LCD_ADDRESS);
            printf("\tDHT11_PIN = %d\n", DHT11PIN);
            printf("\tRATE_SECONDS = %d\n", (int)RATE_SECONDS);
            printf("\tMAX_READ_TRIES = %d\n", (int)MAX_READ_TRIES);
            printf("\tMAX_STORE_TRIES = %d\n", (int)MAX_STORE_TRIES);
            enterToContinue();
        }
        clearScreen();
    }
    if (input != NULL) free(input);
}

int getEnvironmentSetup(SQLSetup *setup) {
    int result = 1;
    const char *server = getenv("EN_SERVER");
    if (server == NULL) result = 0;
    else {
        setup->server = malloc(strlen(server) + 1);
        if (!setup->server) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }
        strcpy(setup->server, server);
    }
    
    const char *user = getenv("EN_USER");
    if (user == NULL) result = 0;
    else {
        setup->user = malloc(strlen(user) + 1);
        if (!setup->user) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }
        strcpy(setup->user, user);
    }
    
    const char *password = getenv("EN_PASSWORD");
    if (password == NULL) result = 0;
    else {
        setup->password = malloc(strlen(password) + 1);
        if (!setup->password) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }
        strcpy(setup->password, password);
    }
    
    const char *database = getenv("EN_DATABASE");
    if (database == NULL) result = 0;
    else {
        setup->database = malloc(strlen(database) + 1);
        if (!setup->database) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }
        strcpy(setup->database, database);
    }
    
    const char *table = getenv("EN_TABLE");
    if (table == NULL) result = 0;
    else {
        setup->table = malloc(strlen(table) + 1);
        if (!setup->table) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }
        strcpy(setup->table, table);
    }
    return result;
}

int printEnvironmentSetupExport(const SQLSetup *setup) {
    if (setup == NULL) return 0;
    clearScreen();
    printf("Environment variables. Copy and paste into shell for cached login.\n\n");
    printf("export EN_SERVER='%s'\n", setup->server);
    printf("export EN_USER='%s'\n", setup->user);
    printf("export EN_PASSWORD='%s'\n", setup->password);
    printf("export EN_DATABASE='%s'\n", setup->database);
    printf("export EN_TABLE='%s'\n", setup->table);
    return 1;
}

int main(int argc, char *argv[]) {    
    if (argc > 1) {
        Argument **args = getArgs(argc, argv);
        if (args == NULL) {
            printf("Invalid arg number OR had issues allocating space\n");
            return -1;
        }
        
        int invalid = 0;
        for (int i = 0; args[i] != NULL; i++) {
            int used = 0;
            if (compareFlag(args[i], "-lcd_address")) {
                if (args[i]->isInt) {
                    LCD_ADDRESS = args[i]->intValue;
                    used = 1;
                }
            }
            if (compareFlag(args[i], "-dht11_pin")) {
                if (args[i]->isInt) {
                    DHT11_PIN = args[i]->intValue;
                    used = 1;
                }
            }
            if (compareFlag(args[i], "-rate")) {
                if (args[i]->isInt) {
                    RATE_SECONDS = (size_t)args[i]->intValue;
                    used = 1;
                }
            }
            if (compareFlag(args[i], "-read_tries")) {
                if (args[i]->isInt) {
                    MAX_READ_TRIES = (size_t)args[i]->intValue;
                    used = 1;
                }
            }
            if (compareFlag(args[i], "-store_tries")) {
                if (args[i]->isInt) {
                    MAX_STORE_TRIES = (size_t)args[i]->intValue;
                    used = 1;
                }
            }
            if (!used) {
                printf("Invalid argument of flag: \"%s\"\n", args[i]->flag);
                printArg(args[i]);
                invalid = 1;
            }
        }
        freeArguments(args);
        if (invalid) {
            puts("\nValid arguments");
            puts("\t-lcd_address {Decimal}");
            puts("\t-dht11_pin {Decimal}");
            puts("\t-rate {Decimal}");
            puts("\t-read_tries {Decimal}");
            puts("\t-store_tries {Decimal}");
            return -1;
        }
    }
    
    SQLSetup setup;
    int exitProgram = 0;
    
    getEnvironmentSetup(&setup);
    if (!testConnection(&setup)) {
        while (1) {
            clearScreen();
            printf("Database information (Quit / Q to exit)\n");
            initSetup(&setup);
            setup.server = promptString("Server: ");
            if (testInput(setup.server, "quit", 1)) { exitProgram = 1; break; }
                
            setup.user = promptString("User: ");
            if (testInput(setup.user, "quit", 1)) { exitProgram = 1; break; }

            setup.password = promptString("Password: ");
            if (testInput(setup.password, "quit", 0)) { exitProgram = 1; break; }
                
            setup.database = promptString("Database: ");
            if (testInput(setup.database, "quit", 0)) { exitProgram = 1; break; }
                
            setup.table = promptString("Table: ");
            if (testInput(setup.table, "quit", 0)) { exitProgram = 1; break; }
            
            if (testConnection(&setup)) {
                if (!exitProgram && printEnvironmentSetupExport(&setup))
                    enterToContinue();
                break;
            }
            freeSetup(&setup);
        }
    }
    clearScreen();
    if (exitProgram) {
        freeSetup(&setup);
        return 0;
    }
    
    if (!dht11_init(DHT11_PIN)) {
        printf("Failed initiallize DHT11 pin\n");
        return -1;
    }
    if (!lcd_init(LCD_ADDRESS)) {
        printf("Failed initiallize I2C_LCD address\n");
        return -1;
    }
        
    pthread_t mainQueryThread;
    if (pthread_create(&mainQueryThread, NULL, mainQuery, &setup) != 0) {
        perror("Failed to create thread");
        exit(EXIT_FAILURE);
    }
        
    menuInput(&setup);
        
    pthread_join(mainQueryThread, NULL);
    freeSetup(&setup);
    
    return 0;
}
