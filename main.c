#include <stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include "ini.h"
#include "mythread.h"

extern workqueue_t workqueue;

void split(char *src,const char *separator,char **dest,int *num) {
	/*
		src 源字符串的首地址(buf的地址) 
		separator 指定的分割字符
		dest 接收子字符串的数组
		num 分割后子字符串的个数
	*/
     char *pNext;
     int count = 0;
     if (src == NULL || strlen(src) == 0) //如果传入的地址为空或长度为0，直接终止 
        return;
     if (separator == NULL || strlen(separator) == 0) //如未指定分割的字符串，直接终止 
        return;
     pNext = (char *)strtok(src,separator); //必须使用(char *)进行强制类型转换(虽然不写有的编译器中不会出现指针错误)
     while(pNext != NULL) {
          *dest++ = pNext;
          ++count;
         pNext = (char *)strtok(NULL,separator);  //必须使用(char *)进行强制类型转换
    }  
    *num = count;
} 	

void deal_msg(job_t *job) {
	printf("index : %d, selfid : %lu\n", index, pthread_self());
	char buf[50],username[50],calling_id[50];
	int id = 0,flag = 0;
 	strcpy(buf,job->user_data);
	socklen_t len=sizeof(job->cli);
	int sockfd = job->sockfd;
	printf("client data is  %s\n",buf);

	char *revbuf[8] = {0}; //存放分割后的子字符串 
	//分割后子字符串的个数
	int num = 0,i = 0;
	split(buf,",",revbuf,&num);
	for(i = 0;i < num; i ++) {
	 	//lr_output_message("%s\n",revbuf[i]);
	 	printf("%s\n",revbuf[i]);
	 }
	if(num == 3){
		//sprintf(username,"%s",revbuf[0]);
		strcpy(username,revbuf[0]);
		if(strstr(revbuf[1],"460") != NULL)flag = 1;
			// sprintf(mdn,revbuf[1]);
		//sprintf(calling_id,"%s",revbuf[1]);
		strcpy(calling_id,revbuf[1]);
		//printf("**********\n");
		id = atoi(revbuf[2]);
		//printf("id is %d\n",id);
		//sprintf(id,"%d",revbuf[2]);
	}else{
		printf("msg format is error\n");
		exit(0);
	}
	
	char reply[50];
	char *ip = NULL;
	if(-1 == AddUser(job->conf,username,calling_id,flag)){
		sprintf(reply,"%d,-1",id);
	}else{
		sprintf(reply,"%d,-1",id);
		if(NULL != (ip = AllocIp(job->conf,username,calling_id))){
			sprintf(reply,"%d,%s",id,ip);
		}
	}

	printf("----------------ip is %s\n",ip);

	sendto(sockfd,reply,sizeof(reply),0,(struct sockaddr*)&job->cli,len);

	free(job->user_data);
	free(job);
}

int main()
{
	CONF *conf = (CONF *)malloc(sizeof(CONF));
   	memset(conf,0x00,sizeof(CONF));
	ConfInit(conf,"./conf.ini");
	
	ShowConf(conf);
	threadpool_init(conf->thread_num);

	DBInit(conf);


	//创建socket对象
    int sockfd=socket(AF_INET,SOCK_DGRAM,0);

    //创建网络通信对象
    struct sockaddr_in addr;
    addr.sin_family =AF_INET;
    //addr.sin_port =htons(12345);
	addr.sin_port =htons(conf->server_port);
    //addr.sin_addr.s_addr=inet_addr("172.18.0.231");
	addr.sin_addr.s_addr=inet_addr(conf->server_ip);

	//绑定socket对象与通信链接
    int ret =bind(sockfd,(struct sockaddr*)&addr,sizeof(addr));
    if(0>ret)
    {
         printf("bind\n");
        return -1;

    }
    struct sockaddr_in cli;
    socklen_t len=sizeof(cli);

    while(1)
    {
        char buf[50] ={'\0'};
        recvfrom(sockfd,buf,sizeof(buf),0,(struct sockaddr*)&cli,&len);
        printf("recv num =%s\n",buf);

		job_t *job = (job_t*)malloc(sizeof(job_t));
		if (job == NULL) {
			perror("malloc");
			exit(1);//remember deal it
		}
		
		job->job_function = deal_msg;
		job->user_data = malloc(sizeof(char)*50);
		job->conf = conf;
		strcpy(job->user_data,buf);
		job->sockfd = sockfd;
		job->cli = cli;

		workqueue_add_job(&workqueue, job);

    }
    close(sockfd);
	
	getchar();
	printf("\n");
	return 0;
}
