#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <oci.h>

#include "sql_prepare_parser.h"
#include "confparse.h"

/*typedef struct sql_socket {
	int     id;
#if HAVE_PTHREAD_H
	pthread_mutex_t mutex;
#endif
	struct sql_socket *next;
	enum { sockconnected, sockunconnected } state;

	void	*conn;
	char**   row;
} SQLSOCK;
*/
/* Each statement for each SQL */
/*typedef struct query_list_t {
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

*/
static const char * sql_error(SQLSOCK *sqlsocket,CONF *config)
{

	static char	msgbuf[512];
	sb4		errcode = 0;
	rlm_sql_oracle_sock *oracle_sock = sqlsocket->conn;
 
	if (!oracle_sock) return "rlm_sql_oracle: no connection to db";

	memset((void *) msgbuf, (int)'\0', sizeof(msgbuf));

	OCIErrorGet((dvoid *) oracle_sock->errHandle, (ub4) 1, (text *) NULL,
		&errcode, msgbuf, (ub4) sizeof(msgbuf), (ub4) OCI_HTYPE_ERROR);
	if (errcode) {
		return msgbuf;
	}
	else {
		return NULL;
	}
}

/*************************************************************************
 *
 *	Function: sql_check_error
 *
 *	Purpose: check the error to see if the server is down
 *
 *************************************************************************/
static int sql_check_error(SQLSOCK *sqlsocket, CONF *config) {

	if (strstr(sql_error(sqlsocket, config), "ORA-03113") || strstr(sql_error(sqlsocket, config), "ORA-12514")||
			strstr(sql_error(sqlsocket, config), "ORA-03114")|| strstr(sql_error(sqlsocket, config), "ORA-12528")||
			strstr(sql_error(sqlsocket, config), "ORA-00028")||strstr(sql_error(sqlsocket, config),"ORA-01012")) {
		printf("rlm_sql_oracle: OCI_SERVER_NOT_CONNECTED\n");
		return -2;
	}
	else {
		printf("rlm_sql_oracle: OCI_SERVER_NORMAL\n");
		return -1;
	}
}

static int sql_close(SQLSOCK *sqlsocket, CONF *config) {

    rlm_sql_oracle_sock *oracle_sock = sqlsocket->conn;
    sql_data_t *data_tmp = NULL;
    query_list_t *query_tmp = oracle_sock->query_list;
    query_list_t *query_next = NULL;
    sql_data_t * data_next = NULL;

    if (oracle_sock->conn) {
        OCILogoff (oracle_sock->conn, oracle_sock->errHandle);
    }

    while (query_tmp) {
        query_next = query_tmp->next;
        free(query_tmp->query);
        query_tmp->query = NULL;
        OCIHandleFree( (dvoid *)query_tmp->stmt, (ub4)OCI_HTYPE_STMT);
        query_tmp->stmt = NULL;
        data_tmp = query_tmp->data;
        while (data_tmp) {
            data_next = data_tmp->next;
            free(data_tmp);
            data_tmp = data_next;
        }
        free(query_tmp);
        query_tmp = query_next;
    }

    if (oracle_sock->queryHandle) {
        OCIHandleFree( (dvoid *)oracle_sock->queryHandle, (ub4) OCI_HTYPE_STMT);
    }
    if (oracle_sock->errHandle) {
        OCIHandleFree( (dvoid *)oracle_sock->errHandle, (ub4) OCI_HTYPE_ERROR);
    }
    if (oracle_sock->env) {
        OCIHandleFree( (dvoid *)oracle_sock->env, (ub4) OCI_HTYPE_ENV);
    }

    oracle_sock->conn = NULL;
    free(oracle_sock);
    sqlsocket->conn = NULL;

    return 0;
}

/*************************************************************************
 *
 *	Function: sql_init_socket
 *
 *	Purpose: Establish connection to the db
 *
 *************************************************************************/
