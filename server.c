#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <openssl/md5.h>
#include <signal.h>

#define BUCKET_SIZE 10000
#define S_SIZE 64
#define M_SIZE 256
#define L_SIZE 1024
#define XL_SIZE 4096

struct user{
	char* account;
	char* passwd_md5;
	int online;
	int fd;
};

struct state{
	char* name;
	int try_login;
	int gaming;
	int invited;
	int oppos, board_idx, player_idx;
	int win, lose, tie;
};

struct board{
	char play_board[3][3];
	int turn;
	int cnt;
};

struct user* list[BUCKET_SIZE];
struct state* table[BUCKET_SIZE];
struct board* board_array[BUCKET_SIZE];

unsigned long hash( unsigned char* str );	//djb2 hash algorithm
char *str2md5( const char *str, int length );	//source : https://stackoverflow.com/a/8389763
int check( const char board[3][3] );
void menu( int fd );
void logo( int fd );
void clear( int fd, fd_set* m );
void init( int fd );
void sighandler_ctrlc();

fd_set set;
int max_socket;
int socket_listen;

int main(){
	signal( SIGINT, sighandler_ctrlc );
	printf("Configuring local address...\n");

	struct addrinfo hints;
	
	memset( &hints, 0, sizeof( hints ) );
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET;		/* ----- ipv4 ----- */
	hints.ai_socktype = SOCK_STREAM;	/* ----- tcp ----- */

	struct addrinfo* bind_address;
	getaddrinfo( 0, "8080", &hints, &bind_address );

	printf("Creating socket...\n");
	socket_listen = socket( bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol );

	if( socket_listen < 0 ){
		perror("socket() failed");
		exit(1);
	}

	printf("Binding socket to local address...\n");

	if( bind( socket_listen, bind_address->ai_addr, bind_address->ai_addrlen ) == -1 ){
		perror("bind() failed");
		exit(1);
	}

	printf("Listening...\n");
	if( listen( socket_listen, 10 ) == -1 ){
		perror("listen() failed");
		exit(1);
	}

	FD_ZERO( &set );
	FD_SET( socket_listen, &set );
	max_socket = socket_listen;

	printf("Waiting for connections...\n");

	/* ----- read and load player account ----- */
	FILE* fp = fopen( "./account.txt", "r" );
	if( fp == NULL ){
		perror("fopen failed: ");
		exit(1);
	}
	
	char account[16], passwd_md5[33];
	while( ( fscanf( fp, "%s %s", account, passwd_md5 ) ) != EOF ){
		struct user* temp = (struct user*)malloc( sizeof( struct user ) );
		temp->account = (char*)malloc( 16 );
		memset( temp->account, '\0', 16 );
		temp->passwd_md5 = (char*)malloc( 33 );
		memset( temp->passwd_md5, '\0', 33 );

		strcpy( temp->account, account );
		strcpy( temp->passwd_md5, passwd_md5 );

		temp->online = 0;
		temp->fd = -1;

		unsigned long h = hash( temp->account ) % BUCKET_SIZE;
		list[h] = temp;

	}
	fclose( fp );


	for(;;){
		fd_set rset = set;

		if( select( max_socket + 1, &rset, 0, 0, 0 ) == -1 ){
			perror("select() failed");
			exit(1);
		}

		for( int i = 1; i <= max_socket; i++ ){
			unsigned long h;
			if( FD_ISSET( i, &rset ) ){
				/* ----- new client connection ----- */
				if( i == socket_listen ){
					struct sockaddr_storage client_address;
					socklen_t client_len = sizeof( client_address );
				
					int socket_client = accept( socket_listen, (struct sockaddr*) &client_address, &client_len );
				
					if( socket_client == -1 ){
						perror("accept fail()");
						exit(1);
					}
				
					FD_SET( socket_client, &set );
					if( socket_client > max_socket )
						max_socket = socket_client;

					char address_buffer[100];
					getnameinfo( (struct sockaddr*) &client_address, client_len, address_buffer, sizeof( address_buffer ), 0, 0, NI_NUMERICHOST );
				
					printf( "New connection from %s\n" , address_buffer );
					
					logo(socket_client);					
					send( socket_client, "Please enter your account or the account you want to register:\n", 63, 0 ); 
				}

				/* ----- client ----- */
				else{
					char read[L_SIZE];
					int bytes_received = recv( i, read, L_SIZE, 0 );
					if( bytes_received < 1 ){
						FD_CLR( i, &set );
						close( i );
						continue;
					}
					read[bytes_received - 1] = '\0';
					
					/* ----- log in ----- */
					if( table[i] == NULL ){
						table[i] = (struct state*)malloc( sizeof( struct state ) );
						table[i]->name = (char*)malloc( 16 );
						strcpy( table[i]->name, read );
						table[i]->try_login = 1;
						
						fprintf( stderr, "Account: %s want to login\n", table[i]->name );
						send( i, "Please enter your password:\n", 28, 0 );
					}

					else if( table[i] != NULL ){
						h = hash( table[i]->name ) % BUCKET_SIZE;
						
						if( table[i]->try_login == 1 ){
							char passwd[16];
							strcpy( passwd, read );
						
							char* md5 = str2md5( passwd, strlen( passwd ) );

							if( list[h] != NULL && !strcmp( md5, list[h]->passwd_md5 ) && list[h]->online == 0 ){
								char buffer[S_SIZE] = {0};
								sprintf( buffer, "Log in successfully. Wellcome, %s!\n", list[h]->account );
								send( i, buffer, strlen( buffer ), 0 );

								menu( i );
								fprintf( stderr, "Account: %s logged in\n", table[i]->name );
								
								list[h]->online = 1;
								list[h]->fd = i;
								
								table[i]->try_login = 0;
								table[i]->win = 0;
								table[i]->lose = 0;
								table[i]->tie = 0;
								init( i );
							}
							else if( list[h] != NULL && !strcmp( md5, list[h]->passwd_md5 ) && list[h]->online == 1 ){

								send( i, "This account is already logged in\n", 34, 0 );
								fprintf( stderr, "Account: %s already logged in\n", table[i]->name );
								
								table[i]->try_login = 0;
								clear( i, &set );
								continue;
							}
							else if( list[h] != NULL && strcmp( md5, list[h]->passwd_md5 ) ){

								send( i, "Account or Password Error! Please try again!\n", 45, 0 );
								fprintf( stderr, "Account %s login failed\n", table[i]->name );

								table[i]->try_login = 0;
								clear( i, &set );
								continue;
							}
							else if( list[h] == NULL ){
								FILE* fp = fopen( "./account.txt", "a" );
								if( fp == NULL )
									perror("fopen failed: ");
								else
									fprintf( fp, "%s %s\n", table[i]->name, md5 );
								
								fclose( fp );

								struct user* temp = (struct user*)malloc( sizeof( struct user ) );
								temp->account = (char*)malloc( 16 );
								memset( temp->account, '\0', 16 );
								temp->passwd_md5 = (char*)malloc( 33 );
								memset( temp->passwd_md5, '\0', 33 );

								strcpy( temp->account, table[i]->name );
								strcpy( temp->passwd_md5, md5 );
								
								temp->online = 1;
								temp->fd = i;
								list[h] = temp;
								
								char buffer[S_SIZE] = {0};
								sprintf( buffer, "Register successfully. Welcome, %s!\n", list[h]->account );
								send( i, buffer, strlen( buffer ), 0 );

								menu( i );
								fprintf( stderr, "Account: %s registered and logged in\n", table[i]->name );
								table[i]->try_login = 0;
								table[i]->win = 0;
								table[i]->lose = 0;
								table[i]->tie = 0;
								init( i );
							}
						}
						else if( table[i]->gaming == 1){
							int oppos = table[i]->oppos;
							int idx = table[i]->board_idx;
							int pos = atoi( read );
							int row, col;
							int ret = -1;
							char buffer[M_SIZE] = {0}, temp[32] = {0};
						
							if( board_array[idx]->turn != i ){
								continue;
							}							
		
							( pos % 3 == 0 ) ? ( row = pos / 3 - 1 ) : ( row = pos / 3 );
							( pos % 3 == 0 ) ? ( col = 2 ) : ( col = pos % 3 - 1 ); 

							if( !strcmp( read , "quit" ) ){
								table[oppos]->win += 1;
								table[i]->lose += 1;
								
								memset( buffer, '\0', M_SIZE );
								sprintf( buffer, "%s gives up, Congratulates %s, You are a winner!\n", table[i]->name,table[oppos]->name );
								send( oppos, buffer, strlen( buffer ), 0 );
								
								memset( buffer, '\0', M_SIZE );
								sprintf( buffer, "You give up, %s wins this round...\n", table[i]->name );
								send( i, buffer, strlen( buffer ), 0 );	
								init( i );
								init( oppos );
								menu( i );
								menu( oppos );
								continue;
							}	
							else if(pos > 9 || pos < 1 || board_array[idx]->play_board[row][col] == 'O' || board_array[idx]->play_board[row][col] == 'X'){
								send( i, "This operation is invalid!\n", 27, 0 );
								continue;
							}				
							board_array[idx]->turn = oppos;
							( table[i]->player_idx == 1 ) ? ( board_array[idx]->play_board[row][col] = 'X' ) : ( board_array[idx]->play_board[row][col] = 'O' );
							board_array[idx]->cnt -= 1;
																
							strcat( buffer, "\t     |     |      \n" );
							sprintf( temp, "\t  %c  |  %c  |  %c  \n", board_array[idx]->play_board[0][0], board_array[idx]->play_board[0][1], board_array[idx]->play_board[0][2] );
							strcat( buffer, temp );
										
							strcat( buffer, "\t_____|_____|_____\n" );
							strcat( buffer, "\t     |     |      \n" );
							sprintf( temp, "\t  %c  |  %c  |  %c  \n", board_array[idx]->play_board[1][0], board_array[idx]->play_board[1][1], board_array[idx]->play_board[1][2] );
							strcat( buffer, temp );
										
							strcat( buffer, "\t_____|_____|_____\n" );
							strcat( buffer, "\t     |     |      \n" );
							sprintf( temp, "\t  %c  |  %c  |  %c  \n", board_array[idx]->play_board[2][0], board_array[idx]->play_board[2][1], board_array[idx]->play_board[2][2] );
							strcat( buffer, temp );
										
							strcat( buffer, "\t     |     |     \n" );

							send( i, buffer, strlen( buffer ), 0 );
							send( oppos, buffer, strlen( buffer ), 0 );
								
								
							memset( buffer, '\0', M_SIZE );
							sprintf( buffer, "%s, please enter the number of the square where you want to place your %c\n", table[oppos]->name, ( table[oppos]->player_idx )?'X':'O' );
							strcat( buffer , "or you can key in 'quit' to leave\n" );
							send( oppos, buffer, strlen( buffer ), 0 );
								
							memset( buffer, '\0', M_SIZE );
							sprintf( buffer, "Waiting for %s...\n", table[oppos]->name );
							send( i, buffer, strlen( buffer ), 0 );
							ret = check( board_array[idx]->play_board );

							if( ret == 0 || ret == 1 ){
								int winner, loser;

								if(table[i]->player_idx != ret){
									winner = oppos;
									loser = i;	
								}
								else{
									winner = i;
									loser = oppos;
								}

								table[winner]->win += 1;
								table[loser]->lose += 1;
								
								memset( buffer, '\0', M_SIZE );
								sprintf( buffer, "Congratulates %s, You are a winner!\n", table[winner]->name );
								send( winner, buffer, strlen( buffer ), 0 );
								
								memset( buffer, '\0', M_SIZE );
								sprintf( buffer, "%s wins this round...\n", table[winner]->name );
								send( loser, buffer, strlen( buffer ), 0 );

								init( i );
								init( oppos );
								menu( i );
								menu( oppos );
							}
							else if( board_array[idx]->cnt == 0 ){
								send( i, "The match ended in a tie\n", 25, 0 );
								send( oppos, "The match ended in a tie\n", 25, 0 );
								
								table[i]->tie += 1;
								table[oppos]->tie += 1;

								init( i );
								init( oppos );
								menu( i );
								menu( oppos );
							}
						}
						else{
							if( !strcmp( read, "whoami" ) ){
								char buffer[L_SIZE];
								memset( buffer, '\0', sizeof( buffer ) );
								int my_fd;
								h = hash( table[i]->name ) % BUCKET_SIZE;
								if( table[i] != NULL ){
								
									strcat( buffer, "Name : " );	
									strcat( buffer, table[i]->name );
									strcat( buffer, "\n");
									strcat( buffer, "Record : " );
									
									if( list[h] != NULL ){
										char record[S_SIZE];
										my_fd = list[h]->fd;
										if( table[my_fd]->win + table[my_fd]->lose + table[my_fd]->tie != 0 ){
											float r = (float)table[my_fd]->win / ( table[my_fd]->win + table[my_fd]->lose + table[my_fd]->tie) * 100.0;
											sprintf( record, "Win = %d // Lose = %d // Tie = %d // Win rate = %.2f%%\n", table[my_fd]->win, table[my_fd]->lose, table[my_fd]->tie, r );
											strcat( buffer, record ); 
										}
										else	
											strcat( buffer, "No Record\n");
									}	
								}						

								send( i, buffer, strlen(buffer), 0 );
								menu( i );
							}

							else if( !strcmp( read, "list" ) ){
								char buffer[L_SIZE];
								memset( buffer, '\0', sizeof( buffer ) );
								strcpy( buffer, "Player Online:\n");

								int none = 1;
								for( int k = 0; k <= max_socket; k++ ){
									if( table[k] != NULL ){
										strcat( buffer, "Name : " );	
										strcat( buffer, table[k]->name );
										if( table[k]->gaming == 1 )
											strcat( buffer, " (Playing)\n" );
										else if( table[k]->gaming == 0 )
											strcat( buffer, " (Idle)\n" );
										
										
										int record_fd;
										h = hash( table[k]->name ) % BUCKET_SIZE;
										if( list[h] != NULL ){
											char record[S_SIZE];
											record_fd = list[h]->fd;
											strcat( buffer, "Record :  " );												
											if( table[record_fd]->win + table[record_fd]->lose + table[record_fd]->tie != 0 ){
												float r = (float)table[record_fd]->win / ( table[record_fd]->win + table[record_fd]->lose + table[record_fd]->tie) * 100.0;
												sprintf( record, "Win = %d // Lose = %d // Tie = %d // Win rate = %.2f%%\n", table[record_fd]->win, table[record_fd]->lose, table[record_fd]->tie, r );
												strcat( buffer, record ); 
											}
											else	
												strcat( buffer, "No Record\n");
										}										
												
					
										int len = strlen( buffer );
										buffer[len] = '\n';
										none = 0;
									}
								}
								
								if( none ){
									send( i, "None...\n", 8, 0 );
								}
								else{
									send( i, buffer, strlen( buffer ), 0 );
								}
								menu( i );
							}
							
							else if( !strncmp( read, "invite", 6 ) ){
								char* invited_user = strstr( read, " " );
								if(!invited_user){
									send( i, "Usage: invite [username]\n", 25, 0 );
									menu( i );
									continue;
								}
								invited_user += 1;
								
								fprintf( stderr, "%s invite %s to play\n", table[i]->name, invited_user );

								int invited_fd;
								h = hash( invited_user ) % BUCKET_SIZE;

								if( list[h] == NULL){
									char buffer[S_SIZE];
									sprintf( buffer, "%s not found...\n", invited_user );
									send( i, buffer, strlen( buffer ), 0 );
									menu( i );
									continue;
								}
								else if( list[h]->online != 1 ){
									send( i, "This player is not online now...\n", 33, 0 );
									menu( i );
									continue;
								}
								else if( !strcmp( table[i]->name, invited_user) ){
									send( i, "You can not invite yourself\n", 28, 0 );
									menu( i );
									continue;
								}
								else
									invited_fd = list[h]->fd;
									
								
														
								if( table[invited_fd]->gaming == 1 ){
									char buffer[S_SIZE];
									sprintf( buffer, "%s is playing now. Wait for a second\n", invited_user );
									send( i, buffer, strlen( buffer ), 0 );
									menu( i );
									continue;
								}
								
								table[i]->oppos = invited_fd;
								
								table[invited_fd]->invited = 1;
								table[invited_fd]->oppos = i;
								
								char buffer[S_SIZE];
								sprintf( buffer, "%s has challenged you. Accept challenge? (y/n)\n", table[i]->name );
								send( invited_fd, buffer, strlen( buffer ), 0 );

								send( i, "Wait for opponent to acknowledge...\n", 36, 0 );
							}

							else if( !strcmp( read, "logout" ) ){
								fprintf( stderr, "Account: %s logged out\n", table[i]->name );
								
								h = hash( table[i]->name ) % BUCKET_SIZE;
								list[h]->online = 0;
								list[h]->fd = -1;

								send( i, "Goodbye\n", 8, 0 );
								clear( i, &set );
							}

							else if( table[i]->invited == 1 ){
								if( !strcmp( read, "y" ) ){
									table[i]->gaming = 1;
									table[i]->invited = 0;
									table[i]->board_idx = i;
									table[i]->player_idx = 1;
									
									int oppos = table[i]->oppos;
									table[oppos]->gaming = 1;
									table[oppos]->board_idx = i;
									table[oppos]->player_idx = 0;

									board_array[i] = (struct board*)malloc( sizeof( struct board ) );
									board_array[i]->turn = oppos;
									board_array[i]->cnt = 9;

									for( int j = 0; j < 3; j++ )
										for( int k = 0; k < 3; k++ )
											board_array[i]->play_board[j][k] = 3 * j + k + 1 + '0';
									
									char buffer[M_SIZE] = {0}, temp[S_SIZE] = {0};
									strcat( buffer, "\t     |     |      \n" );
									sprintf( temp, "\t  %c  |  %c  |  %c  \n", board_array[i]->play_board[0][0], board_array[i]->play_board[0][1], board_array[i]->play_board[0][2] );
									strcat( buffer, temp );
									
									strcat( buffer, "\t_____|_____|_____\n" );
									strcat( buffer, "\t     |     |      \n" );
									sprintf( temp, "\t  %c  |  %c  |  %c  \n", board_array[i]->play_board[1][0], board_array[i]->play_board[1][1], board_array[i]->play_board[1][2] );
									strcat( buffer, temp );
									
									strcat( buffer, "\t_____|_____|_____\n" );
									strcat( buffer, "\t     |     |      \n" );
									sprintf( temp, "\t  %c  |  %c  |  %c  \n", board_array[i]->play_board[2][0], board_array[i]->play_board[2][1], board_array[i]->play_board[2][2] );
									strcat( buffer, temp );
									
									strcat( buffer, "\t     |     |     \n" );

									send( i, buffer, strlen( buffer ), 0 );
									send( oppos, buffer, strlen( buffer ), 0 );

									memset( buffer, '\0', M_SIZE );
									sprintf( buffer, "Waiting for %s...\n", table[oppos]->name );
									send( i, buffer, strlen( buffer ), 0 );
									
									memset( buffer, '\0', M_SIZE );
									sprintf( buffer, "%s, please enter the number of the square where you want to place your %c:\n", table[oppos]->name, ( table[oppos]->player_idx == 1 )?'X':'O' );
									send( oppos, buffer, strlen( buffer ), 0 );
								}
								else if( !strcmp( read, "n" ) ){
									int oppos = table[i]->oppos;
									table[oppos]->oppos = -1;
									table[i]->invited = 0;
									table[i]->oppos = -1;

									char buffer[S_SIZE];
									sprintf( buffer, "%s has rejected your challenge\n", table[i]->name );
									send( oppos, buffer, strlen( buffer ), 0 );
									menu( i );
									menu( oppos );
								}
								else{
									send( i, "please key in y or n\n", 21, 0 );
								}
							}

							else if( !strncmp( read, "send", 4 ) ){
								char* q = strstr( read, " " );
								if( q == NULL ){
									send( i, "Usage: send [username] [message]\n", 33, 0 );
									menu( i );
									continue;
								}
								*q = '\0';
								char* sended_user = q + 1;

								q = strstr( sended_user, " " );
								if( q == NULL ){
									send( i, "Usage: send [username] [message]\n", 33, 0 );
									menu( i );
									continue;
								}
								*q = '\0';
								char* message = q + 1;
								
								fprintf( stderr, "%s sends message to %s\n", table[i]->name, sended_user );

								int sended_fd;
								h = hash( sended_user ) % BUCKET_SIZE;
								if( list[h] != NULL ){
									if( list[h]->online == 1 ){
										sended_fd = list[h]->fd;
										char buffer[M_SIZE];
										sprintf( buffer, "%s send message to you: %s\n", table[i]->name, message );
										send( sended_fd, buffer, strlen( buffer ), 0 );
										menu( sended_fd );
									
										if( sended_fd != i )
											menu( i );
									}
									else{
										send( i, "This player is not online now...\n", 33, 0 );		
									}	
								}
								else{
									char buffer[S_SIZE];
									sprintf( buffer, "%s not found...\n", sended_user );
									send( i, buffer, strlen( buffer ), 0 );
									menu( i );
								}
							
							}
							else{
								send( i, "No such options!\n", 17, 0 );
								menu( i );
							}
						}
					}
				}
			}
		}
	}

	printf("Closing listening socket...\n");
	close( socket_listen );

	printf("Finished.\n");

	return 0;
}

