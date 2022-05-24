CC=g++
CFLAGS = -Wall -Werror -std=c++11



all: build_serverA build_serverB build_mainserver build_client


serverA: build_serverA
	@./serverA

serverB: build_serverB
	@./serverB

mainserver: build_mainserver
	@./mainserver

build_serverA: serverA.cpp
	@$(CC) $(CFLAGS) -o serverA $^

build_serverB: serverB.cpp
	@$(CC) $(CFLAGS) -o serverB $^

build_mainserver: servermain.cpp
	@$(CC) $(CFLAGS) -o mainserver $^

build_client: client.cpp
	@$(CC) $(CFLAGS) -o client $^



clean:
	rm -rf *.o serverA serverB mainserver client