static int sql_init_socket(SQLSOCK *sqlsocket, CONF *config) {
	
	rlm_sql_oracle_sock *oracle_sock;

	if (!sqlsocket->conn) {
		sqlsocket->conn = (rlm_sql_oracle_sock *)malloc(sizeof(rlm_sql_oracle_sock));
		if (!sqlsocket->conn) {
			return -1;
		}
	}
	memset(sqlsocket->conn,0,sizeof(rlm_sql_oracle_sock));

	oracle_sock = sqlsocket->conn;

	if (OCIEnvCreate(&oracle_sock->env, OCI_DEFAULT|OCI_THREADED, (dvoid *)0,
		(dvoid * (*)(dvoid *, size_t)) 0,
		(dvoid * (*)(dvoid *, dvoid *, size_t))0, 
		(void (*)(dvoid *, dvoid *)) 0,
		0, (dvoid **)0 )) {
		printf("rlm_sql_oracle: Couldn't init Oracle OCI environment (OCIEnvCreate())\n");
		sql_close(sqlsocket,config);
		return -1;
	}

	if(OCIHandleAlloc((dvoid *) oracle_sock->env, (dvoid **) &oracle_sock->conn,
		(ub4) OCI_HTYPE_SVCCTX, (size_t) 0, (dvoid **) 0))
	{
		printf("rlm_sqloracle: Couldn't init Oracle SVCCTX handle (OCIHandleAlloc())\n");
		sql_close(sqlsocket,config);
		return -1;	
	}

	if (OCIHandleAlloc((dvoid *) oracle_sock->env, (dvoid **) &oracle_sock->errHandle,
		(ub4) OCI_HTYPE_ERROR, (size_t) 0, (dvoid **) 0))
	{
		printf("rlm_sql_oracle: Couldn't init Oracle ERROR handle (OCIHandleAlloc())\n");
		sql_close(sqlsocket,config);
		return -1;
	}

	/* Allocate handles for select and update queries */
	if (OCIHandleAlloc((dvoid *)oracle_sock->env, (dvoid **) &oracle_sock->queryHandle,
				(ub4)OCI_HTYPE_STMT, (CONST size_t) 0, (dvoid **) 0))
	{
		printf("rlm_sql_oracle: Couldn't init Oracle query handles: %s\n",
			sql_error(sqlsocket, config));
		sql_close(sqlsocket,config);
		return -1;
	}


	if (OCILogon(oracle_sock->env, oracle_sock->errHandle, &oracle_sock->conn,
			config->db_user, strlen(config->db_user),
			config->db_pwd,  strlen(config->db_pwd),
			config->db_name, strlen(config->db_name)))
	{
		printf("rlm_sql_oracle: Oracle logon failed: '%s', id #%d\n", sql_error(sqlsocket, config), sqlsocket->id);
		sql_close(sqlsocket,config);
		return -1;
	}

	return 0;
}


/*************************************************************************
 *
 *	Function: sql_destroy_socket
 *
 *	Purpose: Free socket and private connection data
 *
 *************************************************************************/
static int sql_destroy_socket(SQLSOCK *sqlsocket, CONF *config)
{
	free(sqlsocket->conn);
	sqlsocket->conn = NULL;
	return 0;
}

/*************************************************************************
 *
 *	Function: sql_num_fields
 *
 *	Purpose: database specific num_fields function. Returns number
 *               of columns from query
 *
 *************************************************************************/
static int sql_num_fields(SQLSOCK *sqlsocket, CONF *config) {

	ub4		count;
	rlm_sql_oracle_sock *oracle_sock = sqlsocket->conn;

	/* get the number of columns in the select list */ 
	if (OCIAttrGet ((dvoid *)oracle_sock->queryHandle,
			(ub4)OCI_HTYPE_STMT,
			(dvoid *) &count,
			(ub4 *) 0,
			(ub4)OCI_ATTR_PARAM_COUNT,
			oracle_sock->errHandle)) {
		printf("rlm_sql_oracle: Error retrieving column count in sql_num_fields: %s\n",
			sql_error(sqlsocket, config));
		return -1;
	}
	return count;
}

