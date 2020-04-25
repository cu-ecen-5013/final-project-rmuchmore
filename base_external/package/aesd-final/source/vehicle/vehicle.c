/*
* File: server_socket.c
*
* Description: Create a server socket to accept client connections
*
* Author: Royce Muchmore
*
* Tools:
*
* Leveraged Code:
*
* Links/References:
*   Linux System Programming 2nd Edition
*   https://www.cprogramming.com/
*   https://www.tutorialspoint.com/cprogramming/index.htm
*   https://www.tutorialspoint.com/c_standard_library/index.htm
*
*/

// Includes
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

// File defines and typedefs
#define M_ARRAY_SIZE( x ) ( sizeof(x) / sizeof(x[0]) )
#define SYSLOG_BUF_SIZE 80
#define MAX_CLIENT_CONNECTIONS 1
#define CLIENT_LOG_BUF_SIZE 4096
#define SERVER_PORT_STRING "9000" 
#define SERVER_PORT 9000

// File data and functions
static bool g_stop_signal = false;
static char client_log_file[] = "/var/tmp/aesdsocketdata";
static uint8_t client_buffer[ 2* MAX_CLIENT_CONNECTIONS ][ CLIENT_LOG_BUF_SIZE ];

static int run_daemon( void );
static int create_socket( int *socket_fd );
static int run_server( int socket_fd );
static void transfer_data( int client_socket_fd );

static int setup_signals( void );
static void signal_handler( int signal );

/*
* Name: main
*
* Description: Main loop for handling server socket connection.
*
* Inputs: argc - number of command line arguments
*         argv[] - command line arguments
*         argv[ 0 ] - program name
*
* Returns: program exit status
*
*/
int main( int argc, char *argv[] )
{
   int return_status = EXIT_SUCCESS;
   bool run_as_daemon = false;
   char program[ SYSLOG_BUF_SIZE+1 ];
   int server_fd;

   // Check for program arguments
   if ( argc > 1 )
   {
      if ( 0 == strncmp( argv[ 1 ], "-d", 2 ) )
      {
         run_as_daemon = true;
      }
   } 

   // Use program name as identifier for system log entries:
   //   /var/log/syslog
   sprintf( program, "%.*s", SYSLOG_BUF_SIZE, argv[ 0 ] ); 
   openlog( program, LOG_NDELAY, LOG_USER );
   syslog( LOG_INFO, "Started as PID: %d", getpid() );
   
   setup_signals();
   
   return_status = create_socket( &server_fd );
   if ( EXIT_SUCCESS == return_status )
   {
      if ( run_as_daemon )
      {
         return_status = run_daemon();
         if ( EXIT_SUCCESS == return_status )
         {
            return_status = run_server( server_fd );
         } 
      }
      else
      {
         return_status = run_server( server_fd );
      }
   }

   closelog();   
   return( return_status );
}

/*
* Name: run_daemon
*
* Description: Run program as a daemon.
*
* Inputs: None
*
* Returns: program exit status
*
*/
int run_daemon( void )
{
   int return_status = EXIT_SUCCESS;
   char error[ SYSLOG_BUF_SIZE+1 ];

   // Run as a daemon
   if ( -1 == daemon( 0, 0 ) )
   {
      strerror_r( errno, error, SYSLOG_BUF_SIZE );
      syslog( LOG_ERR, "%s: %s", __func__, error );
      return_status = EXIT_FAILURE;
   }
   else
   {
      syslog( LOG_INFO, "Started daemon as PID: %d", getpid() ); 
   }
   
   return( return_status );
}

