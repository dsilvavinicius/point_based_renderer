#include "omicron/basic/morton_code.h"
#include "omicron/basic/morton_interval.h"
#include "omicron/basic/morton_comparator.h"

#include <gtest/gtest.h>

namespace omicron::test
{
    using namespace omicron::basic;
    
    class MortonCodeTest : public ::testing::Test
    {
    protected:
        void SetUp() {}
    };

    TEST_F( MortonCodeTest, CreationShallow )
    {
        ShallowMortonCode shallowMorton;
        
        shallowMorton.build( 7, 5, 0, 3 ); // 10 1100 1011
        ASSERT_EQ( shallowMorton.getBits(), 0x2CBu );
        
        shallowMorton.build(7, 5, 0, 10); // 100 0000 0000 0000 0000 0000 1100 1011;
        ASSERT_EQ( shallowMorton.getBits(), 0x400000CBu );
        
        // Verifying if the unsigned long has 8 bytes.
        ASSERT_EQ( sizeof(unsigned long), 8 );
        
        MediumMortonCode mediumMorton;
        
        mediumMorton.build( 5000, 6000, 7000, 5 ); // 1110 1010 0000 0000
        ASSERT_EQ( mediumMorton.getBits(), 0xEA00ul ); 
        
        mediumMorton.build( 1002999, 501956, 785965, 11 );
        ASSERT_EQ( mediumMorton.getBits(), 0x3616E99CDul );
        
        mediumMorton.build( 5000, 6000, 7000, 13 );
        // 1111 1000 1011 1111 0011 1001 0110 1010 0000 0000
        ASSERT_EQ( mediumMorton.getBits(), 0xF8BF396A00ul );
        
        mediumMorton.build( 5000, 6000, 7000, 21 );
        // 1000 0000 0000 0000 0000 0000 0111 1000 1011 1111 0011 1001 0110 1010 0000 0000
        ASSERT_EQ( mediumMorton.getBits(), 0x80000078BF396A00ul );
    }
    
    TEST_F( MortonCodeTest, TraversalShallow )
    {
        ShallowMortonCode shallowMorton;
        
        shallowMorton.build(7, 5, 0, 3); // 10 1100 1011
        ShallowMortonCodePtr shallowParent = shallowMorton.traverseUp();
        ASSERT_EQ( shallowParent->getBits(), 0x59u );
        
        vector< ShallowMortonCodePtr > shallowChildren = shallowMorton.traverseDown();
        ASSERT_EQ( shallowChildren[ 0 ]->getBits(), 0x1658u );
        ASSERT_EQ( shallowChildren[ 1 ]->getBits(), 0x1659u );
        ASSERT_EQ( shallowChildren[ 2 ]->getBits(), 0x165Au );
        ASSERT_EQ( shallowChildren[ 3 ]->getBits(), 0x165Bu );
        ASSERT_EQ( shallowChildren[ 4 ]->getBits(), 0x165Cu );
        ASSERT_EQ( shallowChildren[ 5 ]->getBits(), 0x165Du );
        ASSERT_EQ( shallowChildren[ 6 ]->getBits(), 0x165Eu );
        ASSERT_EQ( shallowChildren[ 7 ]->getBits(), 0x165Fu );
        
        // Overflow check.
        //shallowMorton.build( 7, 5, 0, 10 ); // 100 0000 0000 0000 0000 0000 1100 1011;
        // TODO: Need to fix the next line.
        //EXPECT_EXIT( shallowMorton.traverseDown(), ::testing::ExitedWithCode( 6 ),
        //			 "shifted > bits && ""MortonCode traversal overflow.""" );
    }
    
    TEST_F( MortonCodeTest, getLevel )
    {
        ShallowMortonCode morton;
        morton.build( 1u << 3 );
        ASSERT_EQ( 1u, morton.getLevel() );
        
        morton.build( 1u << 30 );
        ASSERT_EQ( 10u, morton.getLevel() );
        
        MediumMortonCode mediumMorton;
        mediumMorton.build( 1ul << 60ul );
        ASSERT_EQ( 20u, mediumMorton.getLevel() );
    }
    
