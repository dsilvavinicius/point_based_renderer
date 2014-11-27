#ifndef FRONT_OCTREE_H
#define FRONT_OCTREE_H

#include <unordered_set>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

#include "RandomSampleOctree.h"

//using boost::multi_index_container;
//using namespace ::boost;
//using namespace boost::multi_index;

namespace model
{
	/** Bidirectional map with an ordered index and a hashed index. */
	/*template< typename FromType, typename ToType >
	struct BidirectionalMap
	{
		struct from{};
		struct to{};
		
		struct ValueType
		{
			ValueType( const FromType& first, const ToType& second ):
			m_first( first ),
			m_second( second )
			{}

			FromType m_first;
			ToType   m_second;
		};
		
		using Container = multi_index_container<
			ValueType,
			indexed_by<
				ordered_unique<
					tag< from >, member< ValueType, FromType, &ValueType::m_first > >,
				hashed_unique<
					tag< to >,   member< ValueType, ToType,   &ValueType::m_second > >
			>
		>;
	};*/
	
	/** Hierarchy front. The front is formed by all nodes in which the hierarchy traversal ends. */
	/*template< typename MortonPrecision >
	using Front = typename BidirectionalMap< unsigned long, MortonCodePtr< MortonPrecision > >::Container;
	
	using ShallowFront = Front< unsigned int >;
	using DeepFront = Front< unsigned long >;*/
	