static int get_fields_num(char *query)
{
    char *pos = NULL;
    char buf[MAX_SQL_LEN];
    char *cur = buf;
    int i = 0;
    int len = strlen(query);
    int num = 1;

    memset(buf, 0, sizeof(buf) );
    strncpy(buf, query, len);

    for ( ; i < len; ++i)
        buf[i] = tolower(buf[i]);

    pos = strstr(buf, "from");
    if (pos == NULL)
        return -1;

    for ( ; cur < pos; )
        if (*cur++ == ',')
            ++num;

    return num;
}

static int sql_prepare(SQLSOCK *sqlsocket, CONF *config, char *origin_sql) {

    char **rowdata = NULL;
    char sqlbuf[MAX_SQL_LEN];
    int col = 0;
    int colcount = 0;
    int len = 0;
    int ret = 0;
    OCIBind *bind = NULL;
    OCIDefine *define = NULL;
    sb2 *indicators = NULL;
    query_list_t *query_node = NULL;
    sql_data_t *data_tmp = NULL;
    rlm_sql_oracle_sock *oracle_sock = sqlsocket->conn;

    if (oracle_sock->conn == NULL) {
        printf("sql_prepare: Socket is not connecting.\n");
        return -1;
    }

    memset(sqlbuf, 0, sizeof(sqlbuf) );
    query_node = (query_list_t *)malloc(sizeof(query_list_t) );
    memset(query_node, 0, sizeof(query_list_t) );

    // 1: translate SQL into binding format
    ret = sql_translate(origin_sql, sqlbuf, &(query_node->data) );
    if (ret != 0) {
        if(query_node)
            free(query_node);
        return -1;
    }

    printf("sql_prepare: SQL after translate: %s\n", sqlbuf);

    len = strlen(sqlbuf);
    query_node->query = (char *)malloc(len + 1);
    memset(query_node->query, 0, len + 1);
    memcpy(query_node->query, sqlbuf, len);

   // 2: prepare the binding format SQL
    if (OCIHandleAlloc( (dvoid *)oracle_sock->env,
                        (dvoid **)&query_node->stmt,
                        (ub4)OCI_HTYPE_STMT,
                        (CONST size_t)0,
                        (dvoid **)0) ) {
        printf("sql_prepare: OCIStmtPrepare failed, %s\n", sql_error(sqlsocket, config) );
        free(query_node->query);
        free(query_node);
        return -1;
    }

    if (OCIStmtPrepare(query_node->stmt,
                       oracle_sock->errHandle,
                       query_node->query,
                       strlen(query_node->query),
                       OCI_NTV_SYNTAX,
                       OCI_DEFAULT) ) {
        printf("sql_prepare: OCIStmtPrepare failed, %s\n", sql_error(sqlsocket, config) );
        OCIHandleFree( (dvoid *)query_node->stmt, (ub4)OCI_HTYPE_STMT);
        free(query_node->query);
        free(query_node);
        return -1;
    }

    // 3: bind database input data
    data_tmp = query_node->data;
    while(data_tmp) {
        if(data_tmp->type == DB_UIN) {
            if (OCIBindByPos(query_node->stmt,
                             &bind,
                             oracle_sock->errHandle,
                             data_tmp->index,
                             (dvoid *)&(data_tmp->data.data_num),
                             sizeof(unsigned int),
                             SQLT_UIN,
                             0,
                             0,
                             0,
                             0,
                             0,
                             OCI_DEFAULT) ) {
                printf( "sql_prepare: SQLBindParameter failed, %s\n", sql_error(sqlsocket, config) );
                OCIHandleFree( (dvoid *)query_node->stmt, (ub4)OCI_HTYPE_STMT);
                free(query_node->query);
                free(query_node);
                return -1;
            }
        } else if (data_tmp->type == DB_NUM) {
            if (OCIBindByPos(query_node->stmt,
                             &bind,
                             oracle_sock->errHandle,
                             data_tmp->index,
                             (dvoid *)&(data_tmp->data.data_double),
                             sizeof(double),
                             SQLT_FLT,
                             0,
                             0,
                             0,
                             0,
                             0,
                             OCI_DEFAULT) ) {
                printf("sql_prepare: SQLBindParameter failed, %s\n", sql_error(sqlsocket, config) );
                OCIHandleFree( (dvoid *)query_node->stmt, (ub4)OCI_HTYPE_STMT);
                free(query_node->query);
                free(query_node);
                return -1;
            }
        } else if (data_tmp->type == DB_STR) {
            if (OCIBindByPos(query_node->stmt,
                             &bind,
                             oracle_sock->errHandle,
                             data_tmp->index,
                             (dvoid *)data_tmp->data.data_str,
                             sizeof(data_tmp->data.data_str),
                             SQLT_STR,
                             0,
                             0,
                             0,
                             0,
                             0,
                             OCI_DEFAULT) ) {
                printf("sql_prepare: SQLBindParameter failed, %s\n", sql_error(sqlsocket, config) );
                OCIHandleFree( (dvoid *)query_node->stmt, (ub4)OCI_HTYPE_STMT);
                free(query_node->query);
                free(query_node);
                return -1;
            }
        } else {
            /* FIXME:
             *   it should not only support char string and number, but also
             * date and so on, sooner or later we should support it
             */
            printf("sql_prepare: unknown data type.\n");
            OCIHandleFree( (dvoid *)query_node->stmt, (ub4)OCI_HTYPE_STMT);
            free(query_node->query);
            free(query_node);
            return -1;
        }
        data_tmp = data_tmp->next;
    }

    // 4: bind database output data
    colcount = get_fields_num(origin_sql);
    if (colcount > 0) {
        rowdata = (char **)malloc(sizeof(char *) * (colcount + 1) );
        memset(rowdata, 0, (sizeof(char *) * (colcount + 1) ) );
        for (col = 0; col < colcount; ++col) {
            // FIXME: different datatype should have differennt length for saving memory space
            rowdata[col] = (char *)malloc(sizeof(char) * MAX_COLUMN_LEN);
            memset(rowdata[col], 0, sizeof(char) * MAX_COLUMN_LEN);
        }

        indicators = (sb2 *)malloc(sizeof(sb2) * (colcount + 1) );
        memset(indicators, 0, sizeof(sb2) * (colcount + 1) );

        for (col = 0; col < colcount; ++col) {
            indicators[col] = 0;
            ret = OCIDefineByPos(query_node->stmt,
                                 &define,
                                 oracle_sock->errHandle,
                                 col + 1,
                                 (ub1 *)rowdata[col],
                                 MAX_COLUMN_LEN,
                                 SQLT_STR,
                                 &indicators[col],
                                 (dvoid *)0,
                                 (dvoid *)0,
                                 OCI_DEFAULT);
            if (ret != OCI_SUCCESS) {
                printf("sql_prepare: OCIDefineByPos failed: %s\n", sql_error(sqlsocket, config) );
                for (col = 0; col < colcount; ++col)
                    free(rowdata[col]);
                free(rowdata);
                free(indicators);
                OCIHandleFree( (dvoid *)query_node->stmt, (ub4)OCI_HTYPE_STMT);
                free(query_node->query);
                free(query_node);
                return -1;
            }
        }

        rowdata[col] = NULL;

        query_node->results = rowdata;
        query_node->indicators = indicators;
    }

    query_node->next = oracle_sock->query_list;
    oracle_sock->query_list = query_node;

    return 0;
}

