#include "Engine.hpp"

#include <exception>
#include <iostream>

int main(const int /*argc*/, const char* /*argv*/[])
{
	Engine engine;

	try
	{
		engine.Start();
		engine.Run();
	}
	catch (const std::exception& aException)
	{
		std::cerr << aException.what() << std::endl;
		return 1;
	}

	return 0;
}
