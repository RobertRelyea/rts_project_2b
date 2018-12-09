/*
 * pwm.c
 *
 *  Created on: Dec 8, 2018
 *      Author: sxs9479
 */


#include "pwm.h"

// PWM Thread
pthread_t pwm_thread_handle;
struct pwm_args_s pwm_args;

//Define duty cycle global variables
int duty_cycle_1 = 50;
int duty_cycle_2 = 50;

//Shared duty cycle mutexes
pthread_mutex_t duty_cycle_1_locker;
pthread_mutex_t duty_cycle_2_locker;


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
void *pwm_thread(void *arg)
{
	struct pwm_args_s *my_args;
	my_args = (struct pwm_args_s *) arg;
	unsigned char register_value =0xff;
	int channel_id = my_args->channel_id;
	int change_count_1 =0, change_count_2=0;

	struct _pulse pulse ;

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
			// Update duty cycle
			pthread_mutex_lock(&duty_cycle_1_locker);
			change_count_1 = duty_cycle_1 * 2;
			pthread_mutex_unlock(&duty_cycle_1_locker);
			pthread_mutex_lock(&duty_cycle_2_locker);
			change_count_2 = duty_cycle_2 * 2;
			pthread_mutex_unlock(&duty_cycle_2_locker);
			counter = 0 ;
			register_value = 0xff;
			out8( d_i_o_port_a_handle, register_value ) ;		// Sets all the A digital output lines to 1
		}
		else
		{
			// If counter higher than the needed PWM
			if ( counter > change_count_1)		// set output high for the duty cycle.
			{
				//DI/O pin A0
				register_value &= ~(0x01);
			}
			if ( counter > change_count_2)		// set output high for the duty cycle.
			{
				//DI/O pin A1
				register_value &= ~(0x02);
			}
			out8( d_i_o_port_a_handle, register_value ) ;
		}
	}
}

void set_pwm(int channel, int duty_cycle)
{
	if(channel == 1)
	{
		pthread_mutex_lock(&duty_cycle_1_locker);
		duty_cycle_1 = duty_cycle;
		pthread_mutex_unlock(&duty_cycle_1_locker);
	}
	else if (channel == 2)
	{
		pthread_mutex_lock(&duty_cycle_2_locker);
		duty_cycle_2 = duty_cycle;
		pthread_mutex_unlock(&duty_cycle_2_locker);
	}
}

int pwm_init()
{
	// Initialize mutexes
	pthread_mutex_init(&duty_cycle_1_locker,NULL);
	pthread_mutex_init(&duty_cycle_2_locker,NULL);
	timer_t timer_id = (timer_t)0 ;
	int privity_err ;
	pthread_mutex_lock(&duty_cycle_1_locker);
	duty_cycle_1 = 40;
	duty_cycle_2 = 40;
	pthread_mutex_unlock(&duty_cycle_2_locker);

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
	// Now set up the timer
	start_timer( timer_id, 0, time_value, 0, time_value ) ;
	printf("Creating pwm thread\n");
	pthread_create(&pwm_thread_handle, NULL, pwm_thread, (void*) &pwm_args);
	printf("Created pwm thread\n");
	return 0;
}

// TODO
void pwm_destroy()
{

}
