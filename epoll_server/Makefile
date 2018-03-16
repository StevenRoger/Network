CC = g++
BIN = epollServer
OBJS = epoll_server.o comm.o
SRC_DIR = ../
SRC=${wildcard ${SRC_DIR}/*.cc}
INC_DIR = ../comm/
CPPFLAGS = -fno-omit-frame-pointer -g -Wall -I${INC_DIR}



$(BIN):$(OBJS)
	$(CC) -o $(BIN) $(OBJS) $(SRC) -lpthread

comm.o:../comm/comm.cc
	$(CC) -c ../comm/comm.cc

.PHONY:clean

clean:
	-rm -f $(OBJS) $(BIN)