static int sql_query_exec(SQLSOCK *sqlsocket, CONF *config) {

    int ret = 0;
    rlm_sql_oracle_sock *oracle_sock = sqlsocket->conn;

    if (oracle_sock->conn == NULL) {
        printf("sql_direct_query: Socket is not connecting.\n");
        return -1;
    }

    ret = OCIStmtExecute(oracle_sock->conn,
                         oracle_sock->queryHandle,
                         oracle_sock->errHandle,
                         (ub4)1,
                         (ub4)0,
                         (OCISnapshot *)NULL,
                         (OCISnapshot *)NULL,
                         (ub4)OCI_COMMIT_ON_SUCCESS);
    if (ret == OCI_SUCCESS)
        return 0;
    else if (ret == OCI_ERROR) {
        printf("sql_query_exec: OCIStmtExecute failed: %s\n", sql_error(sqlsocket, config) );
        return sql_check_error(sqlsocket, config);
    }

    return -1;
}

static int sql_select_query_exec(SQLSOCK *sqlsocket, CONF *config) {

    int ret = 0;
    rlm_sql_oracle_sock *oracle_sock = sqlsocket->conn;

    if (oracle_sock->conn == NULL) {
        printf("sql_direct_query: Socket is not connecting.\n");
        return -1;
    }

    ret = OCIStmtExecute(oracle_sock->conn,
                         oracle_sock->queryHandle,
                         oracle_sock->errHandle,
                         (ub4)0,
                         (ub4)0,
                         (OCISnapshot *)NULL,
                         (OCISnapshot *)NULL,
                         (ub4)OCI_COMMIT_ON_SUCCESS);
    if (ret == OCI_SUCCESS)
        return 0;
    else if (ret == OCI_ERROR) {
        printf("sql_select_query_exec: OCIStmtExecute failed: %s\n", sql_error(sqlsocket, config) );
        return sql_check_error(sqlsocket, config);
    }

    return -1;
}

