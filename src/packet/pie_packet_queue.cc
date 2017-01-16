#include <sys/file.h>
#include <chrono>
#include <cmath>
#include "pie_packet_queue.hh"
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

#define DQ_COUNT_INVALID   (uint32_t)-1


//Some Hack Here...
#define DROPRATE_UPDATE_PERIOD 40
#define NN_UPDATE_PERIOD 100
#define DROPRATE_BOUND 0.1

//#define PATH_TO_PYTHON_INTERFACE "/home/rl/Project/rl-qm/mahimahiInterface/"
#define PATH_TO_PYTHON_INTERFACE "/home/songtaohe/Project/QueueManagement/rl-qm/mahimahiInterface/"


double * _drop_prob = NULL;
double rl_drop_prob = 0.0;
unsigned int _size_bytes_queue = 0;
uint32_t  _current_qdelay = 0;
uint32_t  _current_qdelay_perpacket = 0;
uint64_t dq_counter = 0;
uint64_t eq_counter = 0;
uint64_t dq_bytes = 0;
uint64_t eq_bytes = 0;
uint64_t q_empty_time = 0;
int q_empty = 0;
uint64_t q_empty_ts = 0;

uint32_t max_qdelay = 0;
uint32_t min_qdelay = 10000;
uint64_t acc_qdelay = 0;

PIEPacketQueue *this_ = NULL;




int state_rl_enable = 0;







char next_throughput[1024];


char* generate_next_throughput(int count, int interval)
{
	if(this_ == NULL) return NULL;
	std::vector<uint64_t> s = *(this_->schedule);
	unsigned int cur  = this_->link_ptr;
	uint64_t basetime = this_->basetime;
	uint64_t now = s.at(cur);
	uint64_t base = 0;
	uint32_t shiftnow = timestamp();


	//printf("%lu %lu\n",now, (shiftnow - basetime) % s.back());

	now = (shiftnow - basetime) % s.back();
	



	int pc = 0;
	int ic = 0;

	char * ptr = next_throughput;

	while(ic < count)
	{
		//cur = (cur + 1) % s.size();
		//if(cur == 0) base += s.back();

		if ( s.at(cur) + base <= now + interval)
		{
			pc++;
			
			cur = (cur + 1) % s.size();
			if(cur == 0) base += s.back();
		}
		else
		{
			ic++;
			ptr += sprintf(ptr, " %u", pc);
			pc = 0;
			now = now + interval;
		}

		//cur = (cur + 1) % s.size();
		//if(cur == 0) base += s.back();
	}

	return next_throughput;

}


//Update Drop Rate 
void* UpdateDropRate_thread(void* context)
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
				sscanf(buffer, "W %d %d %d %lf", &a, &b, &c, &rl_drop_prob);
				
				state_rl_enable = b;
			}
			
			if(buffer[0] == 'R')
			{
				//sprintf(buffer, "%lu 0 0 0  0 %lu %lu %u %f 0 0\n", eq_counter, dq_bytes, dq_counter, _current_qdelay, *_drop_prob );
				sprintf(buffer, "%lu 0 0 0  0 %lu %lu %u %f 0 0 %lu %lu %u %u %lu %s\n", eq_counter, dq_bytes, dq_counter, _current_qdelay_perpacket, *_drop_prob, eq_bytes, q_empty_time, max_qdelay, min_qdelay, acc_qdelay, generate_next_throughput(8,20));
				int ret = write(clientfd,buffer,strlen(buffer));
				if(ret <= 0)
				{
					printf("Client Disappeared... (write) \n");
					close(clientfd);
					break;
				}


				max_qdelay = 0;
				min_qdelay = 10000;

			}

		

		}

	}
return context;
}




PIEPacketQueue::PIEPacketQueue( const string & args )
  : DroppingPacketQueue(args),
    qdelay_ref_ ( get_arg( args, "qdelay_ref" ) ),
    max_burst_ ( get_arg( args, "max_burst" ) ),
    alpha_ ( 0.125 ),
    beta_ ( 1.25 ),
    t_update_ ( 20 ),
    dq_threshold_ ( 16384 ),
    drop_prob_ ( 0.0 ),
    burst_allowance_ ( 0 ),
    qdelay_old_ ( 0 ),
    current_qdelay_ ( 0 ),
    dq_count_ ( DQ_COUNT_INVALID ),
    dq_tstamp_ ( 0 ),
    avg_dq_rate_ ( 0 ),
    uniform_generator_ ( 0.0, 1.0 ),
    prng_( random_device()() ),
    last_update_( timestamp() ),
		NN_t( 0 ),
		DP_t( 0 )
{
  if ( qdelay_ref_ == 0 || max_burst_ == 0 ) {
    throw runtime_error( "PIE AQM queue must have qdelay_ref and max_burst parameters" );
  }

	
}

void PIEPacketQueue::enqueue( QueuedPacket && p )
{
	static int counter = 0;
	eq_counter ++;
	eq_bytes += p.contents.size();
	counter++;
	this_ = this;
	if(counter == 2)
	{
		pthread_create(&(this->DP_t),NULL,&UpdateDropRate_thread,NULL);
		printf("Create Pthread!\n");
	}
  calculate_drop_prob();
	
  _drop_prob = &(this->drop_prob_);
	//_current_qdelay = &(this->current_qdelay_);
	if ( this->avg_dq_rate_ > 0 )
	{
      _current_qdelay = size_bytes() / this->avg_dq_rate_;

	}
    else
      _current_qdelay = 0;

	



	_size_bytes_queue = size_bytes();

  //printf("%u\n",size_bytes());

  if ( ! good_with( size_bytes() + p.contents.size(),
		    size_packets() + 1 ) ) {
    // Internal queue is full. Packet has to be dropped.
    return;
  } 

  if (!drop_early() ) {
	if(size_packets() == 0 && q_empty == 1)
	{
      q_empty = 0;
      q_empty_time += timestamp() - q_empty_ts;
	  q_empty_ts = timestamp();
	}

    //This is the negation of the pseudo code in the IETF draft.
    //It is used to enqueue rather than drop the packet
    //All other packets are dropped
    accept( std::move( p ) );
  }

  assert( good() );
}

