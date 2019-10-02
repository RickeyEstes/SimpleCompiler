SRC = $(wildcard src/*.cpp)
OBJ = $(patsubst src/%.cpp, src/%.o, $(SRC))

BUILD_TYPE ?= DEBUG

src/%.o: src/%.cpp
	g++ -D $(BUILD_TYPE) -I include -c -Wall -g -std=c++14 -o $@ $^

%.o: %.cpp
	g++ -D $(BUILD_TYPE) -I include -c -Wall -g -std=c++14 -o $@ $^

main: main.o $(OBJ)
	g++ -Wall -g -std=c++14 -o $@ $^

all: main

clean:
	rm -f $(OBJ) main.o main
