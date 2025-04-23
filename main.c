#include <stdio.h>
#include <mysql/mysql.h>
#include <unistd.h>
#include <pthread.h>
#include "DHT11Control.h"
#include "LCDControl.h"
#include "commandLineControl.h"

const size_t RATE_SECONDS = 10;
const size_t MAX_READ_TRIES = 100;

char *buildQuery(int data[], const char *tableName) {
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

int storeData(int data[], SQLSetup *setup) {
	MYSQL *conn;

    conn = mysql_init(NULL);
    if (conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        return 0;
    }
    if (!mysql_real_connect(conn, setup->server, setup->user, setup->password, setup->database, 0, NULL, 0)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        return 0;
    }
    char *query = buildQuery(data, setup->table);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        free(query);
        return 0;
    }
    free(query);

    mysql_close(conn);
    return 1;
}

char *promptString(const char *prompt) {
	printf("%15s", prompt);
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
			storeData(data, setup);
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
	while (!stopMainQueryThread) {
        processData(setup);
		for (size_t i = 0; i < RATE_SECONDS; i++) {
			if (stopMainQueryThread) break;
			sleep(1);
		}
    }
    return NULL;
}

int checkExitInput(const char *input) {
	if (strlen(input) != 1) return 0;
	return input[0] == 'q' || input[0] == 'Q';
}

void menuInput() {
	char input;
    printf("Press 'q' to stop...\n");
    while (1) {
        input = getchar();
        if (input == 'q' || input == 'Q') {
            stopMainQueryThread = 1;
            break;
        }
        clearScreen();
        printf("Press 'q' to stop...\n");
    }
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

int main(void) {	
	SQLSetup setup;
	int exitProgram = 0;
	
	if (!getEnvironmentSetup(&setup)) {
		while (1) {
			printf("Database information (q to exit)\n");
			initSetup(&setup);
			setup.server = promptString("Server: ");
			if (checkExitInput(setup.server)) { exitProgram = 1; break; }
		
			setup.user = promptString("User: ");
			if (checkExitInput(setup.user)) { exitProgram = 1; break; }
		
			setup.password = promptString("Password: ");
			if (checkExitInput(setup.password)) { exitProgram = 1; break; }
		
			setup.database = promptString("Database: ");
			if (checkExitInput(setup.database)) { exitProgram = 1; break; }
		
			setup.table = promptString("Table: ");
			if (checkExitInput(setup.table)) { exitProgram = 1; break; }
		
			clearScreen();
			if (testConnection(&setup)) break;
			freeSetup(&setup);
		}
	}
	clearScreen();
	if (exitProgram) {
		freeSetup(&setup);
		return 0;
	}
	
	if (!dht11_init(7)) return -1;
	lcd_init(0x27);
	
	pthread_t mainQueryThread;
	if (pthread_create(&mainQueryThread, NULL, mainQuery, &setup) != 0) {
        perror("Failed to create thread");
        exit(EXIT_FAILURE);
    }
	
	menuInput();
	
	pthread_join(mainQueryThread, NULL);
    freeSetup(&setup);
    
	return 0;
}
