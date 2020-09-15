# -udp-
线程池+数据库(mongodb)连接池实现udp服务器


服务器编译
编译前 修改build.sh 环境变量 LD_LIBRARY_PATH 、 LIBRARY_PATH
sh build.sh

服务器端启动
./udp


客户端编译：
gcc c.c -o client 

客户端启动：
./client
