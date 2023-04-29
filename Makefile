CXX = clang++
INCLUDE = include
SRC = src
CXX_FLAGS = -std=c++2b -Wall -Wextra -Wshadow -Wformat=2 -Wunused -Wpedantic -Werror
LINK_FLAGS = -fsanitize=undefined

HEADERS = $(INCLUDE)/*

SRCS = $(wildcard $(SRC)/*.cpp)
OBJS = $(patsubst $(SRC)/%.cpp,%.o,$(SRCS))

all: link clean

link: $(OBJS)
	$(CXX) $(LINK_FLAGS) $(OBJS) -I$(INCLUDE) -g -o fs.out

%.o: $(SRC)/%.cpp $(HEADERS)
	$(CXX) $(CXX_FLAGS) -I$(INCLUDE) -c -g $< -o $@

clean:
	rm *.o