static int sql_direct_query(SQLSOCK *sqlsocket, CONF *config, char *querystr) {

    rlm_sql_oracle_sock *oracle_sock = sqlsocket->conn;

    if (oracle_sock->conn == NULL) {
        printf("sql_direct_query: Socket is not connecting.\n");
        return -1;
    }

    if (OCIStmtPrepare(oracle_sock->queryHandle,
                       oracle_sock->errHandle,
                       querystr,
                       strlen(querystr),
                       OCI_NTV_SYNTAX,
                       OCI_DEFAULT) ) {
        printf("sql_direct_query: OCIStmtPrepare failed: %s\n", sql_error(sqlsocket, config) );
        return -1;
    }

    return sql_query_exec(sqlsocket, config);
}

static int sql_prepare_query(SQLSOCK *sqlsocket, CONF *config, char *querystr) {

    char mod_sql[MAX_SQL_LEN];
    int found_flag = 0;
    rlm_sql_oracle_sock *oracle_sock = sqlsocket->conn;
    query_list_t *query_node = oracle_sock->query_list;
    sql_data_t *data = NULL;
    sql_data_t *data_tmp = NULL;
    sql_data_t *datap = NULL;

    if (oracle_sock->conn == NULL) {
        printf("sql_prepare_query: Socket is not connecting.\n");
        return -1;
    }

    /* firstly, translate final SQL to template format(include :) */
    if(sql_translate(querystr, mod_sql, &data) )
        return 1;

    printf("sql_prepare_query: SQL after translate: %s\n", querystr);

    /*
     *   secondly, try to find SQL template format in preservative query
     * list, if it is not found, add it to the list, which can be used
     * next time
     */
    while (query_node) {
        if (strcmp(query_node->query, mod_sql) == 0) {
            datap = query_node->data;
            data_tmp = data;
            while(data_tmp) {
                if (data_tmp->type == DB_STR) {
                    memset(datap->data.data_str, 0, sizeof(datap->data.data_str) );
                    memcpy(datap->data.data_str, data_tmp->data.data_str,
                        sizeof(data_tmp->data.data_str) );
                } else if (data_tmp->type == DB_UIN) {
                    datap->data.data_num = data_tmp->data.data_num;
                } else if (data_tmp->type == DB_NUM) {
                    datap->data.data_double = data_tmp->data.data_double;
                } else {
                    /*
                     * FIXME:
                     *   it should really be supported more than number and string
                     */
                    printf("sql_prepare_query: Unsupported data type.\n");
                    while (data) {
                        data_tmp = data->next;
                        free(data);
                        data = data_tmp;
                    }
                    return 1;
                }
                data_tmp = data_tmp->next;
                datap = datap->next;
            }
            found_flag = 1;
            break;
        }
        query_node = query_node->next;
    }

    while (data) {
        data_tmp = data->next;
        free(data);
        data = data_tmp;
    }

    if (found_flag == 0) {
        if(sql_prepare(sqlsocket, config, querystr) )
            return 1;

        query_node = oracle_sock->query_list;
        while (query_node) {
            if (strcmp(query_node->query, mod_sql) == 0)
                break;
            query_node = query_node->next;
        }
    }

    if(query_node != NULL) {
        oracle_sock->queryHandle = query_node->stmt;
        oracle_sock->results = query_node->results;
        oracle_sock->indicators = query_node->indicators;
        oracle_sock->is_direct = 0;
    }

    return 0;
}

