#include "mongo_sql.h"

NOSQL_SOCK *sql_pool = NULL;
char *single_quotes_to_double (const char *str)
{
	char *result = bson_strdup (str);
	char *p;

	for (p = result; *p; p++) {
		if (*p == '\'') {
			*p = '"';
		}
	}

	return result;
}

bson_t *tmp_bson(const char *json,...)
{
	va_list args;
	bson_error_t error;
	char *formatted;
	char *double_quoted;
	bson_t *doc;

	if (json) {
		va_start(args,json);
		formatted = bson_strdupv_printf(json,args);
		va_end(args);
		double_quoted = single_quotes_to_double(formatted);
		doc = bson_new_from_json((const uint8_t *)double_quoted,-1,&error);
        
		if (!doc) {
		    fprintf(stderr,"err:%s,'%s'\n",error.message,double_quoted);
		    return NULL;	
		}
	    bson_free(formatted);
	    bson_free(double_quoted);
	}
#if DEBUG
	printf("tmpbson:%s\n",bson_as_canonical_extended_json(doc,NULL));
#endif
	return doc;
}

int fetch_data(const bson_t *doc,char *field,char **output,int type)
{
	bson_iter_t iter;
    
	if(!bson_iter_init_find(&iter,doc,field)) {
	    printf("fetch_data not-found key\n");
		free(*output);
		*output = NULL;
		return 0;
	}
	uint32_t len = 0;
	const char *value = NULL;
	int value2 = -1;

#if 0
	value = bson_iter_code(&iter,&len);
    strncpy(*output,value,len);
	printf("len:%d\n",len);
    
	return 0;
#endif

	switch (type) {
    case STRING :
	    value = bson_iter_utf8(&iter,&len);
		if (value)
	        strncpy(*output,value,len);
		else {
			free(*output);
			*output = NULL;
		} 
		// printf("len:%d\n",len);
	    break;
	
	case INTEGER:
	    value2 = bson_iter_int32(&iter);

		break;

	default:
	    break;
	}
    
	return 0;
}

int nosql_init_socket(CONF *config,NOSQL_SOCK *nosql_sock)
{
    // config->server :mongodb://aaa3:aaa3@172.16.31.134:27017/?authSource=aaa3
    //printf(L_INFO,"rlm_nosql :ready to connect mongodb\n");
    bson_error_t error;
    NOSQL_MONGO *mongo_sock;

    if (!nosql_sock->connect_handle) {
        nosql_sock->connect_handle = (NOSQL_MONGO *)malloc(sizeof(NOSQL_MONGO));
        if (!nosql_sock->connect_handle) {
            return -1;
        }
    }
    mongo_sock = (NOSQL_MONGO *)nosql_sock->connect_handle;
	memset(mongo_sock,0x00,sizeof(NOSQL_MONGO));

    mongoc_init();
    mongo_sock->uri = mongoc_uri_new_with_error(config->db_ip, &error);
    if (!mongo_sock->uri) {
      printf("mongodb: nosql_init_socket failed to parse URI:%s ,err:%s\n",config->db_ip,error.message);
      nosql_destroy_socket(config,nosql_sock);
	  return -1;
    }

    mongo_sock->conn = mongoc_client_new_from_uri(mongo_sock->uri);
    if (!mongo_sock->conn) {
	    printf("mongodb: nosql_init_socket failed to new client from uri\n");
        nosql_destroy_socket(config,nosql_sock);
	    return -1;
    }
    /*MONGOC_ERROR_API_VERSION_LEGACY  or MONGOC_ERROR_API_VERSION_2 */
    mongoc_client_set_error_api(mongo_sock->conn, 2);
    mongo_sock->db = mongoc_client_get_database(mongo_sock->conn, config->db_name);
    mongo_sock->collection = NULL;

    char *str = NULL;
	bson_t ping,reply;
	bool r;
    bson_init (&ping);
    bson_append_int32 (&ping, "ping", 4, 1); 
		     
    r = mongoc_database_command_with_opts(mongo_sock->db, &ping, NULL, NULL, &reply, &error);
    if (r) {
        str = bson_as_canonical_extended_json (&reply, NULL);
        //printf(L_DBG,"mongodb :Ping success,%s\n", str);
        bson_free (str);
    } else {
        printf("mongodb :Ping failure, %s,return -1\n", error.message);
        bson_destroy (&ping);
	    bson_destroy (&reply);
        nosql_destroy_socket(config,nosql_sock);
	    return -1;    
	}   
    bson_destroy (&ping);
	bson_destroy (&reply);

	return 0;
}

