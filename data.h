#ifndef _DATA_H_
#define _DATA_H_

#define DAQ_NAME		"/dev/daq"

/*
 * Solenoid states
 * OFF:   Disengaged
 * On:    Engaged
 * Ready: Waiting for solenoid thread to turn on solenoid
 */
#define OFF			0
#define ON			1
#define READY			2

#define SEC_TO_NSEC		1000000000	/* 10 x 10^-9 */
#define NS_TO_MILS		1000000		/* 10 x 10^-6 */

#define READ_SPRINGS		2
#define WRITE_SOLENOID		2
#define WRITE_LED		1

#define CONFIG_POLL_NS		1000000

#define CONFIG_SPRING_LEFT_DEFAULT	2
#define CONFIG_SPRING_RIGHT_DEFAULT	1
#define CONFIG_SPRING_BOTH_DEFAULT	3
#define CONFIG_SPRING_CENTER_DEFAULT	0

#define STATE_BEGIN		0
#define STATE_LEFT		1
#define STATE_CENTER_FWD	2
#define STATE_RIGHT		3
#define STATE_CENTER_BCK	4

#define MAGIC_ASCII		97

#define NONE			0
#define FORWARD			1
#define BACKWARD		2

#define CHAR_SCREEN_MAX		8
#define CHAR_PIECES_MAX		7

#define MSG_SIZE		1024


#define THREADS_TOTAL		4
#define TID_SENSOR		0
#define TID_SOLENOID		1
#define TID_LED			2
#define TID_IO			3


/*
 * Time, in MILLISECONDS, to delay the solenoid pull
 * relative to the right spring contact
 */
//#define SOLENOID_DELAY_PULL	125
#define SOLENOID_DELAY_PULL	123
/*
 * Time, in MILLISECONDS, to keep the solenoid on
 */
//#define SOLENOID_DELAY_ON	45
#define SOLENOID_DELAY_ON	44


#define PERIOD_MEASURED		382000000

char print_buffer[4096*32];
unsigned long print_len=0;
int gfd=0;					/* device file descriptor */
char msg[MSG_SIZE];
char msg_next[MSG_SIZE];
char msg_next_flag=0;
int msg_len = 20;
pthread_mutex_t lock_msg_next;

int CONFIG_SPRING_LEFT 		= CONFIG_SPRING_LEFT_DEFAULT;
int CONFIG_SPRING_RIGHT		= CONFIG_SPRING_RIGHT_DEFAULT;
int CONFIG_SPRING_BOTH 		= CONFIG_SPRING_BOTH_DEFAULT;
int CONFIG_SPRING_CENTER	= CONFIG_SPRING_CENTER_DEFAULT;

unsigned char letters[27][7] = {
 /* a */ {63, 72, 136, 136, 72, 63, 0},     
 /* b */ {255, 145, 145, 145, 106, 4, 0},   
 /* c */ {60, 66, 129, 129, 66, 102, 0},    
 /* d */ {255, 129, 129, 66, 102, 60, 0},   
 /* e */ {255, 145, 145, 145, 145, 145, 0}, 
 /* f */ {255, 144, 144, 144, 144, 144, 0}, 
 /* g */ {126, 129, 129, 137, 137, 142, 0}, 
 /* h */ {255, 16, 16, 16, 16, 255, 0},
 /* i */ {129, 129, 255, 129, 129, 129, 0},
 /* j */ {129, 129, 129, 255, 128, 128, 0},
 /* k */ {255, 0, 8, 36, 66, 129, 0},
 /* l */ {255, 1, 1, 1, 1, 1, 0},
 /* m */ {255, 64, 32, 32, 64, 255, 0},
 /* n */ {255, 64, 32, 16, 8, 255, 0},
 /* o */ {126, 129, 129, 129, 129, 126, 0},
 /* p */ {255, 144, 144, 144, 144, 96, 0},
 /* q */ {126, 129, 129, 133, 130, 125, 0},
 /* r */ {255, 144, 152, 148, 146, 97, 0},
 /* s */ {96, 145, 145, 145, 145, 142, 0},
 /* t */ {128, 128, 255, 128, 128, 128, 0},
 /* u */ {254, 1, 1, 1 , 1, 254, 0},
 /* v */ {252, 2, 1, 1, 2, 252, 0},
 /* w */ {255, 2, 4, 4, 2, 255, 0},
 /* x */ {195, 36, 24, 24, 36, 195, 0},
 /* y */ {192, 32, 31, 16, 32, 192, 0},
 /* z */ {131, 133, 137, 145, 161, 193, 0}
};


extern void timespec_add_ns(struct timespec *t, unsigned long long ns);

