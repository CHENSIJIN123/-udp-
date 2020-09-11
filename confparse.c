#include "confparse.h"
#include "ini.h"

static INI *ini;

void ConfInit(CONF *conf,char *filename){

    ini = ini_parse_with_filename(filename);

    // conf->server_ip = malloc(sizeof(char)*50);
    conf->server_ip = ini_get_key_value(ini,"server","server_ip");
    conf->server_port = atoi(ini_get_key_value(ini,"server","server_port"));

    conf->db_ip = ini_get_key_value(ini,"database","db_ip");
    conf->db_port = atoi(ini_get_key_value(ini,"database","db_port"));
    conf->db_name = ini_get_key_value(ini,"database","db_name");
    conf->db_user = ini_get_key_value(ini,"database","db_user");
    conf->db_pwd = ini_get_key_value(ini,"database","db_password");
    conf->db_num = atoi(ini_get_key_value(ini,"database","db_num"));
    conf->db_group_name = ini_get_key_value(ini,"database","db_group_name");

    conf->thread_num = atoi(ini_get_key_value(ini,"thread","thread_num"));
}

void ShowConf(CONF *conf)
{
    printf("server_ip :  %s\n",conf->server_ip);
    printf("server_port :%d\n",conf->server_port);
    printf("db_ip :      %s\n",conf->db_ip);
    printf("db_port :    %d\n",conf->db_port);
    printf("db_name :    %s\n",conf->db_name);
    printf("db_user :    %s\n",conf->db_user);
    printf("db_pwd :     %s\n",conf->db_pwd);
    printf("db_num :     %d\n",conf->db_num);
    printf("db_group_name :%s\n",conf->db_group_name);
    printf("thread_num : %d\n",conf->thread_num);
}