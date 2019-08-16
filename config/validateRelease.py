#!/usr/bin/env python
##########################################################################
#
#  Copyright (c) 2019, Cinesite VFX Ltd. All rights reserved.
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

import argparse
import os
import tarfile

# A script to validate a Gaffer release archive

parser = argparse.ArgumentParser()

parser.add_argument(
	"--archive",
	dest = "archive",
	required = True,
	help = "The path to the build archive to publish."
)

args = parser.parse_args()

if not os.path.exists( args.archive ) :
	parser.exit( 1, "The specified archive '%s' does not exist." % args.archive )


print( "Validating %s" % args.archive )

# Validate the release contains our mandatory components

requiredPaths = [
	os.path.join( "doc", "gaffer", "html", "index.html" ),
	os.path.join( "resources", "examples" )
]

for module in (
	"Gaffer", "GafferAppleseed", "GafferArnold", "GafferDelight",
	"GafferDispatch", "GafferImage", "GafferOSL", "GafferScene",
	"GafferTractor", "GafferVDB"
) :
	requiredPaths.append( os.path.join( "python", module ) )
	requiredPaths.append( os.path.join( "python", "%sUI" % module ) )

with tarfile.open( args.archive, "r:gz" ) as a:

	# getmember still reads the whole archive, so might as well grab them all
	# as we go. We need to strip the first directory from all paths too as that
	# contains the release name
	archivePaths = { os.path.join( *m.name.split( os.sep )[1:] ) for m in a.getmembers() if os.sep in m.name }

	missing = [ p for p in requiredPaths if p not in archivePaths ]
	if missing :
		parser.exit( 1,
			"ERROR: The following are missing from the archive:\n%s"
				% "\n".join( missing )
		)

print( "Archive appears OK" )
