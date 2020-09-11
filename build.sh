export LD_LIBRARY_PATH=/home/chensj/udp/lib/:/home/chensj/udp/lib/mongo/
export LIBRARY_PATH=/home/chensj/udp/lib/:/home/chensj/udp/lib/mongo/
#gcc main.c confparse.c mythread.c sql.c sql_prepare_parser.c -I ./include/oci/ -I ./include/ -I ./ -L ./lib/ -L ./lib/oci -ltipwc -lpthread -lclntsh  -o udp
gcc main.c confparse.c mythread.c mongo_sql.c db.c -I ./include/mongo/ -I ./include/mongo/mongoc/ -I ./include/mongo/bson/ -I ./include/oci/ -I ./include/ -I ./ -L ./lib/ -ltipwc -lpthread -lbson-1.0 -lmongoc-1.0 -o udp
#gcc main.c confparse.c mythread.c  -I ./include/ -I ./ -L ./lib/  -ltipwc -lpthread  -o udp

#-lclntsh