	/** Octree that supports temporal coherence by hierarchy front tracking.  */
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point >
	class FrontOctree
	: public RandomSampleOctree< MortonPrecision, Float, Vec3, Point >
	{
		using MortonCode = model::MortonCode< MortonPrecision >;
		using MortonCodePtr = model::MortonCodePtr< MortonPrecision >;
		using RandomSampleOctree = model::RandomSampleOctree< MortonPrecision, Float, Vec3, Point >;
		using RenderingState = model::RenderingState< Vec3 >;
		using MortonPtrVector = vector< MortonCodePtr >;
		using MortonVector = vector< MortonCode >;
		using OctreeNodePtr = model::OctreeNodePtr< MortonPrecision, Float, Vec3 >;
		using PointVector = model::PointVector< Float, Vec3 >;
		using PointVectorPtr = model::PointVectorPtr< Float, Vec3 >;
		
		/** The front node is formed by a MortonCodePtr identifier and a boolean indicating the validity of the node
		 * ( true means valid, false means invalid ). */
		using Front = unordered_set< MortonCode >;
		
	public:
		FrontOctree( const int& maxPointsPerNode, const int& maxLevel );
		
		/** Tracks the hierarchy front. */
		FrontOctreeStats trackFront( QGLPainter* painter, const Attributes& attribs, const Float& projThresh );
		
		/** Checks if the node and their siblings should be pruned from the front, giving place to their parent. */
		bool checkPrune( RenderingState& renderingState, const MortonCodePtr& code, const Float& projThresh ) const;
		
		/** Creates the deletion and insertion entries related with the prunning of the node and their siblings. */
		void prune( const MortonCodePtr& code, RenderingState& renderingState );
		
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
	
	private:
		/** Internal setup method for both leaf and inner node cases. */
		void setupNodeRendering( OctreeNodePtr node, MortonCodePtr code, RenderingState& renderingState );
		
		/** Hierarchy front. */
		Front m_front;
		
		/** List with the nodes that will be deleted in current front tracking. */
		//MortonVector m_frontDeletionList;
		
		/** List with the nodes that will be included in current front tracking. */
		MortonVector m_frontInsertionList;
		
		/** Number of nodes that are deeper than the current desired projection threshold. Used to trigger the front
		 * reconstruction. */
		//unsigned long m_invalidFrontNodes;
		
		/** The projection threshold used in the last frame. Used to track the front. */
		//Float m_lastProjThresh;
	};
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point >
	FrontOctree< MortonPrecision, Float, Vec3, Point >::FrontOctree( const int& maxPointsPerNode, const int& maxLevel )
	: RandomSampleOctree::RandomSampleOctree( maxPointsPerNode, maxLevel )/*,
	m_invalidFrontNodes( 0 )*/
	{}
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point >
	FrontOctreeStats FrontOctree< MortonPrecision, Float, Vec3, Point >::trackFront( QGLPainter* painter,
																					 const Attributes& attribs,
																				  const Float& projThresh )
	{
		//cout << "========== Starting front tracking ==========" << endl;
		m_frontInsertionList.clear();
		
		//
		/*cout << "Front: " << endl;
		for( MortonCode code : m_front )
		{
			cout << hex << code.getBits() << dec << endl;
		}*/
		//
		
		RenderingState renderingState( painter, attribs );
		
		// Flag to indicate if the previous front entry should be deleted. This can be done only after the
		// iterator to the entry to be deleted is incremented. Otherwise the iterator will be invalidated before
		// increment.
		bool erasePrevious = false;
		
		typename Front::iterator end = m_front.end();
		typename Front::iterator prev;
		
		for( typename Front::iterator it = m_front.begin(); it != end; prev = it, ++it, end = m_front.end() )
		{
			//cout << endl << "Current: " << hex << it->getBits() << dec << endl;
			if( erasePrevious )
			{
				//cout << "Erased: " << hex << prev->getBits() << dec << endl;
				m_front.erase( prev );
			}
			erasePrevious = false;
			
			MortonCodePtr code = make_shared< MortonCode >( *it );
			
			bool isCullable = false;
			
			// Code for prunnable front
			if( checkPrune( renderingState, code, projThresh ) )
			{
				//cout << "Prune" << endl;
				erasePrevious = true;
				prune( code, renderingState );
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
						
						QBox3D box = RandomSampleOctree::getBoundaries( child );
						if( !RandomSampleOctree::isCullable( box, renderingState ) )
						{
							//cout << "Point set to render: " << hex << child->getBits() << dec << endl;
							OctreeNodePtr node = nodeIt->second;
							PointVectorPtr points = node-> template getContents< PointVector >();
							renderingState.handleNodeRendering( renderingState, points );
						}
					}
				}
				
				if( !erasePrevious )
				{
					auto nodeIt = RandomSampleOctree::m_hierarchy->find( code );
					assert( nodeIt != RandomSampleOctree::m_hierarchy->end() );
					
					OctreeNodePtr node = nodeIt->second;
					PointVectorPtr points = node-> template getContents< PointVector >();
					renderingState.handleNodeRendering( renderingState, points );
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
					renderingState.handleNodeRendering( renderingState, points );
				}
			}
		}
		
		// Delete the node from last iteration, if necessary.
		if( erasePrevious )
		{
			//cout << "Erased: " << hex << prev->getBits() << dec << endl;
			m_front.erase( prev );
		}
		
		onTraversalEnd();
		
		unsigned int numRenderedPoints = renderingState.render();
		
		//cout << "========== Ending front tracking ==========" << endl << endl;
		
		return FrontOctreeStats( numRenderedPoints, m_front.size()/* , m_invalidFrontNodes*/ );
	}
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point >
	inline void FrontOctree< MortonPrecision, Float, Vec3, Point >::prune( const MortonCodePtr& code,
																		   RenderingState& renderingState )
	{
		MortonPtrVector deletedCodes = code->traverseUp()->traverseDown();
		
		for( MortonCodePtr deletedCode : deletedCodes )
		{
			if( *deletedCode != *code )
			{
				//cout << "Prunning: " << hex << deletedCode->getBits() << dec << endl;
				
				typename Front::iterator it = m_front.find( *deletedCode );
				if( it != m_front.end()  )
				{
					//cout << "Pruned: " << hex << deletedCode->getBits() << dec << endl;
					m_front.erase( it );
				}
			}
		}
	}
	
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point >
	inline bool FrontOctree< MortonPrecision, Float, Vec3, Point >::checkPrune( RenderingState& renderingState,
																				const MortonCodePtr& code,
																			 const Float& projThresh ) const
	{
		if( code->getBits() == 1 )
		{	// Don't prune the root node.
			return false;
		}
		
		MortonCodePtr parent = code->traverseUp();
		QBox3D box = RandomSampleOctree::getBoundaries( parent );
		bool parentIsCullable = RandomSampleOctree::isCullable( box, renderingState );
		
		if( parentIsCullable )
		{
			return true;
		}
		
		bool parentIsRenderable = RandomSampleOctree::isRenderable( box, renderingState, projThresh );
		if( !parentIsRenderable )
		{
			return false;
		}
		
		return true;
	}
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point >
	inline bool FrontOctree< MortonPrecision, Float, Vec3, Point >::checkBranch( RenderingState& renderingState,
																				 const MortonCodePtr& code,
																			  const Float& projThresh,
																			  bool& out_isCullable ) const
	{
		QBox3D box = RandomSampleOctree::getBoundaries( code );
		out_isCullable = RandomSampleOctree::isCullable( box, renderingState );
		
		return !RandomSampleOctree::isRenderable( box, renderingState, projThresh ) && !out_isCullable;
	}
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point >
	inline void FrontOctree< MortonPrecision, Float, Vec3, Point >::setupLeafNodeRendering( OctreeNodePtr leafNode,
																							MortonCodePtr code,
																						 RenderingState& renderingState )
	{
		assert( leafNode->isLeaf() && "leafNode cannot be inner." );
		
		setupNodeRendering( leafNode, code, renderingState );
	}
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point >
	inline void FrontOctree< MortonPrecision, Float, Vec3, Point >::setupInnerNodeRendering( OctreeNodePtr innerNode,
																							 MortonCodePtr code,
																						  RenderingState& renderingState )
	{
		assert( !innerNode->isLeaf() && "innerNode cannot be leaf." );
		
		setupNodeRendering( innerNode, code, renderingState );
	}
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point >
	inline void FrontOctree< MortonPrecision, Float, Vec3, Point >::setupNodeRendering( OctreeNodePtr node, MortonCodePtr code,
																						RenderingState& renderingState )
	{
		//cout << "Inserted draw: " << hex << code->getBits() << dec << endl;
		m_frontInsertionList.push_back( *code );
		
		PointVectorPtr points = node-> template getContents< PointVector >();
		renderingState.handleNodeRendering( renderingState, points );
	}
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point >
	inline void FrontOctree< MortonPrecision, Float, Vec3, Point >::handleCulledNode( MortonCodePtr code )
	{
		//cout << "Inserted cull: " << hex << code->getBits() << dec << endl;
		m_frontInsertionList.push_back( *code );
	}
	
	template< typename MortonPrecision, typename Float, typename Vec3, typename Point >
	void FrontOctree< MortonPrecision, Float, Vec3, Point >::onTraversalEnd()
	{
		m_front.insert( m_frontInsertionList.begin(), m_frontInsertionList.end() );
	}
	
	//=====================================================================
	// Type Sugar.
	//=====================================================================
	
	template< typename Float, typename Vec3, typename Point >
	using ShallowFrontOctree = FrontOctree< unsigned int, Float, Vec3, Point >;
	
	template< typename Float, typename Vec3, typename Point >
	using ShallowFrontOctreePtr = shared_ptr< ShallowFrontOctree< Float, Vec3, Point > >;
	
	template< typename Float, typename Vec3, typename Point >
	using MediumFrontOctree = FrontOctree< unsigned long, Float, Vec3, Point >;
	
	template< typename Float, typename Vec3, typename Point >
	using MediumFrontOctreePtr = shared_ptr< MediumFrontOctree< Float, Vec3, Point > >;
}

#endif