#define _REENTRANT
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include <pthread.h>
#include <sched.h>
#include <ctype.h>	// tolower()
#include <signal.h>

#include "helper.h"
#include "data.h"

int gfd;

void handle_exit(int signum) {
	unsigned char data[5];
	memset(data, 0, 5);
	if (signum != SIGINT) {
		printf("Error: caught wrong signal %d\n", signum);
		exit(signum);
	}

	data[0] = 0;
	dev_write(gfd, data, WRITE_SOLENOID);	/* turn off the solenoid */
	
	data[0] = 0;
	dev_write(gfd, data, WRITE_LED);	/* turn off the led */
	printf("Exit:\n");
	printf("%s\n", print_buffer);
	exit(signum);
}

void *thread_handle_io( void *ptr ) {
	gizmo_t *g = (gizmo_t *) ptr;
	struct sched_param param;
	struct timespec timeout;
	unsigned char data[4];
	char *msg_tmp;
	size_t msg_size=MSG_SIZE;
	int ret=0, i=0;

	pthread_mutex_lock(&(g->lock_start));
	  while (g->start == 0)
		pthread_cond_wait(&(g->cond_start), &(g->lock_start));
	pthread_mutex_unlock(&(g->lock_start));

	printf("%s() Starting\n", __FUNCTION__);	

	/* Set the priority of the process */
	param.sched_priority = sched_get_priority_min(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &param);

	if ((msg_tmp = (char *)malloc(MSG_SIZE * sizeof(char))) == NULL) {
		printf("malloc() failed!\n");
		exit(1);
	}

	while (1) {
		memset(msg_tmp, 0, MSG_SIZE);
		printf("Input message: ");
		if ((ret = getline(&msg_tmp, &msg_size, stdin)) < 0) {
			printf("Error: getline() failed!!\n");
			exit(1);
		}
		msg_tmp[strlen(msg_tmp)-1] = '\0';
		printf("Message Received: %s\n", msg_tmp);

		if (strlen(msg_tmp) > (MSG_SIZE + CHAR_SCREEN_MAX*4)) {
			printf("Input too long, try again\n");
			continue;
		}

		for (i=0; i < strlen(msg_tmp)-1; ++i)
			msg_tmp[i] = tolower(msg_tmp[i]);

		memset(msg_next, ' ', MSG_SIZE);
		memcpy(msg_next+(CHAR_SCREEN_MAX), msg_tmp, strlen(msg_tmp))-1;
		msg_next[strlen(msg_tmp) + CHAR_SCREEN_MAX*2] = '\0';
		pthread_mutex_lock(&(lock_msg_next));
		  msg_next_flag = 1;
		pthread_mutex_unlock(&(lock_msg_next));
	}
	free(msg_tmp);
}

/*
 * (A.1) Poll the state
 * (A.2) Change the state only when the sensor has been checked
 * x ammount of times
 *
 * (B.1) The blade has touched the right sensor x many times
 * (B.2) Reset the right sensor relative counter
 * (B.3) Log the time that the blade touched the right sensor
 * (B.4) Set the logical condition variable to READY
 * (B.5) Wakeup the solenoid thread.
 */
