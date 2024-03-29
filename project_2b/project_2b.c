#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>

#include "pwm.h"
#include "servo_states.h"
#define BUFFER_SIZE (100)

// Global Variables
events user_events[2];
int user_input_ready=0;

// Semaphores
sem_t input_processed;

//Mutexes
pthread_mutex_t input_ready_locker;
pthread_mutex_t user_events_locker;

//Threads
pthread_t user_input_handle;




// Returns the difference between two timespecs in seconds
double time_elapsed(struct timespec *start, struct timespec *current)
{
	double time_s  = (double)(current->tv_sec) - (double)(start->tv_sec);
	double time_ns = (double)(current->tv_nsec) - (double)(start->tv_nsec);
	return time_s + time_ns*1e-9;
}

events parseInput(char input)
{
	// Handle user input
	if (input == 'C' || input == 'c') // Continue
		return resume;
	else if (input == 'P' || input == 'p') // Pause
		return pause_event;
	else if (input == 'L' || input == 'l') // move_left
		return move_left;
	else if (input == 'R' || input == 'r') // move_right
		return move_right;
	else if (input == 'B' || input == 'b') // begin
		return begin;
	else if (input == 'N' || input == 'n') // nop
		return nop;
    return terminate;
}

// User Input Thread
void *userInput (void *arg)
{
	char input_buffer[BUFFER_SIZE];

	printf("\n >>");

	while(1)
	{
		// Wait until latest input is processed
		sem_wait(&input_processed);

		// Receive user input
		scanf("%s", input_buffer);
		if (strlen(input_buffer) < 2)
		{
			printf("\n >>");
			fflush(stdout);
			sem_post(&input_processed);
			continue;
		}

		// Parse user input
		pthread_mutex_lock(&user_events_locker);
		char first_char = input_buffer[0];
		char second_char = input_buffer[1];
		user_events[0] = parseInput(first_char);
		user_events[1] = parseInput(second_char);
		if(user_events[0] == terminate || user_events[1] == terminate)
		{
			printf("\n >>");
			fflush(stdout);
			sem_post(&input_processed);
		}
		else // Input command fully constructed, wait for CR
		{
			// Determine events input by user
			pthread_mutex_lock(&input_ready_locker);
			user_input_ready = 1;
			pthread_mutex_unlock(&input_ready_locker);

			printf("\n >>");
			fflush(stdout);
		}
		pthread_mutex_unlock(&user_events_locker);
	}
}

int main(int argc, char *argv[])
{
	// Attempt to initialize PWM
	int error = pwm_init();
	if (error) // Exit on failure
		return -1;

	initServos();

	// Initialize mutexes
	pthread_mutex_init(&input_ready_locker,NULL);
	pthread_mutex_init(&user_events_locker,NULL);


	// Initialize Semaphores
	sem_init(&input_processed, 0, 0);
	sem_post(&input_processed);

	//Create threads
	pthread_create(&user_input_handle, NULL,userInput,NULL);

	// Time keeping for servo control loop
	struct timespec start;
	struct timespec current;

	while(1)
	{
		// Keep track of loop iteration time
		clock_gettime(CLOCK_REALTIME, &start);

		// Handle user input
		pthread_mutex_lock(&input_ready_locker);
		if (user_input_ready)
		{
			user_input_ready = 0;
			pthread_mutex_lock(&user_events_locker);
			processInput(user_events[0], user_events[1]);
			pthread_mutex_unlock(&user_events_locker);
			sem_post(&input_processed);
		}
		pthread_mutex_unlock(&input_ready_locker);

		recipeStep();

		clock_gettime(CLOCK_REALTIME, &current);
		double elapsed_time = time_elapsed(&start, &current) * 1000;

		// Sleep for the rest of the loop iteration duration
		usleep((100 - elapsed_time) * 1000);
	}

	return 0;
}
