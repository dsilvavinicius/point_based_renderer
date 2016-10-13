#include <gtest/gtest.h>
#include <utils/qtflycamerawidget.hpp>
#include <QApplication>
#include "PlyPointReader.h"
#include "splat_renderer/splat_renderer.hpp"

#include "OglUtils.h"

using namespace std;
using namespace util;
using namespace Tucano;

namespace model
{
	namespace test
	{
		class SplatRendererTestWidget
		: public QtFlycameraWidget
		{
		public:
			using SurfelVector = vector< Surfel >;
			
			SplatRendererTestWidget( QWidget *parent = 0 )
			: QtFlycameraWidget( parent )
			{}
			
			~SplatRendererTestWidget()
			{
				delete m_renderer;
			}
			
			void initialize()
			{
				QtFlycameraWidget::initialize();
				
				PlyPointReader reader( "../data/example/staypuff.ply" );
				m_surfels.resize( reader.getNumPoints() );
				
				Float negInf = -numeric_limits< Float >::max();
				Float posInf = numeric_limits< Float >::max();
				Vec3 origin = Vec3( posInf, posInf, posInf );
				Vec3 maxCoords( negInf, negInf, negInf );
				
				auto iter = m_surfels.begin();
				reader.read(
					[ & ]( const Point& p )
					{
						const Vec3& pos = p.getPos();
						const Vector3f& normal = p.getNormal();
						Vector3f pointOnPlane(
							( normal.x() * pos.x() + normal.y() * pos.y() + normal.z() * pos.z() ) / normal.x(),
							0.f, 0.f );
						Vector3f u = pointOnPlane - pos;
						u.normalize();
						Vector3f v = normal.cross( u );
			
						u *= 0.003f;
						v *= 0.003f;
						
						iter->c = pos;
						iter->u = u;
						iter->v = v;
						iter++;
						
						for( int i = 0; i < 3; ++i )
						{
							origin[ i ] = std::min( origin[ i ], pos[ i ] );
							maxCoords[ i ] = std::max( maxCoords[ i ], pos[ i ] );
						}
					}
				);
				
				Vec3 boxSize = maxCoords - origin;
				float scale = 1.f / std::max( std::max( boxSize.x(), boxSize.y() ), boxSize.z() );
				
				for( Surfel& surfel : m_surfels )
				{
					surfel.c = ( surfel.c - origin ) * scale;
				}
				
				m_renderer = new SplatRenderer( *camera );
				m_renderer->load_to_gpu( m_surfels );
				OglUtils::checkOglErrors();
			}
			
			void resizeGL (int w, int h) override
			{
				QtFlycameraWidget::resizeGL( w, h );
				m_renderer->reshape( w, h );
			}
			
			void paintGL() override
			{
				m_renderer->render_frame();
				OglUtils::checkOglErrors();
			}
			
		private:
			SplatRenderer* m_renderer;
			SurfelVector m_surfels;
		};
		
        class SplatRendererTest : public ::testing::Test
		{
		protected:
			void SetUp() {}
		};
		
		TEST_F( SplatRendererTest, All )
		{
			{
				SplatRendererTestWidget widget;
				widget.initialize();
				widget.show();
				widget.resize( 640, 480 );

				QApplication::exec();
			}
			ASSERT_EQ( 0, AllocStatistics::totalAllocated() );
		}
	}
}