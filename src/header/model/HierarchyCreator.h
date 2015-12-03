#ifndef HIERARCHY_CREATOR_H
#define HIERARCHY_CREATOR_H

#include <mutex>
#include <condition_variable>
#include "ManagedAllocator.h"
#include "O1OctreeNode.h"
#include "OctreeDimensions.h"
#include "SQLiteManager.h"

using namespace util;

namespace model
{
	// TODO: If this algorithm is the best one, change MortonCode API to get rid of shared_ptr.
	/** Multithreaded massive octree hierarchy creator. */
	template< typename Morton, typename Point >
	class HierarchyCreator
	{
	public:
		using PointPtr = shared_ptr< Point >;
		using PointVector = vector< PointPtr, ManagedAllocator< PointPtr > >;
		using Node = O1OctreeNode< PointPtr >;
		using NodeVector = vector< Node, ManagedAllocator< Node > >;
		
		using OctreeDim = OctreeDimensions< Morton, Point >;
		using Reader = PlyPointReader< Point >;
		using Sql = SQLiteManager< Point, Morton, Node >;
		
		/** List of nodes that can be processed parallel by one thread. */
		using NodeList = list< Node, ManagedAllocator< Node > >;
		// List of NodeLists.
		using WorkList = list< NodeList, ManagedAllocator< Node > >;
		// Array with lists that will be processed in a given creation loop iteration.
		using IterArray = Array< NodeList >;
		// Array of first-dirty-node-per-level info.
		using DirtyArray = Array< Morton >;
		
		/** Ctor.
		 * @param sortedPlyFilename is a sorted .ply point filename.
		 * @param dim is the OctreeDim of the octree to be constructed.
		 * @param expectedLoadPerThread is the size of the NodeList that will be passed to each thread in the
		 * hierarchy creation loop iterations.
		 * @param memoryLimit is the allowed soft limit of memory consumption by the creation algorithm. */
		HierarchyCreator( const string& sortedPlyFilename, const OctreeDim& dim, ulong expectedLoadPerThread,
						  const size_t memoryLimit )
		: m_plyFilename( sortedPlyFilename ), 
		m_octreeDim( dim ),
		m_expectedLoadPerThread( expectedLoadPerThread ),
		m_perThreadFirstDirty( M_N_THREADS ),
		m_firstDirty( dim.m_nodeLvl ),
		m_dbs( M_N_THREADS ),
		m_memoryLimit( memoryLimit )
		{
			srand( 1 );
			
			for( int i = 0; i < M_N_THREADS; ++i )
			{
				m_dbs[ i ].init( m_plyFilename.substr( 0, m_plyFilename.find_last_of( "." ) ).append( ".db" ) );
				m_perThreadFirstDirty[ i ] = DirtyArray( dim.m_nodeLvl );
			}
			
			for( int i = m_firstDirty.size() - 1; i > -1; --i )
			{
				if( ( m_firstDirty.size() - i ) % 2 )
				{
					m_firstDirty[ i ] = Morton::getLvlLast( i );
				}
				else
				{
					m_firstDirty[ i ] = Morton::getLvlFirst( i );
				}
			}
		}
		
		/** Creates the hierarchy.
		 * @return hierarchy's root node. */
		Node create();
		
	private:
		void swapWorkLists()
		{
			m_workList = std::move( m_nextLvlWorkList );
			m_nextLvlWorkList = WorkList();
		}
		
		void pushWork( NodeList&& workItem )
		{
			lock_guard< mutex > lock( m_listMutex );
			m_workList.push_back( workItem );
		}
		
		NodeList popWork()
		{
			lock_guard< mutex > lock( m_listMutex );
			NodeList nodeList = std::move( m_workList.front() );
			m_workList.pop_front();
			return nodeList;
		}
		
