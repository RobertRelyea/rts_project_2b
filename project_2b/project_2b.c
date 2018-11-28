// Timer demo
// L. Kiser
// April 17, 2018

#include <stdlib.h>
#include <stdio.h>
#include <sys/neutrino.h>	// for timers
#include <sys/netmgr.h>		// defines ND_LOCAL_NODE
#include <pthread.h>
#include <stdint.h>       /* for uintptr_t */
#include <hw/inout.h>     /* for in*() and out*() functions */
#include <sys/mman.h>     /* for mmap_device_io() */
#include <termios.h>

// PWM Thread
pthread_t pwm_thread;

// PWM Thread arguments
struct pwm_args_s
{
	int channel_id;
};

struct pwm_args_s pwm_args;

//Define Global Variable

int duty_cycle = 50;

//Shared mutexes
pthread_mutex_t duty_cycle_locker;

#define BASE_ADDRESS (0x280)

static uintptr_t d_i_o_port_a_handle ;	// digital I/O port handle

// This function changes the QNX system clock tick size to just 100 microseconds.
static void set_system_clock_period()
{
    struct _clockperiod clkper;
    // set clock period to 100 microseconds.
    clkper.nsec       = 100000;		// 100K nanoseconds is 100 microseconds.
    clkper.fract      = 0;
    ClockPeriod ( CLOCK_REALTIME, &clkper, NULL, 0  );   // set it now.
}

// Sets ALL digital I/O ports to be output ports and get a handle to Digital I/O Port A
static void setup_dio()
{
	//Digital I/O and D/A Control register at offset 11
	uintptr_t d_i_o_control_handle = mmap_device_io( 1, BASE_ADDRESS +0xb ) ;
	out8( d_i_o_control_handle, 0 ) ;	// sets the the DIR (direction bits) to outputs.
	d_i_o_port_a_handle = mmap_device_io( 1, BASE_ADDRESS + 0x8 ) ;	// Digital I/O Port A
}

// This creates a channel that is returned via the ptr_channel_id parameter.
// We use this channel when we wait for a pulse from the timer.
// Note that this returns the channel ID created by this function.
static timer_t create_pulse_timer( int *ptr_channel_id )
{
    timer_t timer_id = 0 ;
 	int pulse_id = 1234 ;	// make permanent storage to be safe
    struct sigevent event;

    *ptr_channel_id = ChannelCreate( 0 ) ;

    // from QNX example code in Programmers Guide
    // Set up the timer and timer event.
    event.sigev_notify            = SIGEV_PULSE;
    event.sigev_coid              = ConnectAttach ( ND_LOCAL_NODE, 0, *ptr_channel_id, 0, 0 );
    event.sigev_priority          = getprio(0) ;		// use our current priority.
    event.sigev_code              = 1023;			// arbitrary number to identify this pulse source
    event.sigev_value.sival_int   = pulse_id ;	// value to pass with the pulse.

    timer_create(  CLOCK_REALTIME, &event, &timer_id ) ;	// create but do not start the timer.
    return timer_id ;
}

// Starts a periodic or one time timer depending on the values we use.
void start_timer( time_t timer_id, int timeOutSec, int timeOutNsec, int periodSec, int periodNsec)
{
	struct itimerspec timer_spec;

	timer_spec.it_value.tv_sec = timeOutSec;
	timer_spec.it_value.tv_nsec = timeOutNsec;
	timer_spec.it_interval.tv_sec = periodSec;
	timer_spec.it_interval.tv_nsec = periodNsec;
	timer_settime( timer_id, 0, &timer_spec, NULL );
}

// Loop forever getting a pulse based on what we set for the timer.
// This demonstrates a crude form of PWM output.
// Takes duty cycle in the form of an integer ranging between 0 and 100
void *timer_demo(void *arg)
{
	struct pwm_args_s *my_args;
	my_args = (struct pwm_args_s *) arg;

	int channel_id = my_args->channel_id;

	struct _pulse pulse ;

	int change_count = duty_cycle * 2;
	static unsigned int counter = 0 ;

	while ( 1 )
	{
		// The idea here is that we set up the timer to fire once every 100 microseconds (the fastest
		// we can go due to the system clock resolution). We then control the counting of the number
		// of pulses that have occurred (count the number of cycles on and the number off)
		MsgReceivePulse ( channel_id, &pulse, sizeof( pulse ), NULL );	// block until that timer fires
		//printf( "Got pulse number %d and got value %d\n", ++count, pulse.value.sival_int) ;
		if ( counter++ > 200 )	// wait for 20 ms for the period
		{
			counter = 0 ;
			out8( d_i_o_port_a_handle, 0xff ) ;		// Sets all the A digital output lines to 1
		}
		else
		{

			if ( counter > change_count )		// set output high for the duty cycle.
				out8( d_i_o_port_a_handle, 0 ) ;// Sets all of the A digital output lines to 0
		}
	}
}


int main(int argc, char *argv[])

{
	// Initialize mutexes
	pthread_mutex_init(&duty_cycle_locker,NULL);
	timer_t timer_id = (timer_t)0 ;
	int privity_err ;
	pthread_mutex_lock(&duty_cycle_locker);
	duty_cycle = 40;
	pthread_mutex_unlock(&duty_cycle_locker);

	/* Give this thread root permissions to access the hardware */
	privity_err = ThreadCtl( _NTO_TCTL_IO, NULL );
	if ( privity_err == -1 )
	{
		fprintf( stderr, "can't get root permissions\n" );
		return -1 ;
	}
	set_system_clock_period() ;		// set to 100 microseconds clock tick.
	setup_dio() ;

	// create the pulse timer
	printf("Setting channel id\n");
	timer_id = create_pulse_timer( &pwm_args.channel_id ) ;
	printf("Done setting channel id\n");

	unsigned int time_value = 100000;
//	printf( "\nSuggested value is 100000 to pulse every 100 microseconds\n" ) ;
//	printf( "\nEnter number of nanoseconds for the base counter: " ) ;
//	scanf( "%u", &time_value ) ;
	// Now set up the timer
	start_timer( timer_id, 0, time_value, 0, time_value ) ;
	printf("Creating pwm thread\n");
	pthread_create(&pwm_thread, NULL, timer_demo, (void*) &pwm_args);
	printf("Created pwm thread\n");

	while(1);

	return 0;
}