/*
* Name: create_socket
*
* Description: Create and bind a socket.
*
* Inputs: None
* 
* Outputs: sockef_fd - new socket file descriptor
*
* Returns: EXIT_SUCCESS - socket was created
*          EXIT_FAILURE - failed to create socket
*
*/
int create_socket( int *socket_fd )
{
   int temp_socket_fd;
   int return_status = EXIT_FAILURE;
   char error[ SYSLOG_BUF_SIZE + 1 ];
      
   if ( socket_fd != NULL )
   {   
      temp_socket_fd = socket(
	      AF_INET,
	      SOCK_STREAM,
	      0
	      );

      if ( temp_socket_fd != -1 )
      {
         int status;
         struct addrinfo select_address = { 0 };
         struct addrinfo *socket_address = NULL;
         
         select_address.ai_family   = AF_INET;
         select_address.ai_socktype = SOCK_STREAM;
         select_address.ai_flags    = AI_PASSIVE; 

         status = getaddrinfo(
            NULL, 
            SERVER_PORT_STRING, 
            &select_address, 
            &socket_address
            );
            
         if ( 0 == status )
         { 
            status = bind(
               temp_socket_fd,
               socket_address->ai_addr,
               socket_address->ai_addrlen
               );
               
            if ( 0 == status )
            { 
               fcntl( 
                  temp_socket_fd, 
                  F_SETFL, 
                  O_NONBLOCK
                  );

               *socket_fd = temp_socket_fd;
	            return_status = EXIT_SUCCESS;
            } 
         }
         
         if ( socket_address != NULL )
         {
            freeaddrinfo( socket_address );
         }
      }
      
	   if ( return_status != EXIT_SUCCESS )
      {
         strerror_r( errno, error, SYSLOG_BUF_SIZE );
         syslog( LOG_ERR, "%s: %s", __func__, error );
      }
   }  
   else
   {
      syslog( LOG_ERR, "%s: Invalid Parameter", __func__ );
   }
   return( return_status );
}

/*
* Name: run_server
*
* Description: Listen and accept socket connections
*
* Inputs: socket_fd - soacket file descriptor
* 
* Returns: EXIT_SUCCESS
*          EXIT_FAILURE
*
*/
int run_server( int socket_fd )
{
   int return_status = EXIT_SUCCESS;
   int status;
   char error[ SYSLOG_BUF_SIZE + 1 ];

   status = listen( socket_fd, MAX_CLIENT_CONNECTIONS );
   if ( 0 == status )
   {
      struct sockaddr_storage client_address;
      socklen_t address_length;      
      int client_fd;
      
      address_length = sizeof( client_address );
      for(;;)
      {
         client_fd = accept(
            socket_fd,
            (struct sockaddr *) &client_address,
            &address_length
            );
            
         if ( client_fd != -1 )
         {
            struct sockaddr_in *client_in = (struct sockaddr_in *) &client_address;
            char client_ip[ INET_ADDRSTRLEN + 1 ];
            
            inet_ntop(
               AF_INET, 
               &(client_in->sin_addr), 
               client_ip, 
               INET_ADDRSTRLEN
               );
            syslog( LOG_INFO, "Accepted connection from %s", client_ip );
            
            transfer_data( client_fd );            
            close( client_fd );
            
            syslog( LOG_INFO, "Closed connection from %s", client_ip );
         }          
         else
         {
            if (   ( errno != EAGAIN      ) 
                && ( errno != EWOULDBLOCK )
               )
            {
               strerror_r( errno, error, SYSLOG_BUF_SIZE );
               syslog( LOG_ERR, "%s: %s", __func__, error );
               break;
            }
         }

         if ( g_stop_signal )
         {
            syslog( LOG_INFO, "%s: %s", __func__, "Caught signal, exiting" );
            break;
         }
      }
   }
   else
   {
      strerror_r( errno, error, SYSLOG_BUF_SIZE );
      syslog( LOG_ERR, "%s: %s", __func__, error );
   }
   
   close( socket_fd );
   remove( client_log_file );

   return( return_status );
}

