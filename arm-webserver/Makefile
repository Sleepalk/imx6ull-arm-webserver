armWebServer : main.o task.o
	arm-linux-gnueabihf-g++ -g -o armWebServer main.o task.o -lpthread
main.o : main.cpp task.h commonMsg.h threadpool.h
	arm-linux-gnueabihf-g++ -g -c main.o main.cpp
task.o : task.cpp task.h commonMsg.h threadpool.h
	arm-linux-gnueabihf-g++ -g -c task.o task.cpp
clean :
	rm -f *.o armWebServer