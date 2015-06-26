#ifndef FRONT_OCTREE_H
#define FRONT_OCTREE_H

#include <unordered_set>

#include "FrontBehavior.h"
#include "RandomSampleOctree.h"

namespace model
{
	/** Octree that supports temporal coherence by hierarchy front tracking. Nodes that are added to a insertion container
	 *	while traversing and added to the actual front at traversal ending.
	 *	@param: Front is the type used as the actual front container.
	 *	@param: FrontInsertionContainer is the insertion container, used to track nodes that should be added to the front. */
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	class FrontOctree
	: public RandomSampleOctree< MortonCode, Point >
	{
		using MortonCodePtr = shared_ptr< MortonCode >;
		using ParentOctree = model::RandomSampleOctree< MortonCode, Point >;
		using MortonPtrVector = vector< MortonCodePtr >;
		using MortonVector = vector< MortonCode >;
		using OctreeNodePtr = model::OctreeNodePtr< MortonCode >;
		using OctreeMap = model::OctreeMap< MortonCode >;
		using OctreeMapPtr = model::OctreeMapPtr< MortonCode >;
		using PointPtr = shared_ptr< Point >;
		using PointVector = vector< PointPtr >;
		using PointVectorPtr = shared_ptr< PointVector >;
		using FrontBehavior = model::FrontBehavior< MortonCode, Point, Front, FrontInsertionContainer >;
		
	public:
		FrontOctree( const int& maxPointsPerNode, const int& maxLevel );
		
		~FrontOctree();
		
		/** Tracks the hierarchy front, by prunning or branching nodes ( one level only ). This method should be called
		 * after ParentOctree::traverse( RenderingState& renderer, const Float& projThresh ),
		 * so the front can be init in a traversal from root. */
		FrontOctreeStats trackFront( RenderingState& renderer, const Float& projThresh );
	
		/** Tracks one node of the front.
		 * @returns true if the node represented by code should be deleted or false otherwise. */
		bool trackNode( MortonCodePtr& code, RenderingState& renderingState, const Float& projThresh );
		
	protected:
		/** Checks if the node and their siblings should be pruned from the front, giving place to their parent. */
		bool checkPrune( RenderingState& renderingState, const MortonCodePtr& code, const Float& projThresh ) const;
		
		/** Creates the deletion and insertion entries related with the prunning of the node and their siblings and
		 * puts parent points into renderingState if needed. */
		void prune( const MortonCodePtr& code, RenderingState& renderingState );
		
		/** Check if the node should be branched, giving place to its children.
		 * @param isCullable is an output that indicates if the node was culled by frustrum. */
		bool checkBranch( RenderingState& renderingState, const MortonCodePtr& code, const Float& projThresh,
			bool& out_isCullable ) const;
		
		/** Creates deletion ansd insertion entries related with the branching of the node and puts children's points
		 * into renderer if necessary.
		 * @returns true if code identifies an inner node (and must be deleted to make place for its children), false
		 * otherwise. */
		bool branch( const MortonCodePtr& code, RenderingState& renderingState );
		
		/** Called when the hierarchy iterator resulting of a prunning operation is acquired. Default implementation
		 * does nothing.
		 * @param it is the hierarchy iterator returned while searching for code.
		 * @param code is the node id used for searching. */
		virtual void onPrunningItAcquired( const typename OctreeMap::iterator& it, const MortonCodePtr& code ) {}
		
		/** Called when the hierarchy iterator resulting of a branching operation is acquired. Default implementation
		 * does nothing.
		 * @param it is the hierarchy iterator returned while searching for code.
		 * @param code is the node id used for searching. */
		virtual void onBranchingItAcquired( const typename OctreeMap::iterator& it, const MortonCodePtr& code ) {}
		
		/** Overriden to add rendered node into front addition list. */
		void setupLeafNodeRendering( OctreeNodePtr leafNode, MortonCodePtr code, RenderingState& renderingState );
		
		/** Overriden to add rendered node into front addition list. */
		void setupInnerNodeRendering( OctreeNodePtr innerNode, MortonCodePtr code, RenderingState& renderingState );
		
		/** Overriden to add culled node into front addition list. */
		void handleCulledNode( MortonCodePtr code );
		
		/** Overriden to push the front addition list to the front itself. */
		virtual void onTraversalEnd();
		
		/** Rendering setup method for both leaf and inner node cases. Adds the node into the front. */
		void setupNodeRendering( OctreeNodePtr node, MortonCodePtr code, RenderingState& renderingState );
		
		/** Object with data related behavior of the front. */
		FrontBehavior* m_frontBehavior;
	};
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	FrontOctree< MortonCode, Point, Front, FrontInsertionContainer >::FrontOctree( const int& maxPointsPerNode,
																						const int& maxLevel )
	: ParentOctree::RandomSampleOctree( maxPointsPerNode, maxLevel )
	{
		m_frontBehavior = new FrontBehavior( *this );
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	FrontOctree< MortonCode, Point, Front, FrontInsertionContainer >::~FrontOctree()
	{
		delete m_frontBehavior;
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	FrontOctreeStats FrontOctree< MortonCode, Point, Front, FrontInsertionContainer >::trackFront(
		RenderingState& renderer, const Float& projThresh )
	{
		clock_t timing = clock();
		
		m_frontBehavior->clearMarkedNodes();
		
		m_frontBehavior->trackFront( renderer, projThresh );
		
		onTraversalEnd();
		
		timing = clock() - timing;
		float traversalTime = float( timing ) / CLOCKS_PER_SEC * 1000;
		
		timing = clock();
		
		unsigned int numRenderedPoints = renderer.render();
		
		timing = clock() - timing;
		float renderingTime = float( timing ) / CLOCKS_PER_SEC * 1000;
		
		return FrontOctreeStats( traversalTime, renderingTime, numRenderedPoints, m_frontBehavior->size() );
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline bool FrontOctree< MortonCode, Point, Front, FrontInsertionContainer >::trackNode(
		MortonCodePtr& code, RenderingState& renderingState, const Float& projThresh )
	{
		bool isCullable = false;
		bool eraseNode = false;
		
		// Code for prunnable front
		if( checkPrune( renderingState, code, projThresh ) )
		{
			eraseNode = true;
			prune( code, renderingState );
		}
		else if( checkBranch( renderingState, code, projThresh, isCullable ) )
		{
			eraseNode = branch( code, renderingState );
		}
		else
		{
			//cout << "Still: " << code->getPathToRoot( true ) << endl;
			
			if( !isCullable )
			{
				// No prunning or branching done. Just send the current front node for rendering.
				auto nodeIt = ParentOctree::m_hierarchy->find( code );
				assert( nodeIt != ParentOctree::m_hierarchy->end() );
				
				OctreeNodePtr node = nodeIt->second;
				ParentOctree::setupNodeRendering( node, renderingState );
			}
		}
		
		return eraseNode;
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline bool FrontOctree< MortonCode, Point, Front, FrontInsertionContainer >::checkPrune(
		RenderingState& renderingState, const MortonCodePtr& code, const Float& projThresh ) const
	{
		if( code->getBits() == 1 )
		{	// Don't prune the root node.
			return false;
		}
		
		MortonCodePtr parent = code->traverseUp();
		pair< Vec3, Vec3 > box = ParentOctree::getBoundaries( parent );
		bool parentIsCullable = renderingState.isCullable( box );
		
		if( parentIsCullable )
		{
			return true;
		}
		
		bool parentIsRenderable = renderingState.isRenderable( box, projThresh );
		if( !parentIsRenderable )
		{
			return false;
		}
		
		return true;
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline void FrontOctree< MortonCode, Point, Front, FrontInsertionContainer >
	::prune( const MortonCodePtr& code, RenderingState& renderingState )
	{
		//cout << "Prune: " << code->getPathToRoot( true ) << endl;
		
		m_frontBehavior->prune( code );
		MortonCodePtr parentCode = code->traverseUp();
			
		auto nodeIt = ParentOctree::m_hierarchy->find( parentCode );
		
		onPrunningItAcquired( nodeIt, parentCode );
		
		if( nodeIt != ParentOctree::m_hierarchy->end() )
		{
			OctreeNodePtr parentNode = nodeIt->second;
			setupNodeRendering( parentNode, parentCode, renderingState );
		}
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline bool FrontOctree< MortonCode, Point, Front, FrontInsertionContainer >
	::checkBranch( RenderingState& renderingState, const MortonCodePtr& code, const Float& projThresh,
				   bool& out_isCullable ) const
	{
		pair< Vec3, Vec3 > box = ParentOctree::getBoundaries( code );
		out_isCullable = renderingState.isCullable( box );
		
		return !renderingState.isRenderable( box, projThresh ) && !out_isCullable;
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline bool FrontOctree< MortonCode, Point, Front, FrontInsertionContainer >
	::branch( const MortonCodePtr& code, RenderingState& renderingState )
	{
		//cout << "Branch:" << code->getPathToRoot( true ) << endl;
		
		auto nodeIt = ParentOctree::m_hierarchy->find( code );
		assert( nodeIt != ParentOctree::m_hierarchy->end() );
		OctreeNodePtr node = nodeIt->second;
		
		bool isInner = !node->isLeaf();
		
		if( isInner )
		{
			MortonCodePtr firstChild = code->getFirstChild();
			auto childIt = ParentOctree::m_hierarchy->lower_bound( firstChild );
			//auto pastLastChildIt = ParentOctree::m_hierarchy->upper_bound( code->getLastChild() );
			
			//cout << "First child: " << childIt->first->getPathToRoot( true ) << "Last child: "
			//	 << std::prev( pastLastChildIt )->first->getPathToRoot( true ) << endl;
			
			onBranchingItAcquired( childIt, firstChild );
			
			while( childIt != ParentOctree::m_hierarchy->end() && *childIt->first->traverseUp() == *code )
			{
				MortonCodePtr childCode = childIt->first;
				
				//cout << "Inserting: " << childCode->getPathToRoot( true ) << endl;
				
				m_frontBehavior->insert( *childCode );
				
				pair< Vec3, Vec3 > box = ParentOctree::getBoundaries( childCode );
				if( !renderingState.isCullable( box ) )
				{
					//cout << "Point set to render: " << hex << child->getBits() << dec << endl;
					OctreeNodePtr node = childIt->second;
					ParentOctree::setupNodeRendering( node, renderingState );
				}
				
				++childIt;
			}
		}
		
		return isInner;
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline void FrontOctree< MortonCode, Point, Front, FrontInsertionContainer >::setupLeafNodeRendering(
		OctreeNodePtr leafNode, MortonCodePtr code, RenderingState& renderingState )
	{
		assert( leafNode->isLeaf() && "leafNode cannot be inner." );
		
		setupNodeRendering( leafNode, code, renderingState );
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline void FrontOctree< MortonCode, Point, Front, FrontInsertionContainer >::setupInnerNodeRendering(
		OctreeNodePtr innerNode, MortonCodePtr code, RenderingState& renderingState )
	{
		assert( !innerNode->isLeaf() && "innerNode cannot be leaf." );
		
		setupNodeRendering( innerNode, code, renderingState );
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline void FrontOctree< MortonCode, Point, Front, FrontInsertionContainer >::setupNodeRendering(
		OctreeNodePtr node, MortonCodePtr code, RenderingState& renderingState )
	{
		//cout << "Inserted draw: " << code->getPathToRoot( true ) << endl;
		m_frontBehavior->insert( *code );
		
		ParentOctree::setupNodeRendering( node, renderingState );
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	inline void FrontOctree< MortonCode, Point, Front, FrontInsertionContainer >::handleCulledNode(
		MortonCodePtr code )
	{
		//cout << "Inserted cull: " << hex << code->getBits() << dec << endl;
		m_frontBehavior->insert( *code );
	}
	
	template< typename MortonCode, typename Point, typename Front, typename FrontInsertionContainer >
	void FrontOctree< MortonCode, Point, Front, FrontInsertionContainer >::onTraversalEnd()
	{
		m_frontBehavior->onFrontTrackingEnd();
	}
	
	//=====================================================================
	// Type Sugar.
	//=====================================================================
	
	/** An front octree with shallow morton code and usual data structures for front and front insertion container. */
	template< typename MortonCode, typename Point  >
	using DefaultFrontOctree = FrontOctree< MortonCode, Point, unordered_set< MortonCode >, vector< MortonCode > >;
	
	using ShallowFrontOctree = DefaultFrontOctree< ShallowMortonCode, Point >;
	using ShallowFrontOctreePtr = shared_ptr< ShallowFrontOctree >;
	
	using MediumFrontOctree = DefaultFrontOctree< MediumMortonCode, Point >;
	using MediumFrontOctreePtr = shared_ptr< MediumFrontOctree >;
	
	using ShallowExtFrontOctree = DefaultFrontOctree< ShallowMortonCode, ExtendedPoint >;
	using ShallowExtFrontOctreePtr = shared_ptr< ShallowExtFrontOctree >;
	
	using MediumExtFrontOctree = DefaultFrontOctree< MediumMortonCode, ExtendedPoint >;
	using MediumExtFrontOctreePtr = shared_ptr< MediumExtFrontOctree >;
}

#endif