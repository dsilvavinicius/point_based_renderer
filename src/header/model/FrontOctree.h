#ifndef FRONT_OCTREE_H
#define FRONT_OCTREE_H

#include <unordered_set>

#include "FrontBehavior.h"
#include "RandomSampleOctree.h"

namespace model
{
	/** Octree that supports temporal coherence by hierarchy front tracking. */
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point, typename Front >
	class FrontOctree
	: public RandomSampleOctree< MortonPrecision, Float, Vec3, Point >
	{
		using MortonCode = model::MortonCode< MortonPrecision >;
		using MortonCodePtr = model::MortonCodePtr< MortonPrecision >;
		using RandomSampleOctree = model::RandomSampleOctree< MortonPrecision, Float, Vec3, Point >;
		using RenderingState = model::RenderingState< Vec3, Float >;
		using TransientRenderingState = model::TransientRenderingState< Vec3, Float >;
		using MortonPtrVector = vector< MortonCodePtr >;
		using MortonVector = vector< MortonCode >;
		using OctreeNodePtr = model::OctreeNodePtr< MortonPrecision, Float, Vec3 >;
		using PointPtr = shared_ptr< Point >;
		using PointVector = vector< PointPtr >;
		using PointVectorPtr = shared_ptr< PointVector >;
		using FrontBehavior = model::FrontBehavior< MortonPrecision, Float, Vec3, Point, Front >;
		
		friend FrontBehavior;
		
	public:
		FrontOctree( const int& maxPointsPerNode, const int& maxLevel );
		
		~FrontOctree();
		
		/** Tracks the hierarchy front, by prunning or branching nodes ( one level only ). This method should be called
		 * after RandomSampleOctree::traverse( RenderingState& renderer, const Float& projThresh ),
		 * so the front can be init in a traversal from root. */
		FrontOctreeStats trackFront( RenderingState& renderer, const Float& projThresh );
	
	protected:
		/** Tracks one node of the front.
		 * @returns true if the node represented by code should be deleted or false otherwise. */
		bool trackNode( MortonCodePtr& code, RenderingState& renderingState, const Float& projThresh );
		
		/** Checks if the node and their siblings should be pruned from the front, giving place to their parent. */
		bool checkPrune( RenderingState& renderingState, const MortonCodePtr& code, const Float& projThresh ) const;
		
		/** Creates the deletion and insertion entries related with the prunning of the node and their siblings. */
		void prune( const MortonCodePtr& code );
		
		/** Check if the node should be branched, giving place to its children.
		 * @param isCullable is an output that indicates if the node was culled by frustrum. */
		bool checkBranch( RenderingState& renderingState, const MortonCodePtr& code, const Float& projThresh,
			bool& out_isCullable ) const;
		
		/** Creates the deletion and insertion entries related with the branching of the node. */
		void branch( const MortonCodePtr& code );
		
		/** Overriden to add rendered node into front addition list. */
		void setupLeafNodeRendering( OctreeNodePtr leafNode, MortonCodePtr code, RenderingState& renderingState );
		
		/** Overriden to add rendered node into front addition list. */
		void setupInnerNodeRendering( OctreeNodePtr innerNode, MortonCodePtr code, RenderingState& renderingState );
		
		/** Overriden to add culled node into front addition list. */
		void handleCulledNode( MortonCodePtr code );
		
		/** Overriden to push the front addition list to the front itself. */
		void onTraversalEnd();
		
		/** Internal setup method for both leaf and inner node cases. */
		void setupNodeRendering( OctreeNodePtr node, MortonCodePtr code, RenderingState& renderingState );
		
		/** Object with data related behavior of the front. */
		FrontBehavior* m_frontBehavior;
		
		/** Hierarchy front. */
		Front m_front;
		
		/** List with the nodes that will be included in current front tracking. */
		MortonVector m_frontInsertionList;
	};
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point, typename Front >
	FrontOctree< MortonPrecision, Float, Vec3, Point, Front >::FrontOctree( const int& maxPointsPerNode,
																			const int& maxLevel )
	: RandomSampleOctree::RandomSampleOctree( maxPointsPerNode, maxLevel )
	{
		m_frontBehavior = new FrontBehavior( *this );
	}
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point, typename Front >
	FrontOctree< MortonPrecision, Float, Vec3, Point, Front >::~FrontOctree()
	{
		delete m_frontBehavior;
	}
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point, typename Front >
	FrontOctreeStats FrontOctree< MortonPrecision, Float, Vec3, Point, Front >::trackFront(
		RenderingState& renderer, const Float& projThresh )
	{
		clock_t timing = clock();
		
		//cout << "========== Starting front tracking ==========" << endl;
		m_frontInsertionList.clear();
		
		//
		/*cout << "Front: " << endl;
		for( MortonCode code : m_front )
		{
			cout << hex << code.getBits() << dec << endl;
		}*/
		//
		
		m_frontBehavior->trackFront( renderer, projThresh );
		
		onTraversalEnd();
		
		timing = clock() - timing;
		float traversalTime = float( timing ) / CLOCKS_PER_SEC * 1000;
		
		timing = clock();
		
		unsigned int numRenderedPoints = renderer.render();
		
		timing = clock() - timing;
		float renderingTime = float( timing ) / CLOCKS_PER_SEC * 1000;
		
		//cout << "========== Ending front tracking ==========" << endl << endl;
		
		return FrontOctreeStats( traversalTime, renderingTime, numRenderedPoints, m_front.size() );
	}
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point, typename Front >
	inline bool FrontOctree< MortonPrecision, Float, Vec3, Point, Front >::trackNode(
		MortonCodePtr& code, RenderingState& renderingState, const Float& projThresh )
	{
		bool isCullable = false;
		bool erasePrevious = false;
		
		// Code for prunnable front
		if( checkPrune( renderingState, code, projThresh ) )
		{
			//cout << "Prune" << endl;
			erasePrevious = true;
			prune( code );
			code = code->traverseUp();
			
			auto nodeIt = RandomSampleOctree::m_hierarchy->find( code );
			assert( nodeIt != RandomSampleOctree::m_hierarchy->end() );
			OctreeNodePtr node = nodeIt->second;
			setupNodeRendering( node, code, renderingState );
		}
		else if( checkBranch( renderingState, code, projThresh, isCullable ) )
		{
			//cout << "Branch" << endl;
			erasePrevious = false;
			
			MortonPtrVector children = code->traverseDown();
			
			for( MortonCodePtr child : children  )
			{
				auto nodeIt = RandomSampleOctree::m_hierarchy->find( child );
				
				if( nodeIt != RandomSampleOctree::m_hierarchy->end() )
				{
					erasePrevious = true;
					//cout << "Inserted in front: " << hex << child->getBits() << dec << endl;
					m_frontInsertionList.push_back( *child );
					
					pair< Vec3, Vec3 > box = RandomSampleOctree::getBoundaries( child );
					if( !renderingState.isCullable( box ) )
					{
						//cout << "Point set to render: " << hex << child->getBits() << dec << endl;
						OctreeNodePtr node = nodeIt->second;
						PointVectorPtr points = node-> template getContents< PointVector >();
						renderingState.handleNodeRendering( points );
					}
				}
			}
			
			if( !erasePrevious )
			{
				auto nodeIt = RandomSampleOctree::m_hierarchy->find( code );
				assert( nodeIt != RandomSampleOctree::m_hierarchy->end() );
				
				OctreeNodePtr node = nodeIt->second;
				PointVectorPtr points = node-> template getContents< PointVector >();
				renderingState.handleNodeRendering( points );
			}
		}
		else
		{
			//cout << "Still" << endl;
			if( !isCullable )
			{
				// No prunning or branching done. Just send the current front node for rendering.
				auto nodeIt = RandomSampleOctree::m_hierarchy->find( code );
				assert( nodeIt != RandomSampleOctree::m_hierarchy->end() );
				
				OctreeNodePtr node = nodeIt->second;
				PointVectorPtr points = node-> template getContents< PointVector >();
				renderingState.handleNodeRendering( points );
			}
		}
		
		return erasePrevious;
	}
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point, typename Front >
	inline void FrontOctree< MortonPrecision, Float, Vec3, Point, Front >::prune( const MortonCodePtr& code )
	{
		m_frontBehavior->prune( code );
	}
	
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point, typename Front >
	inline bool FrontOctree< MortonPrecision, Float, Vec3, Point, Front >::checkPrune(
		RenderingState& renderingState, const MortonCodePtr& code, const Float& projThresh ) const
	{
		if( code->getBits() == 1 )
		{	// Don't prune the root node.
			return false;
		}
		
		MortonCodePtr parent = code->traverseUp();
		pair< Vec3, Vec3 > box = RandomSampleOctree::getBoundaries( parent );
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
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point, typename Front >
	inline bool FrontOctree< MortonPrecision, Float, Vec3, Point, Front >::checkBranch(
		RenderingState& renderingState, const MortonCodePtr& code, const Float& projThresh, bool& out_isCullable )
		const
	{
		pair< Vec3, Vec3 > box = RandomSampleOctree::getBoundaries( code );
		out_isCullable = renderingState.isCullable( box );
		
		return !renderingState.isRenderable( box, projThresh ) && !out_isCullable;
	}
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point, typename Front >
	inline void FrontOctree< MortonPrecision, Float, Vec3, Point, Front >::setupLeafNodeRendering(
		OctreeNodePtr leafNode, MortonCodePtr code, RenderingState& renderingState )
	{
		assert( leafNode->isLeaf() && "leafNode cannot be inner." );
		
		setupNodeRendering( leafNode, code, renderingState );
	}
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point, typename Front >
	inline void FrontOctree< MortonPrecision, Float, Vec3, Point, Front >::setupInnerNodeRendering(
		OctreeNodePtr innerNode, MortonCodePtr code, RenderingState& renderingState )
	{
		assert( !innerNode->isLeaf() && "innerNode cannot be leaf." );
		
		setupNodeRendering( innerNode, code, renderingState );
	}
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point, typename Front >
	inline void FrontOctree< MortonPrecision, Float, Vec3, Point, Front >::setupNodeRendering(
		OctreeNodePtr node, MortonCodePtr code, RenderingState& renderingState )
	{
		//cout << "Inserted draw: " << hex << code->getBits() << dec << endl;
		m_frontInsertionList.push_back( *code );
		
		PointVectorPtr points = node-> template getContents< PointVector >();
		renderingState.handleNodeRendering( points );
	}
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point, typename Front >
	inline void FrontOctree< MortonPrecision, Float, Vec3, Point, Front >::handleCulledNode( MortonCodePtr code )
	{
		//cout << "Inserted cull: " << hex << code->getBits() << dec << endl;
		m_frontInsertionList.push_back( *code );
	}
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point, typename Front >
	void FrontOctree< MortonPrecision, Float, Vec3, Point, Front >::onTraversalEnd()
	{
		m_frontBehavior->insert( m_frontInsertionList );
	}
	
	//=====================================================================
	// Type Sugar.
	//=====================================================================
	
	template< typename Float, typename Vec3, typename Point, typename Front >
	using ShallowFrontOctree = FrontOctree< unsigned int, Float, Vec3, Point, Front >;
	
	template< typename Float, typename Vec3, typename Point, typename Front >
	using ShallowFrontOctreePtr = shared_ptr< ShallowFrontOctree< Float, Vec3, Point, Front > >;
	
	template< typename Float, typename Vec3, typename Point, typename Front >
	using MediumFrontOctree = FrontOctree< unsigned long, Float, Vec3, Point, Front >;
	
	template< typename Float, typename Vec3, typename Point, typename Front >
	using MediumFrontOctreePtr = shared_ptr< MediumFrontOctree< Float, Vec3, Point, Front > >;
}

#endif