unsigned long hash( unsigned char* str ){
	unsigned long hash = 5381;
	int c;

	while( ( c = *str++ ) )
		hash = ( ( hash << 5 ) + hash ) + c;

	return hash;
}

char *str2md5( const char *str, int length ){
    int n;
    MD5_CTX c;
    unsigned char digest[16];
    char *out = (char*)malloc(33);

    MD5_Init( &c );

    while( length > 0 ){
        if( length > 512 ){
            MD5_Update( &c, str, 512 );
        } 
		else{
            MD5_Update( &c, str, length );
        }
        length -= 512;
        str += 512;
    }

    MD5_Final( digest, &c );

    for( n = 0; n < 16; ++n ){
        snprintf( &(out[n*2]), 16*2, "%02x", (unsigned int)digest[n] );
    }

    return out;
}

int check( const char board[3][3] ){
	for( int i = 0; i < 3; i++ ){
		if( board[i][0] == board[i][1] && board[i][1] == board[i][2] ){
			if( board[i][0] == 'X' )
				return 1;
			else
				return 0;
		}
		if( board[0][i] == board[1][i] && board[1][i] == board[2][i] ){
			if( board[0][i] == 'X' )
				return 1;
			else
				return 0;
		}
	}

	if( board[0][0] == board[1][1] && board[1][1] == board[2][2] ){
		if( board[0][0] == 'X' )
			return 1;
		else
			return 0;
	}
	if( board[0][2] == board[1][1] && board[1][1] == board[2][0] ){
		if( board[2][0] == 'X' )
			return 1;
		else
			return 0;
	}

	return 2;
}

