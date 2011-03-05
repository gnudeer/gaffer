//////////////////////////////////////////////////////////////////////////
//  
//  Copyright (c) 2011, John Haddon. All rights reserved.
//  Copyright (c) 2011, Image Engine Design Inc. All rights reserved.
//  
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//  
//      * Redistributions of source code must retain the above
//        copyright notice, this list of conditions and the following
//        disclaimer.
//  
//      * Redistributions in binary form must reproduce the above
//        copyright notice, this list of conditions and the following
//        disclaimer in the documentation and/or other materials provided with
//        the distribution.
//  
//      * Neither the name of John Haddon nor the names of
//        any other contributors to this software may be used to endorse or
//        promote products derived from this software without specific prior
//        written permission.
//  
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//  
//////////////////////////////////////////////////////////////////////////

#include "GafferUI/GraphGadget.h"
#include "GafferUI/NodeGadget.h"
#include "GafferUI/ButtonEvent.h"
#include "GafferUI/Nodule.h"
#include "GafferUI/ConnectionGadget.h"
#include "GafferUI/Style.h"

#include "Gaffer/ScriptNode.h"
#include "Gaffer/NumericPlug.h"
#include "Gaffer/Set.h"

#include "IECore/SimpleTypedData.h"

#include "OpenEXR/ImathRandom.h"
#include "OpenEXR/ImathPlane.h"

#include "boost/bind.hpp"
#include "boost/bind/placeholders.hpp"

using namespace GafferUI;
using namespace Imath;
using namespace IECore;
using namespace std;

IE_CORE_DEFINERUNTIMETYPED( GraphGadget );

GraphGadget::GraphGadget( Gaffer::NodePtr graphRoot )
	:
		m_graphRoot( graphRoot.get() ),
		m_dragStartPosition( 0 ),
		m_lastDragPosition( 0 ),
		m_dragSelecting( false )
{
	graphRoot->childAddedSignal().connect( boost::bind( &GraphGadget::childAdded, this, ::_1,  ::_2 ) );
	graphRoot->childRemovedSignal().connect( boost::bind( &GraphGadget::childRemoved, this, ::_1,  ::_2 ) );

	keyPressSignal().connect( boost::bind( &GraphGadget::keyPressed, this, ::_1,  ::_2 ) );
	buttonPressSignal().connect( boost::bind( &GraphGadget::buttonPress, this, ::_1,  ::_2 ) );
	buttonReleaseSignal().connect( boost::bind( &GraphGadget::buttonRelease, this, ::_1,  ::_2 ) );
	dragBeginSignal().connect( boost::bind( &GraphGadget::dragBegin, this, ::_1, ::_2 ) );
	dragUpdateSignal().connect( boost::bind( &GraphGadget::dragUpdate, this, ::_1, ::_2 ) );
	dragEndSignal().connect( boost::bind( &GraphGadget::dragEnd, this, ::_1, ::_2 ) );
	
	// make gadgets for each node
	for( Gaffer::ChildNodeIterator cIt = graphRoot->childrenBegin<Gaffer::Node>(); cIt!=cIt.end(); cIt++ )
	{
		addNodeGadget( (*cIt).get() );
	}
	// and then make gadgets for each connection
	for( Gaffer::ChildNodeIterator cIt = graphRoot->childrenBegin<Gaffer::Node>(); cIt!=cIt.end(); cIt++ )
	{
		for( Gaffer::InputPlugIterator pIt( (*cIt)->children().begin(), (*cIt)->children().end() ); pIt!=pIt.end(); pIt++ )
		{
			addConnectionGadget( pIt->get() );
		}
	}
}

GraphGadget::~GraphGadget()
{
}

NodeGadgetPtr GraphGadget::nodeGadget( Gaffer::ConstNodePtr node )
{
	return findNodeGadget( node.get() );
}

ConstNodeGadgetPtr GraphGadget::nodeGadget( Gaffer::ConstNodePtr node ) const
{
	return findNodeGadget( node.get() );
}

