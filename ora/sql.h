#ifndef __SQL_H
#define __SQL_H

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <oci.h>

#include "sql_prepare_parser.h"
#include "confparse.h"

typedef struct sql_socket {
	int     id;
#if HAVE_PTHREAD_H
	pthread_mutex_t mutex;
#endif
	struct sql_socket *next;
	enum { sockconnected, sockunconnected } state;

	void	*conn;
	char**   row;
} SQLSOCK;

/* Each statement for each SQL */
typedef struct query_list_t {
    char                 *query;
    OCIStmt              *stmt;
    sql_data_t           *data;
    int                  field_num;
    sb2                  *indicators;
    char                 **results;
    struct query_list_t  *next;
} query_list_t;

typedef struct rlm_sql_oracle_sock {
    OCIEnv        *env;
    OCIError      *errHandle;
    OCISvcCtx     *conn;
    OCIStmt       *queryHandle;
    sb2           *indicators;
    char          **results;
    query_list_t  *query_list;
    int           is_direct;
} rlm_sql_oracle_sock;

#ifndef MAX_COLUMN_LEN
#define MAX_COLUMN_LEN 128
#endif
#ifndef MAX_SQL_LEN
#define MAX_SQL_LEN 512
#endif

static const char * sql_error(SQLSOCK *sqlsocket,CONF *config);

static int sql_check_error(SQLSOCK *sqlsocket, CONF *config);

static int sql_close(SQLSOCK *sqlsocket, CONF *config);
static int sql_init_socket(SQLSOCK *sqlsocket, CONF *config);

static int sql_destroy_socket(SQLSOCK *sqlsocket, CONF *config);
static int sql_num_fields(SQLSOCK *sqlsocket, CONF *config);

static int get_fields_num(char *query);

static int sql_prepare(SQLSOCK *sqlsocket, CONF *config, char *origin_sql);

static int sql_query_exec(SQLSOCK *sqlsocket, CONF *config);

static int sql_select_query_exec(SQLSOCK *sqlsocket, CONF *config);
static int sql_direct_query(SQLSOCK *sqlsocket, CONF *config, char *querystr);

static int sql_prepare_query(SQLSOCK *sqlsocket, CONF *config, char *querystr);

static int sql_query(SQLSOCK *sqlsocket, CONF *config, char *querystr);

static int sql_direct_select_query(SQLSOCK *sqlsocket, CONF *config, char *querystr);

static int sql_select_query(SQLSOCK *sqlsocket, CONF *config, char *querystr);
static int sql_store_result(SQLSOCK *sqlsocket, CONF *config);
static int sql_num_rows(SQLSOCK *sqlsocket, CONF *config);

static int sql_fetch_row(SQLSOCK *sqlsocket, CONF *config);

static int sql_free_result(SQLSOCK *sqlsocket, CONF *config);
static int sql_finish_query(SQLSOCK *sqlsocket, CONF *config);
static int sql_finish_select_query(SQLSOCK *sqlsocket, CONF *config);
static int sql_affected_rows(SQLSOCK *sqlsocket, CONF *config);

#endif
