# Makefile

CXX = g++
CXXFLAGS = -Wall -O2 -std=c++11
LDFLAGS = -lwiringPi -lz
SOURCES = main.cpp Spi.cpp SX1278.cpp
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = tx

all: $(TARGET)
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

distclean: clean
	rm -f *~

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean distclean run
