##########################################################################
#
#  Copyright (c) 2013, Image Engine Design Inc. All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are
#  met:
#
#      * Redistributions of source code must retain the above
#        copyright notice, this list of conditions and the following
#        disclaimer.
#
#      * Redistributions in binary form must reproduce the above
#        copyright notice, this list of conditions and the following
#        disclaimer in the documentation and/or other materials provided with
#        the distribution.
#
#      * Neither the name of John Haddon nor the names of
#        any other contributors to this software may be used to endorse or
#        promote products derived from this software without specific prior
#        written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
#  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
#  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
#  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
#  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
##########################################################################

import os
import unittest

import Gaffer
import GafferTest
import GafferScene
import GafferSceneTest

class ScenePathTest( GafferSceneTest.SceneTestCase ) :

	def test( self ) :

		a = GafferScene.SceneReader()
		a["fileName"].setValue( os.path.dirname( __file__ ) + "/alembicFiles/cube.abc" )

		p = GafferScene.ScenePath( a["out"], Gaffer.Context(), "/" )
		c = p.children()

		self.assertEqual( len( c ), 1 )
		self.assertEqual( str( c[0] ), "/group1" )

	def testRelative( self ) :

		a = GafferScene.SceneReader()
		a["fileName"].setValue( os.path.dirname( __file__ ) + "/alembicFiles/cube.abc" )

		p = GafferScene.ScenePath( a["out"], Gaffer.Context(), "group1" )
		self.assertEqual( str( p ), "group1" )
		self.assertEqual( p.root(), "" )
		self.assertEqual( [ str( c ) for c in p.children() ], [ "group1/pCube1" ] )

		p2 = p.copy()
		self.assertEqual( str( p2 ), "group1" )
		self.assertEqual( p2.root(), "" )
		self.assertEqual( [ str( c ) for c in p2.children() ], [ "group1/pCube1" ] )

	def testIsValid( self ) :

		plane = GafferScene.Plane()
		group = GafferScene.Group()
		group["in"][0].setInput( plane["out"] )

		p = GafferScene.ScenePath( group["out"], Gaffer.Context(), "/" )
		self.assertTrue( p.isValid() )

		p.setFromString( "/group" )
		self.assertTrue( p.isValid() )

		p.setFromString( "/group/plane" )
		self.assertTrue( p.isValid() )

		p.setFromString( "/group/plane2" )
		self.assertFalse( p.isValid() )

		p.setFromString( "/group2/plane" )
		self.assertFalse( p.isValid() )

		p.setFromString( "" )
		self.assertFalse( p.isValid() )

	def testContextSignals( self ) :

		plane = GafferScene.Plane()

		context = Gaffer.Context()
		self.assertEqual( context.changedSignal().num_slots(), 0 )

		p = GafferScene.ScenePath( plane["out"], context, "/" )

		# The path shouldn't connect to the context changed signal
		# until it really need to - when something is connected
		# to the path's own changed signal.
		self.assertEqual( context.changedSignal().num_slots(), 0 )
		cs = GafferTest.CapturingSlot( p.pathChangedSignal() )
		self.assertEqual( context.changedSignal().num_slots(), 1 )
		self.assertEqual( len( cs ), 0 )

		context["test"] = 10
		self.assertTrue( len( cs ), 1 )

		# Changing the context should disconnect from the old one
		# and reconnect to the new one.
		context2 = Gaffer.Context()
		self.assertEqual( context2.changedSignal().num_slots(), 0 )

		p.setContext( context2 )
		self.assertEqual( context.changedSignal().num_slots(), 0 )
		self.assertEqual( context2.changedSignal().num_slots(), 1 )

		context["test"] = 20
		self.assertTrue( len( cs ), 1 )

		context["test"] = 10
		self.assertTrue( len( cs ), 2 )

	def testSignallingAfterDestruction( self ) :

		plane = GafferScene.Plane()
		context = Gaffer.Context()
		path = GafferScene.ScenePath( plane["out"], context, "/" )

		# force path to connect to signals
		path.pathChangedSignal()

		# destroy path
		del path

		# force emission of signals on scene and context
		plane["name"].setValue( "dontCrashNow" )
		context["dontCrashNow"] = 10

	def testPlugRemovedFromNode( self ) :

		box = Gaffer.Box()
		box["p"] = Gaffer.IntPlug()
		box["out"] = GafferScene.ScenePlug( direction = Gaffer.Plug.Direction.Out )
		context = Gaffer.Context()
		path = GafferScene.ScenePath( box["out"], context, "/" )

		# force path to connect to signals
		path.pathChangedSignal()

		# mess things up
		del box["out"]
		del path

		# trigger plug dirtied on the Box
		box["p"].setValue( 10 )

	def testSceneAccessors( self ) :

		s1 = GafferScene.Plane()
		s2 = GafferScene.Plane()

		path = GafferScene.ScenePath( s1["out"], Gaffer.Context(), "/" )
		self.assertTrue( path.getScene().isSame( s1["out"] ) )

		cs = GafferTest.CapturingSlot( path.pathChangedSignal() )

		s1["name"].setValue( "p" )
		self.assertEqual( len( cs ), 1 )
		del cs[:]

		path.setScene( s1["out"] )
		self.assertEqual( len( cs ), 0 )
		self.assertTrue( path.getScene().isSame( s1["out"] ) )
		s1["name"].setValue( "pp" )
		self.assertEqual( len( cs ), 1 )
		del cs[:]

		path.setScene( s2["out"] )
		self.assertEqual( len( cs ), 1 )
		self.assertTrue( path.getScene().isSame( s2["out"] ) )
		s2["name"].setValue( "a" )
		self.assertEqual( len( cs ), 2 )
		del cs[:]

		s1["name"].setValue( "b" )
		self.assertEqual( len( cs ), 0 )

	def testStandardFilter( self ) :

		camera = GafferScene.Camera()
		plane = GafferScene.Plane()
		parent = GafferScene.Parent()
		parent["in"].setInput( camera["out"] )
		parent["children"][0].setInput( plane["out"] )
		parent["parent"].setValue( "/" )

		path = GafferScene.ScenePath( parent["out"], Gaffer.Context(), "/" )
		self.assertEqual( { str( c ) for c in path.children() }, { "/camera", "/plane" } )

		path.setFilter( GafferScene.ScenePath.createStandardFilter( [ "__cameras" ] ) )
		self.assertEqual( { str( c ) for c in path.children() }, { "/camera" } )

	def testNone( self ) :

		plane = GafferScene.Plane()

		with self.assertRaisesRegexp( Exception, "Python argument types" ) :
			GafferScene.ScenePath( None, Gaffer.Context() )

		with self.assertRaisesRegexp( Exception, "Python argument types" ) :
			GafferScene.ScenePath( plane["out"], None )

if __name__ == "__main__":
	unittest.main()
