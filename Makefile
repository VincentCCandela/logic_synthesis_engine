CC := g++
FLAGS := -std=c++17 -O2 -Wall

all: parser.o 
	$(CC) $(FLAGS) -o main main.cpp parser.o
parser.o: parser.cpp
	$(CC) $(FLAGS) -o parser.o -c parser.cpp

clean:
	rm *.o