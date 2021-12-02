#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* code is revised from https://github.com/PacktPublishing/Hands-On-Network-Programming-with-C/blob/master/chap13/connect_blocking.c*/

#define BUFFER_SIZE 4096

int main( int argc, char *argv[] ){
    if( argc != 3 ){
        fprintf( stderr, "usage: ./client [hostname] [port]\n" );
        return 1;
    }

    printf("Configuring remote address...\n");
    
    struct addrinfo hints;
    memset( &hints, 0, sizeof( hints ) );
    hints.ai_socktype = SOCK_STREAM;
    
    struct addrinfo *peer_address;
    if( getaddrinfo( argv[1], argv[2], &hints, &peer_address ) ){
        perror("getaddrinfo() failed");
        exit(1);
    }


    printf("Remote address is: ");
    
    char address_buffer[BUFFER_SIZE];
    char service_buffer[BUFFER_SIZE];
    getnameinfo( peer_address->ai_addr, peer_address->ai_addrlen,
        address_buffer, sizeof( address_buffer ),
        service_buffer, sizeof( service_buffer ),
        NI_NUMERICHOST );
    printf( "%s %s\n", address_buffer, service_buffer );


    printf("Creating socket...\n");
    
    int socket_peer;
    socket_peer = socket( peer_address->ai_family, peer_address->ai_socktype, peer_address->ai_protocol );
    if( socket_peer < 0 ){
        perror("socket() failed");
        exit(1);
    }

    printf("Connecting...\n");
    if( connect( socket_peer, peer_address->ai_addr, peer_address->ai_addrlen ) ){
        perror("connect() failed");
        exit(1);
    }
    freeaddrinfo( peer_address );

    printf("Connected.\n\n");

	for(;;){
		fd_set set;
		FD_ZERO( &set );
		FD_SET( socket_peer, &set );
		FD_SET( 0, &set );
        
		if( select( socket_peer + 1, &set, 0, 0, NULL ) < 0 ){
            		perror("select() failed");
            		exit(1);
        	}
		/* peer */
		if( FD_ISSET( socket_peer, &set ) ){
		    char buffer[BUFFER_SIZE];
		    int bytes_received = recv( socket_peer, buffer, BUFFER_SIZE, 0 );
		    if( bytes_received < 1 ){
		        printf("\nConnection closed by peer.\n");
		        break;
		    }
		    printf( "%.*s", bytes_received, buffer );
				
		    if( !strncmp( buffer, "Server is closed.\n", 18 ) ) 
		    	break;
		}
		/* stdin */
		else if( FD_ISSET( 0, &set ) ){
		    char buffer[BUFFER_SIZE];
		    if ( !fgets( buffer, BUFFER_SIZE, stdin ) )	
		    	break;
		    
		    send( socket_peer, buffer, strlen( buffer ), 0 );
		}
    	}

    printf("Closing socket...\n");
    close( socket_peer );

    printf("Finished.\n");
    return 0;
}

