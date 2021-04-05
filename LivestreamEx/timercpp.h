#pragma once

#include <boost/thread.hpp>
#include <chrono>
#include <atomic>

class Timer
{
	boost::thread sleepyThread;

public:
	~Timer()
	{
		//We should never leave the sleepy thread alone.
		stop();
	}

	template <typename Function>
	void setTimeout(Function function, int delay);

	template <typename Function>
	void setInterval(Function function, int interval);

	void stop();
};

template <typename Function>
void Timer::setTimeout(Function function, int delay)
{
	//Check if there's a previous thread not finished, cancel it. 
	stop();

	sleepyThread = boost::thread([=]()
	{
		try
		{
			boost::this_thread::sleep_for(boost::chrono::milliseconds(delay));
			function();
		}
		catch (boost::thread_interrupted interrupted)
		{
			//Wake up.
		}
	});
}

template <typename Function>
void Timer::setInterval(Function function, int interval)
{
	//Check if there's a previous thread not finished, cancel it. 
	stop();

	sleepyThread = boost::thread([=]()
	{
		try
		{
			while (true)
			{
				boost::this_thread::sleep_for(boost::chrono::milliseconds(interval));
				function();
			}
		}
		catch (boost::thread_interrupted interrupted)
		{
			//Wake up.
		}
	});
}	
