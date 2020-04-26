/*
* File: vehicle.c
*
* Description: Create a server socket to provide vehicle data.
*              Simulates a vehicle ECU
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
typedef struct obd2_message
{
   uint32_t id;
   uint8_t  num_bytes;
   uint8_t  mode;
   uint8_t  pid;
   uint8_t  data[ 4 ];
   uint8_t  unused;
} obd2_message;

#define M_ARRAY_SIZE( x ) ( sizeof(x) / sizeof(x[0]) )
#define SYSLOG_BUF_SIZE 80
#define MAX_CLIENT_CONNECTIONS 1
#define CLIENT_LOG_BUF_SIZE 4096
#define SERVER_PORT_STRING "9000" 
#define SERVER_PORT 9000

// File data and functions
static bool g_stop_signal = false;
static char client_log_file[] = "/var/tmp/aesdvehicle";
static uint8_t client_buffer[ 2* MAX_CLIENT_CONNECTIONS ][ CLIENT_LOG_BUF_SIZE ];

static uint32_t vehicle_id   = 0x000007EF;
static uint32_t scan_tool_id = 0x000007DF;

static int run_daemon( void );
static int create_socket( int *socket_fd );
static int run_server( int socket_fd );
static void transfer_data( int client_socket_fd );

static void handle_obd2_request( int client_socket_fd, obd2_message* obd2_request );
static void send_obd2_response( int client_socket_fd, obd2_message* obd2_response );
static void handle_obd2_engine_rpm( obd2_message* obd2_msg );

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
   obd2_message obd2_msg;
   size_t msg_bytes;
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
   
   msg_bytes = sizeof ( obd2_msg );
   
   for(;;)
   {
      rx_bytes = recv(
         client_socket_fd, 
         client_buffer[ 0 ], 
         CLIENT_LOG_BUF_SIZE, 
         0
         );

      // Write received data to the end of the file
      tx_bytes = write(
         client_log_file_fd,
         client_buffer,
         msg_bytes
         );

      // Assume data is received in the size of message,
      // this corresponds to a CAN frame.         
      if ( rx_bytes == msg_bytes )
      {
         memcpy( &obd2_msg, client_buffer, msg_bytes );
         
         if ( -1 == tx_bytes )
         {
            // File write errror occurred
            strerror_r( errno, error, SYSLOG_BUF_SIZE );
            syslog( LOG_ERR, "%s: %s", __func__, error );
         }
         
         handle_obd2_request( client_socket_fd, &obd2_msg );
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

#define MODE_SHOW_CURRENT_DATA      1
#define MODE_REQUEST_VEHICLE_INFO   9

#define PID_ENGINE_RPM             12
#define PID_VEHICLE_SPEED          13
#define PID_AMBIENT_AIR_TEMP       70
#define PID_ODOMETER              166

/*
* Name: handle_obd2_request
*
* Description: Handle OBD2 messages.
*              Send response to supported message
*
* Inputs: obd2_request - obd2 message to process
* *
* Returns: None
*
*/
void handle_obd2_request( int client_socket_fd, obd2_message* obd2_request )
{
   obd2_message obd2_response = { 0 };

   obd2_response.id = vehicle_id;
   
   if ( obd2_request != NULL )
   {
      //printf( "Received OBD2 Request: %lu\n",  obd2_request->id );
   
      // Filter message ids
      if ( obd2_request->id == scan_tool_id )
      {
         //printf( "Received Scan Tool Request\n" );
         switch( obd2_request->mode )
         {
            case MODE_SHOW_CURRENT_DATA:
            {
               //printf( "Mode: Current Data\n" );
               switch( obd2_request->pid )
               {
                  case PID_ENGINE_RPM:
                  {
                     printf( "Received RPM Request\n" );
                     handle_obd2_engine_rpm( &obd2_response );
                     obd2_response.pid = obd2_request->pid | 0x40;
                     send_obd2_response( client_socket_fd, &obd2_response );
                     break;
                  }
                  
                  default:
                  {
                     break;
                  }
               }
               break;
            }
            
            default:
            {
               break;
            }
         }
      }
   }
   return;
}


/*
* Name: send_obd2_response
*
* Description: Send an OBD2 response
*
* Inputs: obd2_response
* *
* Returns: None
*
*/
void send_obd2_response( int client_socket_fd, obd2_message* obd2_response )
{
   size_t tx_length = sizeof( obd2_message );
   size_t tx_bytes;
   char error[ SYSLOG_BUF_SIZE + 1 ];

    
   if ( obd2_response != NULL )
   {
      while ( tx_length )
      {
         tx_bytes = send(
            client_socket_fd,
            obd2_response,
            tx_length,
            0
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
   }
   return;
}


/*
* Name: handle_obd2_engine_rpm
*
* Description: Add the RPM data to the input message
*
* Inputs: obd2_msg
* 
* Returns: None
*
*/
void handle_obd2_engine_rpm( obd2_message* obd2_msg )
{
   static uint32_t rpm = 1000;
   int i = 0;
   
   if ( obd2_msg != NULL )
   {
      obd2_msg->num_bytes = 2;
      obd2_msg->data[ i++ ] = (uint8_t) (((rpm * 4) & 0xFF00) >> 8 );
      obd2_msg->data[ i++ ] = (uint8_t)  ((rpm * 4) & 0x00FF);
      obd2_msg->data[ i++ ] = 0;
      obd2_msg->data[ i++ ] = 0;

      rpm += 100;
      if ( rpm > 10000 )
      {
         rpm = 1000;
      }
   }   
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
