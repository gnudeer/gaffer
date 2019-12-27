##########################################################################
#
#  Copyright (c) 2018, John Haddon. All rights reserved.
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

import imath

import IECore

import Gaffer
import GafferImage
import GafferScene
import GafferUI
import GafferImageUI
import GafferSceneUI

class UVInspector( GafferUI.NodeSetEditor ) :

	def __init__( self, scriptNode, **kw ) :

		column = GafferUI.ListContainer()

		GafferUI.NodeSetEditor.__init__( self, column, scriptNode, **kw )

		self.__uvView = GafferSceneUI.UVView()

		with column :

			with GafferUI.Frame( borderWidth = 4, borderStyle = GafferUI.Frame.BorderStyle.None ) :
				toolbar = GafferUI.NodeToolbar.create( self.__uvView )

			self.__gadgetWidget = GafferUI.GadgetWidget(
				bufferOptions = {
					GafferUI.GLWidget.BufferOptions.Double,
					GafferUI.GLWidget.BufferOptions.AntiAlias
				},
			)

			Gaffer.NodeAlgo.applyUserDefaults( self.__uvView )
			self.__uvView.setContext( self.getContext() )

			self.__gadgetWidget.setViewportGadget( self.__uvView.viewportGadget() )
			self.__gadgetWidget.getViewportGadget().frame( imath.Box3f( imath.V3f( 0, 0, 0 ), imath.V3f( 1, 1, 0 ) ) )

		self.keyPressSignal().connect( Gaffer.WeakMethod( self.__keyPress ), scoped = False )

		self._updateFromSet()

	def __repr__( self ) :

		return "GafferSceneUI.UVInspector( scriptNode )"

	def _updateFromSet( self ) :

		GafferUI.NodeSetEditor._updateFromSet( self )

		scene = None
		if len( self.getNodeSet() ) :
			scene = next( GafferScene.ScenePlug.RecursiveOutputRange( self.getNodeSet()[-1] ), None )

		self.__uvView["in"].setInput( scene )

	def __keyPress( self, widget, event ) :

		if event.key == "F" :
			bound = self.__gadgetWidget.getViewportGadget().getPrimaryChild().bound()
			if bound.isEmpty() :
				bound = imath.Box3f( imath.V3f( 0 ), imath.V3f( 1, 1, 0 ) )
			self.__gadgetWidget.getViewportGadget().frame( bound )
			return True

		return False

GafferUI.Editor.registerType( "UVInspector", UVInspector )

Gaffer.Metadata.registerNode(

	GafferSceneUI.UVView,

	"toolbarLayout:customWidget:StateWidget:widgetType", "GafferSceneUI.UVInspector._StateWidget",
	"toolbarLayout:customWidget:StateWidget:section", "Top",
	"toolbarLayout:customWidget:StateWidget:index", -1,

	plugs = {

		"textureFileName" : [

			"plugValueWidget:type", "GafferUI.FileSystemPathPlugValueWidget",
			"path:leaf", True,
			"path:bookmarks", "image",
			"fileSystemPath:extensions", " ".join( GafferImage.ImageReader.supportedExtensions() ),
			"fileSystemPath:extensionsLabel", "Show only image files",

		],

		"displayTransform" : [

			"description",
			"""
			Applies colour space transformations for viewing the textures correctly.
			""",

			"plugValueWidget:type", "GafferUI.PresetsPlugValueWidget",
			"label", "",
			"toolbarLayout:width", 100,

			"presetNames", lambda plug : IECore.StringVectorData( GafferImageUI.ImageView.registeredDisplayTransforms() ),
			"presetValues", lambda plug : IECore.StringVectorData( GafferImageUI.ImageView.registeredDisplayTransforms() ),

		]


	}

)

## \todo This widget is basically the same as the SceneView and ImageView ones. Perhaps the
# View base class should provide standard functionality for pausing and state, and we could
# use one standard widget for everything.
class _StateWidget( GafferUI.Widget ) :

	def __init__( self, uvView, **kw ) :

		row = GafferUI.ListContainer( GafferUI.ListContainer.Orientation.Horizontal, spacing = 4 )
		GafferUI.Widget.__init__( self, row, **kw )

		with row :

			self.__busyWidget = GafferUI.BusyWidget( size = 20 )
			self.__button = GafferUI.Button( hasFrame = False )

		self.__uvView = uvView

		self.__buttonClickedConnection = self.__button.clickedSignal().connect(
			Gaffer.WeakMethod( self.__buttonClick )
		)

		self.__stateChangedConnection = self.__uvView.stateChangedSignal().connect(
			Gaffer.WeakMethod( self.__stateChanged )
		)

		self.__update()

	def __stateChanged( self, sceneGadget ) :

		self.__update()

	def __buttonClick( self, button ) :

		self.__uvView.setPaused( not self.__uvView.getPaused() )
		self.__update()

	def __update( self ) :

		paused = self.__uvView.getPaused()
		self.__button.setImage( "viewPause.png" if not paused else "viewPaused.png" )
		self.__busyWidget.setBusy( self.__uvView.state() == self.__uvView.State.Running )

UVInspector._StateWidget = _StateWidget