static int sql_query(SQLSOCK *sqlsocket, CONF *config, char *querystr) {

	int	ret = 0;
	rlm_sql_oracle_sock *oracle_sock = sqlsocket->conn;

	if (oracle_sock->conn == NULL) {
		printf("sql_query: Socket is not connecting.\n");
		return -1;
	}

    ret = sql_prepare_query(sqlsocket, config, querystr);
    if (ret < 0)
        return -1;
    else if (ret == 1) {
        oracle_sock->is_direct = 1;
        return sql_direct_query(sqlsocket, config, querystr);
    }

    return  sql_query_exec(sqlsocket, config);
}

static int sql_direct_select_query(SQLSOCK *sqlsocket, CONF *config, char *querystr) {

    char **rowdata = NULL;
    int col = 0;
    int colcount = 0;
    int ret = 0;
    OCIDefine *define = NULL;
    OCIParam *param = NULL;
    sb2 *indicators;
    ub2 dsize;
    ub2 dtype;
    rlm_sql_oracle_sock *oracle_sock = sqlsocket->conn;

    if (oracle_sock->conn == NULL) {
        printf("sql_direct_select_query: Socket is not connecting.\n");
        return -1;
    }

    if (OCIStmtPrepare(oracle_sock->queryHandle,
                       oracle_sock->errHandle,
                       querystr,
                       strlen(querystr),
                       OCI_NTV_SYNTAX,
                       OCI_DEFAULT) ) {
        printf("sql_direct_select_query: OCIStmtPrepare failed: %s\n", sql_error(sqlsocket, config) );
        return -1;
    }

    sql_select_query_exec(sqlsocket, config);

    colcount = sql_num_fields(sqlsocket, config);
    if (colcount < 0)
        return -1;

    rowdata = (char **)malloc(sizeof(char *) * (colcount + 1) );
    memset(rowdata, 0, (sizeof(char *) * (colcount + 1) ) );
    indicators = (sb2 *)malloc(sizeof(sb2) * (colcount + 1) );
    memset(indicators, 0, sizeof(sb2) * (colcount + 1) );

    for (col = 1; col <= colcount; ++col) {
        ret = OCIParamGet(oracle_sock->queryHandle,
                          OCI_HTYPE_STMT,
                          oracle_sock->errHandle,
                          (dvoid **)&param,
                          (ub4)col);
        if (ret != OCI_SUCCESS) {
            printf("sql_select_query: OCIParamGet() failed: %s\n", sql_error(sqlsocket, config) );
            return -1;
        }

        ret = OCIAttrGet( (dvoid*)param,
                          OCI_DTYPE_PARAM,
                          (dvoid*)&dtype,
                          (ub4*)0,
                          OCI_ATTR_DATA_TYPE,
                          oracle_sock->errHandle);
        if (ret != OCI_SUCCESS) {
            printf("sql_select_query: OCIAttrGet() failed: %s\n", sql_error(sqlsocket, config) );
            return -1;
        }

        dsize = 64;

        /*
         * Use the retrieved length of dname to allocate an output
         * buffer, and then define the output variable (but only
         * for char/string type columns).
         */
        switch(dtype) {
#ifdef SQLT_AFC
        case SQLT_AFC: /* ansii fixed char */
#endif
#ifdef SQLT_AFV
        case SQLT_AFV: /* ansii var char */
#endif
        case SQLT_VCS: /* var char */
        case SQLT_CHR: /* char */
        case SQLT_STR: /* string */
            ret = OCIAttrGet( (dvoid *)param,
                              (ub4) OCI_DTYPE_PARAM,
                              (dvoid *)&dsize,
                              (ub4 *)0,
                              (ub4) OCI_ATTR_DATA_SIZE,
                              oracle_sock->errHandle);
            if (ret != OCI_SUCCESS) {
                printf("sql_select_query: OCIAttrGet() failed: %s\n", sql_error(sqlsocket, config) );
                return -1;
            }

            rowdata[col - 1]=malloc(dsize + 1);
            memset(rowdata[col - 1], 0, dsize + 1);
            break;
        case SQLT_DAT:
        case SQLT_INT:
        case SQLT_UIN:
        case SQLT_FLT:
        case SQLT_PDN:
        case SQLT_BIN:
        case SQLT_NUM:
            rowdata[col - 1]=malloc(dsize + 1);
            memset(rowdata[col - 1], 0, dsize + 1);
            break;
        default:
            dsize = 0;
            rowdata[col - 1] = NULL;
            break;
        }

        indicators[col - 1] = 0;
        ret = OCIDefineByPos(oracle_sock->queryHandle,
                             &define,
                             oracle_sock->errHandle,
                             col,
                             (ub1 *)rowdata[col - 1],
                             dsize + 1,
                             SQLT_STR,
                             &indicators[col - 1],
                             (dvoid *)0,
                             (dvoid *)0,
                             OCI_DEFAULT);
        /* FIXME: memory leaks of indicators & rowdata? */
        if (ret != OCI_SUCCESS) {
            printf("sql_select_query: OCIDefineByPos() failed: %s\n", sql_error(sqlsocket, config) );
            return -1;
        }
    }

    rowdata[col - 1] = NULL;

    oracle_sock->results = rowdata;
    oracle_sock->indicators = indicators;

    return 0;
}

