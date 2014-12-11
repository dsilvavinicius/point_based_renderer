#ifndef DUMMY_QGLVIEW_H
#define DUMMY_QGLVIEW_H

#include <vector>
#include <memory>
#include <QGLView>

using namespace std;

namespace model
{
	namespace test
	{
		class ScanQGLView
		: public QGLView
		{
		public:
			ScanQGLView( const QSurfaceFormat &format, QWindow *parent = 0 );
			
			vector< unsigned int > m_scanResults;
		
		protected:
			void initializeGL( QGLPainter * painter );
			void paintGL( QGLPainter * painter );
		};
	}
}

#endif