void GraphGadget::doRender( IECore::RendererPtr renderer ) const
{
	renderer->attributeBegin();
	
		renderer->setAttribute( "gl:depthTest", new IECore::BoolData( false ) );

		// render connection first so they go underneath
		for( ChildContainer::const_iterator it=children().begin(); it!=children().end(); it++ )
		{
			if( ConnectionGadgetPtr c = IECore::runTimeCast<ConnectionGadget>( *it ) )
			{
				c->render( renderer );
			}
		}

		// then render the rest on top
		for( ChildContainer::const_iterator it=children().begin(); it!=children().end(); it++ )
		{
			if( !((*it)->isInstanceOf( ConnectionGadget::staticTypeId() )) )
			{
				IECore::staticPointerCast<const Gadget>( *it )->render( renderer );
			}
		}
		
		// render drag select thing if needed
		if( m_dragSelecting )
		{
			Box2f b;
			b.extendBy( m_dragStartPosition );
			b.extendBy( m_lastDragPosition );
			/// \figure out how we can modify the Style classes to allow this to be a nice blue colour
			renderer->setAttribute( "opacity", new IECore::Color3fData( Color3f( 0.2f ) ) );
			getStyle()->renderFrame( renderer, b, 0.5f );		
		}
	
	renderer->attributeEnd();
}

bool GraphGadget::keyPressed( GadgetPtr gadget, const KeyEvent &event )
{
	return false;
}

void GraphGadget::childAdded( GraphComponent *parent, GraphComponent *child )
{
	Gaffer::Node *node = IECore::runTimeCast<Gaffer::Node>( child );
	if( node )
	{
		addNodeGadget( node );

		// make gadgets for each connection
		for( Gaffer::InputPlugIterator pIt( node->children().begin(), node->children().end() ); pIt!=pIt.end(); pIt++ )
		{
			addConnectionGadget( pIt->get() );
		}
	}
}

void GraphGadget::childRemoved( GraphComponent *parent, GraphComponent *child )
{
	Gaffer::Node *node = IECore::runTimeCast<Gaffer::Node>( child );
	if( node )
	{
		NodeGadget *g = findNodeGadget( node );
		if( g )
		{
			removeChild( g );
		}
	}
}

void GraphGadget::inputChanged( Gaffer::PlugPtr dstPlug )
{
	ConnectionGadget *oldConnection = connectionGadget( dstPlug.get() );
	if( oldConnection )
	{
		m_connectionGadgets.erase( dstPlug.get() );
		removeChild( oldConnection );
	}

	Gaffer::PlugPtr srcPlug = dstPlug->getInput<Gaffer::Plug>();
	if( !srcPlug )
	{
		// it's a disconnection, no need to make a new gadget.
		return;
	}
	
	addConnectionGadget( dstPlug.get() );
}

void GraphGadget::plugSet( Gaffer::PlugPtr plug )
{
	const std::string &name = plug->getName();
	if( name=="__uiX" || name=="__uiY" )
	{
		Gaffer::NodePtr node = plug->node();
		NodeGadget *ng = findNodeGadget( node.get() );
		if( ng )
		{
			updateNodeGadgetTransform( ng );
		}
	}
}

bool GraphGadget::buttonRelease( GadgetPtr gadget, const ButtonEvent &event )
{
	return true;
}

bool GraphGadget::buttonPress( GadgetPtr gadget, const ButtonEvent &event )
{
	if( gadget->isInstanceOf( Nodule::staticTypeId() ) || gadget->isInstanceOf( ConnectionGadget::staticTypeId() ) )
	{
		// they can handle their own stuff.
		return false;
	}

	if( event.buttons==ButtonEvent::Left )
	{
		// selection/deselection
		
		Gaffer::ScriptNodePtr scriptNode = script();
		if( !scriptNode )
		{
			return false;
		}
				
		if( gadget==this )
		{
			// background click - clear the current selection
			scriptNode->selection()->clear();
			return true;
		}
		else
		{
			NodeGadgetPtr nodeGadget = runTimeCast<NodeGadget>( gadget );
			if( !nodeGadget )
			{
				nodeGadget = gadget->ancestor<NodeGadget>();
			}
			if( nodeGadget )
			{				
				Gaffer::NodePtr node = nodeGadget->node();
				bool shiftHeld = event.modifiers && ButtonEvent::Shift;
				bool nodeSelected = scriptNode->selection()->contains( node );

				if( nodeSelected )
				{
					if( shiftHeld )
					{
						scriptNode->selection()->remove( node );
					}
				}
				else
				{
					if( !shiftHeld )
					{
						scriptNode->selection()->clear();
					}
					scriptNode->selection()->add( node );			
				}

				return true;
			}
		}
	}
	return false;
}

