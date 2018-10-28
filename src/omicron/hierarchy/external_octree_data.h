#ifndef EXTERNAL_OCTREE_DATA_H
#define EXTERNAL_OCTREE_DATA_H

#include <stxxl/vector>
#include "omicron/renderer/splat_renderer/surfel.hpp"
#include "omicron/hierarchy/octree_dimensions.h"

namespace omicron::hierarchy
{
    /** All external datastructures of the octree nodes and methods to operate them. */
    class ExtOctreeData
    {
    public:
        using SurfelIter = Surfel*;
        using IndexVector = std::vector< ulong, TbbAllocator< ulong > >;

        static void pushSurfel( const Surfel& s ) { m_surfels.push_back( s ); }
        
        template< typename Morton >
		static Morton calcMorton( const ulong index, const OctreeDimensions< Morton >& octreeDim ) { return octreeDim.calcMorton( m_surfels[ index ] ); }
        static const Surfel& getSurfel( const ulong index ) { return m_surfels[ index ]; }


        static ulong reserveIndices( const uint nIndices );
        static void copy2External( const IndexVector& indices, const ulong pos );
        static void copyFromExternal( SurfelIter& iter, const ulong pos, const int hierarchyLvl, const uint size );
    
    private:
        #ifdef LAB
            static constexpr ulong EXT_SURFEL_MEM_BUDGET = 1ul * 1024ul * 1024ul * 1024ul;
            static constexpr ulong EXT_INDEX_MEM_BUDGET = 1ul * 1024ul * 1024ul * 1024ul;
        #else
            static constexpr ulong EXT_SURFEL_MEM_BUDGET = 5ul * 1024ul * 1024ul * 1024ul;
            static constexpr ulong EXT_INDEX_MEM_BUDGET = 5ul * 1024ul * 1024ul * 1024ul;
        #endif
        static constexpr ulong EXT_PAGE_SIZE = 4ul;
        static constexpr ulong EXT_BLOCK_SIZE = 2ul * 1024ul * 1024ul;

        using ExtSurfelVector = stxxl::VECTOR_GENERATOR< Surfel, EXT_PAGE_SIZE, EXT_SURFEL_MEM_BUDGET / ( EXT_BLOCK_SIZE * EXT_PAGE_SIZE ), EXT_BLOCK_SIZE >::result;
        using ExtIndexVector = stxxl::VECTOR_GENERATOR< ulong, EXT_PAGE_SIZE, EXT_INDEX_MEM_BUDGET / ( EXT_BLOCK_SIZE * EXT_PAGE_SIZE ), EXT_BLOCK_SIZE >::result;
        
        static ExtSurfelVector m_surfels;
		static ExtIndexVector m_indices;
        static mutex m_indexMutex;
    };

    inline ulong ExtOctreeData::reserveIndices( const uint nIndices )
    {
        lock_guard< mutex > lock( m_indexMutex );
        ulong sizeBefore = m_indices.size();
        m_indices.resize( sizeBefore + nIndices );
        return sizeBefore;
    }
    
    inline void ExtOctreeData::copy2External( const IndexVector& indices, ulong pos )
    {
        for( const ulong i : indices )
        {
            m_indices[ pos++ ] = i;
        }
    }

    inline void ExtOctreeData::copyFromExternal( SurfelIter& iter, const ulong pos, const int hierarchyLvl, const uint size )
    {
        Vector2f multipliers = ReconstructionParams::calcMultipliers( hierarchyLvl );

        for( ulong i = pos; i < pos + size; ++i )
        {
            Surfel s = m_surfels[ m_indices[ i ] ];
            s.multiplyTangents( multipliers );
            *iter++ = s;
        }
    }
}

#endif