void logo( int fd ){
	char buffer[XL_SIZE] = {0};
	strcat( buffer, "+=============================================================================================+\n" );
	strcat( buffer, "|  ▄██████▄  ▀████    ▐████▀  ▄████████    ▄█    █▄       ▄████████    ▄████████    ▄████████ |\n" );
	strcat( buffer, "| ███    ███   ███▌   ████▀  ███    ███   ███    ███     ███    ███   ███    ███   ███    ███ |\n" );
	strcat( buffer, "| ███    ███    ███  ▐███    ███    █▀    ███    ███     ███    █▀    ███    █▀    ███    █▀  |\n" );
	strcat( buffer, "| ███    ███    ▀███▄███▀    ███         ▄███▄▄▄▄███▄▄  ▄███▄▄▄       ███          ███        |\n" );
	strcat( buffer, "| ███    ███    ████▀██▄     ███        ▀▀███▀▀▀▀███▀  ▀▀███▀▀▀     ▀███████████ ▀███████████ |\n" );
	strcat( buffer, "| ███    ███   ▐███  ▀███    ███    █▄    ███    ███     ███    █▄           ███          ███ |\n" );
	strcat( buffer, "| ███    ███  ▄███     ███▄  ███    ███   ███    ███     ███    ███    ▄█    ███    ▄█    ███ |\n" );
	strcat( buffer, "| ▀██████▀  ████       ███▄ ████████▀    ███    █▀      ██████████  ▄████████▀   ▄████████▀   |\n" );
	strcat( buffer, "+=============================================================================================+\n" );
	send( fd, buffer, strlen( buffer ), 0 );
}

