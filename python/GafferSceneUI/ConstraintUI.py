##########################################################################
#
#  Copyright (c) 2015, Image Engine Design Inc. All rights reserved.
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

import Gaffer
import GafferUI
import GafferScene
import GafferSceneUI

##########################################################################
# Metadata
##########################################################################

Gaffer.Metadata.registerNode(

	GafferScene.Constraint,

	"description",
	"""
	Base type for nodes which constrain objects to a target
	object by manipulating their transform.
	""",

	plugs = {

		"target" : [

			"description",
			"""
			The scene location to which the objects are constrained.
			The world space transform of this location forms the basis
			of the constraint target, but is modified by the targetMode
			and targetOffset values before the constraint is applied.
			""",

			"plugValueWidget:type", "GafferSceneUI.ScenePathPlugValueWidget",

		],

		"ignoreMissingTarget" : [

			"description",
			"""
			Causes the constraint to do nothing if the target location
			doesn't exist in the scene, instead of erroring.
			""",

		],

		"targetMode" : [

			"description",
			"""
			The precise location of the target transform - this can be
			derived from the origin or bounding box of the target location.
			""",

			"preset:Origin", GafferScene.Constraint.TargetMode.Origin,
			"preset:BoundMin", GafferScene.Constraint.TargetMode.BoundMin,
			"preset:BoundMax", GafferScene.Constraint.TargetMode.BoundMax,
			"preset:BoundCenter", GafferScene.Constraint.TargetMode.BoundCenter,

			"plugValueWidget:type", "GafferUI.PresetsPlugValueWidget",

		],

		"targetOffset" : [

			"description",
			"""
			An offset applied to the target transform before the constraint
			is applied. The offset is measured in the object space of the
			target location.
			""",

		],

	},

)
