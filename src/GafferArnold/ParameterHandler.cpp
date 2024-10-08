//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2013, Image Engine Design Inc. All rights reserved.
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
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR43

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

#include "GafferArnold/ParameterHandler.h"

#include "GafferOSL/ClosurePlug.h"

#include "Gaffer/CompoundNumericPlug.h"
#include "Gaffer/Node.h"
#include "Gaffer/PlugAlgo.h"
#include "Gaffer/ScriptNode.h"
#include "Gaffer/StringPlug.h"
#include "Gaffer/TypedPlug.h"

#include "IECore/MessageHandler.h"

#include "boost/container/flat_set.hpp"

#include "ai_metadata.h"

using namespace std;
using namespace Imath;
using namespace boost;
using namespace IECore;
using namespace Gaffer;
using namespace GafferArnold;

namespace
{

const AtString g_nameArnoldString( "name" );
const AtString g_gafferPlugTypeArnoldString( "gaffer.plugType" );
const AtString g_gafferDefaultArnoldString( "gaffer.default" );

const AtString g_FloatPlugArnoldString( "FloatPlug" );
const AtString g_Color3fPlugArnoldString( "Color3fPlug" );
const AtString g_Color4fPlugArnoldString( "Color4fPlug" );
const AtString g_ClosurePlugArnoldString( "ClosurePlug" );

template<typename PlugType>
Gaffer::Plug *setupNumericPlug( const AtNodeEntry *node, const AtParamEntry *parameter, Gaffer::GraphComponent *plugParent, Gaffer::Plug::Direction direction )
{
	typedef typename PlugType::ValueType ValueType;

	ValueType defaultValue = 0;
	ValueType minValue = Imath::limits<ValueType>::min();
	ValueType maxValue = Imath::limits<ValueType>::max();

	AtString name = AiParamGetName( parameter );
	bool defaultOverridden = false;
	if( std::is_same< ValueType, float >::value )
	{
		float metadataDefault;
		if( AiMetaDataGetFlt( node, name, g_gafferDefaultArnoldString, &metadataDefault ) )
		{
			defaultValue = metadataDefault;
			defaultOverridden = true;
		}
	}
	else
	{
		int metadataDefault;
		if( AiMetaDataGetInt( node, name, g_gafferDefaultArnoldString, &metadataDefault ) )
		{
			defaultValue = metadataDefault;
			defaultOverridden = true;
		}
	}

	if( !defaultOverridden )
	{
		switch( AiParamGetType( parameter ) )
		{
			case AI_TYPE_BYTE :
				defaultValue = (ValueType)AiParamGetDefault( parameter )->BYTE();
				minValue = 0;
				maxValue = 255;
				break;
			case AI_TYPE_INT :
				defaultValue = (ValueType)AiParamGetDefault( parameter )->INT();
				break;
			case AI_TYPE_UINT :
				defaultValue = (ValueType)AiParamGetDefault( parameter )->UINT();
				minValue = 0;
				break;
			case AI_TYPE_FLOAT :
				defaultValue = (ValueType)AiParamGetDefault( parameter )->FLT();
				break;
		}
	}

	PlugType *existingPlug = plugParent->getChild<PlugType>( name.c_str() );
	if(
		existingPlug &&
		existingPlug->direction() == direction &&
		existingPlug->defaultValue() == defaultValue &&
		existingPlug->minValue() == minValue &&
		existingPlug->maxValue() == maxValue
	)
	{
		existingPlug->setFlags( Gaffer::Plug::Dynamic, false );
		return existingPlug;
	}

	typename PlugType::Ptr plug = new PlugType( name.c_str(), direction, defaultValue, minValue, maxValue, Plug::Default );
	PlugAlgo::replacePlug( plugParent, plug );

	return plug.get();
}

Gaffer::Plug *setupPlug( const IECore::InternedString &parameterName, Gaffer::GraphComponent *plugParent, Gaffer::Plug::Direction direction )
{
	Plug *existingPlug = plugParent->getChild<Plug>( parameterName );
	if(
		existingPlug &&
		existingPlug->direction() == direction &&
		existingPlug->typeId() == Plug::staticTypeId()
	)
	{
		existingPlug->setFlags( Gaffer::Plug::Dynamic, false );
		return existingPlug;
	}

	PlugPtr plug = new Plug( parameterName, direction, Plug::Default );
	PlugAlgo::replacePlug( plugParent, plug );

	return plug.get();
}

template<typename PlugType>
Gaffer::Plug *setupTypedPlug( const IECore::InternedString &parameterName, Gaffer::GraphComponent *plugParent, Gaffer::Plug::Direction direction, const typename PlugType::ValueType &defaultValue )
{
	PlugType *existingPlug = plugParent->getChild<PlugType>( parameterName );
	if(
		existingPlug &&
		existingPlug->direction() == direction &&
		existingPlug->defaultValue() == defaultValue
	)
	{
		existingPlug->setFlags( Gaffer::Plug::Dynamic, false );
		return existingPlug;
	}

	typename PlugType::Ptr plug = new PlugType( parameterName, direction, defaultValue );

	PlugAlgo::replacePlug( plugParent, plug );

	return plug.get();
}

template<typename PlugType>
Gaffer::Plug *setupTypedPlug( const AtNodeEntry *node, const AtParamEntry *parameter, Gaffer::GraphComponent *plugParent, Gaffer::Plug::Direction direction, const typename PlugType::ValueType &defaultValue )
{
	return setupTypedPlug<PlugType>( AiParamGetName( parameter ).c_str(), plugParent, direction, defaultValue );
}

template<typename PlugType>
Gaffer::Plug *setupColorPlug( const AtNodeEntry *node, const AtParamEntry *parameter, Gaffer::GraphComponent *plugParent, Gaffer::Plug::Direction direction )
{
	typedef typename PlugType::ValueType ValueType;
	typedef typename ValueType::BaseType BaseType;

	ValueType defaultValue( 1 );

	AtString name = AiParamGetName( parameter );
	bool defaultOverridden = false;
	if( std::is_same< ValueType, Color4f >::value )
	{
		AtRGBA metadataDefault;
		if( AiMetaDataGetRGBA( node, name, g_gafferDefaultArnoldString, &metadataDefault ) )
		{
			memcpy( (void *)&defaultValue, &metadataDefault.r, sizeof( ValueType ) );
			defaultOverridden = true;
		}
	}
	else
	{
		AtRGB metadataDefault;
		if( AiMetaDataGetRGB( node, name, g_gafferDefaultArnoldString, &metadataDefault ) )
		{
			memcpy( (void *)&defaultValue, &metadataDefault.r, sizeof( ValueType ) );
			defaultOverridden = true;
		}
	}

	if( !defaultOverridden )
	{
		switch( AiParamGetType( parameter ) )
		{
			case AI_TYPE_RGB :
				defaultValue[0] = AiParamGetDefault( parameter )->RGB().r;
				defaultValue[1] = AiParamGetDefault( parameter )->RGB().g;
				defaultValue[2] = AiParamGetDefault( parameter )->RGB().b;
				break;
			case AI_TYPE_RGBA :
				defaultValue[0] = AiParamGetDefault( parameter )->RGBA().r;
				defaultValue[1] = AiParamGetDefault( parameter )->RGBA().g;
				defaultValue[2] = AiParamGetDefault( parameter )->RGBA().b;
				defaultValue[3] = AiParamGetDefault( parameter )->RGBA().a;
				break;
			default :
				return nullptr;
		}
	}

	ValueType minValue( Imath::limits<BaseType>::min() );
	ValueType maxValue( Imath::limits<BaseType>::max() );

	PlugType *existingPlug = plugParent->getChild<PlugType>( name.c_str() );
	if(
		existingPlug &&
		existingPlug->direction() == direction &&
		existingPlug->defaultValue() == defaultValue &&
		existingPlug->minValue() == minValue &&
		existingPlug->maxValue() == maxValue
	)
	{
		existingPlug->setFlags( Gaffer::Plug::Dynamic, false );
		return existingPlug;
	}

	typename PlugType::Ptr plug = new PlugType( name.c_str(), direction, defaultValue, minValue, maxValue, Plug::Default );
	PlugAlgo::replacePlug( plugParent, plug );

	return plug.get();
}

Gaffer::Plug *setupClosurePlug( const IECore::InternedString &parameterName, Gaffer::GraphComponent *plugParent, Gaffer::Plug::Direction direction )
{
	GafferOSL::ClosurePlug *existingPlug = plugParent->getChild<GafferOSL::ClosurePlug>( parameterName );
	if(
		existingPlug &&
		existingPlug->direction() == direction
	)
	{
		existingPlug->setFlags( Gaffer::Plug::Dynamic, false );
		return existingPlug;
	}

	GafferOSL::ClosurePlugPtr plug = new GafferOSL::ClosurePlug( parameterName, direction );

	PlugAlgo::replacePlug( plugParent, plug );

	return plug.get();
}

const string nodeName ( Gaffer::GraphComponent *plugParent )
{
	const Gaffer::Node *node = IECore::runTimeCast<const Gaffer::Node>( plugParent );
	if( !node )
	{
		node = plugParent->ancestor<Gaffer::Node>();
	}

	return node->relativeName( node->scriptNode() );
}


} // namespace