void *thread_handle_sensor( void *ptr ) {
	gizmo_t *g = (gizmo_t *) ptr;
	struct sched_param param;
	struct timespec timeout;
	unsigned long long period_count=0, slice_count=0;
	int new_state, state_count=0, ret;
	unsigned char data[4];
	double alpha = .5;

	int junk_count=0;
	memset(data, 0, 4);

	pthread_mutex_lock(&(g->lock_start));
	  while (g->start == 0)
		pthread_cond_wait(&(g->cond_start), &(g->lock_start));
	pthread_mutex_unlock(&(g->lock_start));


	printf("%s() Starting\n", __FUNCTION__);	

	/* Set the priority of the process */
	param.sched_priority = sched_get_priority_max(SCHED_FIFO)-1;
	sched_setscheduler(0, SCHED_FIFO, &param);

	timeout = g->epoch;	// keep our own version of the epoch

	for (;;) {
		g->read(g, data, READ_SPRINGS);					// (A.1)

		new_state = state_change(g->state, data[0], &state_count);	// (A.2)

		if (g->state != new_state && new_state == STATE_RIGHT) {	// (B.1)
			pthread_mutex_lock(&g->lock_scount);
			  g->sensor_count_r = 0;				// (B.2)
			pthread_mutex_unlock(&g->lock_scount);
			g->period_ended = 0;

			/*
 			 * We could call get_time here but if this thread woke up
 			 * near the time it ws supposed to then timeout is a good
 			 * estimate.  Consider calling get_time
 			 *
 			 * Log time that the blade touches the right sensor.
 			 * Then wake up the solenoid thread so it can decide what
 			 * to do.
 			 */
			g->sensor_contact_time_r = timeout;			// (B.3)
			pthread_mutex_lock(&(g->lock_solenoid));
			  g->solenoid_state = READY;				// (B.4)
			pthread_mutex_unlock(&(g->lock_solenoid));

			pthread_cond_signal(&(g->cond_solenoid));		// (B.5)

		} else if (g->state != new_state && new_state == STATE_LEFT) {
			// Recalculate period	
			g->period = (unsigned long long)(g->period*(1-alpha)) + (unsigned long long)(((unsigned long long)period_count*(unsigned long)NS_TO_MILS)*alpha);
//			print_len += sprintf(print_buffer+print_len, "period_count: %d\n", period_count);
			period_count=0;
		}
		g->state = new_state;

// State unchanged
		if ((slice_count * NS_TO_MILS) >= g->period) {
			slice_count = 0;
		}
//300
//100
		if (g->sensor_count_r == 300) {
			g->direction = FORWARD;
			g->display_done = 0;
		/*
 		 * A larger BACKWARD SHIFTS the scroll to the left
 		 * A smaller BACKWARD SHIFTS the scroll to the right
 		 */
		} else if (g->sensor_count_r == 100) {	
			g->direction = BACKWARD;
			g->display_done = 0;
		}

		pthread_mutex_lock(&g->lock_scount);
		  g->sensor_count_r++;	// increment right relative counter
		pthread_mutex_unlock(&g->lock_scount);

		slice_count++;		// perfect number of cycles in period
		++period_count;		// actual number of cyles in period
		timespec_add_ns(&timeout, CONFIG_POLL_NS);
		if ((ret = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &timeout, NULL)) < 0) {	/* sleep */
			perror("clock_nanosleep failed");
			exit(0);
		}
	}
}

/*
 * Times to turn on and off the solenoid are relative to the time
 * that the system recognized the blade was touching the right spring.
 * 
 * Consider making the times to turn on and off the solenoid relative to 
 * the current time.
 * Update: Tried the above proposal.  There doesn't seem to be much difference.
 * The only difference is that I have to change the static delay and pull time of the solenoid.
 */
void *thread_handle_solenoid( void *ptr ) {
	gizmo_t *g = (gizmo_t *)ptr;
	struct sched_param param;
	struct timespec timeout;
	unsigned char data[4];
	int ret;
	memset(data, 0, 4);

	pthread_mutex_lock(&(g->lock_start));
	  while (g->start == 0)
		pthread_cond_wait(&(g->cond_start), &(g->lock_start));
	pthread_mutex_unlock(&(g->lock_start));
	
	printf("%s() Starting\n", __FUNCTION__);	

	/* Set the priority of the process */
	param.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &param);

	for (;;) {
		pthread_mutex_lock(&(g->lock_solenoid));
		  while (g->solenoid_state != READY)
			pthread_cond_wait(&(g->cond_solenoid), &(g->lock_solenoid));
		pthread_mutex_unlock(&(g->lock_solenoid));
		
		/*
		 * Sleep until it's time to pull the solenoid
		 */
		timeout = g->sensor_contact_time_r;
		
		timespec_add_ns(&timeout, SOLENOID_DELAY_PULL * NS_TO_MILS);
		if ((ret = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &timeout, NULL)) < 0) {
			perror("clock_nansleep failed");
			exit(0);
		}

		/*
		 * Wakeup and pull the solenoid
		 */

		data[0] = 5;	// 5 volts
		g->write(g, data, WRITE_SOLENOID);
		g->solenoid_state = ON;	// Atmoic operation, no locking required

		/*
		 * Sleep until it's time to turn off the solenoid
		 */

//		safe_clock_gettime(CLOCK_REALTIME, &timeout);
		timespec_add_ns(&timeout, SOLENOID_DELAY_ON * NS_TO_MILS);
		if ((ret = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &timeout, NULL)) < 0) {
			perror("clock_nansleep failed");
			exit(0);
		}

		/*
		 * Wakeup and turn off the solenoid
		 */

		data[0] = 0;
		g->write(g, data, WRITE_SOLENOID);
		g->solenoid_state = OFF;	// Atomic operation, no locking required
	}
}

