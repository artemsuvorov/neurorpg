#include "Precompiled.h"
#include "Server.h"

#include <iostream>


int main(int arc, char** argv)
{
	Neuro::Core::Log::Init();

	Neuro::Net::Server::Config config;
	config.Port = 8000;

	Neuro::Net::Server server(std::move(config));
	server.Start();

	NR_INFO("Press Enter to stop...");
	std::cin.get();

	server.Stop();

	Neuro::Core::Log::Shutdown();
}
