#ifndef SQLITE_MANAGER_H
#define SQLITE_MANAGER_H

#include <stdexcept>
#include <functional>
#include <sqlite3.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <OctreeNode.h>
#include "SQLiteQuery.h"

using namespace std;

namespace util
{
	/** Manages all SQLite operations. Provides a sync and async API. The async API can be used to make node requests
	 * and to acquire them later on. */
	template< typename Point, typename MortonCode, typename OctreeNode >
	class SQLiteManager
	{
	public:
		using IdNode = util::IdNode< MortonCode >;
		using IdNodeVector = vector< IdNode >;
		using SQLiteQuery = util::SQLiteQuery< IdNode >;
		using PointPtr = shared_ptr< Point >;
		using OctreeNodePtr = model::OctreeNodePtr< MortonCode >;
		using MortonCodePtr = shared_ptr< MortonCode >;
		
		SQLiteManager();
		~SQLiteManager();
		
		/** Inserts point into database.
		 *	@returns the index of the pointer. The first index is 0 and each next point increments it by 1. */
		sqlite_int64 insertPoint( const Point& point );
		
		/** Searches the point in the database, given its index.
		 *	@return the acquired node or nullptr if no point is found. The pointer ownership is caller's. */
		PointPtr getPoint( const sqlite3_uint64& index );
		
		/** Inserts the node into database, using the given morton code as identifier.
		 *	@param NodeContents is the type of the contents of the node. */
		template< typename NodeContents >
		void insertNode( const MortonCode& morton, const OctreeNode& node );
		
		/** Searches the node in the database, given its morton code.
		 *	@param NodeContents is the type of the contents of the node.
		 *	@param morton is the node morton code id.
		 *	@returns the acquired node or nullptr if no node is found. The pointer ownership is caller's. */
		template< typename NodeContents >
		OctreeNodePtr getNode( const MortonCode& morton );
		
		/** Searches for a range of nodes in the database, given the morton code interval [a, b].
		 *	@param NodeContents is the type of the contents of the node.
		 *	@param a is the minor boundary of the morton code closed interval.
		 *	@param b is the major boundary of the morton closed interval.
		 *	@returns the acquired nodes. The ownership of the node pointers is caller's. */
		template< typename NodeContents >
		vector< OctreeNodePtr > getNodes( const MortonCode& a, const MortonCode& b );
		
		/** Searches for a range of nodes in the database, given the morton code interval [a, b].
		 *	@param NodeContents is the type of the contents of the node.
		 *	@param a is the minor boundary of the morton code closed interval.
		 *	@param b is the major boundary of the morton closed interval.
		 *	@returns pairs of acquired nodes and morton id. The ownership of the node pointers is caller's. */
		template< typename NodeContents >
		IdNodeVector getIdNodes( const MortonCode& a, const MortonCode& b );
		
		/** Searches for a range of nodes in the database, given the morton code interval [a, b]. This version returns
		 * a query for stepping and should be used for probable big queries or to avoid the performance penalty of
		 * creating a result vector.
		 *	@param NodeContents is the type of the contents of the node.
		 *	@param a is the minor boundary of the morton code closed interval.
		 *	@param b is the major boundary of the morton closed interval.
		 *	@returns a query that can be iterated for IdNodeAcquisition. */
		template< typename NodeContents >
		SQLiteQuery getIdNodesQuery( const MortonCode& a, const MortonCode& b );
		
		/** Deletes a range of nodes in the database, given the morton code interval [ a, b ]
		 * @param a is the minor boundary of the morton code closed interval.
		 * @param b is the major boundary of the morton closed interval.*/
		void deleteNodes( const MortonCode& a, const MortonCode& b );
		
		/** Async request the closed interval of nodes [ a, b ]. They will be available later on and can be acessed
		 * using getRequestResults().
		 * @param a is the minor boundary of the morton code closed interval.
		 * @param b is the major boundary of the morton closed interval.*/
		void requestNodesAsync( const MortonCode& a, const MortonCode& b ) {}
		
		/** Get results available from earlier async node requests.
		 * @param maxResults maximum number of results to be returned.
		 * @returns a vector which each entry represents the results of one async request. */
		vector< IdNodeVector > getRequestResults( const unsigned int& maxResults ) {}
		
	private:
		/** Release all acquired resources. */
		void release();
		
		/** Creates the needed tables in the database. */
		void createTables();
		
		/** Drops all needed tables. */
		void dropTables();
		
		/** Creates the needed statements for database operations. */
		void createStmts();
		
		/** Checks return code of a called SQLite function.*/
		void checkReturnCode( const int& returnCode, const int& expectedCode );
		
