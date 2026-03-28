#include "Precompiled.h"
#include "Server.h"

#include <iostream>


int main(int arc, char** argv)
{
	Neuro::Net::Server::Config config;
	config.Port = 8000;

	Neuro::Net::Server server(std::move(config));
	server.Start();

	std::cout << "\nPress Enter to stop..." << std::endl;
	std::cin.get();

	server.Stop();
}
