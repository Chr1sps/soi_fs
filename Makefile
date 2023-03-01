CXX = g++
INCLUDE = include
SRC = src
CXX_FLAGS = -g -Wall -Wextra -pedantic -Werror -std=c++2a

HEADERS = $(INCLUDE)/*

SRCS = $(wildcard $(SRC)/*.cpp)
OBJS = $(patsubst $(SRC)/%.cpp,%.o,$(SRCS))

all: link clean

link: $(OBJS)
	$(CXX) $(CXX_FLAGS) $(OBJS) -I$(INCLUDE) -Llib -g -o fs.out

%.o: $(SRC)/%.cpp $(HEADERS)
	$(CXX) $(CXX_FLAGS) -I$(INCLUDE) -c -g $< -o $@

clean:
	rm *.o