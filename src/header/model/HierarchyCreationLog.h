#ifndef HIERARCHY_CREATOR_LOG_H
#define HIERARCHY_CREATOR_LOG_H

#include <fstream>
#include <mutex>

using namespace std;

namespace model
{
	class HierarchyCreationLog
	{
	public:
		/** Logs a debug message. */
		static void logDebugMsg( const string& msg )
		{
			lock_guard< recursive_mutex > lock( m_logMutex );
			m_log << msg;
		}
	
		/** Logs a debug message and fail by throwing an exception. */
		static void logAndFail( const string& msg )
		{
			{
				lock_guard< recursive_mutex > lock( m_logMutex );
				logDebugMsg( msg );
				m_log.flush();
			}
			throw logic_error( msg );
		}
	
	private:
		static recursive_mutex m_logMutex;
		static ofstream m_log;
	};
}

#endif