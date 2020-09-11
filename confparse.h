#ifndef __CONFPARSE_H
#define __COFPARSE_H

typedef struct conf{
	char *server_ip;
	int server_port;
	char *db_ip;
	int db_port;
	char *db_user;
	char *db_pwd;
	char *db_name;
	int db_num;	
	char *db_group_name;	
	int thread_num;
}CONF;

void ConfInit(CONF *conf,char *filename);
void ShowConf(CONF *conf);


#endif