int nosql_destroy_socket(CONF *config,NOSQL_SOCK *nosql_sock)
{
	if(!nosql_sock){
		return -1;
	}

	NOSQL_MONGO *mongo_sock;
	mongo_sock = nosql_sock->connect_handle;
	if (mongo_sock == NULL){
		return -1;
	}
    
	if (mongo_sock->collection) {
		mongoc_collection_destroy (mongo_sock->collection);
	}
	mongo_sock->collection = NULL;

	if (mongo_sock->db) {
		mongoc_database_destroy (mongo_sock->db);
	}
	mongo_sock->db = NULL;

	if (mongo_sock->conn) {
		mongoc_client_destroy (mongo_sock->conn);
	}
	mongo_sock->conn = NULL;
    
	if (mongo_sock->uri) {
	    mongoc_uri_destroy(mongo_sock->uri);
	}
    mongo_sock->uri = NULL;
	mongoc_cleanup();
	
	free(nosql_sock->connect_handle);
	nosql_sock->connect_handle = NULL;

	return 0;
}

void release_collection(NOSQL_SOCK *nosql_sock){
	NOSQL_MONGO *mongo_sock = (NOSQL_MONGO *)nosql_sock->connect_handle;
	if(!mongo_sock){
		return;
	}
	if (mongo_sock->collection) {
		mongoc_collection_destroy (mongo_sock->collection);
	}
	mongo_sock->collection = NULL;

	return;
}

/*************************************************************************
 *
 *	Function: nosql_select_count
 *
 *	param:  setstr   {'$and':[{'user_name':'test'},{'calling_id':'123456'}]}) 
 *
 *	Purpose: Do query count operations from mongodb
 * 
*************************************************************************/
int nosql_select_count(NOSQL_SOCK *nosql_sock,const char *setstr)
{
	bson_error_t error;
	bson_t *query;

	NOSQL_MONGO *mongo_sock = (NOSQL_MONGO *)nosql_sock->connect_handle;
	if(!mongo_sock){
		printf("nosql_select_count: nosql_sock->connect_handle is NULL\n");
		return -1;
	}
	if(strlen(nosql_sock->table_name) != 0)
		mongo_sock->collection = mongoc_database_get_collection(mongo_sock->db, nosql_sock->table_name);
	else{
		printf("nosql_select_query: nosql_sock->table_name is NULL\n");
		return -1;
	}

	query = tmp_bson(setstr);
	
	// int64_t count = mongoc_collection_count(mongo_sock->collection, MONGOC_QUERY_NONE, query, 0, 0, NULL, &error);
	int64_t count = mongoc_collection_count_documents(mongo_sock->collection, query, NULL, NULL, NULL, &error);
	if (count < 0) {
		int ret = nosql_error_judge("nosql_select_count",error);
		release_collection(nosql_sock);
		bson_free(query);

		return ret;
	}
	
	char **row = (char **) malloc(sizeof(char *) * 1);
	row[0] = (char *) malloc(256);
	memset(row[0],0x00,256);

	sprintf(row[0],"%d",count);
	nosql_sock->nosql_row = row;

	release_collection(nosql_sock);
	bson_free(query);

	return 0;
}

int nosql_error_judge(const char *msg,bson_error_t error)
{
	switch (error.domain){
		case MONGOC_ERROR_SERVER:{
			printf("%s: mongo check :%s,return -1\n",msg,error.message);
			return -1;
		}
		case MONGOC_ERROR_SERVER_SELECTION:{
			printf("%s: mongo check :%s,return NOSQL_DOWN\n",msg,error.message);
			return -2;
		}
		default:{
			printf("%s: mongo check :%s, error code is %d,return -1\n",msg,error.message,error.domain);
			return -1;
		}
	}
}


