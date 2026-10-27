CC := g++
FLAGS := -std=c++17 -O2 -Wall

# simple build: reads one expression piped in on stdin (needs only main.cpp)
simple:
	$(CC) $(FLAGS) -o lse main.cpp

# extended build: also reads a .blif circuit file given as argument
blif: parser.o
	$(CC) $(FLAGS) -DBLIF -o lse main.cpp parser.o

parser.o: parser.cpp parser.h
	$(CC) $(FLAGS) -o parser.o -c parser.cpp

clean:
	rm -f *.o lse

.PHONY: simple blif clean