typedef struct gizmo_struct_t {
	int start;		// signal for threads to start running meaningful code
	char period_ended;
	char display_done;

	int state;
	int direction;
	int sensor_count_r;

	struct timespec epoch;	// reference start time for every task
	struct timespec sensor_contact_time_r;

	int solenoid_state;
	int solenoid_count;
	int period;

	int fd;

	pthread_mutex_t lock_scount;
	pthread_mutex_t lock_io;
	pthread_mutex_t lock_start;
	pthread_mutex_t lock_solenoid;

	pthread_cond_t cond_solenoid;
	pthread_cond_t cond_start;

	void (*init) (struct gizmo_struct_t *);
	void (*destroy) (struct gizmo_struct_t *);

	int (*read) (struct gizmo_struct_t *, void *, int);
	int (*write) (struct gizmo_struct_t *, void *, int);
	int (*write_letter) (struct gizmo_struct_t *, char, int);
	

	int (*jumpstart) (struct gizmo_struct_t *);
} gizmo_t;

void gizmo_destroy(struct gizmo_struct_t *g) {
	/* 
 	 * RTFM: destroying while locked is bad
 	 * consider unlocking here first??
 	 */
	pthread_mutex_destroy(&(g->lock_scount));
	pthread_mutex_destroy(&(g->lock_io));
	pthread_mutex_destroy(&(g->lock_start));
	pthread_mutex_destroy(&(g->lock_solenoid));

	pthread_cond_destroy(&(g->cond_solenoid));
}

int gizmo_read(struct gizmo_struct_t *g, void *data, int count) {
	int ret=0;
	
	pthread_mutex_lock(&(g->lock_io));
	  if ((ret = read(g->fd, data, count)) != count) {
		perror("read failed!");
		ret = -1;
	  }
	pthread_mutex_unlock(&(g->lock_io));
	return ret;
}

int gizmo_write(struct gizmo_struct_t *g, void *data, int count) {
	int ret=0;
	
	pthread_mutex_lock(&(g->lock_io));
	  if ((ret = write(g->fd, data, count)) != count) {
		perror("write failed!");
		ret = -1;
	  }
	pthread_mutex_unlock(&(g->lock_io));
	return ret;
}

int gizmo_write_letter(struct gizmo_struct_t *g, char c, int piece) {
	int ret=0;
	unsigned char data[5];
	memset(data, 0, 5);

	if (c == ' ' || c == '\0')
		data[0] = 0;
	else
		data[0] = letters[(int)c - MAGIC_ASCII][piece];
	ret = g->write(g, data, WRITE_LED);
	return ret;
}


int gizmo_jumpstart(struct gizmo_struct_t *g) {
	struct timespec timeout;
	int init_solenoid_on = 0, spring, ret;
	unsigned long init_count=0, init_right_count=0;
	unsigned char data[4];

	memset(data, 0, 4);

	safe_clock_gettime(CLOCK_REALTIME, &timeout);
	while (1) {

		if (!init_solenoid_on && init_count >= 100) {	// wait 100 ms to turn solenoid back on, let it "whip" some
			data[0] = 5;
			g->write(g, data, WRITE_SOLENOID);
			init_solenoid_on = 1;
		}

		if (init_solenoid_on && init_count >= 400) {	// turn the solenoid off after 300 ms
			data[0] = 0;
			g->write(g, data, WRITE_SOLENOID);
			init_solenoid_on = 0;
			init_count = 0;
		}

		g->read(g, data, READ_SPRINGS);
		spring = data[0];

		if (spring == CONFIG_SPRING_RIGHT) {		
			init_right_count++;
			if (init_right_count >= 200) {		// when the blade touches the right sensor x times consider it started
				data[0] = 0;
				g->write(g, data, WRITE_SOLENOID);
				break;
			}
		}

		init_count++;
		timespec_add_ns(&timeout, CONFIG_POLL_NS);
		if ((ret = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &timeout, NULL)) < 0) {
			perror("clock_nanosleep failed");
			return -1;
		}
	}
}

void gizmo_init(struct gizmo_struct_t *g) {
	g->start = 0;
	g->period_ended=0;
	g->display_done = 0;
	g->state = STATE_RIGHT;
	g->direction = BACKWARD;
	g->sensor_count_r = 0;
	g->solenoid_state = OFF;
	g->solenoid_count = 0;
	g->fd = -1;
	g->period = PERIOD_MEASURED;

	pthread_mutex_init(&(g->lock_scount), NULL);
	pthread_mutex_init(&(g->lock_io), NULL);
	pthread_mutex_init(&(g->lock_solenoid), NULL);
	pthread_mutex_init(&(g->lock_start), NULL);

	pthread_cond_init(&(g->cond_solenoid), NULL);
	pthread_cond_init(&(g->cond_start), NULL);

	g->read = &(gizmo_read);
	g->write = &(gizmo_write);
	g->write_letter = &(gizmo_write_letter);
	g->jumpstart = &(gizmo_jumpstart);
	g->destroy = &(gizmo_destroy);
}

#endif
