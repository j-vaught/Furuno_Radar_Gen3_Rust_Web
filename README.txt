---------------------------------------------------
NAVNETDLL Installation Procedure
---------------------------------------------------

* Installation
# cd /
# zcat NAVNETDLL_RADAR_Linux_32bit/64bit_xx.xx.tar.gz | tar -xvf -

* File composition
The file is installed in /usr/lib/NAVNETDLL/

* Compilation and link
The sample source is in /usr/lib/NAVNETDLL/sample directory.
You can compile and run the sample program by executing the following commands.
NOTICE: Make sure to specify the option to link NAVNETDLL for compilation.

* compile (sample-program)
# cd usr/lib/NAVNETDLL/sample
# g++ -O2 -Wall -I../include -c -o main.o Furuno_Sample.cpp -DFLM_COMPILE_NEW_LICENSE=1
# g++ -o main main.o -Wall -lNAVNETDLL -L../lib -lpthread -lm -ldl

* run (sample-program).[Be careful to space.]
# export LD_LIBRARY_PATH=../lib
# ./main [vendor.txt path] [target.dat path] [IPv4 address, for example(xxx.xxx.xxx.xxx)] [hostname,for example PC1]


SPECIFIC for my use cases:**********
* compile (sample-program)
cd sample
sudo g++ -O2 -Wall -I../include -c -o main.o Furuno_Sample.cpp -DFLM_COMPILE_NEW_LICENSE=1
sudo g++ -o main main.o -Wall -lNAVNETDLL -L../lib -lpthread -lm -ldl

* run (sample-program).[Be careful to space.]
# export LD_LIBRARY_PATH=../lib
# ./main vendor.txt target.dat 172.31.1.100 PC1

:**********


What is "target.dat" file?
The target.dat file provided with the DLL package contains a unique certification code/s of Radar sensor model/s.
The code in the file defines the approved Radar sensor models to be connected. A mismatched code with a sensor model will result in failing to establish the connection.

Storing the target.dat file:
"target.dat" should be save in your local,Please specified filepath as the third argument for command line.
This file is necessary to connect with a radar.

Connecting an appropriate Radar model:
Make sure that the Radar sensor supported by the provided target.dat file is connected to the network. Failure to do that will result in the failure of Flm_certification and/or RmInitialModule.

Checking Firewall setting:
If the connected Radar is not detected by the sample even when the correct target.dat file is stored at the correct location, check if the firewall is active and see if the connection is established with the firewall deactivated.


* Uninstallation
To uninstall NAVNETDLL, the rm command is used and uninstalled. 

# cd /usr/lib
# rm -rf NAVNETDLL