Gaffer::Plug *ParameterHandler::setupPlug( const IECore::InternedString &parameterName, int parameterType, Gaffer::GraphComponent *plugParent, Gaffer::Plug::Direction direction )
{
	switch( parameterType )
	{
		case AI_TYPE_RGB :

			return setupTypedPlug<Color3fPlug>( parameterName, plugParent, direction, Color3f( 0.0f ) );

		case AI_TYPE_RGBA :

			return setupTypedPlug<Color4fPlug>( parameterName, plugParent, direction, Color4f( 0.0f ) );

		case AI_TYPE_FLOAT :

			return setupTypedPlug<FloatPlug>( parameterName, plugParent, direction, 0.0f );

		case AI_TYPE_INT :

			return setupTypedPlug<IntPlug>( parameterName, plugParent, direction, 0 );

		case AI_TYPE_VECTOR2 :

			return setupTypedPlug<V2fPlug>( parameterName, plugParent, direction, V2f( 0.0f ) );

		case AI_TYPE_VECTOR :

			return setupTypedPlug<V3fPlug>( parameterName, plugParent, direction, V3f( 0.0f ) );

		case AI_TYPE_POINTER :

			return ::setupPlug( parameterName, plugParent, direction );

		case AI_TYPE_CLOSURE :

			return setupClosurePlug( parameterName, plugParent, direction );

		case AI_TYPE_STRING :

			return setupTypedPlug<StringPlug>( parameterName, plugParent, direction, "" );

		case AI_TYPE_BOOLEAN :

			return setupTypedPlug<BoolPlug>( parameterName, plugParent, direction, false );

		case AI_TYPE_MATRIX :

			return setupTypedPlug<M44fPlug>( parameterName, plugParent, direction, false );

		default :

			msg(
				Msg::Warning,
				"GafferArnold::ParameterHandler::setupPlug",
				format( "Unsupported parameter type \"%s\" for \"%s\" on node \"%s\"" ) %
					AiParamGetTypeName( parameterType ) %
					parameterName.string() %
					nodeName( plugParent )
			);
			return nullptr;

	}
}