void menu( int fd ){
	char buffer[L_SIZE] = {0};
	strcat( buffer, "\n" );
	strcat( buffer, "---------------------------------\n" );
	strcat( buffer, "Command:\n" );
	strcat( buffer, "\twhoami\n" );
	strcat( buffer, "\tlist\n" );
	strcat( buffer, "\tinvite [username]\n" );
	strcat( buffer, "\tsend [username] [message]\n" );
	strcat( buffer, "\tlogout\n" );
	strcat( buffer, "---------------------------------\n" );
	strcat( buffer, "\n" );
	
	send( fd, buffer, strlen( buffer ), 0 );
}

void clear( int fd, fd_set* m ){
	free( table[fd]->name );
	table[fd]->name = NULL;
	free( table[fd] );
	table[fd] = NULL;
	FD_CLR( fd, m );
	close( fd );
}

void init( int fd ){
	table[fd]->gaming = 0;
	table[fd]->oppos = -1;
	table[fd]->board_idx = -1;
	table[fd]->player_idx = -1;
}

void sighandler_ctrlc(){
	fprintf( stderr, "Ctrl-c\n" );
	int time = 3, fd;
	unsigned long h;
	while( time-- ){
		for( int i = 0; i <= max_socket; i++ )
			if( table[i] != NULL ){
				h = hash( table[i]->name ) % BUCKET_SIZE;
				fd = list[h]->fd;
				char buffer[] = "Server is closed.\n";
				send( fd, buffer, strlen( buffer ), 0 );	
			}
		sleep(1);
	}
	printf("Closing listening socket...\n");
	close( socket_listen );

	printf("Finished.\n");
	exit(0);
}