		/** Calls sqlite3_prepare_v2 safely. */
		void safePrepare( const char* stringStmt, sqlite3_stmt** stmt );
		
		/** Calls sqlite3_prepare_v2 unsafely. */
		void unsafePrepare( const char* stringStmt, sqlite3_stmt** stmt );
		
		/** Calls sqlite3_step function safely.
		 *  @returns true if a row was found, false otherwise.*/
		bool safeStep( sqlite3_stmt* statement );
		
		/** Calls sqlite3_step function unsafely. */
		void unsafeStep( sqlite3_stmt* statement );
		
		/** Safe finalizes an sqlite3_stmt. */
		void safeFinalize( sqlite3_stmt* statement );
		
		/** Safe finalizes an sqlite3_stmt. */
		void unsafeFinalize( sqlite3_stmt* statement );
		
		/** Resets a prepared sqlite3_stmt. */
		void safeReset( sqlite3_stmt* statement );
		
		sqlite3* m_db;
		
		sqlite3_stmt* m_pointInsertion;
		sqlite3_stmt* m_pointQuery;
		
		sqlite3_stmt* m_nodeInsertion;
		sqlite3_stmt* m_nodeQuery;
		sqlite3_stmt* m_nodeIntervalQuery;
		sqlite3_stmt* m_nodeIntervalIdQuery;
		sqlite3_stmt* m_nodeIntervalDeletion;
		
		/** Current number of inserted points. */
		sqlite_int64 m_nPoints;
		