void *thread_handle_led( void *ptr ) {
	gizmo_t *g = (gizmo_t *)ptr;
	struct sched_param param;
	struct timespec timeout;
	int slice_count=0, direction, ret=0;
	char c, display_done;
	int piece=0, index_rel=0, window_start=0, window_end=CHAR_SCREEN_MAX, piece_real=0;

	pthread_mutex_lock(&(g->lock_start));
	  while (g->start == 0)
		pthread_cond_wait(&(g->cond_start), &(g->lock_start));
	pthread_mutex_unlock(&(g->lock_start));
	
	printf("%s() Starting\n", __FUNCTION__);	

	/* Set the priority of the process */
	param.sched_priority = sched_get_priority_max(SCHED_FIFO) - 1;
	sched_setscheduler(0, SCHED_FIFO, &param);


	timeout = g->epoch;	// keep our own version of the epoch
//	timespec_add_ns(&timeout, CONFIG_POLL_NS);	/* offset the led display and sensor polling */

	strcpy(msg, "          this is a test           ");

	for (;;) {
		direction = g->direction;
		display_done = g->display_done;

		if (display_done == 0) {
			if (direction == BACKWARD) {
				c = msg[window_end - index_rel];
				piece_real = CHAR_PIECES_MAX-1 - piece; 
			} else if (direction == FORWARD) {
				c = msg[window_start + index_rel];
				piece_real = piece;
			}
			g->write_letter(g, c, piece);
			//display_letter(g->fd, c, piece_real);

	//		printf("Character: %c[%d]\n", c, piece);

			if (++piece >= CHAR_PIECES_MAX) {
				++index_rel;
				piece = 0;
			}
			if (index_rel > CHAR_SCREEN_MAX) {
				index_rel = 0;
				g->display_done = 1;
				g->write_letter(g, ' ', 0);
				//display_letter(g->fd, ' ', 0);
				window_start++;
				window_end++;
			}
			if (window_end > strlen(msg)) {
				window_start = 0;
				window_end = CHAR_SCREEN_MAX;
				
				pthread_mutex_lock(&(lock_msg_next));
				  if (msg_next_flag) {
					strcpy(msg, msg_next);
					msg_next_flag = 0;
				  }
				pthread_mutex_unlock(&(lock_msg_next));
			}
		}

		timespec_add_ns(&timeout, CONFIG_POLL_NS*2);
		if ((ret = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &timeout, NULL)) < 0) {
			perror("clock_nansleep failed");
			exit(0);
		}
	}
}

int main(int argc, char *argv[]) {
	struct timespec timeout;
	struct sched_param param;
	int i=0;

	pthread_t thread_id[] = { TID_SENSOR, TID_LED, TID_SOLENOID, TID_IO };
	gizmo_t *gizmo;

//	memset(msg, ' ', MSG_SIZE);

	pthread_mutex_init(&(lock_msg_next), NULL);

	signal(SIGINT, handle_exit);	// handle ctrl+c

	/* setup gizmo data */
	if ((gizmo = (gizmo_t *)malloc(sizeof(gizmo_t))) == NULL) {
		printf("Error in malloc() for gizmo\n");
		exit(0);
	}

	gizmo->init = &gizmo_init;
	gizmo->init(gizmo);
	gfd = gizmo->fd = dev_open(DAQ_NAME);

	pthread_create(&thread_id[TID_SENSOR], NULL, thread_handle_sensor, gizmo);
	pthread_create(&thread_id[TID_SOLENOID], NULL, thread_handle_solenoid, gizmo);
	pthread_create(&thread_id[TID_LED], NULL, thread_handle_led, gizmo);
	pthread_create(&thread_id[TID_IO], NULL, thread_handle_io, gizmo);


	/* Set the priority of the process */
	param.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &param);

//	for (i=0; i < (sizeof(thread_id) / sizeof(pthread_t)); i++)
	for (i=0; i < 4; i++)
		pthread_detach(thread_id[i]);

	gizmo->jumpstart(gizmo);
	/* 
 	 * Grab the clock for the first time,
 	 * This will be the base time for our task
 	 */
	safe_clock_gettime(CLOCK_REALTIME, &gizmo->epoch);	
	pthread_mutex_lock(&(gizmo->lock_start));
	  gizmo->start = 1;
	pthread_mutex_unlock(&(gizmo->lock_start));
	pthread_cond_broadcast(&(gizmo->cond_start));

	while (1)
		sleep(20);

	return 0;
}
