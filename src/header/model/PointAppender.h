#ifndef POINT_APPENDER_H
#define POINT_APPENDER_H

#include "OctreeNode.h"
#include "ExtendedPoint.h"

namespace model
{
	/** Class with utilitary methods for appending points into vectors. Used by octree code. */
	template< typename MortonCode, typename Point >
	class PointAppender
	{
		using PointPtr = shared_ptr< Point >;
		using PointVector = vector< PointPtr, ManagedAllocator< PointPtr > >;
		using PointVectorPtr = shared_ptr< PointVector >;
		
	public:
		virtual void appendPoints( OctreeNodePtr node, PointVector& vec, int& numChildren, int& numLeaves )
		const
		{
			++numChildren;
			if( node->isLeaf() )
			{
				PointVector& childPoints = node-> template getContents< PointVector >();
				vec.insert( vec.end(), childPoints.begin(), childPoints.end() );
				++numLeaves;
			}
			else
			{
				PointPtr LODPoint = node-> template getContents< PointPtr >();
				vec.push_back( LODPoint );
			}
		}
		
		/** API for appending points as indices for a point array. */
		virtual void appendPoints( OctreeNodePtr node, IndexVector& vec, int& numChildren,
								   int& numLeaves ) const {}
		
	};
	
	template< typename MortonCode, typename Point >
	class RandomPointAppender
	: public PointAppender< MortonCode, Point >
	{
		using PointPtr = shared_ptr< Point >;
		using PointVector = vector< PointPtr, ManagedAllocator< PointPtr > >;
		using PointVectorPtr = shared_ptr< PointVector >;
		
	public:
		virtual void appendPoints( OctreeNodePtr node, PointVector& vec, int& numChildren, int& numLeaves )
		const
		{
			++numChildren;
			if( node->isLeaf() )
			{
				++numLeaves;
			}
			
			PointVector& childPoints = node-> template getContents< PointVector >();
			vec.insert( vec.end(), childPoints.begin(), childPoints.end() );
		}
		
		virtual void appendPoints( OctreeNodePtr node, IndexVector& vec, int& numChildren, int& numLeaves )
		const
		{
			++numChildren;
			if( node->isLeaf() )
			{
				++numLeaves;
			}
			
			IndexVector& childIndices = node-> template getContents< IndexVector >();
			vec.insert( vec.end(), childIndices.begin(), childIndices.end() );
		}
	};
}

#endif