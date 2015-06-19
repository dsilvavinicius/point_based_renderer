#ifndef OUT_OF_CORE_OCTREE_H
#define OUT_OF_CORE_OCTREE_H

#include "FrontOctree.h"
#include "PlyPointReader.h"
#include "SQLiteManager.h"
#include <MemoryInfo.h>

using namespace std;
using namespace util;

namespace model
{
	/** Octree for massive point clouds that cannot be held in main memory because of their sizes. The main memory is
	 * used as a cache, with data being fetched on demand from a database in disk. The persistence is tracked, so
	 * the construction and front tracking algorithms try to minimize database access. */
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	class OutOfCoreOctree
	: public FrontOctree< MortonCode, Point, Front, FrontInsertionContainer >
	{
		using MortonCodePtr = shared_ptr< MortonCode >;
		using PointPtr = shared_ptr< Point >;
		using PointVector = vector< PointPtr >;
		using OctreeNode = model::OctreeNode< MortonCode >;
		using OctreeNodePtr = shared_ptr< OctreeNode >;
		using OctreeMap = model::OctreeMap< MortonCode >;
		using OctreeMapPtr = shared_ptr< OctreeMap >;
		using PlyPointReader = util::PlyPointReader< Point >;
		using SQLiteManager = util::SQLiteManager< Point, MortonCode, OctreeNode >;
		using ParentOctree = model::FrontOctree< MortonCode, Point, Front, FrontInsertionContainer >;
		using IdNode = util::IdNode< MortonCode >;
		using SQLiteQuery = util::SQLiteQuery< IdNode >;
		
	public:
		OutOfCoreOctree( const int& maxPointsPerNode, const int& maxLevel );
		
		/** Builds octree using the database. */
		virtual void build();
		
		virtual void buildFromFile( const string& plyFileName, const typename PlyPointReader::Precision& precision,
									const Attributes& attribs ) override;
		
		SQLiteManager& getSQLiteManager() { return m_sqLite; }
		
		/** DEPRECATED: use build() instead. */
		virtual void build( PointVector& points ) override;
	
	protected:
		virtual void insertPointInLeaf( const PointPtr& point ) override;
		
		/** Acquires a node with given morton code. Searches in-memory hierarchy first and database if not found.
		 * DOES NOT TRACK PERSISTENCE, since the traversal and creation algorithms expect the atomic persistence
		 * operation to have a per-sibling basis. 
		 * @return A smart-pointer to the node or nullptr if not found. */
		OctreeNodePtr getNode( const MortonCodePtr& code );
		
		/** Acquires all child nodes of a given parent node code. Searches in-memory hierarchy first and database if
		 * not found. It assumes that the parameter is a code for an inner node. Using this method for a leaf node code
		 * will result in unnecessary accesses to database. TRACKS PERSISTENCE.
		 * @return A vector with all found nodes. */
		vector< OctreeNodePtr > getChildren( const MortonCodePtr& parent );
		
		/** Get a query for a range of nodes in closed interval [ a, b ]. */
		SQLiteQuery getRangeInDB( const MortonCodePtr& a, const MortonCodePtr& b );
		
		void buildInners() override;
		
		void eraseNodes( const typename OctreeMap::iterator& first, const typename OctreeMap::iterator& last ) override;
		
		/** DEPRECATED. */
		virtual void buildBoundaries( const PointVector& points ) override;
		
		/** DEPRECATED. */
		virtual void buildNodes( PointVector& points ) override;
		
		/** DEPRECATED. */
		virtual void buildLeaves( const PointVector& points ) override;
		
	private:
		/** Releases node in the in-memory hierarchy at hierarchy creation time in order to ease memory pressure.
		 * Persists all "dirty" nodes in the database. TRACKS PERSISTENCE.
		 * @returns the last released node code or nullptr in case of no release. */
		MortonCodePtr releaseNodesAtCreation();
		
		/** Persists and release leaf nodes at hierarchy creation in order to ease memory pressure. Since leaf nodes
		 * are acessed in a random pattern and a loaded node is assured to be modified in the near future, this method
		 * just assumed all nodes in memory dirty, sending them to database whenever released. This trait also imposes
		 * NO PERSISTENCE TRACKING. */
		void persistAndReleaseLeaves();
		
		/** Persists all leaf nodes in order to start bottom-up inner nodes creation. Also load a few nodes to start
		 work. */
		void persistAllLeaves();
		
		/** Checks if a node is dirty and needs to be persisted before released. */
		bool isDirty( const MortonCodePtr& code ) const;
		
		/** Load nodes from database at hierarchy creation and revalidates the iterator for the first child of the
		 * current parent. */
		void loadNodesAndValidateIter( const MortonCodePtr& nextFirstChildCode, const MortonCodePtr& lvlBoundary,
									   typename OctreeMap::iterator& firstChildIt );
		
		/** Load sibling groups in a query.
		 * @param query is the query with nodes to load.
		 * @returns The first loaded MortonCode or nullptr if the query returns no node. */
		shared_ptr< MortonCode > loadSiblingGroups( SQLiteQuery& query );
		
		SQLiteManager m_sqLite;
		
		// Octree creation related data:
		
		/** Last fully persisted node's morton code. Any morton code less than this one is dirty and should be written
		 * in database before released. */
		MortonCodePtr m_lastPersisted;
		
		unsigned long m_nodesUntilLastPersistence; 
		
		static unsigned int M_NODES_PER_PERSISTENCE_ITERATION;
		
		/** Number of sibling groups loaded from database at hierarchy creation. */
		static unsigned int M_SIBLING_GROUPS_PER_LOAD;
		
		// End of octree creation related data.
		
		static unsigned long M_MIN_FREE_MEMORY_TO_RELEASE;
		static unsigned long M_MIN_FREE_MEMORY_AFTER_RELEASE;
	};
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	unsigned long OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >
	::M_MIN_FREE_MEMORY_TO_RELEASE = MemoryInfo::getMemorySize() * 0.1;
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	unsigned long OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >
	::M_MIN_FREE_MEMORY_AFTER_RELEASE = MemoryInfo::getMemorySize() * 0.2;
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	unsigned int OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >
	::M_NODES_PER_PERSISTENCE_ITERATION = 400;
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	unsigned int OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >
	::M_SIBLING_GROUPS_PER_LOAD = 100;
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >::OutOfCoreOctree( const int& maxPointsPerNode,
																						   const int& maxLevel )
	: ParentOctree( maxPointsPerNode, maxLevel ),
	m_nodesUntilLastPersistence( 0uL )
	{
		m_lastPersisted = make_shared< MortonCode >( MortonCode::getLvlLast( maxLevel ) );
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	void OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >::build()
	{
		buildInners();
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	void OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >
	::buildFromFile( const string& plyFileName, const typename PlyPointReader::Precision& precision,
					 const Attributes& attribs )
	{
		// Octree boundary variables.
		Float negInf = -numeric_limits< Float >::max();
		Float posInf = numeric_limits< Float >::max();
		Vec3 minCoords( posInf, posInf, posInf );
		Vec3 maxCoords( negInf, negInf, negInf );
		
		// The points are read in two passes. First to calculate octree boundaries and second to populate leaf nodes.
		// First pass: whenever a point is full read, update the boundary variables.
		auto *reader = new PlyPointReader(
			[ & ]( const Point& point )
			{
				const shared_ptr< const Vec3 > pos = point.getPos();
				
				for( int i = 0; i < 3; ++i )
				{
					minCoords[ i ] = glm::min( minCoords[ i ], ( *pos )[ i ] );
					maxCoords[ i ] = glm::max( maxCoords[ i ], ( *pos )[ i ] );
				}
			}
		);
		reader->read( plyFileName, precision, attribs );
		
		cout << "Attributes:" << reader->getAttributes() << endl << endl;
		cout << "After reading points first pass." << endl << endl;
		
		// Save boundary info.
		*ParentOctree::m_origin = minCoords;
		*ParentOctree::m_size = maxCoords - minCoords;
		*ParentOctree::m_leafSize = *ParentOctree::m_size *
									( ( Float )1 / ( ( unsigned long long )1 << ParentOctree::m_maxLevel ) );
		
		// Second pass: whenever a point is full read, inserts it in the hierarchy.
		*reader = PlyPointReader(
			[ & ]( const Point& point )
			{
				insertPointInLeaf( make_shared< Point >( point ) );
			}
		);
		reader->read( plyFileName, precision, attribs );
		
		cout << "After reading points second pass." << endl << endl;
		
		// From now on the reader is not necessary. Delete it in order to save memory.
		delete reader;
		cout << "Before persisting all leaves" << endl;
		
		// Persist all leaves in order to start bottom-up octree creation.
		persistAllLeaves();
		
		cout << "Before build." << endl;
		build();
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline void OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >
	::insertPointInLeaf( const PointPtr& point )
	{
		cout << "===== Inserting point in leaf: ====" << endl << *point << endl;
		
		MortonCode tempCode = ParentOctree::calcMorton( *point );
		cout << "Calc morton: " << endl << tempCode.getPathToRoot( true );
		
		MortonCodePtr code = make_shared< MortonCode >( tempCode );
		
		cout << "Calc morton ptr: " << endl << code->getPathToRoot( true );
		
		OctreeNodePtr node = getNode( code );
		
		if( node == nullptr )
		{
			cout << "Creating node" << endl;
			
			++m_nodesUntilLastPersistence;
			
			// Creates leaf node.
			PointVector points;
			points.push_back( point );
			auto leafNode = make_shared< LeafNode< MortonCode, PointVector > >();
			leafNode->setContents( points );
			( *ParentOctree::m_hierarchy )[ code ] = leafNode;
		}
		else
		{
			cout << "Appending in existent node" << endl;
			
			// Node already exists. Appends the point there.
			shared_ptr< PointVector > points = node-> template getContents< PointVector >();
			points->push_back( point );
		}
		
		if( m_nodesUntilLastPersistence > M_NODES_PER_PERSISTENCE_ITERATION )
		{
			cout << "Leaf release and persistence checking" << endl;
			persistAndReleaseLeaves();
		}
		
		cout << "===== Inserting point in leaf end ====" << endl << endl;
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline OctreeNodePtr< MortonCode > OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >
	::getNode( const MortonCodePtr& code )
	{
		OctreeMapPtr hierarchy = ParentOctree::m_hierarchy;
		typename OctreeMap::iterator nodeIt = hierarchy->find( code );
		
		if( nodeIt != hierarchy->end() )
		{
			return nodeIt->second;
		}
		else
		{
			OctreeNode* node = m_sqLite. template getNode< PointVector >( *code );
			if( node )
			{
				++m_nodesUntilLastPersistence;
				
				OctreeNodePtr nodePtr( node );
				return nodePtr;
			}
			else
			{
				return nullptr;
			}
		}
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline vector< OctreeNodePtr< MortonCode > > OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >
	::getChildren( const MortonCodePtr& parent )
	{
		MortonCodePtr firstChildCode = parent->getFirstChild();
		OctreeMapPtr hierarchy = ParentOctree::m_hierarchy;
		typename OctreeMap::iterator it = hierarchy->lower_bound( firstChildCode );
		typename OctreeMap::iterator end = hierarchy->end();
		vector< OctreeNodePtr > nodes;
		
		while( it != end && it->second->isChildOf( parent ) )
		{
			nodes.push_back( it->second );
			++it;
		}
		
		if( nodes.empty() )
		{
			// Nodes aren't in memory
			vector< IdNode > queried = m_sqLite. template getIdNodes< PointVector >( firstChildCode,
																					 parent->getLastChild() );
			for( IdNode idNode : queried )
			{
				MortonCodePtr code( idNode.first );
				OctreeNodePtr node( idNode.second );
				hierarchy[ code ] = node;
				
				nodes.push_back( node );
			}
		}
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline SQLiteQuery< IdNode< MortonCode > > OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >
	::getRangeInDB(  const MortonCodePtr& a, const MortonCodePtr& b  )
	{
		return m_sqLite. template getIdNodesQuery< PointVector >( *a, *b );
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline void OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >::persistAndReleaseLeaves()
	{
		if( MemoryInfo::getAvailableMemorySize() < M_MIN_FREE_MEMORY_TO_RELEASE )
		{
			cout << "==== Leaf persistence triggered ====" << endl;
			
			while( MemoryInfo::getAvailableMemorySize() < M_MIN_FREE_MEMORY_AFTER_RELEASE )
			{
				m_nodesUntilLastPersistence = 0;
				
				OctreeMapPtr hierarchy = ParentOctree::m_hierarchy;
				typename OctreeMap::reverse_iterator nodeIt = hierarchy->rbegin();
				
				int i = 0;
				while( i < M_NODES_PER_PERSISTENCE_ITERATION )
				{
					MortonCodePtr code = nodeIt->first;
					OctreeNodePtr node = nodeIt->second;
					m_sqLite. template insertNode< PointVector >( *code, *node );
					
					typename OctreeMap::reverse_iterator toRelease = nodeIt++;
					++i;
					hierarchy->erase( toRelease.base() );
					
					if( nodeIt == hierarchy->rend() )
					{
						throw runtime_error( "Node release emptied hierarchy. This is not supposed to do." );
					}
				}
			}
			
			cout << "==== Leaf persistence ended ====" << endl << endl;
		}
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	void OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >::persistAllLeaves()
	{
		cout << "========================== persistsAllLeaves ===============================" << endl;
		
		OctreeMapPtr hierarchy = ParentOctree::m_hierarchy;
		
		cout << "Before inserting nodes. Hierarchy size:" << hierarchy->size() << endl;
		
		// Send all in-memory leaves to database.
		for( auto elem : *hierarchy )
		{
			m_sqLite. template insertNode< PointVector >( *elem.first, *elem.second );
		}
		
		cout << "Before querying:" << endl << "a: " << hierarchy->begin()->first->getPathToRoot( true )
			 << "b: " << m_lastPersisted->getPathToRoot( true ) << endl;
		
		// Since leaf creation is unsorted by nature, some sibling groups must be loaded from database to ensure no
		// "holes" in the in-memory sibling groups.
		SQLiteQuery query = getRangeInDB( hierarchy->begin()->first, m_lastPersisted );
		
		cout << "Before clearing hierarchy" << endl;
		// Clears the hierarchy to eliminate possible sibling groups with holes.
		hierarchy->clear();
		
		cout << "Before loading siblings" << endl;
		// Loads a few sibling groups in order to start bottom-up construction.
		loadSiblingGroups( query );
		
		cout << "========================== persistsAllLeaves end ===============================" << endl << endl;
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline shared_ptr< MortonCode > OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >
	::releaseNodesAtCreation()
	{
		OctreeMapPtr hierarchy = ParentOctree::m_hierarchy;
		typename OctreeMap::reverse_iterator nodeIt = hierarchy->rbegin();
		MortonCodePtr currentCode = nullptr;
		
		if( MemoryInfo::getAvailableMemorySize() < M_MIN_FREE_MEMORY_TO_RELEASE )
		{
			currentCode = nodeIt->first;
			
			while( MemoryInfo::getAvailableMemorySize() < M_MIN_FREE_MEMORY_AFTER_RELEASE )
			{
				MortonCodePtr parentCode = currentCode->traverseUp();
				
				while( currentCode->isChildOf( *parentCode ) )
				{
					if( isDirty( currentCode ) )
					{
						m_sqLite. template insertNode< PointVector >( *currentCode, *nodeIt->second );
					}
					
					typename OctreeMap::reverse_iterator toRelease = nodeIt++;
					hierarchy->erase( toRelease.base() );
					
					if( nodeIt == hierarchy->rend() )
					{
						throw runtime_error( "Node release emptied hierarchy. This is not supposed to do." );
					}
					
					currentCode = nodeIt->first;
				}
			}
			
			if( isDirty( currentCode ) )
			{
				*m_lastPersisted = *currentCode;
			}
		}
		
		return currentCode;
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	void OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >::buildInners()
	{
		OctreeMapPtr hierarchy = ParentOctree::m_hierarchy;
		
		// Do a bottom-up per-level construction of inner nodes.
		for( int level = ParentOctree::m_maxLevel - 1; level > -1; --level )
		{
			cout << "========== Octree construction, level " << level << " ==========" << endl << endl;
			// The idea behind this boundary is to get the minimum morton code that is from one level deeper than
			// the current one.
			MortonCodePtr lvlBoundary = make_shared< MortonCode >( MortonCode::getLvlLast( level + 1 ) );
			
			cout << "Lvl boundary calculated. Bits: 0x" << hex << lvlBoundary->getBits() << dec << endl;
			cout << "Lvl boundary: " << lvlBoundary->getPathToRoot( true );
			
			typename OctreeMap::iterator firstChildIt = hierarchy->begin(); 
			typename OctreeMap::iterator hierarchyEnd = hierarchy->end();
			bool isLevelEnded = false;
			
			// Loops per siblings in a level.
			while( !isLevelEnded )
			{
				//cout << *this << endl;
				
				MortonCodePtr parentCode = firstChildIt->first->traverseUp();
				
				auto children = vector< OctreeNodePtr >();
				
				// Puts children into vector.
				children.push_back( firstChildIt->second );
				
				typename OctreeMap::iterator currentChildIt = firstChildIt;
				while( ( ++currentChildIt ) != hierarchyEnd && *currentChildIt->first->traverseUp() == *parentCode )
				{
					OctreeNodePtr currentChild = currentChildIt->second;
					children.push_back( currentChild );
				}
				
				cout << "Before node build: " << parentCode->getPathToRoot( true );
				
				ParentOctree::buildInnerNode( firstChildIt, currentChildIt, parentCode, children );
				
				cout << "Before release" << endl;
				// Release node if memory pressure is high enough.
				MortonCodePtr lastReleased = releaseNodesAtCreation();
				
				if( lastReleased == nullptr )
				{
					cout << "No release necessary" << endl;
					
					if( firstChildIt == hierarchyEnd )
					{
						MortonCodePtr nextFirstChildCode = parentCode->getLastChild()->getNext();
						if( *nextFirstChildCode <= *lvlBoundary )
						{
							// No more in-memory nodes in this lvl. Load more if any.
							loadNodesAndValidateIter( nextFirstChildCode, lvlBoundary, firstChildIt );
						}
					}
				}
				else
				{
					cout << "Release done" << endl;
					
					MortonCodePtr nextFirstChildCode = parentCode->getLastChild()->getNext();
					if( *lastReleased < *nextFirstChildCode )
					{
						// Nodes should be loaded and iterator revalidated, since it was released.
						loadNodesAndValidateIter( nextFirstChildCode, lvlBoundary, firstChildIt );
					}
				}
				
				cout << "Before lvl end checking" << endl;
				
				if( firstChildIt == hierarchyEnd )
				{
					cout << "Hierarchy ended" << endl;
					isLevelEnded = true;
				}
				else
				{
					cout << "Hierarchy not ended ended, checking lvl boundary" << endl;
					isLevelEnded = !( *firstChildIt->first <= *lvlBoundary );
				}
			}
			
			cout << "========== End of level " << level << " ==========" << endl << endl;
		}
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline void OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >
	::loadNodesAndValidateIter( const MortonCodePtr& nextFirstChildCode, const MortonCodePtr& lvlBoundary,
								typename OctreeMap::iterator& firstChildIt )
	{
		OctreeMapPtr hierarchy = ParentOctree::m_hierarchy;
		typename OctreeMap::iterator hierarchyEnd = hierarchy->end();
		
		SQLiteQuery query = getRangeInDB( nextFirstChildCode, lvlBoundary );
		
		MortonCodePtr firstLoadedCode = loadSiblingGroups( query );
		
		if( firstLoadedCode == nullptr )
		{
			//No more nodes in this lvl. Nodes are not needed to be loaded. End iteration.
			firstChildIt = hierarchyEnd;
		}
		else
		{
			// Revalidating iterator
			firstChildIt = hierarchy->find( firstLoadedCode );
		}
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	shared_ptr< MortonCode > OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >
	::loadSiblingGroups( SQLiteQuery& query )
	{
		IdNode idNode;
		bool isQueryEnded = !query.step( &idNode );
		
		if( isQueryEnded )
		{
			cout << "Query have no results." << endl;
			return nullptr;
		}
		
		MortonCodePtr firstLoadedCode = make_shared< MortonCode >( *idNode.first );
		MortonCodePtr currentCode( idNode.first );
		MortonCodePtr parentCode = currentCode->traverseUp();
		
		int nLoadedSiblingGroups = 0;
		for( int i = 0; i < M_SIBLING_GROUPS_PER_LOAD; ++i )
		{
			while( currentCode->isChildOf( *parentCode ) )
			{
				cout << "Loading: "<< currentCode->getPathToRoot( true );
				( *ParentOctree::m_hierarchy )[ currentCode ] = OctreeNodePtr( idNode.second );
				
				isQueryEnded = !query.step( &idNode );
				if( isQueryEnded )
				{
					break;
				}
				currentCode = MortonCodePtr( idNode.first );
			}
			
			if( isQueryEnded )
			{
				break;
			}
			
			parentCode = currentCode->traverseUp();
		}
		cout << "Hierarchy size after persisting:" << ParentOctree::m_hierarchy->size() << endl;
		
		return firstLoadedCode;
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline void OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >
	::eraseNodes( const typename OctreeMap::iterator& first, const typename OctreeMap::iterator& last )
	{
		auto prevLast = last;
		--prevLast;
		
		cout << "Deleting siblings:" << endl;
		cout << "From: " << first->first->getPathToRoot( true );
		cout << "To: " << prevLast->first->getPathToRoot( true );
		
		m_sqLite.deleteNodes( *first->first, *prevLast->first );
		
		cout << "Erasing nodes in-memory" << endl;
		
		ParentOctree::m_hierarchy->erase( first, last );
		
		cout << "After in-memory erase." << endl;
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline bool OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >
	::isDirty( const MortonCodePtr& code ) const
	{
		return *code < *m_lastPersisted;
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	void OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >::buildBoundaries( const PointVector& points )
	{
		throw logic_error(  "buildBoundaries( PointVector& ) is unsuported. Use buildBoundaries() to take into consideration"
							" the database or another non out of core octree implementation" );
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	void OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >::buildNodes( PointVector& points )
	{
		throw logic_error(  "buildNodes( PointVector& ) is unsuported. Use ***() to take into consideration"
							" the database or another non out of core octree implementation" );
	}
		
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	void OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >::buildLeaves( const PointVector& points )
	{
		throw logic_error(  "buildLeaves( PointVector& ) is unsuported. Use ***() to take into consideration"
							" the database or another non out of core octree implementation" );
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	void OutOfCoreOctree< MortonCode, Point, Front, FrontInsertionContainer >::build( PointVector& points )
	{
		throw logic_error(  "build( PointVector& ) is unsuported. Use buildFromFile or another non out of core octree"
							"implementation" );
	}
	
	// ====================== Type Sugar ================================ /
	template< typename MortonCode, typename Point >
	using DefaultOutOfCoreOctree = OutOfCoreOctree< MortonCode, Point, unordered_set< MortonCode >, vector< MortonCode > >;
	
	using ShallowOutOfCoreOctree = DefaultOutOfCoreOctree< ShallowMortonCode, Point >;
	using ShallowOutOfCoreOctreePtr = shared_ptr< ShallowOutOfCoreOctree >;
	
	using MediumOutOfCoreOctree = DefaultOutOfCoreOctree< MediumMortonCode, Point >;
	using MediumOutOfCoreOctreePtr = shared_ptr< MediumOutOfCoreOctree >;
	
	using ShallowExtOutOfCoreOctree = DefaultOutOfCoreOctree< ShallowMortonCode, ExtendedPoint >;
	using ShallowExtOutOfCoreOctreePtr = shared_ptr< ShallowExtOutOfCoreOctree >;
	
	using MediumExtOutOfCoreOctree = DefaultOutOfCoreOctree< MediumMortonCode, ExtendedPoint >;
	using MediumExtOutOfCoreOctreePtr = shared_ptr< MediumExtOutOfCoreOctree >;
}

#endif