/*
* Name: transfer_data
*
* Description: Transfer data over the client socket connection
*
* Inputs: client_socket_fd
*
* Returns: None
*
*/
void transfer_data( int client_socket_fd )
{
   int client_log_file_fd;
   int rx_bytes;
   int tx_bytes;
   int tx_length;
   struct stat file_stat;
   char *start_packet;
   char *end_packet;
   char error[ SYSLOG_BUF_SIZE + 1 ];
   
   client_log_file_fd = open( 
      client_log_file,
      O_RDWR | O_CREAT | O_APPEND,
      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
      );
      
   fcntl( 
      client_socket_fd, 
      F_SETFL, 
      O_NONBLOCK
      );
   
   for(;;)
   {
      rx_bytes = recv(
         client_socket_fd, 
         client_buffer[ 0 ], 
         CLIENT_LOG_BUF_SIZE, 
         0
         );
         
      if ( rx_bytes > 0 )
      {
         start_packet = (char *) client_buffer[ 0 ];
         do
         {
            end_packet = strchr( start_packet, '\n' );            
            if ( end_packet != NULL )
            {
               tx_length = end_packet - start_packet + 1;
               
               while ( tx_length )
               {
                  // Write received data to the end of the file
                  tx_bytes = write(
                     client_log_file_fd,
                     start_packet,
                     tx_length
                     );
                     
                  if ( -1 == tx_bytes )
                  {
                     // File write errror occurred
                     strerror_r( errno, error, SYSLOG_BUF_SIZE );
                     syslog( LOG_ERR, "%s: %s", __func__, error );
                     break;
                  }
                  else
                  {
                     tx_length -= tx_bytes;
                  }
               } 
                              
               // Send the entire file 
               fstat( client_log_file_fd, &file_stat ); 
               tx_length = file_stat.st_size;                 
               //printf( "Files size: %d\n", tx_length );                              
               lseek( client_log_file_fd, 0, SEEK_SET );
               while ( tx_length )
               {
                  tx_bytes = sendfile(
                     client_socket_fd,
                     client_log_file_fd,
                     NULL,
                     tx_length
                     );
                     
                  if ( -1 == tx_bytes )
                  {
                     // Socket errror occurred
                     strerror_r( errno, error, SYSLOG_BUF_SIZE );
                     syslog( LOG_ERR, "%s: %s", __func__, error );
                     break;
                  }
                  else
                  {
                     tx_length -= tx_bytes;
                  }
               }
               lseek( client_log_file_fd, 0, SEEK_END );
               start_packet = end_packet + 1;
            }
            
         } while ( end_packet != NULL );
         
      }
      else if ( -1 == rx_bytes )
      {
         if (   ( errno != EAGAIN      ) 
             && ( errno != EWOULDBLOCK )
            )
         {
            // Socket errror occurred
            strerror_r( errno, error, SYSLOG_BUF_SIZE );
            syslog( LOG_ERR, "%s: %s", __func__, error );
            break;
         }
      }
      else
      {
         // Assume socket is closed
         break;
      }
      
      if ( g_stop_signal )
      {
         break;
      }
      
   }   
       
   close( client_log_file_fd );
    
   return;
}


/*
* Name: setup_signals
*
* Description: Setup signal handlers for:
*                SIGINT
*                SIGTERM
*
* Inputs: None
* *
* Returns: EXIT_SUCCESS - signal handlers configured
*          EXIT_FAILURE - failed to setup signal handlers
*
*/
int setup_signals( void )
{
   int return_status = EXIT_SUCCESS;
   int signal_status;
   int i;
   int signals[] = { SIGINT, SIGTERM };
   char error[ SYSLOG_BUF_SIZE + 1 ];
   struct sigaction program_action = { 0 };
   
   program_action.sa_handler = signal_handler;
   for ( i=0; i < M_ARRAY_SIZE( signals ); i++ )
   {
      signal_status = sigaction( 
         signals[ i ],
         &program_action,
         NULL
         );
      if ( -1 == signal_status )
      {
         strerror_r( errno, error, SYSLOG_BUF_SIZE );
         syslog( LOG_ERR, "%s: %s", __func__, error );
         return_status = EXIT_FAILURE;
      }
   }   
   
   return( return_status );
}

/*
* Name: signal_handler
*
* Description: Signal handler for this program, handles these signals:
*                SIGINT
*                SIGTERM
*              Note: This function is called asynchronously and
*                    must be reentrant.
*
* Inputs: None
* *
* Returns: None
*
*/
void signal_handler( int signal )
{
   switch( signal )
   {
      case SIGINT:
      case SIGTERM:
      {
         g_stop_signal = true;
         break;
      }
      
      default:
         break;
   }
   
   return;
}
