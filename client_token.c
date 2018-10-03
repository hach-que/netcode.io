
/*
    netcode.io reference implementation

    Copyright © 2017, The Network Protocol Company, Inc.

    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

        2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer 
           in the documentation and/or other materials provided with the distribution.

        3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived 
           from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
    USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "netcode.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <inttypes.h>
#include "b64/b64.h"

static volatile int quit = 0;

void interrupt_handler( int signal )
{
    (void) signal;
    quit = 1;
}

int main( int argc, char ** argv )
{
    (void) argc;
    (void) argv;

    if ( netcode_init() != NETCODE_OK )
    {
        printf( "error: failed to initialize netcode.io\n" );
        return 1;
    }

	if (argc < 2)
	{
		printf("error: expected argument: base64-encoded connection token\n");
		return 1;
	}

	char* base64_connection_token = argv[1];
	
	size_t decsize;
	char* connect_token_decoded = b64_decode_ex(base64_connection_token, strlen(base64_connection_token), &decsize);
	if (decsize != NETCODE_CONNECT_TOKEN_BYTES) 
	{
		printf("error: invalid length: base64-encoded connection token\n");
		return 1;
	}

	uint8_t* connect_token = connect_token_decoded;

    netcode_log_level( NETCODE_LOG_LEVEL_INFO );

    double time = 0.0;
    double delta_time = 1.0 / 60.0;

    printf( "[client_token]\n" );

    struct netcode_client_t * client = netcode_client_create( "0.0.0.0", time );

    if ( !client )
    {
        printf( "error: failed to create client\n" );
        return 1;
    }

    netcode_client_connect( client, connect_token );

    signal( SIGINT, interrupt_handler );

    uint8_t packet_data[NETCODE_MAX_PACKET_SIZE];
    int i;
    for ( i = 0; i < NETCODE_MAX_PACKET_SIZE; ++i )
        packet_data[i] = (uint8_t) i;

    while ( !quit )
    {
        netcode_client_update( client, time );

        if ( netcode_client_state( client ) == NETCODE_CLIENT_STATE_CONNECTED )
        {
            netcode_client_send_packet( client, packet_data, NETCODE_MAX_PACKET_SIZE );
        }

        while ( 1 )             
        {
            int packet_bytes;
            uint64_t packet_sequence;
            void * packet = netcode_client_receive_packet( client, &packet_bytes, &packet_sequence );
            if ( !packet )
                break;
            (void) packet_sequence;
            assert( packet_bytes == NETCODE_MAX_PACKET_SIZE );
            assert( memcmp( packet, packet_data, NETCODE_MAX_PACKET_SIZE ) == 0 );            
            netcode_client_free_packet( client, packet );
        }

        if ( netcode_client_state( client ) <= NETCODE_CLIENT_STATE_DISCONNECTED )
            break;

        netcode_sleep( delta_time );

        time += delta_time;
    }

    if ( quit )
    {
        printf( "\nshutting down\n" );
    }

    netcode_client_destroy( client );

    netcode_term();
    
    return 0;
}
