#include "Engine.hpp"

#include <iostream>
#include <stdexcept>

int main(const int /*argc*/, const char* /*argv*/[])
{
	Engine engine;

	try
	{
		engine.Start();
		engine.Run();
	}
	catch (const std::runtime_error& aError)
	{
		std::cerr << aError.what() << std::endl;
		return 1;
	}

	return 0;
}
