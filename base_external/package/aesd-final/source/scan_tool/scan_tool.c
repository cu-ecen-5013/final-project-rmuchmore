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

#define LINE_LENGTH 20

#define MODE_SHOW_CURRENT_DATA      1
#define MODE_REQUEST_VEHICLE_INFO   9

#define MODE_RESPONSE            0x40

#define PID_ENGINE_RPM             12
#define PID_VEHICLE_SPEED          13
#define PID_AMBIENT_AIR_TEMP       70
#define PID_ODOMETER              166

#define KMPH_TO_MPH       0.62137119f

typedef struct obd2_message
{
   uint32_t id;
   uint8_t  num_bytes;
   uint8_t  mode;
   uint8_t  pid;
   uint8_t  data[ 4 ];
   uint8_t  unused;
} obd2_message;

#define MAIN_MENU_ENGINE_RPM       '1'
#define MAIN_MENU_VEHICLE_SPEED    '2'
#define MAIN_MENU_AMBIENT_AIR_TEMP '3'
#define MAIN_MENU_ODOMETER         '4'
#define MAIN_MENU_EXIT             '0'

// File data and functions
const char main_menu[] = "\nMain Menu\n"
                         "1 - Engine RPM\n"
                         "2 - Vehicle Speed\n"
                         "3 - Ambient Air Temperature\n"
                         "4 - Odometer\n"
                         "0 - Exit\n";

static bool g_stop_signal = false;
static char client_log_file[] = "/var/tmp/aesdscantool";
static uint8_t client_buffer[ 2* MAX_CLIENT_CONNECTIONS ][ CLIENT_LOG_BUF_SIZE ];

static uint32_t vehicle_id   = 0x000007EF;
static uint32_t scan_tool_id = 0x000007DF;

static int run_menu( void );
static int create_socket( int *socket_fd );
static uint8_t get_menu_input( void );
static void send_obd2_request( int socket_fd, obd2_message* obd2_request );
static int recive_obd2_response( int socket_fd, obd2_message* obd2_response );
static void handle_obd2_engine_rpm( obd2_message* obd2_msg );
static void handle_obd2_vehicle_speed( obd2_message* obd2_msg );
static void handle_obd2_ambient_air_temp( obd2_message* obd2_msg );
static void handle_obd2_odometer( obd2_message* obd2_msg );

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
   char program[ SYSLOG_BUF_SIZE+1 ];

   // Use program name as identifier for system log entries:
   //   /var/log/syslog
   sprintf( program, "%.*s", SYSLOG_BUF_SIZE, argv[ 0 ] ); 
   openlog( program, LOG_NDELAY, LOG_USER );
   syslog( LOG_INFO, "Started as PID: %d", getpid() );
   
   setup_signals();
   run_menu();

   closelog();   
   return( return_status );
}


