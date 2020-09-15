# -udp-
线程池+数据库(mongodb)连接池实现udp服务器


服务器编译
编译前 修改build.sh 环境变量 LD_LIBRARY_PATH 、 LIBRARY_PATH
sh build.sh

服务器端启动
./udp

效果：
[chensj@localhost udp]$ ./udp
server_ip :  172.16.31.134
server_port :12345
db_ip :      mongodb://172.16.31.134:27018/
db_port :    27018
db_name :    aaa171
db_user :    aaa3
db_pwd :     aaa3
db_num :     5
db_group_name :special
thread_num : 16
rlm_nosql : Attempting to connect #0
rlm_nosql : Connected new NOSQL handle,#0
rlm_nosql : Attempting to connect #1
rlm_nosql : Connected new NOSQL handle,#1
rlm_nosql : Attempting to connect #2
rlm_nosql : Connected new NOSQL handle,#2
rlm_nosql : Attempting to connect #3
rlm_nosql : Connected new NOSQL handle,#3
rlm_nosql : Attempting to connect #4
rlm_nosql : Connected new NOSQL handle,#4

客户端编译：
gcc c.c -o client 

客户端启动：
./client
