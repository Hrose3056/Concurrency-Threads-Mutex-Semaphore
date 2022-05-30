CC=g++
EXE=prog
OBJ = prog.o
CFLAGS = -std=c++11 -lpthread -lrt
FILES_TO_TAR = makefile prog.cpp test.dat

%.o: %.cpp 
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXE): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean tar
clean:
	rm -f $(OBJ) $(EXE)
tar:
	tar -cvf Concurrency-Threads-Mutex-Semaphore-hdesmara.tar $(FILES_TO_TAR)
