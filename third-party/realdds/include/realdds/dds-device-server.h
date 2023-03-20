// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 Intel Corporation. All Rights Reserved.

#pragma once

#include <realdds/dds-option.h>
#include <realdds/dds-defines.h>

#include <rsutils/concurrency/concurrency.h>
#include <nlohmann/json_fwd.hpp>

#include <map>
#include <vector>
#include <memory>
#include <string>
#include <functional>

namespace realdds {


// Forward declaration
namespace topics {
class flexible_msg;
class device_info;
namespace raw {
class device_info;
}  // namespace raw
}  // namespace topics


class dds_participant;
class dds_publisher;
class dds_subscriber;
class dds_stream_server;
class dds_notification_server;
class dds_topic_reader;
class dds_topic_writer;
struct image_header;


// Responsible for representing a device at the DDS level.
// Each device has a root path under which the device topics are created, e.g.:
//     realsense/L515/F0090353    <- root
//         /notification
//         /control
//         /stream1
//         /stream2
//         ...
// The device server is meant to manage multiple streams and be able to publish frames to them, while
// also receive instructions (controls) from a client and generate callbacks to the user.
// 
// Streams are named, and are referred to by names, to maintain synchronization (e.g., when starting
// to stream, a notification may be sent, and the metadata may be split to another stream).
//
class dds_device_server
{
public:
    dds_device_server( std::shared_ptr< dds_participant > const & participant, const std::string & topic_root );
    ~dds_device_server();

    // A server is not valid until init() is called with a list of streams that we want to publish.
    // On successful return from init(), each of the streams will be alive so clients will be able
    // to subscribe.
    void init( const std::vector< std::shared_ptr< dds_stream_server > > & streams,
               const dds_options & options, const extrinsics_map & extr );

    bool is_valid() const { return( nullptr != _notification_server.get() ); }
    bool operator!() const { return ! is_valid(); }

    std::map< std::string, std::shared_ptr< dds_stream_server > > const & streams() const { return _stream_name_to_server; }

    void publish_notification( topics::flexible_msg && );
    void publish_metadata( topics::flexible_msg && );
    
    typedef std::function< void( const nlohmann::json & msg ) > control_callback;
    void on_open_streams( control_callback callback ) { _open_streams_callback = std::move( callback ); }

    typedef std::function< void( const std::shared_ptr< realdds::dds_option > & option, float value ) > set_option_callback;
    typedef std::function< float( const std::shared_ptr< realdds::dds_option > & option ) > query_option_callback;
    void on_set_option( set_option_callback callback ) { _set_option_callback = std::move( callback ); }
    void on_query_option( query_option_callback callback ) { _query_option_callback = std::move( callback ); }

private:
    void on_control_message_received();
    void handle_control_message( topics::flexible_msg control_message );

    void handle_set_option( const nlohmann::json & msg );
    void handle_query_option( const nlohmann::json & msg );
    std::shared_ptr< dds_option > find_option( const std::string & option_name, const std::string & owner_name );
    void send_set_option_success( uint32_t counter );
    void send_set_option_failure( uint32_t counter, const std::string & fail_reason );
    void send_query_option_success( uint32_t counter, float value );
    void send_query_option_failure( uint32_t counter, const std::string & fail_reason );

    std::shared_ptr< dds_publisher > _publisher;
    std::shared_ptr< dds_subscriber > _subscriber;
    std::string _topic_root;
    std::map< std::string, std::shared_ptr< dds_stream_server > > _stream_name_to_server;
    std::shared_ptr< dds_notification_server > _notification_server;
    std::shared_ptr< dds_topic_reader > _control_reader;
    std::shared_ptr< dds_topic_writer > _metadata_writer;
    dispatcher _control_dispatcher;
    
    control_callback _open_streams_callback = nullptr;
    set_option_callback _set_option_callback = nullptr;
    query_option_callback _query_option_callback = nullptr;

    extrinsics_map _extrinsics_map; // <from stream, to stream> to extrinsics
};  // class dds_device_server


}  // namespace realdds