#include "timercpp.h"

void Timer::stop()
{
	//Stop the thread from sleep state
	sleepyThread.interrupt();
	//Wait for exit
	if (sleepyThread.joinable())
		sleepyThread.join();
}
