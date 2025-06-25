#!/bin/bash

# Delete the old build artifacts
rm -f main main.o stb_image_write.o web_server.o

# Compile stb_image_write.cpp into an object file
sudo g++ -O2 -Wall -I../include \
    -c stb_image_write.cpp \
    -o stb_image_write.o

# Compile your main source (Furuno_Sample.cpp) into an object file 
# with the necessary defines
sudo g++ -O2 -Wall -I../include \
    -c Furuno_Sample.cpp \
    -o main.o \
    -DFLM_COMPILE_NEW_LICENSE=1 \
    -DUSE_BOOST_WEB_INTERFACE=1

# Compile web_server.cpp into an object file 
sudo g++ -O2 -Wall -I../include \
    -c web_server.cpp \
    -o web_server.o \
    -DUSE_BOOST_WEB_INTERFACE=1

# Link the object files into the final executable 'main'
sudo g++ -o main main.o web_server.o stb_image_write.o -Wall \
    -L../lib -lNAVNETDLL \
    -lboost_system -lboost_thread \
    -lpthread -lm -ldl

# Set the library path so the loader can find ../lib/libNAVNETDLL
export LD_LIBRARY_PATH=../lib

# Run the program (adjust arguments as needed)
sudo ./main vendor.txt target.dat 172.31.1.100 PC1
