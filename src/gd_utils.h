#ifndef _GD_UTILS_H_
#define _GD_UTILS_H_

#include <time.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "gd_types.h"

#ifndef LOG_PREFIX
#define LOG_PREFIX "[gd_sched] "
#endif
#ifndef LOG_LEVEL
#define LOG_LEVEL 100
#endif

#define LOG_LEVEL_DEBUG 100
#define LOG_LEVEL_INFO 75
#define LOG_LEVEL_NOTICE 50
#define LOG_LEVEL_ERROR 10
#define LOG_LEVEL_CRITICAL 10

#define BUF_SIZE 100

/* This prepend a string to a message */
#define gd_log_to(where, level, level_pfx, msg, args...)     \
do {                                    \
    if (level <= LOG_LEVEL) {                       \
        fprintf(where, LOG_PREFIX level_pfx msg "\n", ##args);      \
    }                                   \
} while (0);

#define log_ftrace(mark_fd, msg, args...)               \
do {                                    \
    ftrace_write(mark_fd, msg, ##args);                 \
} while (0);

#define log_notice(msg, args...)                    \
do {                                    \
    gd_log_to(stderr, LOG_LEVEL_NOTICE, "<notice> ", msg, ##args);   \
} while (0);

#define log_info(msg, args...)                      \
do {                                    \
    gd_log_to(stderr, LOG_LEVEL_INFO, "<info> ", msg, ##args);   \
} while (0);

#define log_error(msg, args...)                     \
do {                                    \
    gd_log_to(stderr, LOG_LEVEL_ERROR, "<error> ", msg, ##args); \
} while (0);

#define log_debug(msg, args...)                     \
do {                                    \
    gd_log_to(stderr, LOG_LEVEL_DEBUG, "<debug> ", msg, ##args); \
} while (0);

#define log_critical(msg, args...)                  \
do {                                    \
    gd_log_to(stderr, LOG_LEVEL_CRITICAL, "<crit> ", msg, ##args);   \
} while (0);

unsigned int
timespec_to_msec(struct timespec *ts);

long
timespec_to_lusec(struct timespec *ts);

unsigned long
timespec_to_usec(struct timespec *ts);

struct timespec
usec_to_timespec(unsigned long usec);

struct timespec
usec_to_timespec(unsigned long usec);

struct timespec
msec_to_timespec(unsigned int msec);

struct timespec
timespec_add(struct timespec *t1, struct timespec *t2);

struct timespec
timespec_sub(struct timespec *t1, struct timespec *t2);

int
timespec_lower(struct timespec *what, struct timespec *than);

void
log_timing(FILE *handler, gd_timing_meta_t *t);

int
string_to_policy(const char *policy_name, int *policy);

int
policy_to_string(int policy, char *policy_name);

void
ftrace_write(int mark_fd, const char *fmt, ...);

#endif