/*************************************************************************
 *
 *	Function: nosql_select_query
 *
 *	param:  getstr   {'$and':[{'user_name':'test'},{'calling_id':'123456'}]}) 
 *			fieldss  fieldss[0] = "session_id"; fieldss[1] = "user_name";
 *			connid    no used
 *
 *	Purpose: Do query operations from mongo
 * 
*************************************************************************/
int nosql_select_query(NOSQL_SOCK *nosql_sock,const char *getstr,char **fieldss,int connid)
{
	bson_t *query,*fields;
	mongoc_cursor_t *cursor;
	const bson_t *doc;
	bson_error_t error;
	int idflag = 0, i = 0;
	char fields_t[256] = {"\0"},tmp[256] = {"\0"};
	char *str = NULL;

	if(!nosql_sock){
		return -1;
	}

	NOSQL_MONGO *mongo_sock = (NOSQL_MONGO *)nosql_sock->connect_handle;
	if(!mongo_sock){
		return -1;
	}

	if(strlen(nosql_sock->table_name) != 0)
		mongo_sock->collection = mongoc_database_get_collection(mongo_sock->db, nosql_sock->table_name);
	else{
		printf("nosql_select_query: nosql_sock->table_name is NULL\n");
		return -1;
	}

	//Determine if fieldss contains '_id'
	for(i = 0; i < nosql_sock->numfields; i++){
		if(strcmp(fieldss[i],"_id") == 0)
			idflag = 1;
	}
	//nosql_sock->numfields = i;

	if(idflag == 0){
		strcpy(fields_t,"{'projection':{'_id':0");
	}else{
		strcpy(fields_t,"{'projection':{'_id':1");
	}

	for(i = 0; i < nosql_sock->numfields; i++){
		memset(tmp,0x00,sizeof(tmp));
		if(strcmp(fieldss[i],"_id") == 0) continue;
		sprintf(tmp,",'%s':1",fieldss[i]);
		strcat(fields_t,tmp);
	}
	strcat(fields_t,"}}");

	query = tmp_bson(getstr);
	fields = tmp_bson(fields_t);
	if(query == NULL || fields == NULL){
		printf("nosql_select_query: tmp_bson parse error\n");
		if(!query)bson_free(query);
		if(!fields)bson_free(fields);
		return -1;
	}

	char **row = (char **) malloc(sizeof(char *) * nosql_sock->numfields);
	for(i = 0; i < nosql_sock->numfields; i++) {
		row[i] = (char *) malloc(256);
		memset(row[i],0x00,256);
	}

	cursor = mongoc_collection_find_with_opts(mongo_sock->collection, query, fields, NULL);
	while(mongoc_cursor_next(cursor,&doc)) {
		str = bson_as_canonical_extended_json(doc,NULL);
		for(i = 0;i < nosql_sock->numfields; i++){	
			fetch_data(doc,fieldss[i],&(row[i]),0); 
		}
		bson_free(str);
	}
	if (mongoc_cursor_error(cursor,&error)) {
		int ret = nosql_error_judge("nosql_select_query",error);
		release_collection(nosql_sock);
		bson_free(query);
		bson_free(fields); 
		mongoc_cursor_destroy(cursor);
		return ret;
  	}

	//select is  '',isn't null
	for(i = 0; i < nosql_sock->numfields; i++) {
		if(row[i] != NULL){
			if(strlen(row[i]) == 0){
				free(row[i]);
				row[i] = NULL;
			}
		}	
	}
	nosql_sock->nosql_row = row;

	release_collection(nosql_sock);
	bson_free(query);
	bson_free(fields);
	mongoc_cursor_destroy(cursor);

	return 0;
}