//returns true if packet should be dropped.
bool PIEPacketQueue::drop_early ()
{

  if(state_rl_enable == 0)
  {  
  if ( burst_allowance_ > 0 ) {
    return false;
  }

  if ( qdelay_old_ < qdelay_ref_/2 && drop_prob_ < 0.2) {
  //if ( qdelay_old_ < qdelay_ref_/2 && rl_drop_prob < 0.2) {
    return false;        
  }

  if ( size_bytes() < (2 * PACKET_SIZE) ) {
    return false;
  }
  }
  



  double random = uniform_generator_(prng_);

  if(state_rl_enable == 1)
  {
    drop_prob_ = rl_drop_prob;
  }

  if ( random < drop_prob_ ) {
  //if ( random < rl_drop_prob ) {
    return true;
  }
  else
    return false;
}

QueuedPacket PIEPacketQueue::dequeue( void )
{
  QueuedPacket ret = std::move( DroppingPacketQueue::dequeue () );
  uint32_t now = timestamp();

  if(size_packets() == 0 && q_empty == 0)
  {
    q_empty_ts = now;
    q_empty = 1;
  }

  _current_qdelay_perpacket = now - ret.arrival_time;

  if(_current_qdelay_perpacket > max_qdelay) max_qdelay = _current_qdelay_perpacket;
  if(_current_qdelay_perpacket < min_qdelay) min_qdelay = _current_qdelay_perpacket;
  acc_qdelay += _current_qdelay_perpacket;




  dq_counter ++;
  dq_bytes += ret.contents.size();

  if ( size_bytes() >= dq_threshold_ && dq_count_ == DQ_COUNT_INVALID ) {
    dq_tstamp_ = now;
    dq_count_ = 0;
  }

  if ( dq_count_ != DQ_COUNT_INVALID ) {
    dq_count_ += ret.contents.size();

    if ( dq_count_ > dq_threshold_ ) {
      uint32_t dtime = now - dq_tstamp_;

      if ( dtime > 0 ) {
	uint32_t rate_sample = dq_count_ / dtime;
	if ( avg_dq_rate_ == 0 ) 
	  avg_dq_rate_ = rate_sample;
	else
	  avg_dq_rate_ = ( avg_dq_rate_ - (avg_dq_rate_ >> 3 )) +
		     (rate_sample >> 3);
                
	if ( size_bytes() < dq_threshold_ ) {
	  dq_count_ = DQ_COUNT_INVALID;
	}
	else {
	  dq_count_ = 0;
	  dq_tstamp_ = now;
	} 

	if ( burst_allowance_ > 0 ) {
	  if ( burst_allowance_ > dtime )
	    burst_allowance_ -= dtime;
	  else
	    burst_allowance_ = 0;
	}
      }
    }
  }

  calculate_drop_prob();

  return ret;
}

void PIEPacketQueue::calculate_drop_prob( void )
{
  uint64_t now = timestamp();
	
  //We can't have a fork inside the mahimahi shell so we simulate
  //the periodic drop probability calculation here by repeating it for the
  //number of periods missed since the last update. 
  //In the interval [last_update_, now] no change occured in queue occupancy 
  //so when this value is used (at enqueue) it will be identical
  //to that of a timer-based drop probability calculation.
  while (now - last_update_ > t_update_) {
    bool update_prob = true;
    qdelay_old_ = current_qdelay_;

    if ( avg_dq_rate_ > 0 ) 
      current_qdelay_ = size_bytes() / avg_dq_rate_;
    else
      current_qdelay_ = 0;

    if ( current_qdelay_ == 0 && size_bytes() != 0 ) {
      update_prob = false;
    }

    double p = (alpha_ * (int)(current_qdelay_ - qdelay_ref_) ) +
      ( beta_ * (int)(current_qdelay_ - qdelay_old_) );

    if ( drop_prob_ < 0.01 ) {
      p /= 128;
    } else if ( drop_prob_ < 0.1 ) {
      p /= 32;
    } else  {
      p /= 16;
    } 

    drop_prob_ += p;

    if ( drop_prob_ < 0 ) {
      drop_prob_ = 0;
    }
    else if ( drop_prob_ > 1 ) {
      drop_prob_ = 1;
      update_prob = false;
    }

        
    if ( current_qdelay_ == 0 && qdelay_old_==0 && update_prob) {
      drop_prob_ *= 0.98;
    }
        
    burst_allowance_ = max( 0, (int) burst_allowance_ -  (int)t_update_ );
    last_update_ += t_update_;

    if ( ( drop_prob_ == 0 )
	 && ( current_qdelay_ < qdelay_ref_/2 ) 
	 && ( qdelay_old_ < qdelay_ref_/2 ) 
	 && ( avg_dq_rate_ > 0 ) ) {
      dq_count_ = DQ_COUNT_INVALID;
      avg_dq_rate_ = 0;
      burst_allowance_ = max_burst_;
    }

  }
}
