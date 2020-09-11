#ifndef __MONGO_SQL_H
#define __MONGO_SQL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mongoc.h>
#include <bson.h>
#include "confparse.h"

#ifndef MAX_CMD_LEN
#define MAX_CMD_LEN 128
#endif

typedef struct nosql_sock {
	int id;
    void *connect_handle;
	char **nosql_row;
	char **cursor_row;
	int numfields;
	int cursor_numfields;
	int connected;
	char table_name[256];
	struct nosql_sock *next;
	pthread_mutex_t nosql_sock_mutex;
}NOSQL_SOCK;


typedef struct rlm_nosql_mongo_t
{
	mongoc_uri_t *uri;
	mongoc_client_t *conn;
	mongoc_database_t *db;
	mongoc_collection_t *collection;
}NOSQL_MONGO;

enum{
STRING = 0,
INTEGER
};

int nosql_destroy_socket(CONF *config,NOSQL_SOCK *nosql_sock);
int nosql_select_query(NOSQL_SOCK *nosql_sock,const char *getstr,char **fields,int connid);
int nosql_exec_direct(NOSQL_SOCK *nosql_sock,const char *setstr,const char *updatestr,int connid);
int nosql_exec_del_direct(NOSQL_SOCK *nosql_sock,const char *setstr);

char *single_quotes_to_double (const char *str);

bson_t *tmp_bson(const char *json,...);

int fetch_data(const bson_t *doc,char *field,char **output,int type);

int nosql_init_socket(CONF *config,NOSQL_SOCK *nosql_sock);

void release_collection(NOSQL_SOCK *nosql_sock);
int nosql_select_count(NOSQL_SOCK *nosql_sock,const char *setstr);

int nosql_error_judge(const char *msg,bson_error_t error);

int nosql_free_result(NOSQL_SOCK *nosql_sock);

int nosql_free_cursor_result(NOSQL_SOCK *nosql_sock);
#endif