		// Async request manager associated data:
		// The manager itself.
		//thread requestManager;
		// Lock to the request container.
		//mutex requestLock;
		// Indicates that request were generated for requestManager.
		//condition_variable requestFlag;
		// Indicates that all async requests are sent to requestManager and no more will be generated. requestManager
		// should then finish processing all request and join with main thread.
		//bool requestsDone;
	};
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	SQLiteManager< Point, MortonCode, OctreeNode >::SQLiteManager()
	: m_db( nullptr ),
	m_pointInsertion( nullptr ),
	m_pointQuery( nullptr ),
	m_nodeInsertion( nullptr ),
	m_nodeQuery( nullptr ),
	m_nodeIntervalQuery( nullptr ),
	m_nodeIntervalIdQuery( nullptr ),
	m_nodeIntervalDeletion( nullptr ),
	m_nPoints( 0 )
	{
		checkReturnCode(
			sqlite3_open_v2( "Octree.db", &m_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, NULL ),
			SQLITE_OK
		);
		
		createTables();
		createStmts();
		
		//requestManager = thread(  );
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	SQLiteManager< Point, MortonCode, OctreeNode >::~SQLiteManager()
	{
		release();
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	sqlite_int64 SQLiteManager< Point, MortonCode, OctreeNode >::insertPoint( const Point& point )
	{
		byte* serialization;
		size_t blobSize = point.serialize( &serialization );
		
		checkReturnCode( sqlite3_bind_int64( m_pointInsertion, 1, m_nPoints ), SQLITE_OK );
		checkReturnCode( sqlite3_bind_blob( m_pointInsertion, 2, serialization, blobSize, SQLITE_STATIC ), SQLITE_OK );
		safeStep( m_pointInsertion );
		safeReset( m_pointInsertion );
		
		delete[] serialization;
		
		return m_nPoints++;
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	shared_ptr< Point > SQLiteManager< Point, MortonCode, OctreeNode >::getPoint( const sqlite3_uint64& index )
	{
		checkReturnCode( sqlite3_bind_int64( m_pointQuery, 1, index ), SQLITE_OK );
		bool isPointFound = safeStep( m_pointQuery );
		
		PointPtr point = nullptr;
		
		if( isPointFound )
		{
			byte* blob = ( byte* ) sqlite3_column_blob( m_pointQuery, 0 );
			point = make_shared< Point >( blob );
		}
		else
		{
			cout << "Cannot find point with index " << index << endl << endl;
		}
		
		safeReset( m_pointQuery );
		return point;
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	template< typename NodeContents >
	void SQLiteManager< Point, MortonCode, OctreeNode >::insertNode( const MortonCode& morton, const OctreeNode& node )
	{
		byte* serialization;
		size_t blobSize = node. template serialize< NodeContents >( &serialization );
		
		checkReturnCode( sqlite3_bind_int64( m_nodeInsertion, 1, morton.getBits() ), SQLITE_OK );
		checkReturnCode( sqlite3_bind_blob( m_nodeInsertion, 2, serialization, blobSize, SQLITE_STATIC ), SQLITE_OK );
		
		safeStep( m_nodeInsertion );
		safeReset( m_nodeInsertion );
		
		delete[] serialization;
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	template< typename NodeContents >
	OctreeNodePtr< MortonCode > SQLiteManager< Point, MortonCode, OctreeNode >::getNode( const MortonCode& morton )
	{
		checkReturnCode( sqlite3_bind_int64( m_nodeQuery, 1, morton.getBits() ), SQLITE_OK );
		bool isNodeFound = safeStep( m_nodeQuery );
		
		OctreeNodePtr node = nullptr;
		if( isNodeFound )
		{
			byte* blob = ( byte* ) sqlite3_column_blob( m_nodeQuery, 0 );
			node = OctreeNodePtr( OctreeNode:: template deserialize< NodeContents >( blob ) );
		}
		
		safeReset( m_nodeQuery );
		return node;
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	template< typename NodeContents >
	vector< OctreeNodePtr< MortonCode > > SQLiteManager< Point, MortonCode, OctreeNode >
	::getNodes( const MortonCode& a, const MortonCode& b )
	{
		checkReturnCode( sqlite3_bind_int64( m_nodeIntervalQuery, 1, a.getBits() ), SQLITE_OK );
		checkReturnCode( sqlite3_bind_int64( m_nodeIntervalQuery, 2, b.getBits() ), SQLITE_OK );
		
		vector< OctreeNodePtr > nodes;
		
		while( safeStep( m_nodeIntervalQuery ) )
		{
			byte* blob = ( byte* ) sqlite3_column_blob( m_nodeIntervalQuery, 0 );
			OctreeNodePtr node( OctreeNode:: template deserialize< NodeContents >( blob ) );
			nodes.push_back( node );
		}
		
		safeReset( m_nodeIntervalQuery );
		return nodes;
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	template< typename NodeContents >
	IdNodeVector< MortonCode > SQLiteManager< Point, MortonCode, OctreeNode >
	::getIdNodes( const MortonCode& a, const MortonCode& b )
	{
		SQLiteQuery query = getIdNodesQuery< NodeContents >( a, b );
		
		IdNodeVector queried;
		
		IdNode idNode;
		while( query.step( &idNode ) )
		{
			queried.push_back( idNode );
		}
		
		return queried;
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	template< typename NodeContents >
	SQLiteQuery< IdNode< MortonCode > > SQLiteManager< Point, MortonCode, OctreeNode >
	::getIdNodesQuery( const MortonCode& a, const MortonCode& b )
	{
		assert( a <= b && "a <= b should holds in order to define an interval." );
		
		checkReturnCode( sqlite3_bind_int64( m_nodeIntervalIdQuery, 1, a.getBits() ), SQLITE_OK );
		checkReturnCode( sqlite3_bind_int64( m_nodeIntervalIdQuery, 2, b.getBits() ), SQLITE_OK );
		
		SQLiteQuery query(
			[ & ] ( IdNode* queried )
			{
				bool rowIsFound = safeStep( m_nodeIntervalIdQuery );
				if( rowIsFound )
				{
					sqlite3_int64 mortonBits = sqlite3_column_int64( m_nodeIntervalIdQuery, 0 );
					MortonCodePtr code = make_shared< MortonCode >();
					code->build( mortonBits );
					
					byte* blob = ( byte* ) sqlite3_column_blob( m_nodeIntervalIdQuery, 1 );
					OctreeNodePtr node( OctreeNode:: template deserialize< NodeContents >( blob ) );
					
					*queried = IdNode( code, node );
				}
				else
				{
					*queried = IdNode( nullptr, nullptr );
				}
				
				return rowIsFound;
			}
			,
			[ & ] () { safeReset( m_nodeIntervalIdQuery ); }
		);
		
		return query;
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	void SQLiteManager< Point, MortonCode, OctreeNode >::deleteNodes( const MortonCode& a, const MortonCode& b )
	{
		checkReturnCode( sqlite3_bind_int64( m_nodeIntervalDeletion, 1, a.getBits() ), SQLITE_OK );
		checkReturnCode( sqlite3_bind_int64( m_nodeIntervalDeletion, 2, b.getBits() ), SQLITE_OK );
		
		safeStep( m_nodeIntervalDeletion );
		safeReset( m_nodeIntervalDeletion );
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	void SQLiteManager< Point, MortonCode, OctreeNode >::createTables()
	{
		sqlite3_stmt* creationStmt;
		
		safePrepare(
			"CREATE TABLE IF NOT EXISTS Points ("
				"Id INTEGER PRIMARY KEY,"
				"Point BLOB"
			");",
			&creationStmt
		);
		safeStep( creationStmt );
		safeFinalize( creationStmt );
		
		safePrepare(
			"CREATE TABLE IF NOT EXISTS Nodes("
				"Morton INTEGER PRIMARY KEY,"
				"Node BLOB"
			");",
			&creationStmt
		);
		safeStep( creationStmt );
		safeFinalize( creationStmt );
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	void SQLiteManager< Point, MortonCode, OctreeNode >::dropTables()
	{
		sqlite3_stmt* dropStmt;
		
		unsafePrepare( "DROP TABLE IF EXISTS Points;", &dropStmt );
		unsafeStep( dropStmt );
		unsafeFinalize( dropStmt );
		
		unsafePrepare( "DROP TABLE IF EXISTS Nodes;", &dropStmt );
		unsafeStep( dropStmt );
		unsafeFinalize( dropStmt );
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	void SQLiteManager< Point, MortonCode, OctreeNode >::createStmts()
	{
		safePrepare( "INSERT OR REPLACE INTO Points VALUES ( ?, ? );", &m_pointInsertion );
		safePrepare( "INSERT OR REPLACE INTO Nodes VALUES ( ?, ? );", &m_nodeInsertion );
		safePrepare( "SELECT Point FROM Points WHERE Id = ?;", &m_pointQuery );
		safePrepare( "SELECT Node FROM Nodes WHERE Morton = ?;", &m_nodeQuery );
		safePrepare( "SELECT Node FROM Nodes WHERE Morton BETWEEN ? AND ?;", &m_nodeIntervalQuery );
		safePrepare( "SELECT Morton, Node FROM Nodes WHERE Morton BETWEEN ? AND ?;", &m_nodeIntervalIdQuery );
		safePrepare( "DELETE FROM Nodes WHERE Morton BETWEEN ? AND ?;", &m_nodeIntervalDeletion );
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	void SQLiteManager< Point, MortonCode, OctreeNode >::release()
	{
		//requestsDone = true;
		//requestManager.join();
		
		dropTables();
		
		unsafeFinalize( m_pointInsertion );
		unsafeFinalize( m_pointQuery );
		unsafeFinalize( m_nodeInsertion );
		unsafeFinalize( m_nodeQuery );
		unsafeFinalize( m_nodeIntervalQuery );
		unsafeFinalize( m_nodeIntervalIdQuery );
		unsafeFinalize( m_nodeIntervalDeletion );
		
		if( m_db )
		{
			sqlite3_close( m_db );
		}
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	inline void SQLiteManager< Point, MortonCode, OctreeNode >::checkReturnCode( const int& returnCode,
																				 const int& expectedCode )
	{
		if( returnCode != expectedCode )
		{
			release();
			throw runtime_error( sqlite3_errstr( returnCode ) );
		}
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	inline void SQLiteManager< Point, MortonCode, OctreeNode >::safePrepare( const char* stringStmt, sqlite3_stmt** stmt )
	{
		checkReturnCode( sqlite3_prepare_v2( m_db, stringStmt, -1, stmt, NULL ), SQLITE_OK );
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	inline void SQLiteManager< Point, MortonCode, OctreeNode >::unsafePrepare( const char* stringStmt, sqlite3_stmt** stmt )
	{
		sqlite3_prepare_v2( m_db, stringStmt, -1, stmt, NULL );
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	inline bool SQLiteManager< Point, MortonCode, OctreeNode >::safeStep( sqlite3_stmt* statement )
	{
		int returnCode = sqlite3_step( statement );
		switch( returnCode )
		{
			case SQLITE_ROW: return true;
			case SQLITE_DONE: return false;
			default:
			{
				release();
				throw runtime_error( sqlite3_errstr( returnCode ) );
			}
		}
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	inline void SQLiteManager< Point, MortonCode, OctreeNode >::unsafeStep( sqlite3_stmt* statement )
	{
		sqlite3_step( statement );
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	inline void SQLiteManager< Point, MortonCode, OctreeNode >::safeFinalize( sqlite3_stmt* statement )
	{
		if( statement )
		{
			checkReturnCode( sqlite3_finalize( statement ), SQLITE_OK );
		}
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	inline void SQLiteManager< Point, MortonCode, OctreeNode >::unsafeFinalize( sqlite3_stmt* statement )
	{
		if( statement )
		{
			sqlite3_finalize( statement );
		}
	}
	
	template< typename Point, typename MortonCode, typename OctreeNode >
	inline void SQLiteManager< Point, MortonCode, OctreeNode >::safeReset( sqlite3_stmt* statement )
	{
		checkReturnCode( sqlite3_reset( statement ), SQLITE_OK );
	}
}

#endif