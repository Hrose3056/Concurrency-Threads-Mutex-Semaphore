CC=g++
EXE=a4w22
OBJ = a4w22.o
CFLAGS = -std=c++11 -lpthread -lrt
FILES_TO_TAR = makefile a4w22.cpp test.dat projectReport.pdf

%.o: %.cpp 
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXE): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean tar
clean:
	rm -f $(OBJ) $(EXE)
tar:
	tar -cvf desmarais-a4.tar $(FILES_TO_TAR)