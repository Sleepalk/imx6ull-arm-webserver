ubuntuWebServer : main.o task.o
	g++ -g -o ubuntuWebServer main.o task.o
main.o : main.cpp task.h commonMsg.h
	g++ -g -c main.o main.cpp
task.o : task.cpp task.h commonMsg.h
	g++ -g -c task.o task.cpp
clean :
	rm -f *.o ubuntuWebServer