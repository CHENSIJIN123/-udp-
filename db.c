#include <stdio.h>
#include <stdlib.h>
#include "mongo_sql.h"

extern NOSQL_SOCK *sql_pool;

char * AllocIp(CONF *conf,char *user_name, char *calling_id);
int DBInit(CONF *conf)
{
	int i = 0;
	for(i = 0;i < conf->db_num; i++) {
		printf("rlm_nosql : Attempting to connect #%d\n",i);
	    NOSQL_SOCK *nosql_sock = (NOSQL_SOCK *)malloc(sizeof(NOSQL_SOCK));
        if (!nosql_sock) return -1;
		memset(nosql_sock,0x00,sizeof(NOSQL_SOCK));
		
        int ret = nosql_init_socket(conf,nosql_sock);
		if (ret == -1) 
			nosql_sock->connected = -1;
		else
			nosql_sock->connected = 1;
	    
		pthread_mutex_init(&nosql_sock->nosql_sock_mutex,NULL);
		nosql_sock->next = sql_pool;
		nosql_sock->id = i;
		//nosql_sock->connected = 1;
		sql_pool = nosql_sock;
		printf("rlm_nosql : Connected new NOSQL handle,#%d\n",i);
	}

    return 0;
}

NOSQL_SOCK *nosql_get_socket(CONF *conf)
{

	NOSQL_SOCK *start = sql_pool;
	
	NOSQL_SOCK *cur = start;

	while(cur) {
	   // radlog(L_INFO,"nosql_get_socket curr stat:%s\n",(1 == cur->connected ? "valid":"invalid"));
        if (0 != pthread_mutex_trylock(&cur->nosql_sock_mutex)) {
		    goto next;
		}
	    
	    if (-1 == cur->connected ) {
		    int ret = nosql_init_socket(conf,cur);
			if (ret == -1) 
				cur->connected = -1;
			else
				cur->connected = 1;
		}
		
		if (-1 == cur->connected) {
		    pthread_mutex_unlock(&cur->nosql_sock_mutex);
		    goto next;
		}
		return cur;

next:
        cur = cur->next;

		if (!cur) {
		    cur = sql_pool;
		}

		if (cur == start) {
		    break;
		}

	}
    printf("rlm_nosql :no db handles.\n");

    return NULL;
}

void nosql_release_socket(NOSQL_SOCK *nosql_sock)
{
    pthread_mutex_unlock(&nosql_sock->nosql_sock_mutex);

	return;
}

int  AddUser(CONF *conf,char *user_name, char *calling_id,int flag){
	NOSQL_SOCK *nosql_sock = NULL;
	nosql_sock = nosql_get_socket(conf);

	char nosql_query[1024] = {"\0"};
	char table_name[256] = {"\0"};
	char group_id[256] = {"\0"};

	//step 1: get group_id from groups where group_name is special group
	strncpy(table_name,"groups",strlen("groups")+1);
	strncpy(nosql_sock->table_name,table_name,sizeof(table_name));
	char **fields = (char **) malloc(sizeof(char *) * 1);
    fields[0] = (char *)malloc(256);
	memset(fields[0],0x00,sizeof(fields[0]));
	strcpy(fields[0],"group_id");
	nosql_sock->numfields = 1;
	snprintf(nosql_query,sizeof(nosql_query),"{'group_name':'%s'}",conf->db_group_name);
	if (-1 == nosql_select_query(nosql_sock,nosql_query,fields,0)) {
        printf("nosql select query failed.\n");
		nosql_free_result(nosql_sock);
		nosql_release_socket(nosql_sock);
		free(fields[0]);
		free(fields);
		fields[0] = NULL;
		fields = NULL;
		return -1;
	}
	if (nosql_sock->nosql_row && nosql_sock->nosql_row[0]) {
		strcpy(group_id,nosql_sock->nosql_row[0]);
		printf("group_id is %s\n",group_id);

	}else{
		printf("request add group for Self-study account opening\n");
		nosql_free_result(nosql_sock);
		nosql_release_socket(nosql_sock);
		free(fields[0]);
		free(fields);
		fields[0] = NULL;
		fields = NULL;
		return -1;
	}

	//step2: add user to table users
	strncpy(table_name,"users",strlen("users")+1);
    strncpy(nosql_sock->table_name,table_name,sizeof(table_name));

	// char **fields = (char **) malloc(sizeof(char *) * 1);
    // fields[0] = (char *)malloc(256);
	memset(fields[0],0x00,sizeof(fields[0]));
    strcpy(fields[0],"user_name");
	nosql_sock->numfields = 1;
	if(flag == 1)
		snprintf(nosql_query,sizeof(nosql_query),"{'mdn':'%s'}",calling_id);
	else
		snprintf(nosql_query,sizeof(nosql_query),"{'calling_id':'%s'}",calling_id);

	if (-1 == nosql_select_query(nosql_sock,nosql_query,fields,0)) {
        printf("nosql select query failed.\n");
		nosql_free_result(nosql_sock);
		nosql_release_socket(nosql_sock);
		free(fields[0]);
		free(fields);
		fields[0] = NULL;
		fields = NULL;
		return -1;
	}
	if (nosql_sock->nosql_row && nosql_sock->nosql_row[0]) {
		printf("already has this user which calling_is or mdn is %s\n",calling_id);
		nosql_free_result(nosql_sock);
		nosql_release_socket(nosql_sock);
		free(fields[0]);
		free(fields);
		fields[0] = NULL;
		fields = NULL;
		return -1;
	}else{
		char updatestr_query[1024] = {"\0"};
		char setstr_query[1024] = {"\0"};

		snprintf(setstr_query,sizeof(setstr_query),"{'user_name':'%s'}",user_name);

		snprintf(updatestr_query,sizeof(updatestr_query),"{'$set':{'user_name':'%s','mdn':'%s','calling_id':'%s','user_password':'%s','user_status':'1','group_id':'%s'}}",user_name,flag==1?calling_id:"",flag==0?calling_id:"",user_name,group_id);
		if(-1 == nosql_exec_direct(nosql_sock,setstr_query,updatestr_query,0)) {
			printf("nosql_exec_direct failed.\n");
			nosql_release_socket(nosql_sock);
			free(fields[0]);
			free(fields);
			fields[0] = NULL;
			fields = NULL;
			return -1;
		}
		nosql_release_socket(nosql_sock);
	}

	nosql_free_result(nosql_sock);
	nosql_release_socket(nosql_sock);
	free(fields[0]);
	free(fields);
	fields[0] = NULL;
	fields = NULL;

	return 0;

}

