CouldServer:CouldServer.cpp
	g++ -std=c++0x $^ -o $@ -lpthread -lboost_filesystem -lboost_system 