    TEST_F( MortonCodeTest, TraversalMedium )
    {
        MediumMortonCode mediumMorton;
        
        mediumMorton.build( 1002999, 501956, 785965, 11 ); // 3616E99CD
        MediumMortonCodePtr mediumParent = mediumMorton.traverseUp();
        ASSERT_EQ( mediumParent->getBits(), 0x6C2DD339ul );
        
        vector< MediumMortonCodePtr > mediumChildren = mediumMorton.traverseDown();
        ASSERT_EQ( mediumChildren[ 0 ]->getBits(), 0x1B0B74CE68ul );
        ASSERT_EQ( mediumChildren[ 1 ]->getBits(), 0x1B0B74CE69ul );
        ASSERT_EQ( mediumChildren[ 2 ]->getBits(), 0x1B0B74CE6Aul );
        ASSERT_EQ( mediumChildren[ 3 ]->getBits(), 0x1B0B74CE6Bul );
        ASSERT_EQ( mediumChildren[ 4 ]->getBits(), 0x1B0B74CE6Cul );
        ASSERT_EQ( mediumChildren[ 5 ]->getBits(), 0x1B0B74CE6Dul );
        ASSERT_EQ( mediumChildren[ 6 ]->getBits(), 0x1B0B74CE6Eul );
        ASSERT_EQ( mediumChildren[ 7 ]->getBits(), 0x1B0B74CE6Ful );
        
        // Overflow check. Need to fix.
        //mediumMorton.build(5000, 6000, 7000, 21);
        // TODO: Need to fix the next line.
        //EXPECT_EXIT( mediumMorton.traverseDown(), ::testing::ExitedWithCode( 6 ),
        //			 "shifted > bits && ""MortonCode traversal overflow.""" );
    }
    
    TEST_F(MortonCodeTest, Comparison)
    {
        ShallowMortonCodePtr morton0 = makeManaged< ShallowMortonCode >();
        morton0->build( 1, 1, 1, 3 );
        
        ShallowMortonCodePtr morton1 = makeManaged< ShallowMortonCode >();
        morton1->build( 1, 1, 1, 2 );
        
        ShallowMortonCodePtr morton2 = makeManaged< ShallowMortonCode >();
        morton2->build( 1, 1, 1, 3 );
        
        ASSERT_TRUE(*morton0 != *morton1);
        ASSERT_TRUE(*morton0 == *morton2);
        
        ShallowMortonComparator comp;
        ASSERT_FALSE(comp(morton0, morton1)) << morton0 << " should be greater than " << morton1;
    }
    
    TEST_F( MortonCodeTest, DecodingShallow )
    {
        // Root node
        unsigned int level = 0;
        ShallowMortonCode shallowMorton;
        shallowMorton.build( 0x1u );
        vector< unsigned int > decoded = shallowMorton.decode( level );
        ASSERT_EQ( decoded[ 0 ], 0 );
        ASSERT_EQ( decoded[ 1 ], 0 );
        ASSERT_EQ( decoded[ 2 ], 0 );
        
        // Leaf (shallow).
        level = 10;
        unsigned int coords[3] = { 7, 5, 0 };
        shallowMorton.build( coords[0], coords[1], coords[2], level );
        decoded = shallowMorton.decode( level );
        
        ASSERT_EQ( decoded[ 0 ], coords[ 0 ] );
        ASSERT_EQ( decoded[ 1 ], coords[ 1 ] );
        ASSERT_EQ( decoded[ 2 ], coords[ 2 ] );
        
        decoded = shallowMorton.decode();
        
        ASSERT_EQ( decoded[ 0 ], coords[ 0 ] );
        ASSERT_EQ( decoded[ 1 ], coords[ 1 ] );
        ASSERT_EQ( decoded[ 2 ], coords[ 2 ] );
    }
    
    TEST_F( MortonCodeTest, DecodingMedium )
    {
        // Leaf (medium).
        unsigned int level = 21;
        unsigned int coordsL[ 3 ] = { 5000, 6000, 7000 };
        MediumMortonCode mediumMorton;
        mediumMorton.build( coordsL[0], coordsL[1], coordsL[2], level );
        vector< unsigned long > decodedL = mediumMorton.decode( level );
        
        ASSERT_EQ( decodedL[ 0 ], coordsL[ 0 ] );
        ASSERT_EQ( decodedL[ 1 ], coordsL[ 1 ] );
        ASSERT_EQ( decodedL[ 2 ], coordsL[ 2 ] );
        
        decodedL = mediumMorton.decode();
        
        ASSERT_EQ( decodedL[ 0 ], coordsL[ 0 ]);
        ASSERT_EQ( decodedL[ 1 ], coordsL[ 1 ]);
        ASSERT_EQ( decodedL[ 2 ], coordsL[ 2 ]);
    }
    