		/** Merge nextProcessed into previousProcessed if there is not enough work yet to form a WorkList or push it to
		 * nextLvlWorkLists otherwise. Repetitions are checked while linking lists, since this case can occur when the
		 * lists have nodes from the same sibling group. */
		void mergeOrPushWork( NodeList& previousProcessed, const NodeList& nextProcessed )
		{
			if( nextProcessed.size() < m_expectedLoadPerThread )
			{
				if( *( --previousProcessed.end() ) == *nextProcessed.begin() )
				{
					// Nodes from same sibling groups were in different threads
					previousProcessed.pop_back();
				}
				previousProcessed.splice( previousProcessed.end(), nextProcessed );
			}
			else
			{
				m_nextLvlWorkList.push_front( std::move( nextProcessed ) );
			}
		}
		
		/** Creates an inner Node, given its children. */
		Node createInnerNode( NodeVector&& children ) const;
		
		/** Releases the sibling group, persisting it if the persistenceFlag is true.
		 * @param siblings is the sibling group to be released and persisted if it is the case.
		 * @param threadIdx is the index of the executing thread.
		 * @param lvl is the hierarchy level of the sibling group.
		 * @param firstSiblingMorton is the morton code of the first sibling.
		 * @param persistenceFlag is true if the sibling group needs to be persisted too. */
		void releaseAndPersistSiblings( Array< Node >& siblings, const int threadIdx, const uint lvl,
										const Morton& firstSiblingMorton, const bool persistenceFlag );
		
		/** Releases a given sibling group. */
		void releaseSiblings( Array< Node >& node, const int threadIdx, const uint nodeLvl );
		
		/** Releases nodes in order to ease memory stress. */
		void releaseNodes( uint currentLvl );
		
		/** Thread[ i ] uses database connection m_sql[ i ]. */
		Array< Sql > m_dbs;
		
		/** Holds the work list with nodes of the lvl above the current one. */
		WorkList m_nextLvlWorkList;
		
		/** SHARED. Holds the work list with nodes of the current lvl. Database thread and master thread have access. */
		WorkList m_workList;
		
		/** m_perThreadFirstDirty[ i ] contains the first dirty array for thread i. */
		Array< DirtyArray > m_perThreadFirstDirty;
		
		/** m_firstDirty[ i ] contains the first dirty (i.e. not persisted) Morton in lvl i. */
		DirtyArray m_firstDirty;
		
		OctreeDim m_octreeDim;
		
		string m_plyFilename;
		
		/** Mutex for the work list. */
		mutex m_listMutex;
		
		size_t m_memoryLimit;
		
		ulong m_expectedLoadPerThread;
		
		static constexpr int M_N_THREADS = 8;
	};
	