static int sql_select_query(SQLSOCK *sqlsocket, CONF *config, char *querystr) {

    int ret = 0;
    rlm_sql_oracle_sock *oracle_sock = sqlsocket->conn;

    ret = sql_prepare_query(sqlsocket, config, querystr);
    if (ret < 0)
        return -1;
    else if (ret == 1) {
        oracle_sock->is_direct = 1;
        return sql_direct_select_query(sqlsocket, config, querystr);
    }

    return sql_select_query_exec(sqlsocket, config);
}

/*************************************************************************
 *
 *	Function: sql_store_result
 *
 *	Purpose: database specific store_result function. Returns a result
 *               set for the query.
 *
 *************************************************************************/
static int sql_store_result(SQLSOCK *sqlsocket, CONF *config) {
	/* Not needed for Oracle */
	return 0;
}

/*************************************************************************
 *
 *	Function: sql_num_rows
 *
 *	Purpose: database specific num_rows. Returns number of rows in
 *               query
 *
 *************************************************************************/
static int sql_num_rows(SQLSOCK *sqlsocket, CONF *config) {

	ub4	rows=0;
	rlm_sql_oracle_sock *oracle_sock = sqlsocket->conn;

	OCIAttrGet((CONST dvoid *)oracle_sock->queryHandle,
			OCI_HTYPE_STMT,
			(dvoid *)&rows, 
			(ub4 *) sizeof(ub4),
			OCI_ATTR_ROW_COUNT,
			oracle_sock->errHandle);

	return rows;
}