Gaffer::Plug *ParameterHandler::setupPlug( const AtNodeEntry *node, const AtParamEntry *parameter, Gaffer::GraphComponent *plugParent, Gaffer::Plug::Direction direction )
{
	Plug *plug = nullptr;

	int parameterType = AiParamGetType( parameter );

	AtString plugTypeOverride;
	AtString name = AiParamGetName( parameter );
	if( AiMetaDataGetStr( node, name, g_gafferPlugTypeArnoldString, &plugTypeOverride ) )
	{
		if( plugTypeOverride == g_FloatPlugArnoldString )
		{
			parameterType = AI_TYPE_FLOAT;
		}
		else if( plugTypeOverride == g_Color3fPlugArnoldString )
		{
			parameterType = AI_TYPE_RGB;
		}
		else if( plugTypeOverride == g_Color4fPlugArnoldString )
		{
			parameterType = AI_TYPE_RGBA;
		}
		else if( plugTypeOverride == g_ClosurePlugArnoldString )
		{
			parameterType = AI_TYPE_CLOSURE;
		}
		else
		{
			msg(
				Msg::Warning,
				"GafferArnold::ParameterHandler::setupPlug",
				format( "Unsupported plug type \"%s\" for parameter \"%s\"" ) %
				plugTypeOverride %
				name.c_str()
			);
		}
	}

	switch( parameterType )
	{
		case AI_TYPE_BYTE :
		case AI_TYPE_INT :
		case AI_TYPE_UINT :

			plug = setupNumericPlug<IntPlug>( node, parameter, plugParent, direction );
			break;

		case AI_TYPE_FLOAT :

			plug = setupNumericPlug<FloatPlug>( node, parameter, plugParent, direction );
			break;

		case AI_TYPE_BOOLEAN :
			{
				bool defaultValue = AiParamGetDefault( parameter )->BOOL();
				AiMetaDataGetBool( node, name, g_gafferDefaultArnoldString, &defaultValue );
				plug = setupTypedPlug<BoolPlug>(
					node,
					parameter,
					plugParent,
					direction,
					defaultValue
				);
			}
			break;

		case AI_TYPE_RGB :

			plug = setupColorPlug<Color3fPlug>( node, parameter, plugParent, direction );
			break;

		case AI_TYPE_RGBA :

			plug = setupColorPlug<Color4fPlug>( node, parameter, plugParent, direction );
			break;

		case AI_TYPE_VECTOR2 :
			{
				AtVector2 defaultValue = AiParamGetDefault( parameter )->VEC2();
				AiMetaDataGetVec2( node, name, g_gafferDefaultArnoldString, &defaultValue );

				plug = setupTypedPlug<V2fPlug>(
					node,
					parameter,
					plugParent,
					direction,
					V2f( defaultValue.x, defaultValue.y )
				);
			}
			break;
		case AI_TYPE_VECTOR :
			{
				AtVector defaultValue = AiParamGetDefault( parameter )->VEC();
				AiMetaDataGetVec( node, name, g_gafferDefaultArnoldString, &defaultValue );

				plug = setupTypedPlug<V3fPlug>(
					node,
					parameter,
					plugParent,
					direction,
					V3f( defaultValue.x, defaultValue.y, defaultValue.z )
				);
			}
			break;
		case AI_TYPE_ENUM :

			{
				AtEnum e = AiParamGetEnum( parameter );

				std::string defaultValue;
				AtString metadataDefault;
				if( AiMetaDataGetStr( node, name, g_gafferDefaultArnoldString, &metadataDefault ) )
				{
					defaultValue = metadataDefault.c_str();
				}
				else
				{
					defaultValue = AiEnumGetString( e, AiParamGetDefault( parameter )->INT() );
				}

				plug = setupTypedPlug<StringPlug>(
					node,
					parameter,
					plugParent,
					direction,
					defaultValue
				);
			}
			break;

		case AI_TYPE_STRING :

			{
				AtString defaultValue = AiParamGetDefault( parameter )->STR();
				AiMetaDataGetStr( node, name, g_gafferDefaultArnoldString, &defaultValue );
				plug = setupTypedPlug<StringPlug>(
					node,
					parameter,
					plugParent,
					direction,
					defaultValue.c_str()
				);
			}
			break;

		case AI_TYPE_MATRIX :

			plug = setupTypedPlug<M44fPlug>(
				node,
				parameter,
				plugParent,
				direction,
				M44f( AiParamGetDefault( parameter )->pMTX()->data )
			);
			break;

		case AI_TYPE_CLOSURE :

			plug = setupClosurePlug(
				AiParamGetName( parameter ).c_str(),
				plugParent,
				direction
			);
			break;

	}

	if( !plug )
	{
		msg(
			Msg::Warning,
			"GafferArnold::ParameterHandler::setupPlug",
			format( "Unsupported parameter \"%s\" of type \"%s\" on node \"%s\" of type \"%s\"" ) %
				AiParamGetName( parameter ) %
				AiParamGetTypeName( AiParamGetType( parameter ) ) %
				nodeName( plugParent ) %
				AiNodeEntryGetName( node )
		);
	}

	return plug;
}

