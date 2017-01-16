#include <math.h>
#include "codel_packet_queue.hh"
#include "timestamp.hh"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


using namespace std;

static uint64_t dq_counter = 0;
static uint64_t dq_bytes = 0;
static uint64_t eq_counter = 0;
static uint32_t  _current_qdelay = 0;
//Update Drop Rate 
static void* UpdateDropRate_thread(void* context)
{
	int socketfd = 0;
	int clientfd = 0;
	socklen_t clilen;
	char buffer[1024];

	struct sockaddr_in serv_addr, cli_addr;

	

	socketfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero((char*) &serv_addr, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(4999);
	
	int ret_bind = bind(socketfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));

	int ret_listen = listen(socketfd, 5);
	clilen = sizeof(cli_addr);
	printf("Start to listen %d %d %d\n", socketfd, ret_bind, ret_listen);
	while(true)
	{
		clientfd = accept(socketfd, (struct sockaddr*) &cli_addr, &clilen);
		if(clientfd < 0)
		{
			printf("ERROR on ACCEPT\n");
			return NULL;
		}	
		printf("Accept successfully\n");
		
		while(true)
		{		
			bzero(buffer,1024);
			int n = read(clientfd, buffer, 1023);
			if(n <= 0)
			{
				printf("Client Disappeared... (read) \n");
				close(clientfd);
				break;
			}

			//printf("Read %s\n", buffer);
		
			if(buffer[0] == 'W')
			{
				int a = 0;
				int b = 0;
				int c = 0;
				double d = 0;
				sscanf(buffer, "W %d %d %d %lf", &a, &b, &c, &d);
				
				
			}
			
			if(buffer[0] == 'R')
			{
				sprintf(buffer, "%lu 0 0 0  0 %lu %lu %u %f 0 0\n", eq_counter, dq_bytes, dq_counter, _current_qdelay, 0.0f );
				int ret = write(clientfd,buffer,strlen(buffer));
				if(ret <= 0)
				{
					printf("Client Disappeared... (write) \n");
					close(clientfd);
					break;
				}
			}

		

		}

	}
return context;
}











CODELPacketQueue::CODELPacketQueue( const string & args )
  : DroppingPacketQueue(args),
    target_ ( get_arg( args, "target") ),
    interval_ ( get_arg( args, "interval") ),
    first_above_time_ ( 0 ),
    drop_next_( 0 ),
    count_ ( 0 ),
    lastcount_ ( 0 ),
    dropping_ ( 0 ),
    DP_t( 0 )
{
  if ( target_ == 0 || interval_ == 0 ) {
    throw runtime_error( "CoDel queue must have target and interval arguments." );
  }
}

//NOTE: CoDel makes drop decisions at dequeueing. 
//However, this function cannot return NULL. Therefore we ignore
//the drop decision if the current packet is the only one in the queue.
//We know that if this function is called, there is at least one packet in the queue.
dodequeue_result CODELPacketQueue::dodequeue ( uint64_t now )
{
  uint64_t sojourn_time;

  dodequeue_result r;
  r.p = std::move( DroppingPacketQueue::dequeue () );
  r.ok_to_drop = false;

  if ( empty() ) {
    _current_qdelay = 0;
    first_above_time_ = 0;
    return r;
  }

  sojourn_time = now - r.p.arrival_time;
  _current_qdelay = sojourn_time; 
  
  if ( sojourn_time < target_ || size_bytes() <= PACKET_SIZE ) {
    first_above_time_ = 0;
  }
  else {
    if ( first_above_time_ == 0 ) {
      first_above_time_ = now + interval_;
    }
    else if (now >= first_above_time_) {
      r.ok_to_drop = true;
    }
  }

  return r;
}

uint64_t CODELPacketQueue::control_law ( uint64_t t, uint32_t count ) 
{
  double d = interval_ / sqrt (count);
  return t + (uint64_t) d;
}

QueuedPacket CODELPacketQueue::dequeue( void )
{   
  const uint64_t now = timestamp();
  dodequeue_result r = std::move( dodequeue ( now ) );
  dq_bytes += r.p.contents.size();
  uint32_t delta;
  dq_counter ++;
  if ( dropping_ ) {
    if ( !r.ok_to_drop ) {
      dropping_ = false;
    }

    while ( now >= drop_next_ && dropping_ ) {
      dodequeue_result r = std::move( dodequeue ( now ) );
      count_++;
      if ( ! r.ok_to_drop ) {
	dropping_ = false;
      } else {
	drop_next_ = control_law(drop_next_, count_);
      }
    }
  }
  else if ( r.ok_to_drop ) {
    dodequeue_result r = std::move( dodequeue ( now ) );
    dropping_ = true;
    delta = count_ - lastcount_;
    count_ = ( ( delta > 1 ) && ( now - drop_next_ < 16 * interval_ ))? 
      delta : 1;
    drop_next_ = control_law ( now, count_ );
    lastcount_ = count_;
  }

  return r.p;
}


void CODELPacketQueue::enqueue( QueuedPacket && p )
{
  static int counter = 0;
  eq_counter ++;
  counter++;
  if(counter == 2)
  {
    pthread_create(&(this->DP_t),NULL,&UpdateDropRate_thread,NULL);
    printf("Create Pthread from codel!\n");
  }

  if ( good_with( size_bytes() + p.contents.size(),
		  size_packets() + 1 ) ) {
    accept( std::move( p ) );
  }

  assert( good() );
}
