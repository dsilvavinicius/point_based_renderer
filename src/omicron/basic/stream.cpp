#include "omicron/basic/stream.h"

namespace omicron::basic
{
// 	template<>
// 	ostream& operator<< < vec3 >( ostream& out, const vector< vec3 >& v ) 
// 	{
// 		out << "{";
// 		bool first = true;
// 		for( vec3 element : v )
// 		{
// 			if( first )
// 			{
// 				out << element << endl;
// 				first = false;
// 			}
// 			else
// 			{
// 				out << ", " << element << endl;
// 			}
// 		}
// 		out << "}";
// 		
// 		return out;
// 	}
// 	
// 	template<>
// 	ostream& operator<< < vec4 >( ostream& out, const vector< vec4 >& v ) 
// 	{
// 		out << "{";
// 		bool first = true;
// 		for( vec4 element : v )
// 		{
// 			if( first )
// 			{
// 				out << element << endl;
// 				first = false;
// 			}
// 			else
// 			{
// 				out << ", " << element << endl;
// 			}
// 		}
// 		out << "}";
// 		
// 		return out;
// 	}
	
	template<>
	ostream& operator<<( ostream& out, const vector< PointPtr >& v )
	{
		out << "size: " << v.size() << endl << "{" << endl;
		bool first = true;
		for( PointPtr element : v )
		{
			if( first )
			{
				out << *element;
				first = false;
			}
			else
			{
				out << ", " << endl << *element;
			}
		}
		out << "}";
		
		return out;
	}
	
	ostream& operator<<( ostream& out, const QPoint& point )
	{
		out << "( " << point.x() << ", " << point.y()  <<  " )";
		return out;
	}
	
	ostream& operator<<( ostream& out, const QSize& size )
	{
		out << "( " << size.width() << ", " << size.height()  <<  " )";
		return out;
	}
	
	ostream& operator<<( ostream& out, const QRect& rect )
	{
		out << "top left:" << rect.topLeft() << endl << "bottom right:" << rect.bottomRight()
			<< endl << "size:" << rect.size() << endl;
		return out;
	}
}
