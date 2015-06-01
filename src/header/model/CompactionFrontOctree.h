#ifndef COMPACTION_FRONT_OCTREE_H
#define COMPACTION_FRONT_OCTREE_H

#include "FrontOctree.h"
#include "Front.h"
#include "CompactionRenderingState.h"

namespace model
{
	/** Octree that uses stream compaction in order to reuse points rendered in previous frames and transfer less data to the GPU. */
	template< typename MortonCode, typename Point >
	class CompactionFrontOctree
	: public FrontOctree< MortonCode, Point, typename FrontTypes< MortonCode >::Front >
	{
		using CompactionFrontOctree = model::CompactionFrontOctree< MortonCode, Point >;
		using CompactionRenderingState = model::CompactionRenderingState;
		using Front = typename model::FrontTypes< MortonCode >::Front;
		using CompactionFrontOctree = model::CompactionFrontOctree< MortonCode, Point, Front >;
		using FrontWrapper = model::FrontWrapper< MortonCode, Point, Front >;
	public:
		CompactionFrontOctree( const int& maxPointsPerNode, const int& maxLevel );
		
		~CompactionFrontOctree();
		
		/** @copydoc FrontOctree::trackFront It also assumed that m_renderingState is initialized on the end of the traversal from
		 * the root. */
		FrontOctreeStats trackFront( QGLPainter* painter, const Attributes& attribs, const Float& projThresh );
	
	private:
		/** Non-transient rendering state used to ensure temporal coherence in rendering. */
		CompactionRenderingState* m_renderingState;
		
		vector< unsigned int > compactionFlags;
	};
	
	template< typename MortonCode, typename Point >
	CompactionFrontOctree( const int& maxPointsPerNode, const int& maxLevel )
	: FrontOctree( maxPointsPerNode, maxLevel ) {}
	
	template< typename MortonCode, typename Point >
	~CompactionFrontOctree()
	{
		delete m_renderingState;
	}
	
	/*template< typename MortonCode, typename Float, typename Vec3, typename Point >
	FrontOctreeStats CompactionFrontOctree::trackFront( QGLPainter* painter, const Attributes& attribs, const Float& projThresh )
	{
		clock_t timing = clock();
		
		//cout << "========== Starting front tracking ==========" << endl;
		m_frontInsertionList.clear();
		
		m_renderingState->setPainter( painter );
		
		FrontWrapper::trackFront( *this, m_renderingState, projThresh );
		
		onTraversalEnd();
		
		timing = clock() - timing;
		float traversalTime = float( timing ) / CLOCKS_PER_SEC * 1000;
		
		timing = clock();
		
		unsigned int numRenderedPoints = renderingState.render();
		
		timing = clock() - timing;
		float renderingTime = float( timing ) / CLOCKS_PER_SEC * 1000;
		
		//cout << "========== Ending front tracking ==========" << endl << endl;
		
		return FrontOctreeStats( traversalTime, renderingTime, numRenderedPoints, m_front.size() );
	}*/
}

#endif