IECore::RunTimeTypedPtr GraphGadget::dragBegin( GadgetPtr gadget, const DragDropEvent &event )
{
	if( gadget->isInstanceOf( Nodule::staticTypeId() ) )
	{
		// nodules can handle their own drag and drop so we don't handle the event
		return 0;
	}
	
	Gaffer::ScriptNodePtr scriptNode = script();
	if( !scriptNode )
	{
		return 0;
	}
	
	V3f i;
	if( !event.line.intersect( Plane3f( V3f( 0, 0, 1 ), 0 ), i ) )
	{
		return 0;
	}
	
	m_dragStartPosition = m_lastDragPosition = V2f( i.x, i.y );
	if( scriptNode->selection()->size() )
	{
		return scriptNode->selection();
	}
	else
	{
		return new IECore::V2fData( m_dragStartPosition );
	}
	return 0;
}

bool GraphGadget::dragUpdate( GadgetPtr gadget, const DragDropEvent &event )
{
	Gaffer::ScriptNodePtr scriptNode = script();
	if( !scriptNode )
	{
		return false;
	}
	
	V3f i;
	if( !event.line.intersect( Plane3f( V3f( 0, 0, 1 ), 0 ), i ) )
	{
		return false;
	}
	
	if( event.data->isInstanceOf( Gaffer::Set::staticTypeId() ) )
	{
		// we're dragging some nodes around
		V2f pos = V2f( i.x, i.y );
		V2f delta = pos - m_lastDragPosition;
		for( ChildContainer::const_iterator it=children().begin(); it!=children().end(); it++ )
		{
			NodeGadgetPtr nodeGadget = runTimeCast<NodeGadget>( *it );
			if( nodeGadget )
			{
				Gaffer::NodePtr node = nodeGadget->node();
				if( scriptNode->selection()->contains( node ) )
				{
					Gaffer::FloatPlugPtr xp = node->getChild<Gaffer::FloatPlug>( "__uiX" );
					Gaffer::FloatPlugPtr yp = node->getChild<Gaffer::FloatPlug>( "__uiY" );
					xp->setValue( xp->getValue() + delta.x );
					yp->setValue( yp->getValue() + delta.y );
				}
			}
		}
		m_lastDragPosition = pos;
		return true;
	}
	else
	{
		// we're drag selecting
		m_dragSelecting	= true;
		m_lastDragPosition = V2f( i.x, i.y );
		renderRequestSignal()( this );
		return true;
	}
		
	assert( 0 ); // shouldn't get here
	return false;
}

bool GraphGadget::dragEnd( GadgetPtr gadget, const DragDropEvent &event )
{
	m_dragSelecting = false;
	
	Gaffer::ScriptNodePtr scriptNode = script();
	if( !scriptNode )
	{
		return false;
	}
	
	V3f i;
	if( !event.line.intersect( Plane3f( V3f( 0, 0, 1 ), 0 ), i ) )
	{
		return false;
	}

	if( event.data->isInstanceOf( V2fData::staticTypeId() ) )
	{
		Box2f selectionBound;
		selectionBound.extendBy( m_dragStartPosition );
		selectionBound.extendBy( m_lastDragPosition );
	
		for( ChildContainer::const_iterator it=children().begin(); it!=children().end(); it++ )
		{
			NodeGadgetPtr nodeGadget = runTimeCast<NodeGadget>( *it );
			if( nodeGadget )
			{
				Box3f nodeBound3 = nodeGadget->transformedBound();
				Box2f nodeBound2( V2f( nodeBound3.min.x, nodeBound3.min.y ), V2f( nodeBound3.max.x, nodeBound3.max.y ) );
				if( selectionBound.intersects( nodeBound2 ) )
				{
					scriptNode->selection()->add( nodeGadget->node() );
				}
			}
		}
	
		renderRequestSignal()( this );
	}

	return true;
}