/*
* Name: run_menu
*
* Description: 
*
* Inputs: 
* 
* Returns: EXIT_SUCCESS
*          EXIT_FAILURE
*
*/
int run_menu( void )
{
   int return_status = EXIT_SUCCESS;
   int socket_status;
   char error[ SYSLOG_BUF_SIZE + 1 ];
   int socket_fd;
   obd2_message obd2_request = { 0 };
   obd2_message obd2_response = { 0 };
   uint8_t selection;
   int i = 0;

   socket_status = create_socket( &socket_fd );

   for(;;)
   {
      if ( socket_status == EXIT_FAILURE )
      {
         // Try to re-open the connection
         socket_status = create_socket( &socket_fd );
      }
      else
      {   
         printf( main_menu );
         selection = get_menu_input();

         switch( selection )
         {
            case MAIN_MENU_ENGINE_RPM:
            {
               i = 0;
               obd2_request.id = scan_tool_id;
               obd2_request.num_bytes = 2;
               obd2_request.mode = MODE_SHOW_CURRENT_DATA;
               obd2_request.pid = PID_ENGINE_RPM;
               obd2_request.data[ i++ ] = 0;
               obd2_request.data[ i++ ] = 0;
               obd2_request.data[ i++ ] = 0;
               obd2_request.data[ i++ ] = 0;

               printf( "Send RPM Request\n" );
               send_obd2_request( socket_fd, &obd2_request );
               socket_status = recive_obd2_response( socket_fd, &obd2_response );
               handle_obd2_engine_rpm( &obd2_response );

               break;
            }
            case MAIN_MENU_VEHICLE_SPEED:
            {
               i = 0;
               obd2_request.id = scan_tool_id;
               obd2_request.num_bytes = 2;
               obd2_request.mode = MODE_SHOW_CURRENT_DATA;
               obd2_request.pid = PID_VEHICLE_SPEED;
               obd2_request.data[ i++ ] = 0;
               obd2_request.data[ i++ ] = 0;
               obd2_request.data[ i++ ] = 0;
               obd2_request.data[ i++ ] = 0;

               printf( "Send Speed Request\n" );
               send_obd2_request( socket_fd, &obd2_request );
               socket_status = recive_obd2_response( socket_fd, &obd2_response );
               handle_obd2_vehicle_speed( &obd2_response );

               break;
            }
            case MAIN_MENU_AMBIENT_AIR_TEMP:
            {
               i = 0;
               obd2_request.id = scan_tool_id;
               obd2_request.num_bytes = 2;
               obd2_request.mode = MODE_SHOW_CURRENT_DATA;
               obd2_request.pid = PID_AMBIENT_AIR_TEMP;
               obd2_request.data[ i++ ] = 0;
               obd2_request.data[ i++ ] = 0;
               obd2_request.data[ i++ ] = 0;
               obd2_request.data[ i++ ] = 0;

               printf( "Send RPM Request\n" );
               send_obd2_request( socket_fd, &obd2_request );
               socket_status = recive_obd2_response( socket_fd, &obd2_response );
               handle_obd2_ambient_air_temp( &obd2_response );

               break;
            }
            case MAIN_MENU_ODOMETER:
            {
               i = 0;
               obd2_request.id = scan_tool_id;
               obd2_request.num_bytes = 2;
               obd2_request.mode = MODE_SHOW_CURRENT_DATA;
               obd2_request.pid = PID_ODOMETER;
               obd2_request.data[ i++ ] = 0;
               obd2_request.data[ i++ ] = 0;
               obd2_request.data[ i++ ] = 0;
               obd2_request.data[ i++ ] = 0;

               printf( "Send Speed Request\n" );
               send_obd2_request( socket_fd, &obd2_request );
               socket_status = recive_obd2_response( socket_fd, &obd2_response );
               handle_obd2_odometer( &obd2_response );

               break;
            }
            case MAIN_MENU_EXIT:
            {
               g_stop_signal = 1;
               break;
            }

            default:
            {
               break;
            }
         }
      }
      
      if ( g_stop_signal )
      {
         syslog( LOG_INFO, "%s: %s", __func__, "Caught signal, exiting" );
         break;
      }
    
   }
   
   close( socket_fd );
   remove( client_log_file );

   return( return_status );
}


/*
* Name: create_socket
*
* Description: Create and connect a socket.
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
            status = connect(
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
* Name: get_menu_input
*
* Description: Get user input
*
* Inputs: None
*
* Returns: First byte of user input
*
*/
uint8_t get_menu_input( void )
{
   int byte;
   int i = 0;
   uint8_t selection = 0xff;
   uint8_t buffer[ LINE_LENGTH ];
   
   do
   {
     byte = getchar();
     
     if ( byte != EOF )
     {
        if ( i >= LINE_LENGTH )
        {
           i = 0;
        }
        buffer[ i ] = (uint8_t) byte;
        
        if ( buffer[ i ] == '\n' )
        {
           selection = buffer[ 0 ];
           break;
        }
        i++;
     }
     
   } while ( byte != EOF );
   
   return( selection );
}