void ParameterHandler::setupPlugs( const AtNodeEntry *nodeEntry, Gaffer::GraphComponent *plugsParent, Gaffer::Plug::Direction direction )
{

	// Make sure we have a plug to represent each parameter, reusing plugs wherever possible.

	std::set<const Plug *> validPlugs;

	AtParamIterator *it = AiNodeEntryGetParamIterator( nodeEntry );
	const std::string nodeName = AiNodeEntryGetName( nodeEntry );
	while( const AtParamEntry *param = AiParamIteratorGetNext( it ) )
	{
		AtString name = AiParamGetName( param );
		if( name == g_nameArnoldString )
		{
			continue;
		}

		AtString plugType;
		if( AiMetaDataGetStr( nodeEntry, name, g_gafferPlugTypeArnoldString, &plugType ) )
		{
			if( plugType.length() == 0 )
			{
				continue;
			}
		}
		validPlugs.insert( setupPlug( nodeEntry, param, plugsParent, direction ) );
	}
	AiParamIteratorDestroy( it );

	// Remove any old plugs which it turned out we didn't need.

	for( int i = plugsParent->children().size() - 1; i >= 0; --i )
	{
		Plug *child = plugsParent->getChild<Plug>( i );
		if( validPlugs.find( child ) == validPlugs.end() )
		{
			plugsParent->removeChild( child );
		}
	}
}
