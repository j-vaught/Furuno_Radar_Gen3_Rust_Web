# $Revision: 170 $

LICENSE_PATTERN=-DFLM_COMPILE_NEW_LICENSE=1

# Compiler variables
CXX = g++
CXXFLAGS = `wx-config --cppflags` -I $(LICENSE_PATTERN) -g
LDFLAGS = `wx-config --libs` -L../lib -lNAVNETDLL

# Add Boost libraries
BOOST_LIBS = -lboost_system -lboost_thread -lpthread

# Source files
SOURCES = StartForm.cpp RadarConnection.cpp CommandServer.cpp RadarCommand.cpp SensorDataForm.cpp EchoServer.cpp RadarEchoFree.cpp Api.cpp WebServer.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Target executable
TARGET = main

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo Linking $(TARGET)
	$(CXX) $(CPPFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS) $(BOOST_LIBS)

%.o: %.cpp
	@echo Compiling $<
	$(CXX) -c $(CXXFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS)
	rm -f $(TARGET)
