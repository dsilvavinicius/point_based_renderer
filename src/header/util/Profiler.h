#ifndef PROFILING_H
#define PROFILING_H

#include <chrono>
#include <iostream>

using namespace std;

namespace util
{
	// Profiling methods.
	class Profiler
	{
	public:
		static chrono::system_clock::time_point now()
		{
			auto timestamp = chrono::high_resolution_clock::now();
			
			std::time_t now = chrono::high_resolution_clock::to_time_t( timestamp );
			cout << "Started at: " << ctime( &now ) << endl << endl;
			
			return timestamp;
		}
		
		/** @returns the elapsed time in milliseconds from a reference. */
		static int elapsedTime( const chrono::system_clock::time_point& reference )
		{
			auto timestamp = now();
			
			std::time_t now = chrono::high_resolution_clock::to_time_t( timestamp );
			cout << "Finished at: " << ctime( &now ) << endl << endl;
			
			return chrono::duration_cast< std::chrono::milliseconds >( timestamp - reference ).count();
		}
	};
}

#endif