	template< typename Morton, typename Point >
	typename HierarchyCreator< Morton, Point >::Node HierarchyCreator< Morton, Point >::create()
	{
		// SHARED. The disk access thread sets this true when it finishes reading all points in the sorted file.
		bool leaflvlFinished;
		
		mutex releaseMutex;
		bool isReleasing = false;
		// SHARED. Indicates when a node release is ocurring.
		condition_variable releaseFlag;
		
		// Thread that loads data from sorted file or database.
		thread diskAccessThread(
			[ & ]()
			{
				NodeList nodeList;
				PointVector points;
				
				Morton currentParent;
				Reader reader( m_plyFilename );
				reader.read( Reader::SINGLE,
					[ & ]( const Point& p )
					{
						if( isReleasing )
						{
							{
								unique_lock< mutex > lock( releaseMutex );
									releaseFlag.wait( lock, [ & ]{ return !isReleasing; } );
							}
							
							points.push_back( makeManaged< Point >( p ) );
						}
						else 
						{
							Morton code = m_octreeDim.calcMorton( p );
							Morton parent = *code.traverseUp();
							
							if( parent != currentParent )
							{
								nodeList.push_back( Node( std::move( points ), true ) );
								points = PointVector();
								
								if( nodeList.size() == m_expectedLoadPerThread )
								{
									pushWork( std::move( nodeList ) );
									nodeList = NodeList();
								}
							}
							
							points.push_back( makeManaged< Point >( p ) );
						}
					}
				);
				
				leaflvlFinished = true;
			}
		);
		
		uint lvl = m_octreeDim.m_nodeLvl;
		
		// Hierarchy construction loop.
		while( lvl )
		{
			while( true )
			{
				if( m_workList.size() > 0 )
				{
					int dispatchedThreads = ( m_workList.size() > M_N_THREADS ) ? M_N_THREADS : m_workList.size();
					IterArray iterInput( dispatchedThreads );
					
					for( int i = dispatchedThreads - 1; i > -1; --i )
					{
						iterInput[ i ] = popWork();
					}
					
					IterArray iterOutput( dispatchedThreads );
					
					#pragma omp parallel for
					for( int i = 0; i < M_N_THREADS; ++i )
					{
						NodeList& input = iterInput[ i ];
						NodeList& output = iterOutput[ i ];
						while( !input.empty() )
						{
							Node& node = input.front();
							input.pop_front();
							Morton parentCode = *m_octreeDim.calcMorton( *node.getContents()[ 0 ] ).traverseUp();
							
							NodeVector siblings( 8 );
							siblings[ 0 ] = node;
							int nSiblings = 1;
							
							while( !input.empty() && *m_octreeDim.calcMorton( *input.front().getContents()[ 0 ] ).traverseUp() == parentCode )
							{
								++nSiblings;
								siblings[ nSiblings ] = input.front();
								input.pop_front();
							}	
							
							if( nSiblings == 1 )
							{
								// Merging, just put the node to be processed in next level.
								output.push_front( siblings[ 0 ] );
							}
							else
							{
								// LOD
								Node inner = createInnerNode( std::move( siblings ) );
								output.push_front( inner );
							}
						}
					}

					for( int i = dispatchedThreads - 1; i > -1; --i )
					{
						mergeOrPushWork( iterOutput[ i - 1 ], iterOutput[ i ] );
					}
					mergeOrPushWork( m_nextLvlWorkList.front(), iterOutput[ 0 ] );
					
					// Check memory stress and release memory if necessary.
					if( AllocStatistics::totalAllocated() > m_memoryLimit )
					{
						isReleasing = true;
						releaseNodes( lvl );
						isReleasing = false;
						releaseFlag.notify_one();
					}
				}
				else
				{
					if( leaflvlFinished )
					{
						break;
					}
				}
			}
				
			--lvl;
			swapWorkLists();
		}
		
		return m_nextLvlWorkList.front().front();
	}
	
	template< typename Morton, typename Point >
	inline void HierarchyCreator< Morton, Point >
	::releaseAndPersistSiblings( Array< Node >& siblings, const int threadIdx, const uint lvl,
								 const Morton& firstSiblingMorton, const bool persistenceFlag )
	{
		Sql& sql = m_dbs[ threadIdx ];
		if( persistenceFlag )
		{
			Morton siblingMorton = firstSiblingMorton;
			
			for( int i = 0; i < siblings.size(); ++i )
			{
				if( !siblings[ i ].isLeaf() )
				{
					releaseSiblings( siblings[ i ].children(), sql, lvl + 1 );
				}
				
				// Persisting node
				sql.insertNode( siblingMorton, siblings[ i ] );
				siblingMorton = siblingMorton.getNext();
			}
			
			// Updating thread dirtiness info.
			DirtyArray& firstDirty = m_perThreadFirstDirty[ threadIdx ];
			
			if( ( m_octreeDim.m_nodeLvl - lvl ) % 2 )
			{
				firstDirty[ lvl ] = firstSiblingMorton.getPrevious();
			}
			else
			{
				firstDirty[ lvl ] = siblingMorton;
			}
		}
		else
		{
			for( int i = 0; i < siblings.size(); ++i )
			{
				if( !siblings[ i ].isLeaf() )
				{
					releaseSiblings( siblings[ i ].children(), sql, lvl + 1 );
				}
			}
		}
	}
	
