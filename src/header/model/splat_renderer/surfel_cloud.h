#ifndef SURFEL_CLOUD_H
#define SURFEL_CLOUD_H

#include "splat_renderer/surfel.hpp"
#include "Array.h"
#include "GpuAllocStatistics.h"
#include "OglUtils.h"

using namespace model;

class SurfelCloud
{
public:
	SurfelCloud();
	SurfelCloud( const model::Array< Surfel >& surfels, const Eigen::Matrix4f& model = Eigen::Matrix4f::Identity() );
	SurfelCloud( const SurfelCloud& other ) = delete;
	SurfelCloud( SurfelCloud&& other );
	~SurfelCloud();
	
	SurfelCloud& operator=( const SurfelCloud& other ) = delete;
	SurfelCloud& operator=( SurfelCloud&& other );
	
	void render() const;
	uint numPoints() const { return m_numPts; }
	const Matrix4f& model() const { return m_model; }
	
	friend ostream& operator<<( ostream& out, const SurfelCloud& cloud );
	
private:
	void clean();
	void shallowClean();
	
	GLuint m_vbo, m_vao;
    uint m_numPts;
	Matrix4f m_model;
};

inline SurfelCloud::SurfelCloud()
: m_vbo( 0 ),
m_vao( 0 ),
m_numPts( 0 ),
m_model( Eigen::Matrix4f::Identity() )
{
	#ifndef NDEBUG
		cout << "Cloud default ctor. " << endl << *this << endl << endl;
	#endif
}

inline SurfelCloud::SurfelCloud( const model::Array< Surfel >& surfels, const Matrix4f& model )
{
	if( !surfels.empty() )
	{
		#ifndef NDEBUG
		cout << "Surfels: " << endl;
		for( int i = 0; i < 10; ++i )
		{
			cout << surfels[ i ] << endl;
		}
		cout << endl;
		#endif
		
		m_model = model;
		m_numPts = static_cast< uint >( surfels.size() );
		
		GpuAllocStatistics::notifyAlloc( m_numPts * GpuAllocStatistics::pointSize() );
		
		glGenBuffers(1, &m_vbo);

		glGenVertexArrays(1, &m_vao);
		glBindVertexArray(m_vao);

		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

		// Center c.
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
			sizeof(Surfel), reinterpret_cast<const GLfloat*>(0));

		// Tagent vector u.
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
			sizeof(Surfel), reinterpret_cast<const GLfloat*>(12));

		// Tangent vector v.
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE,
			sizeof(Surfel), reinterpret_cast<const GLfloat*>(24));
		
		glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_STATIC_DRAW);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Surfel) * m_numPts, surfels.data(), GL_STATIC_DRAW);
		
		glBindVertexArray(0);
		
		#ifndef NDEBUG
			util::OglUtils::checkOglErrors();
		#endif
	}
	else
	{
		m_vbo = 0;
		m_vao = 0;
		m_numPts = 0;
		m_model = Eigen::Matrix4f::Identity();
	}
	
	#ifndef NDEBUG
		cout << "Cloud ctor. " << endl << *this << endl << endl;
	#endif
}

inline SurfelCloud::SurfelCloud( SurfelCloud&& other )
: m_vbo( other.m_vbo ),
m_vao( other.m_vao ),
m_numPts( other.m_numPts ),
m_model( other.m_model )
{
	#ifndef NDEBUG
		cout << "Cloud move ctor. " << endl << *this << endl
			 << "Moved: " << other << endl << endl;
	#endif
	
	other.shallowClean();
}

inline SurfelCloud& SurfelCloud::operator=( SurfelCloud&& other )
{
	clean();
	
	m_vbo = other.m_vbo;
	m_vao = other.m_vao;
	m_numPts = other.m_numPts;
	m_model = other.m_model;
	
	#ifndef NDEBUG
		cout << "Cloud move assign. " << endl << *this << endl
			 << "Moved: " << other << endl << endl;
	#endif
	
	other.shallowClean();
	
	return *this;
}

inline SurfelCloud::~SurfelCloud()
{
	#ifndef NDEBUG
		cout << "Cloud dtor. " << endl << *this << endl << endl;
	#endif
	
	clean();
}

inline void SurfelCloud::render() const
{
	glBindVertexArray( m_vao );
	
	#ifndef NDEBUG
		util::OglUtils::checkOglErrors();
	#endif
	
// 	glDrawArrays( GL_POINTS, 0, m_numPts );
	glDrawArrays( GL_POINTS, 0, 1000 );
	
	#ifndef NDEBUG
		cout << "Rendering cloud" << endl << *this << endl << endl;
		util::OglUtils::checkOglErrors();
	#endif
		
	glBindVertexArray( 0 );
	
	#ifndef NDEBUG
		util::OglUtils::checkOglErrors();
	#endif
}

inline void SurfelCloud::clean()
{
	if( m_vao )
	{
		#ifndef NDEBUG
			cout << "Buffers deleted in " << *this << endl << endl;
		#endif
		
		glDeleteVertexArrays( 1, &m_vao );
		glDeleteBuffers( 1, &m_vbo );
		
		GpuAllocStatistics::notifyDealloc( m_numPts * GpuAllocStatistics::pointSize() );
	}
	
	#ifndef NDEBUG
		cout << "Cloud clean. " << endl << *this << endl << endl;
	#endif
}

inline void SurfelCloud::shallowClean()
{
	#ifndef NDEBUG
		cout << "Cloud shallow clean. " << endl << *this << endl << endl;
	#endif
	
	m_vbo = 0;
	m_vao = 0;
	m_numPts = 0;
	m_model = Matrix4f::Identity();
}

inline ostream& operator<<( ostream& out, const SurfelCloud& cloud )
{
	out << "Address: " << &cloud << ". vao: " << cloud.m_vao << " vbo: " << cloud.m_vbo << " nPoints: "
		<< cloud.m_numPts << " model matrix: " << endl << cloud.m_model;
	return out;
}

#endif