/*
* Name: send_obd2_request
*
* Description: Send an OBD2 response
*
* Inputs: obd2_request
*
* Returns: None
*
*/
void send_obd2_request( int socket_fd, obd2_message* obd2_request )
{
   size_t tx_length = sizeof( obd2_message );
   size_t tx_bytes;
   char error[ SYSLOG_BUF_SIZE + 1 ];

   if ( obd2_request != NULL )
   {
      while ( tx_length )
      {
         tx_bytes = send(
            socket_fd,
            obd2_request,
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
* Name: recive_obd2_response
*
* Description: Recieve data over the client socket connection
*
* Inputs: client_socket_fd
*
* Returns: None
*
*/
int recive_obd2_response( int socket_fd, obd2_message* obd2_response )
{
   int return_status = EXIT_SUCCESS;
   int rx_bytes;
   size_t msg_bytes;
   char error[ SYSLOG_BUF_SIZE + 1 ];
   
   msg_bytes = sizeof ( obd2_message );
   
   if ( obd2_response != NULL )
   {
      for(;;)
      {
         rx_bytes = recv(
            socket_fd, 
            client_buffer[ 0 ], 
            CLIENT_LOG_BUF_SIZE, 
            0
            );

         // Assume data is received in the size of a message,
         // this corresponds to a CAN frame.         
         if ( rx_bytes == msg_bytes )
         {
            // Write received data to the end of the file
            //tx_bytes = write(
            //   client_log_file_fd,
            //   client_buffer[ 0 ],
            //   msg_bytes
            //   );

            printf( "Received CAN Response\n" );
            printf( 
               "RX: %02X %02X %02X %02X %02X %02X "
                   "%02X %02X %02X %02X %02X %02X\n",
               client_buffer[ 0 ][  0 ],
               client_buffer[ 0 ][  1 ],
               client_buffer[ 0 ][  2 ],
               client_buffer[ 0 ][  3 ],
               client_buffer[ 0 ][  4 ],
               client_buffer[ 0 ][  5 ],
               client_buffer[ 0 ][  6 ],
               client_buffer[ 0 ][  7 ],
               client_buffer[ 0 ][  8 ],
               client_buffer[ 0 ][  9 ],
               client_buffer[ 0 ][ 10 ],
               client_buffer[ 0 ][ 11 ]
               );
            memcpy( obd2_response, client_buffer[ 0 ], msg_bytes );
            
            break;         
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
            close( socket_fd );
            return_status = EXIT_FAILURE;
            break;
         }      
      }   
   }       
   
   return( return_status );
}


/*
* Name: handle_obd2_engine_rpm
*
* Description: Convert the RPM data to a value
*
* Inputs: obd2_msg
* 
* Returns: None
*
*/
void handle_obd2_engine_rpm( obd2_message* obd2_msg )
{
   static uint32_t rpm = 0;
   int i = 0;
   uint8_t mode = MODE_SHOW_CURRENT_DATA | MODE_RESPONSE;
   
   if ( obd2_msg != NULL )
   {
      //printf( "Handle RPM Response\n" );
      //printf( "Bytes: %u\n", obd2_msg->num_bytes );
      //printf( " Mode: %u\n", obd2_msg->mode );
      //printf( "  PID: %u\n", obd2_msg->pid );
      if (   (              4 == obd2_msg->num_bytes )
          && (           mode == obd2_msg->mode      )
          && ( PID_ENGINE_RPM == obd2_msg->pid       )
         )
      {      
         // RPM = (256 * A + B) / 4
         rpm = ( (uint32_t) obd2_msg->data[ 0 ] ) << 8; 
         rpm += obd2_msg->data[ 1 ];
         rpm = rpm >> 2;
         printf( "Engine: %u rpm\n", rpm );
      }
      else
      {
         printf( "RPM: Invalid Response\n" );
      }
   }  
    
   return;
}


/*
* Name: handle_obd2_vehicle_speed
*
* Description: Convert the speed data to a value
*
* Inputs: obd2_msg
* 
* Returns: None
*
*/
void handle_obd2_vehicle_speed( obd2_message* obd2_msg )
{
   static float speed = 0;
   int i = 0;
   uint8_t mode = MODE_SHOW_CURRENT_DATA | MODE_RESPONSE;
   
   if ( obd2_msg != NULL )
   {
      //printf( "Handle Speed Response\n" );
      //printf( "Bytes: %u\n", obd2_msg->num_bytes );
      //printf( " Mode: %u\n", obd2_msg->mode );
      //printf( "  PID: %u\n", obd2_msg->pid );
      if (   (                 3 == obd2_msg->num_bytes )
          && (              mode == obd2_msg->mode      )
          && ( PID_VEHICLE_SPEED == obd2_msg->pid       )
         )
      {      
         // Speed km/h -> mph
         speed = (float) obd2_msg->data[ 0 ]; 
         speed *= KMPH_TO_MPH;
         printf( "Speed: %.2f mph\n", speed );
      }
      else
      {
         printf( "Speed: Invalid Response\n" );
      }
   }  
    
   return;
}


/*
* Name: handle_obd2_ambient_air_temp
*
* Description: Convert the temperature data to a value
*
* Inputs: obd2_msg
* 
* Returns: None
*
*/
void handle_obd2_ambient_air_temp( obd2_message* obd2_msg )
{
   static float temperature = 0;
   int i = 0;
   uint8_t mode = MODE_SHOW_CURRENT_DATA | MODE_RESPONSE;
   
   if ( obd2_msg != NULL )
   {
      //printf( "Handle Temperature Response\n" );
      //printf( "Bytes: %u\n", obd2_msg->num_bytes );
      //printf( " Mode: %u\n", obd2_msg->mode );
      //printf( "  PID: %u\n", obd2_msg->pid );
      if (   (                    3 == obd2_msg->num_bytes )
          && (                 mode == obd2_msg->mode      )
          && ( PID_AMBIENT_AIR_TEMP == obd2_msg->pid       )
         )
      {      
         // Temperature in C scaled by 40 -> F
         temperature = (float) obd2_msg->data[ 0 ]; 
         temperature = (temperature - 40) * 1.8f + 32.0f;
         printf( "Ambient Air Temp: %.2f F\n", temperature );
      }
      else
      {
         printf( "Temperature: Invalid Response\n" );
      }
   }  
    
   return;
}


/*
* Name: handle_obd2_odometer
*
* Description: Convert the odometer data to a value
*
* Inputs: obd2_msg
* 
* Returns: None
*
*/
void handle_obd2_odometer( obd2_message* obd2_msg )
{
   static uint32_t odometer = 0;
   int i = 0;
   uint8_t mode = MODE_SHOW_CURRENT_DATA | MODE_RESPONSE;
   
   if ( obd2_msg != NULL )
   {
      //printf( "Handle Odometer Response\n" );
      //printf( "Bytes: %u\n", obd2_msg->num_bytes );
      //printf( " Mode: %u\n", obd2_msg->mode );
      //printf( "  PID: %u\n", obd2_msg->pid );
      if (   (            6 == obd2_msg->num_bytes )
          && (         mode == obd2_msg->mode      )
          && ( PID_ODOMETER == obd2_msg->pid       )
         )
      {      
         // RPM = (256 * A + B) / 4
         odometer  = ( (uint32_t) obd2_msg->data[ 0 ] ) << 24; 
         odometer += ( (uint32_t) obd2_msg->data[ 1 ] ) << 16; 
         odometer += ( (uint32_t) obd2_msg->data[ 2 ] ) <<  8; 
         odometer += obd2_msg->data[ 3 ];
         odometer = (uint32_t) ((odometer / 10.0f) * KMPH_TO_MPH);
         printf( "Odometer: %u Miles\n", odometer );
      }
      else
      {
         printf( "Odometer: Invalid Response\n" );
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
