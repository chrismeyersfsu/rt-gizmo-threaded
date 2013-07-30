#ifndef _HELPER_H_
#define _HELPER_H_

#include "data.h"



int safe_clock_gettime(clockid_t clk_id, struct timespec *tp) {
	int ret=0;
	if ((ret = clock_gettime(clk_id, tp)) < 0) {
		perror("clock_gettime failed");
		ret = -1;
		exit(0);
	}
	return ret;
}

void timespec_add_ns(struct timespec *t, unsigned long long ns) {
	int tmp = ns / 1000000000;
	t->tv_sec += tmp;
	ns -= tmp * 1000000000;
	t->tv_nsec += ns;

	tmp = t->tv_nsec / 1000000000;
	t->tv_sec += tmp;
	t->tv_nsec -= tmp * 1000000000;
}

struct timespec timespec_sub(struct timespec time1, struct timespec time2) {
	struct timespec result;

/* Subtract the second time from the first. */

    if ((time1.tv_sec < time2.tv_sec) ||
        ((time1.tv_sec == time2.tv_sec) &&
         (time1.tv_nsec <= time2.tv_nsec))) {		/* TIME1 <= TIME2? */
        result.tv_sec = result.tv_nsec = 0 ;
    } else {						/* TIME1 > TIME2 */
        result.tv_sec = time1.tv_sec - time2.tv_sec ;
        if (time1.tv_nsec < time2.tv_nsec) {
            result.tv_nsec = time1.tv_nsec + 1000000000L - time2.tv_nsec ;
            result.tv_sec-- ;				/* Borrow a second. */
        } else {
            result.tv_nsec = time1.tv_nsec - time2.tv_nsec ;
        }
    }

    return (result);
}

struct timespec timespec_add(struct timespec time1, struct timespec time2) {
    struct timespec result;

    result.tv_sec = time1.tv_sec + time2.tv_sec ;
    result.tv_nsec = time1.tv_nsec + time2.tv_nsec ;
    if (result.tv_nsec > 1000000000L) {			/* Carry? */
        result.tv_sec++ ;  result.tv_nsec = result.tv_nsec - 1000000000L ;
    }

    return (result) ;
}

struct timespec timespec_div(struct timespec result,int divisor)
{
	unsigned long	temp;
	struct timespec ret;
	temp = result.tv_sec % divisor;
	ret.tv_sec /= divisor;
	ret.tv_nsec /= divisor;
	ret.tv_nsec += temp * (SEC_TO_NSEC / divisor);
	return ret;
}

int timespec_cmp(struct timespec *t1, struct timespec *t2, const char *op) {
	int ret=0;

	if (memcmp(op, "<", 1) == 0) {
		if (t1->tv_sec < t2->tv_sec) {
			ret = 1;
		} else if (t1->tv_sec > t2->tv_sec) {
			ret = 0;
		} else {				/* equal, so check the ns */
			if (t1->tv_nsec < t2->tv_nsec) {
				ret = 1;
				goto out;
			} else {
				ret = 0;
				goto out;
			}
		}
	} else if (memcmp(op, ">", 1) == 0) {
		if (t1->tv_sec > t2->tv_sec) {
			ret = 1;
		} else if (t1->tv_sec < t2->tv_sec) {
			ret = 0;
		} else {				/* equal, so check the ns */
			if (t1->tv_nsec > t2->tv_nsec) {
				ret = 1;
				goto out;
			} else {			/* less than or equal to */
				ret = 0;
				goto out;
			}
		}
	} else {
		printf("%s %s() Error: unsupported comparative operation\n", __FILE__, __FUNCTION__);
		ret = -1;
	}
out:
	return ret;
}

unsigned long long timespec_to_llu(struct timespec *t) {
	unsigned long long ret=0;
	ret += (t->tv_sec * SEC_TO_NSEC);
	ret += t->tv_nsec;
	return ret;
}

int dev_open(char *dev_name) {
	int ret=0;
	if ((ret = open(dev_name, O_RDWR)) < 0) {
		perror ("open device failed");
		ret = -1;
		exit(0);
	}
	return ret;
}

int dev_read(int fd, void *data, int count) {
	int ret=0;
	if ((ret = read(fd, data, count)) != count) {
		perror ("read failed!");
		ret = -1;
		exit(0);
	}
	return ret;
}

int dev_write(int fd, void *data, int count) {
	int ret=0;
	if ((write (fd, data, count)) != count) {
		perror("write failed!");
		ret = -1;
		exit(0);
	}
	return ret;
}

void print_spring(unsigned char data) {

	if (data == CONFIG_SPRING_LEFT)
		printf("CONFIG_SPRING_LEFT\n");
	else if (data == CONFIG_SPRING_RIGHT)
		printf("CONFIG_SPRING_RIGHT\n");
	else if (data == CONFIG_SPRING_BOTH)
		printf("CONFIG_SPRING_BOTH\n");
	else if (data == CONFIG_SPRING_CENTER)
		printf("CONFIG_SPRING_CENTER\n");
}

void print_state(unsigned char data) {

	if (data == STATE_LEFT)
		printf("STATE_LEFT\n");
	else if (data == STATE_CENTER_FWD)
		printf("STATE_CENTER_FWD\n");
	else if (data == STATE_RIGHT)
		printf("STATE_RIGHT\n");
	else if (data == STATE_CENTER_BCK)
		printf("STATE_CENTER_BCK\n");
	else if (data == STATE_LEFT)
		printf("STATE_LEFT\n");
}

void display_bar(int fd, int on) {
	unsigned char data[5];
	if (on == ON)
		data[0] = 0xff;
	else if (on == OFF)
		data[0] = 0x00;
	dev_write(fd, data, WRITE_LED);
}

void display_letter(int fd, char c, int piece) {
	unsigned char data[5];

	if (c == ' ' || c == '\0')
		data[0] = 0;
	else
		data[0] = letters[(int)c - MAGIC_ASCII][piece];
	dev_write(fd, data, WRITE_LED);
}

int state_change(const int state, const unsigned char spring, int *count_state) {
	int new_state = state;
	int count = *count_state;

	if (state == STATE_LEFT && spring == CONFIG_SPRING_RIGHT) {		/* go from left to CENTER*/
		if (count >= 3) {
			new_state = STATE_RIGHT;
			count = 0;
		} else {
			count++;
		}
	} else if (state == STATE_RIGHT && spring == CONFIG_SPRING_LEFT) {	/* go from center to RIGHT */
		if (count >= 3) {
			new_state = STATE_LEFT;
			count = 0;
		} else {
			count++;
		}
	} else if (spring == CONFIG_SPRING_BOTH) {

	}
	*count_state = count;
	return new_state;
}

#endif