/*************************************************************************
 *
 *	Function: sql_fetch_row
 *
 *	Purpose: database specific fetch_row. Returns a SQL_ROW struct
 *               with all the data for the query in 'sqlsocket->row'. Returns
 *		 0 on success, -1 on failure, SQL_DOWN if database is down.
 *
 *************************************************************************/
static int sql_fetch_row(SQLSOCK *sqlsocket, CONF *config) {

	int	x;
	rlm_sql_oracle_sock *oracle_sock = sqlsocket->conn;

	if (oracle_sock->conn == NULL) {
		printf("rlm_sql_oracle: Socket not connected\n");
		return -2;
	}

	sqlsocket->row = NULL;
	x=OCIStmtFetch(oracle_sock->queryHandle,
			oracle_sock->errHandle,
			1,
			OCI_FETCH_NEXT,
			OCI_DEFAULT);

	if (x == OCI_NO_DATA) {
		return 0;
	} else if (x == OCI_SUCCESS) {
		sqlsocket->row = oracle_sock->results;
		return 0;
	} else if (x == OCI_ERROR) {
		printf("rlm_sql_oracle: fetch failed in sql_fetch_row: %s\n",
				sql_error(sqlsocket, config));
		return sql_check_error(sqlsocket, config);
	}
	return -1;
}



/*************************************************************************
 *
 *	Function: sql_free_result
 *
 *	Purpose: database specific free_result. Frees memory allocated
 *               for a result set
 *
 *************************************************************************/
static int sql_free_result(SQLSOCK *sqlsocket, CONF *config) {

	int x;
	int num_fields;

	rlm_sql_oracle_sock *oracle_sock = sqlsocket->conn;

	/* Cancel the cursor first */
	x=OCIStmtFetch(oracle_sock->queryHandle,
			oracle_sock->errHandle,
			0,
			OCI_FETCH_NEXT,
			OCI_DEFAULT);
	
	num_fields = sql_num_fields(sqlsocket, config);
	if (num_fields >= 0) {
		for(x=0; x < num_fields; x++) {
			free(oracle_sock->results[x]);
		}
		free(oracle_sock->results);
		free(oracle_sock->indicators);
	}

	if (oracle_sock->is_direct)
		OCIHandleFree( (dvoid *)oracle_sock->queryHandle, (ub4)OCI_HTYPE_STMT);

	oracle_sock->results=NULL;
	return 0;
}



/*************************************************************************
 *
 *	Function: sql_finish_query
 *
 *	Purpose: End the query, such as freeing memory
 *
 *************************************************************************/
static int sql_finish_query(SQLSOCK *sqlsocket, CONF *config)
{
	return 0;
}



/*************************************************************************
 *
 *	Function: sql_finish_select_query
 *
 *	Purpose: End the select query, such as freeing memory or result
 *
 *************************************************************************/
static int sql_finish_select_query(SQLSOCK *sqlsocket, CONF *config) {

	int 	x=0;
	rlm_sql_oracle_sock *oracle_sock = sqlsocket->conn;

	if (oracle_sock && oracle_sock->is_direct && oracle_sock->results) {
		while(oracle_sock->results[x]) free(oracle_sock->results[x++]);
		free(oracle_sock->results);
		oracle_sock->results=NULL;
		free(oracle_sock->indicators);
        oracle_sock->indicators = NULL;
	}

	return 0;
}


/*************************************************************************
 *
 *	Function: sql_affected_rows
 *
 *	Purpose: Return the number of rows affected by the query (update,
 *               or insert)
 *
 *************************************************************************/
static int sql_affected_rows(SQLSOCK *sqlsocket, CONF *config) {

	return sql_num_rows(sqlsocket, config);
}