char * AllocIp(CONF *conf,char *user_name, char *calling_id){
	NOSQL_SOCK *nosql_sock = NULL;
	nosql_sock = nosql_get_socket(conf);

	char nosql_query[1024] = {"\0"};
	char table_name[256] = {"\0"};
	static char ipaddr[256] = {"\0"};

	strncpy(table_name,"group_ip",strlen("group_ip")+1);
	strncpy(nosql_sock->table_name,table_name,sizeof(table_name));
	
	char **fields = (char **) malloc(sizeof(char *) * 1);
    fields[0] = (char *)malloc(256);
	memset(fields[0],0x00,sizeof(fields[0]));

	strcpy(fields[0],"ipaddr");
	nosql_sock->numfields = 1;
	strncpy(nosql_query,"{'type':'1'}",sizeof(nosql_query));
	if (-1 == nosql_select_query(nosql_sock,nosql_query,fields,0)) {
        printf("nosql select query failed.\n");
		nosql_free_result(nosql_sock);
		nosql_release_socket(nosql_sock);
		free(fields[0]);
		free(fields);
		fields[0] = NULL;
		fields = NULL;
		return NULL;
	}
	if (nosql_sock->nosql_row && nosql_sock->nosql_row[0]) {
		strcpy(ipaddr,nosql_sock->nosql_row[0]);
		printf("ipaddr is %s\n",ipaddr);
	}else{
		printf("No ip can be allocated\n");
		nosql_free_result(nosql_sock);
		nosql_release_socket(nosql_sock);
		free(fields[0]);
		free(fields);
		fields[0] = NULL;
		fields = NULL;
		return NULL;
	}


	char updatestr_query[1024] = {"\0"};
	char setstr_query[1024] = {"\0"};

	snprintf(setstr_query,sizeof(setstr_query),"{'ipaddr':'%s'}",ipaddr);

	snprintf(updatestr_query,sizeof(updatestr_query),"{'$set':{'type':'2'}}");
	if(-1 == nosql_exec_direct(nosql_sock,setstr_query,updatestr_query,0)) {
		printf("nosql_exec_direct failed.\n");
		nosql_release_socket(nosql_sock);
		free(fields[0]);
		free(fields);
		fields[0] = NULL;
		fields = NULL;
		return NULL;
	}
	nosql_free_result(nosql_sock);
	nosql_release_socket(nosql_sock);
	free(fields[0]);
	free(fields);
	fields[0] = NULL;
	fields = NULL;
	return ipaddr;

}