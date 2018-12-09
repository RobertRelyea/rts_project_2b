/*
 * pwm.h
 *
 *  Created on: Dec 8, 2018
 *      Author: sxs9479
 */

#ifndef PWM_H_
#define PWM_H_

#include <stdlib.h>
#include <stdio.h>
#include <sys/neutrino.h>	// for timers
#include <sys/netmgr.h>		// defines ND_LOCAL_NODE
#include <pthread.h>
#include <stdint.h>       /* for uintptr_t */
#include <hw/inout.h>     /* for in*() and out*() functions */
#include <sys/mman.h>     /* for mmap_device_io() */
#include <termios.h>
#include <unistd.h>

#define BASE_ADDRESS (0x280)

// PWM Thread arguments
struct pwm_args_s
{
	int channel_id;
};

int pwm_init();
void pwm_destroy();
void set_pwm(int channel, int duty_cycle);

#endif /* PWM_H_ */