/*************************************************************************
 *
 *	Function: nosql_exec_direct
 *
 *	param:  setstr   {'$and':[{'user_name':'test'},{'calling_id':'123456'}]}) 
 *			updatestr  {'$set':{'_id':'xxx','user_id':'%s','}
 *			connid    no used
 *
 *	Purpose: Do update operations from mongo
 * 
*************************************************************************/
int nosql_exec_direct(NOSQL_SOCK *nosql_sock,const char *setstr,const char *updatestr,int connid)
{
	bson_t *query,*update,*opts;
	bson_t reply;
	bson_error_t error;
	char opts_t[256] ="{'upsert':true}";
	NOSQL_MONGO *mongo_sock;
	
	mongo_sock = (NOSQL_MONGO *)nosql_sock->connect_handle;
	if (!mongo_sock || (mongo_sock && !mongo_sock->db)){
		printf("nosql_exec_direct: nosql_sock->connect_handle is NULL\n");
		return -1;
	}

	if(strlen(nosql_sock->table_name) != 0)
		mongo_sock->collection = mongoc_database_get_collection (mongo_sock->db, nosql_sock->table_name);
	else{
		printf("nosql_exec_direct: nosql_sock->table_name is NULL\n");
		return -1;
	}

	update = tmp_bson(updatestr);
	opts = tmp_bson(opts_t);
	query = tmp_bson(setstr);

	if (!mongoc_collection_update_one(mongo_sock->collection,query,update,opts,&reply,&error)) {
		int ret = nosql_error_judge("nosql_exec_direct",error);
		release_collection(nosql_sock);
		bson_free(query);
		bson_free(update);
		bson_free(opts);
		return ret;
	}
	printf("nosql_exec_direct reply:%s\n",bson_as_canonical_extended_json(&reply,NULL));

	release_collection(nosql_sock);
	bson_free(query);
	bson_free(update);
	bson_free(opts);

	return 0;
}

/*************************************************************************
 *
 *	Function: nosql_exec_del_direct
 *
 *	param:  setstr   {'$and':[{'user_name':'test'},{'calling_id':'123456'}]}) 
 *
 *	Purpose: Do delete operations from mongo
 * 
*************************************************************************/
int nosql_exec_del_direct(NOSQL_SOCK *nosql_sock,const char *setstr)
{
	NOSQL_MONGO *mongo_sock;
	bson_t *query;
	bson_t reply;
	bson_error_t error;

	mongo_sock = (NOSQL_MONGO *)nosql_sock->connect_handle; 
	if (!mongo_sock){
		printf("nosql_exec_del_direct: nosql_sock->connect_handle is NULL\n");
		return -1;
	}

	if(strlen(nosql_sock->table_name) != 0)
		mongo_sock->collection = mongoc_database_get_collection (mongo_sock->db, nosql_sock->table_name);
	else{
		printf("nosql_exec_del_direct: nosql_sock->table_name is NULL\n");
		return -1;
	}

	query = tmp_bson(setstr);

	if (!mongoc_collection_delete_one(mongo_sock->collection,query,NULL,&reply,&error)) {
		int ret = nosql_error_judge("nosql_exec_del_direct",error);
		bson_free(query);
		release_collection(nosql_sock);
		return ret;
	}
    printf("nosql_exec_del_direct reply:%s\n",bson_as_canonical_extended_json(&reply,NULL));

    bson_free(query);
	release_collection(nosql_sock);

    return 0;
}

int nosql_free_result(NOSQL_SOCK *nosql_sock)
{
	if (nosql_sock->nosql_row) {
	    int i = 0;
	    for(i = 0;i<nosql_sock->numfields;i++) {
		    if (nosql_sock->nosql_row[i]) {
			    free(nosql_sock->nosql_row[i]);
				nosql_sock->nosql_row[i] = NULL;
			}
		}
		free(nosql_sock->nosql_row);
		nosql_sock->nosql_row = NULL;
		nosql_sock->numfields = 0;
	}

	return 1;    
}

int nosql_free_cursor_result(NOSQL_SOCK *nosql_sock)
{
	return 0;
}