	template< typename Morton, typename Point >
	inline void HierarchyCreator< Morton, Point >
	::releaseSiblings( Array< Node >& siblings, const int threadIdx, const uint lvl )
	{
		OctreeDim currentLvlDim( m_octreeDim.m_origin, m_octreeDim.m_size, lvl );
		Morton firstSiblingMorton = currentLvlDim.calcMorton( siblings[ 0 ].getContents()[ 0 ] );
		
		if( ( m_octreeDim.m_nodeLvl - lvl ) % 2 )
		{
			releaseAndPersistSiblings( siblings, threadIdx, lvl, firstSiblingMorton,
									   firstSiblingMorton < m_firstDirty[ lvl ] );
		}
		else
		{
			releaseAndPersistSiblings( siblings, threadIdx, lvl, firstSiblingMorton,
									   m_firstDirty[ lvl ] < firstSiblingMorton );
		}
		
		siblings.clear();
	}
	
	template< typename Morton, typename Point >
	inline void HierarchyCreator< Morton, Point >::releaseNodes( uint currentLvl )
	{
		auto workListIt = m_nextLvlWorkList.back();
		
		while( AllocStatistics::totalAllocated() > m_memoryLimit )
		{
			IterArray iterArray( M_N_THREADS );
			for( int i = 0; i < M_N_THREADS; ++i )
			{
				iterArray[ i ] = *( workListIt-- );
			}
			
			#pragma omp parallel for
			for( int i = 0; i < M_N_THREADS; ++i )
			{
				int threadIdx = i;
				
				// Cleaning up thread dirtiness info.
				for( int j = 0; j < m_octreeDim.m_lvl; ++j )
				{
					m_perThreadFirstDirty[ i ][ j ].build( 0 );
				}
				
				Sql& sql = m_dbs[ i ];
				sql.beginTransaction();
				
				NodeList& nodeList = iterArray[ i ];
				for( Node& node : nodeList )
				{
					if( !node.isLeaf() )
					{
						releaseSiblings( node.children(), threadIdx, currentLvl );
					}
				}
				
				sql.endTransaction();
			}
			
			// Updating hierarchy dirtiness info.
			for( int i = 0; i < M_N_THREADS; ++i )
			{
				for( int j = 0; j < m_octreeDim.m_lvl; ++j )
				{
					if( ( m_octreeDim.m_lvl - j ) % 2 )
					{
						m_firstDirty[ j ] = std::min( m_firstDirty[ j ], m_perThreadFirstDirty[ i ][ j ] );
					}
					else
					{
						m_firstDirty[ j ] = std::max( m_firstDirty[ j ], m_perThreadFirstDirty[ i ][ j ] );
					}
				}
			}
		}
	}
	
	template< typename Morton, typename Point >
	inline typename HierarchyCreator< Morton, Point >::Node HierarchyCreator< Morton, Point >
	::createInnerNode( NodeVector&& vChildren ) const
	{
		using Map = map< int, Node&, less< int >, ManagedAllocator< pair< const int, Node& > > >;
		using MapEntry = typename Map::value_type;
		
		Array< Node > children( std::move( vChildren ) );
		
		int nPoints = 0;
		Map prefixMap;
		for( int i = 0; i < children.size(); ++i )
		{
			Node& child = children[ i ];
			prefixMap.insert( prefixMap.end(), MapEntry( nPoints, child ) );
			nPoints += child.getContents().size();
		}
		
		// LoD has 1/8 of children points.
		int numSamplePoints = std::max( 1., nPoints * 0.125 );
		Array< PointPtr > selectedPoints( numSamplePoints );
		
		for( int i = 0; i < numSamplePoints; ++i )
		{
			int choosenIdx = rand() % nPoints;
			MapEntry choosenEntry = *( --prefixMap.upper_bound( choosenIdx ) );
			selectedPoints[ i ] = choosenEntry.second.getContents()[ choosenIdx - choosenEntry.first ];
		}
		
		return Node( selectedPoints, false );
	}
}

#endif