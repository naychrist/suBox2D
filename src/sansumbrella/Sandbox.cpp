/*
 * Copyright (c) 2010—2013, David Wicks
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Sandbox.h"

using namespace cinder;
using namespace sansumbrella;

void Sandbox::step()
{
	mWorld.Step(mTimeStep, mVelocityIterations, mPositionIterations);
}

void Sandbox::setGravity( Vec2f gravity )
{
	mWorld.SetGravity( b2Vec2{ toPhysics( gravity.x ), toPhysics( gravity.y ) } );
}

void Sandbox::setPointsPerMeter(float points)
{
  mPointsPerMeter = points;
  mMetersPerPoint = 1.0f / mPointsPerMeter;
}

void Sandbox::clear()
{
	//get rid of everything
	b2Body* node = mWorld.GetBodyList();
	while (node)
	{
		b2Body* b = node;
		node = node->GetNext();
		destroyBody(b);
	}

	b2Joint* joint = mWorld.GetJointList();
	while (joint) {
		b2Joint* j = joint;
		joint = joint->GetNext();
		mWorld.DestroyJoint(j);
	}
}

void Sandbox::setContactFilter( b2ContactFilter filter )
{
	mContactFilter = filter;
	mWorld.SetContactFilter(&mContactFilter);
}

void Sandbox::connectUserSignals(ci::app::WindowRef window)
{
  window->getSignalMouseDown().connect( [this]( app::MouseEvent &event ){ mouseDown( event ); } );
  window->getSignalMouseUp().connect( [this]( app::MouseEvent &event ){ mouseUp( event ); } );
  window->getSignalMouseDrag().connect( [this]( app::MouseEvent &event ){ mouseDrag( event ); } );
}

void Sandbox::debugDraw( bool drawBodies, bool drawContacts )
{
	// should utilize an extension of b2DebugDraw (will soon)
	//
	if( drawBodies )
	{
		//draw all bodies, contact points, etc

		gl::color( ColorA(1.0f, 0.0f, 0.1f, 0.5f) );

		//draw bodies
		b2Body* bodies = mWorld.GetBodyList();
		while( bodies != NULL )
		{
      Vec2f pos{ bodies->GetPosition().x, bodies->GetPosition().y };
			float32 angle = bodies->GetAngle();

			gl::pushMatrices();

			gl::translate( toPoints(pos).x, toPoints(pos).y );
			gl::rotate( angle * 180 / M_PI );

			//draw the fixtures for this body
			b2Fixture* fixtures = bodies->GetFixtureList();
			while( fixtures != NULL )
			{
				//not sure why the base b2Shape doesn't contain the vertex methods...
				switch (fixtures->GetType()) {
					case b2Shape::e_polygon:
          {
            b2PolygonShape* shape = (b2PolygonShape*)fixtures->GetShape();

            glBegin(GL_POLYGON);

            for( int i=0; i != shape->GetVertexCount(); ++i )
            {
              gl::vertex( toPoints( shape->GetVertex(i).x ), toPoints( shape->GetVertex(i).y ) );
            }

            glEnd();
          }
						break;
					case b2Shape::e_circle:
          {
            b2CircleShape* shape = (b2CircleShape*)fixtures->GetShape();
            Vec2f pos( shape->m_p.x, shape->m_p.y );
            gl::drawSolidCircle( toPoints( pos ), toPoints( shape->m_radius ) );
          }
						break;

					default:
						break;
				}


				fixtures = fixtures->GetNext();
			}

			gl::popMatrices();

			bodies = bodies->GetNext();
		}
	}

	if( drawContacts )
	{
		//draw contacts
		b2Contact* contacts = mWorld.GetContactList();

		gl::color( ColorA( 0.0f, 0.0f, 1.0f, 0.8f ) );
		glPointSize(3.0f);
		glBegin(GL_POINTS);

		while( contacts != NULL )
		{
			b2WorldManifold m;
			contacts->GetWorldManifold(&m);	//grab the

			for( int i=0; i != b2_maxManifoldPoints; ++i )
			{
				Vec2f p{ toPoints( Vec2f{ m.points[i].x, m.points[i].y } ) };
				gl::vertex( p );
			}

			contacts = contacts->GetNext();
		}
		glEnd();
	}
}

b2Body* Sandbox::createBody(const b2BodyDef &body_def, const b2FixtureDef &fixture_def)
{
  b2Body *body = mWorld.CreateBody( &body_def );
  body->CreateFixture( &fixture_def );
  return body;
}

b2Body* Sandbox::createBody(const b2BodyDef &body_def, const std::vector<b2FixtureDef> &fixture_defs)
{
  b2Body *body = mWorld.CreateBody( &body_def );
  for( auto &def : fixture_defs )
  {
    body->CreateFixture( &def );
  }
  return body;
}

b2Body* Sandbox::createBox(ci::Vec2f pos, ci::Vec2f size)
{
  b2BodyDef bodyDef;
	bodyDef.position.Set(	toPhysics(pos.x),
                       toPhysics( pos.y ) );
	bodyDef.type = b2_dynamicBody;

	b2PolygonShape box;
	box.SetAsBox(	toPhysics(size.x),
               toPhysics( size.y ) );

	b2FixtureDef bodyFixtureDef;
	bodyFixtureDef.shape = &box;
	bodyFixtureDef.density = 1.0f;
	bodyFixtureDef.friction = 0.3f;

  return createBody( bodyDef, bodyFixtureDef );
}

b2Body* Sandbox::createBoundaryRect(ci::Rectf screen_bounds, float thickness)
{
  // half width and half height
  const float w = toPhysics( screen_bounds.getWidth() / 2.0f ) + thickness;
  const float h = toPhysics( screen_bounds.getHeight() / 2.0f ) + thickness;
  // center x, y
  const Vec2f upperLeft = toPhysics( screen_bounds.getUpperLeft() );
  const float x = upperLeft.x + w - thickness;
  const float y = upperLeft.y + h - thickness;

  b2BodyDef bodyDef;
  bodyDef.position.Set( x, y );
  bodyDef.type = b2_staticBody;

  // Left
  b2FixtureDef left;
  b2PolygonShape leftShape;
  leftShape.SetAsBox( thickness, h, b2Vec2( -w, 0 ), 0 );
  left.shape = &leftShape;

  // Right
  b2FixtureDef right;
  b2PolygonShape rightShape;
  rightShape.SetAsBox( thickness, h, b2Vec2( w, 0 ), 0 );
  right.shape = &rightShape;

  // Top
  b2FixtureDef top;
  b2PolygonShape topShape;
  topShape.SetAsBox( w, thickness, b2Vec2( 0, -h ), 0 );
  top.shape = &topShape;

  // Bottom
  b2FixtureDef bottom;
  b2PolygonShape bottomShape;
  bottomShape.SetAsBox( w, thickness, b2Vec2( 0, h ), 0 );
  bottom.shape = &bottomShape;

  if( mBoundaryBody )
  {
    destroyBody( mBoundaryBody );
  }
  mBoundaryBody = createBody( bodyDef, { left, right, top, bottom } );
  return mBoundaryBody;
}

void Sandbox::init( bool useScreenBounds )
{
	if( useScreenBounds ){
		createBoundaryRect( app::getWindowBounds() );
	}
}


//
//	Mouse interaction
//

// QueryCallback taken from box2d testbed
class QueryCallback : public b2QueryCallback
{
public:
	QueryCallback(const b2Vec2& point)
	{
		m_point = point;
		m_fixture = NULL;
	}

	bool ReportFixture(b2Fixture* fixture)
	{
		b2Body* body = fixture->GetBody();
		if (body->GetType() == b2_dynamicBody)
		{
			bool inside = fixture->TestPoint(m_point);
			if (inside)
			{
				m_fixture = fixture;

				// We are done, terminate the query.
				return false;
			}
		}

		// Continue the query.
		return true;
	}

	b2Vec2 m_point;
	b2Fixture* m_fixture;
};

bool Sandbox::mouseDown( app::MouseEvent &event )
{
	if (mMouseJoint)
	{
		return false;
	}
	b2Vec2 p{ toPhysics( event.getPos().x ), toPhysics( event.getPos().y ) };
	// Make a small box.
	b2AABB aabb;
	b2Vec2 d;
	d.Set(0.001f, 0.001f);
	aabb.lowerBound = p - d;
	aabb.upperBound = p + d;

	// Query the world for overlapping shapes.
	QueryCallback callback(p);
	mWorld.QueryAABB(&callback, aabb);

	if (callback.m_fixture)
	{
		b2Body* body = callback.m_fixture->GetBody();
		b2MouseJointDef md;
		md.bodyA = body;
		md.bodyB = mBoundaryBody;
		md.target = p;
		md.maxForce = 1000.0f * body->GetMass();
		mMouseJoint = (b2MouseJoint*)mWorld.CreateJoint(&md);
		body->SetAwake(true);
	}

	return false;
}

bool Sandbox::mouseUp( app::MouseEvent &event )
{
	if (mMouseJoint)
	{
		mWorld.DestroyJoint(mMouseJoint);
		mMouseJoint = nullptr;
	}
	return false;
}

bool Sandbox::mouseDrag( app::MouseEvent &event )
{
	if(mMouseJoint){
		mMouseJoint->SetTarget( b2Vec2{ toPhysics(event.getPos().x), toPhysics(event.getPos().y) } );
	}
	return false;
}