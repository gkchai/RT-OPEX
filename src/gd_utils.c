#include "gd_utils.h"

unsigned int
timespec_to_msec(struct timespec *ts)
{
    return (ts->tv_sec * 1E9 + ts->tv_nsec) / 1000000;
}

long
timespec_to_lusec(struct timespec *ts)
{
    return round((ts->tv_sec * 1E9 + ts->tv_nsec) / 1000.0);
}

unsigned long
timespec_to_usec(struct timespec *ts)
{
    return round((ts->tv_sec * 1E9 + ts->tv_nsec) / 1000.0);
}


struct timespec
usec_to_timespec(unsigned long usec)
{
    struct timespec ts;

    ts.tv_sec = usec / 1000000;
    ts.tv_nsec = (usec % 1000000) * 1000;

    return ts;
}

struct timespec
msec_to_timespec(unsigned int msec)
{
    struct timespec ts;

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    return ts;
}

struct timespec
timespec_add(struct timespec *t1, struct timespec *t2)
{
    struct timespec ts;

    ts.tv_sec = t1->tv_sec + t2->tv_sec;
    ts.tv_nsec = t1->tv_nsec + t2->tv_nsec;

    while (ts.tv_nsec >= 1E9) {
        ts.tv_nsec -= 1E9;
        ts.tv_sec++;
    }

    return ts;
}

struct timespec
timespec_sub(struct timespec *t1, struct timespec *t2)
{
    struct timespec ts;

    if (t1->tv_nsec < t2->tv_nsec) {
        ts.tv_sec = t1->tv_sec - t2->tv_sec -1;
        ts.tv_nsec = t1->tv_nsec  + 1000000000 - t2->tv_nsec;
    } else {
        ts.tv_sec = t1->tv_sec - t2->tv_sec;
        ts.tv_nsec = t1->tv_nsec - t2->tv_nsec;
    }

    return ts;

}

int
timespec_lower(struct timespec *what, struct timespec *than)
{
    if (what->tv_sec > than->tv_sec)
        return 0;

    if (what->tv_sec < than->tv_sec)
        return 1;

    if (what->tv_nsec < than->tv_nsec)
        return 1;

    return 0;
}

void
log_timing(FILE *handler, gd_timing_meta_t *t)
{
        fprintf(handler,
        "%d\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%ld\n",
        t->ind,
        t->abs_period_time,
        t->abs_deadline,
        t->abs_start_time,
        t->abs_end_time,
        t->rel_period_time,
        t->rel_start_time,
        t->rel_end_time,
        t->actual_duration,
        t->miss
        );
}

void
proc_log_timing(FILE *handler, gd_proc_timing_meta_t *t)
{
        fprintf(handler,
        "%d\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%ld\n",
        t->ind,
        t->abs_period_time,
        t->abs_deadline,
        t->abs_start_time,
        t->abs_end_time,
        t->rel_period_time,
        t->rel_start_time,
        t->rel_end_time,
        t->original_duration,
        t->actual_duration,
        t->no_offload,
        t->dur_offload,
        t->miss
        );
}

void
off_log_timing(FILE *handler, gd_off_timing_meta_t *t)
{

    fprintf(handler,
        "%d\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%s\n",
        t->ind,
        t->abs_period_time,
        t->abs_start_time,
        t->abs_end_time,
        t->abs_task_start_time,
        t->abs_task_end_time,
        t->rel_period_time,
        t->rel_start_time,
        t->rel_end_time,
        t->rel_task_start_time,
        t->rel_task_end_time,
        t->total_duration,
        t->task_duration,
        t->type
        );

}





int
string_to_policy(const char *policy_name, int *policy)
{
    if (strcmp(policy_name, "SCHED_OTHER") == 0)
        *policy = SCHED_OTHER;
    else if (strcmp(policy_name, "SCHED_RR") == 0)
        *policy =  SCHED_RR;
    else if (strcmp(policy_name, "SCHED_FIFO") == 0)
        *policy =  SCHED_FIFO;
    else
        return 1;
    return 0;
}

int
policy_to_string(int policy, char *policy_name)
{
    switch (policy) {
        case SCHED_OTHER:
            strcpy(policy_name, "SCHED_OTHER");
            break;
        case SCHED_RR:
            strcpy(policy_name, "SCHED_RR");
            break;
        case SCHED_FIFO:
            strcpy(policy_name, "SCHED_FIFO");
            break;
        default:
            return 1;
    }
    return 0;
}


void ftrace_write(int mark_fd, const char *fmt, ...)
{
    va_list ap;
    int n, size = BUF_SIZE;
    char *tmp, *ntmp;

    if (mark_fd < 0) {
        log_error("invalid mark_fd");
        exit(EXIT_FAILURE);
    }

    if ((tmp = malloc(size)) == NULL) {
        log_error("Cannot allocate ftrace buffer");
        exit(EXIT_FAILURE);
    }

    while(1) {
        /* Try to print in the allocated space */
        va_start(ap, fmt);
        n = vsnprintf(tmp, BUF_SIZE, fmt, ap);
        va_end(ap);

        /* If it worked return success */
        if (n > -1 && n < size) {
            write(mark_fd, tmp, n);
            free(tmp);
            return;
        }

        /* Else try again with more space */
        if (n > -1) /* glibc 2.1 */
            size = n+1;
        else        /* glibc 2.0 */
            size *= 2;

        if ((ntmp = realloc(tmp, size)) == NULL) {
            free(tmp);
            log_error("Cannot reallocate ftrace buffer");
            exit(EXIT_FAILURE);
        } else {
            tmp = ntmp;
        }
    }

}

// return -1 if offloading does not cut it, always deadline-miss
// else return the loops that must be offloaded when
// N_done parallel loops are already completed
int req_offload_loops(long T, long t_p, long t_s, int N_rem){

    // T is remaining time

    int N_off = -1;
    int i;
    // (N_rem - N_off)t_p + t_s <= T and N_off*t_p <= (N_rem - N_off)t_p
    // ==> N_off >= N_r - (T-t_s)/t_p and N_off <= N_rem/2

    for (i=1; i<= (int)N_rem/2; i++){
        if (i >= N_rem - (int)((T-t_s)/t_p)){
            N_off = i;
            break;
        }
    }
    return N_off;
}

