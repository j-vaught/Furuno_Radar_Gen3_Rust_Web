#
# $Revision: 168 $
#

LICENSE_PATTERN=-DFLM_COMPILE_NEW_LICENSE=1

all:	
	g++ -c `wx-config --cppflags` StartForm.cpp -I../include  -o StartForm.o $(LICENSE_PATTERN)
	g++ -c `wx-config --cppflags` RadarConnection.cpp -I../include  -o RadarConnection.o $(LICENSE_PATTERN)
	g++ -c `wx-config --cppflags` RadarCommand.cpp -I../include  -o RadarCommand.o $(LICENSE_PATTERN)
	g++ -c `wx-config --cppflags` SensorDataForm.cpp -I../include  -o SensorDataForm.o $(LICENSE_PATTERN)
	g++ -c `wx-config --cppflags` RadarEchoFree.cpp -I../include  -o RadarEchoFree.o $(LICENSE_PATTERN)
	g++ -c `wx-config --cppflags` Api.cpp -I../include -o Api.o $(LICENSE_PATTERN)
	g++ $(CPPFLAGS) -o main StartForm.o RadarConnection.o RadarCommand.o SensorDataForm.o RadarEchoFree.o Api.o `wx-config -cppflags --libs` -L../lib -lNAVNETDLL

clean:
	rm *.o
	rm ./main
