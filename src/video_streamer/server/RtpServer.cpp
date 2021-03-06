#include "video_streamer/server/RtpServer.h"

using namespace HighFlyers;

RtpServer::RtpServer()
{
	//initialization
	src = NULL;

	//create elements
	fmt = create_gst_element_safe( "capsfilter", "fmt" );
	video_rate = create_gst_element_safe( "videorate", "video_rate" );
	video_convert = create_gst_element_safe( "videoconvert", "video_convert" );
	payloader = create_gst_element_safe( "rtpvrawpay", "payloader" );
	sink = create_gst_element_safe( "udpsink", "sink" );

	g_object_set( sink, "sync", false, NULL );

	GstBus* bus = gst_pipeline_get_bus( GST_PIPELINE( pipeline ) );
	bus_watch_id = gst_bus_add_watch( bus, bus_call, this );
	gst_object_unref( bus );

	// set debug log
#ifdef DEBUG
	gst_debug_set_active( TRUE );
	gst_debug_set_default_threshold( GST_LEVEL_WARNING );
	gst_debug_add_log_function( ( GstLogFunction )debug_log_fnc, this, NULL );
#endif
}

RtpServer::~RtpServer()
{
}

void RtpServer::set_ip( const std::string& host )
{
	ip = host.c_str();

	if ( ip.empty() ) ip = "127.0.0.1";

	g_object_set( sink, "host", ip.c_str(), NULL );
}


void RtpServer::set_port( int port )
{
	this->port = port;
	g_object_set( sink, "port", port, NULL );
}

void RtpServer::set_video_source( const std::string& source, SourceType type )
{
	if ( src != NULL ) gst_object_unref( GST_OBJECT( src ) );

	switch ( type )
	{
	case V4lDevice :
		src = create_gst_element_safe( "v4l2src", "source" );
		g_object_set( src, "device", source.c_str(), NULL );
		break;
	case VideoTest:
	{
		src = create_gst_element_safe( "videotestsrc", "source" );
		GParamSpec* param = g_object_class_find_property( G_OBJECT_GET_CLASS ( src ), "pattern" );
		GEnumValue *values = G_ENUM_CLASS ( g_type_class_ref( param->value_type ) )->values;
		guint j = 0;

		while (values[j].value_name)
		{
			if (values[j].value_nick == source)
			{
				g_object_set( src, "pattern", j, NULL );
				break;
			}
			j++;
		}
	}
		break;
	default:
		GST_ERROR( "Invalid source type\n" );
	}
}

void RtpServer::set_caps( const std::string& caps )
{
	GstCaps* fmt_caps = gst_caps_from_string( caps.c_str() );

	g_object_set( fmt, "caps", fmt_caps, NULL );
}

bool RtpServer::init_stream()
{
	if ( !src || port < 1 || ip.empty() ) return false;

	//adding elements to pipeline
	gst_bin_add_many( GST_BIN( pipeline ), src, fmt, video_rate, video_convert, payloader, sink, NULL );

	//linking
	if ( !gst_element_link_many( src, fmt, video_rate, video_convert, payloader, sink, NULL ) )
		throw std::runtime_error( "Failed to link elements!" );

	return true;
}

void RtpServer::start_stream()
{
	gst_element_set_state( GST_ELEMENT( pipeline ), GST_STATE_PLAYING );
}

void RtpServer::stop_stream()
{
	gst_element_set_state( GST_ELEMENT( pipeline ), GST_STATE_NULL );
}
