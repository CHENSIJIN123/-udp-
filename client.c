#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>

int main()
{
   //创建socket对象
     int sockfd=socket(AF_INET,SOCK_DGRAM,0);

    //创建网络通信对象
    struct sockaddr_in addr;
    addr.sin_family =AF_INET;
    addr.sin_port =htons(12345);
    addr.sin_addr.s_addr = inet_addr("172.16.31.134");

    while(1)
    {
        //printf("请输入一个数字：");
        printf("是否发送？0/1\n");
        //char buf=0;
        int issend = 0;
        scanf("%d",&issend);
        if(issend == 0)continue;
        char buf[50];
        sprintf(buf,"test,18735245456,1",sizeof("test,18735245456,1"));
        sendto(sockfd,buf,
        strlen(buf)+1,0,(struct sockaddr*)&addr,sizeof(addr));
        memset(buf,0x00,sizeof(buf));

        socklen_t len=sizeof(addr);
        recvfrom(sockfd,buf,sizeof(buf),0,(struct sockaddr*)&addr,&len);
        printf("recv buf is %s\n",buf);

    }
    close(sockfd);

}
