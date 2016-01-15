#ifndef GD_TYPES_H
#define GD_TYPES_H

#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <sched.h>
#include <complex.h>


typedef struct _gd_conn_desc_t{
int* node_ids;
int num_nodes;
int host_id;
int num_samples;
double complex* buffer;

} gd_conn_desc_t;


typedef struct _gd_rng_buff_t{
int head;
int tail;
int nitems;
int size;
double complex* buff;
int samples_per_item;

} gd_rng_buff_t;

typedef struct _gd_thread_data_t {
    int ind;
    char *name;
    int lock_pages;
    int duration;
    cpu_set_t *cpuset;
    char *cpuset_str;
    struct timespec exec;
    struct timespec period, deadline;
    struct timespec main_start;
    FILE *log_handler;
    int sched_policy;
    int sched_prio;

} gd_thread_data_t;



typedef struct _gd_timing_meta_t{
    int ind;
    unsigned long abs_period_time;
    unsigned long abs_start_time;
    unsigned long abs_end_time;
    unsigned long abs_deadline;

    unsigned long rel_period_time;
    unsigned long rel_start_time;
    unsigned long rel_end_time;
    unsigned long rel_deadline;

    unsigned long actual_duration;
    unsigned long period;
    long slack;


} gd_timing_meta_t;

#endif