    TEST_F( MortonCodeTest, isChild )
    {
        ShallowMortonCode code0;
        code0.build( 0xF );
        
        ShallowMortonCode code1;
        code1.build( 0x1 );
        
        ASSERT_TRUE( code0.isChildOf( code1 ) );
        
        code1.build( 0x3 );
        
        ASSERT_FALSE( code0.isChildOf( code1 ) );
    }
    
    TEST_F( MortonCodeTest, isDescendantShallow )
    {
        ShallowMortonCode code0;
        code0.build( 0xF );
        
        ShallowMortonCode code1;
        code1.build( 0xFFF7 );
        
        ASSERT_TRUE( code1.isDescendantOf( code0 ) );
        
        code1.build( 0x8FF0 );
        
        ASSERT_FALSE( code1.isDescendantOf( code0 ) );
        
        code1.build( 0x1 );
        
        ASSERT_FALSE( code1.isDescendantOf( code0 ) );
        
        code1.build( 0xCF19ABC );
        
        ASSERT_FALSE( code1.isDescendantOf( code0 ) );
    }
    
    TEST_F( MortonCodeTest, isDescendantMedium )
    {
        MediumMortonCode code0;
        code0.build( 0xFFFFFFFFFFFFFul );
        
        MediumMortonCode code1;
        code1.build( 0xFFFFFFFFFFFFFE01ul );
        
        ASSERT_TRUE( code1.isDescendantOf( code0 ) );
        
        code1.build( 0xFFFFFFFFFFFFEE01ul );
        
        ASSERT_FALSE( code1.isDescendantOf( code0 ) );
        
        code1.build( 0xF012578ul );
        
        ASSERT_FALSE( code1.isDescendantOf( code0 ) );
        
        code1.build( 0xF012578FFFFFFFFFul );
        
        ASSERT_FALSE( code1.isDescendantOf( code0 ) );
    }
    
    TEST_F( MortonCodeTest, getFirstChild )
    {
        ShallowMortonCode code;
        code.build( 0x1 );
        ASSERT_EQ( code.getFirstChild()->getBits(), 0x8 );
    }
    
    TEST_F( MortonCodeTest, getLastChild )
    {
        ShallowMortonCode code;
        code.build( 0x1 );
        ASSERT_EQ( code.getLastChild()->getBits(), 0xF );
    }
    
    TEST_F( MortonCodeTest, getChildInterval )
    {
        ShallowMortonCode code;
        code.build( 0x1 );
        ShallowMortonInterval interval = code.getChildInterval();
        
        ASSERT_EQ( interval.first->getBits(), 0x8 );
        ASSERT_EQ( interval.second->getBits(), 0xF );
    }
    
    TEST_F( MortonCodeTest, getPrevious )
    {
        ShallowMortonCode code;
        code.build( 0x1 );
        ShallowMortonCode next = *code.getNext();
        
        ASSERT_EQ( *next.getPrevious(), code );
    }
    
    TEST_F( MortonCodeTest, getNext )
    {
        ShallowMortonCode code;
        code.build( 0x1 );
        ShallowMortonCode next = *code.getNext();
        
        ASSERT_EQ( next.getBits(), 0x2 );
    }
    
    TEST_F( MortonCodeTest, lessThan )
    {
        ShallowMortonCode code;
        code.build( 0x1 );
        ShallowMortonCode next = *code.getNext();
        
        ASSERT_TRUE( code < next );
        ASSERT_FALSE( code < code );
        ASSERT_FALSE( next < code );
    }
    
    TEST_F( MortonCodeTest, lessOrEqual )
    {
        ShallowMortonCode code;
        code.build( 0x1 );
        ShallowMortonCode next = *code.getNext();
        
        ASSERT_TRUE( code <= next );
        ASSERT_TRUE( code <= code );
        ASSERT_FALSE( next <= code );
    }
    
    TEST_F( MortonCodeTest, getLvlFirst )
    {
        ShallowMortonCode code = ShallowMortonCode::getLvlFirst( 7 );
        
        ASSERT_EQ( code.getBits(), 0x200000 );
    }
    
    TEST_F( MortonCodeTest, getLvlLastShallow )
    {
        ShallowMortonCode shallowCode = ShallowMortonCode::getLvlLast( 7 );
        ASSERT_EQ( shallowCode.getBits(), 0x3FFFFF );
    }
    
    TEST_F( MortonCodeTest, getLvlLastMedium )
    {
        MediumMortonCode mediumCode = MediumMortonCode::getLvlLast( 20 );
        ASSERT_EQ( mediumCode.getBits(), 0x1FFFFFFFFFFFFFFFul );
    }
}