Gaffer::ScriptNodePtr GraphGadget::script()
{
	Gaffer::ScriptNodePtr script = runTimeCast<Gaffer::ScriptNode>( m_graphRoot );
	if( !script )
	{
		script = m_graphRoot->scriptNode();
	}
	return script;
}

void GraphGadget::addNodeGadget( Gaffer::Node *node )
{	
	node->plugInputChangedSignal().connect( boost::bind( &GraphGadget::inputChanged, this, ::_1 ) );
	node->plugSetSignal().connect( boost::bind( &GraphGadget::plugSet, this, ::_1 ) );
	
	NodeGadgetPtr nodeGadget = NodeGadget::create( node );
	
	addChild( nodeGadget );
	m_nodeGadgets[node] = nodeGadget.get();
	
	// place it if it's not placed already.
	/// \todo we need to do this intelligently rather than randomly!!
	/// this probably means knowing what part of the graph is being viewed at the time. i think we
	/// can do this by having the panning and zooming handled by a parent ViewportGadget rather than
	/// letting the GadgetWidget do it. or do we need to query mouse position??
	
	/// \todo Use a V2f plug when we get one
	static Imath::Rand32 r;
	Gaffer::FloatPlugPtr xPlug = node->getChild<Gaffer::FloatPlug>( "__uiX" );
	if( !xPlug )
	{
		xPlug = new Gaffer::FloatPlug( "__uiX", Gaffer::Plug::In );
		xPlug->setFlags( Gaffer::Plug::Dynamic, true );
		xPlug->setValue( r.nextf( -10, 10 ) );
		node->addChild( xPlug );
	}
	
	Gaffer::FloatPlugPtr yPlug = node->getChild<Gaffer::FloatPlug>( "__uiY" );
	if( !yPlug )
	{	
		Gaffer::FloatPlugPtr yPlug = new Gaffer::FloatPlug( "__uiY", Gaffer::Plug::In );
		yPlug->setFlags( Gaffer::Plug::Dynamic, true );
		yPlug->setValue( r.nextf( -10, 10 ) );
		node->addChild( yPlug );
	}
	
	updateNodeGadgetTransform( nodeGadget.get() );
}

NodeGadget *GraphGadget::findNodeGadget( const Gaffer::Node *node ) const
{
	NodeGadgetMap::const_iterator it = m_nodeGadgets.find( node );
	if( it==m_nodeGadgets.end() )
	{
		return 0;
	}
	return it->second;
}

void GraphGadget::updateNodeGadgetTransform( NodeGadget *nodeGadget )
{
	Gaffer::NodePtr node = nodeGadget->node();
	V3f t( 0 );

	Gaffer::FloatPlugPtr x = node->getChild<Gaffer::FloatPlug>( "__uiX" );
	if( x )
	{
		t[0] = x->getValue();
	}

	Gaffer::FloatPlugPtr y = node->getChild<Gaffer::FloatPlug>( "__uiY" );
	if( y )
	{
		t[1] = y->getValue();
	}

	M44f m; m.translate( t );
	nodeGadget->setTransform( m );
}

void GraphGadget::addConnectionGadget( Gaffer::Plug *dstPlug )
{
	Gaffer::NodePtr dstNode = dstPlug->node();
	Gaffer::PlugPtr srcPlug = dstPlug->getInput<Gaffer::Plug>();
	if( !srcPlug )
	{
		return;
	}
	
	Gaffer::NodePtr srcNode = srcPlug->node();
	
	NodulePtr srcNodule = findNodeGadget( srcNode.get() )->nodule( srcPlug );
	NodulePtr dstNodule = findNodeGadget( dstNode.get() )->nodule( dstPlug );
	
	if( !(srcNodule && dstNodule ) )
	{
		return;
	}
		
	ConnectionGadgetPtr connection = new ConnectionGadget( srcNodule, dstNodule );
	addChild( connection );

	m_connectionGadgets[dstPlug] = connection.get();
}

ConnectionGadget *GraphGadget::connectionGadget( Gaffer::Plug *plug )
{
	ConnectionGadgetMap::iterator it = m_connectionGadgets.find( plug );
	if( it==m_connectionGadgets.end() )
	{
		return 0;
	}
	return it->second;
}
