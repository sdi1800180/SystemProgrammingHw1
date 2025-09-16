CC = g++
CFLAGS = -std=c++11 -Wall -c -Iinclude/
LDFLAGS = -pthread

EXEC1 = bin/jobExecutorServer
EXEC2 = bin/jobCommander

OBJ1 = build/jobExecutorServer.o
OBJ2 = build/jobCommander.o

all: $(EXEC1) $(EXEC2)

$(EXEC1): $(OBJ1)
	$(CC) $(LDFLAGS) -o $(EXEC1) $(OBJ1)

$(OBJ1): src/jobExecutorServer.cpp
	$(CC) $(CFLAGS) src/jobExecutorServer.cpp -o $(OBJ1)

$(EXEC2): $(OBJ2)
	$(CC) $(LDFLAGS) -o $(EXEC2) $(OBJ2)

$(OBJ2): src/jobCommander.cpp
	$(CC) $(CFLAGS) src/jobCommander.cpp -o $(OBJ2)

clean:
	rm -rf build/*
	rm bin/jobCommander
	rm bin/jobExecutorServer
