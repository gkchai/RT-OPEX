#ifndef GD_TYPES_H
#define GD_TYPES_H

#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <sched.h>
#include "../cwarp/my_complex.h"


typedef struct _gd_conn_desc_t{
int node_id;
int node_sock;
int host_id;
int buffer_id;
int num_samples;
int start_sample;
double my_complex* buffer;

} gd_conn_desc_t;


typedef struct _gd_rng_buff_t{
int head;
int tail;
int nitems;
int size;
double my_complex* buff;
int samples_per_item;

} gd_rng_buff_t;

typedef struct _gd_thread_data_t {
    int ind;
    char *name;
    int lock_pages;
    int duration;
    cpu_set_t *cpuset;
    char *cpuset_str;
    struct timespec period, deadline;
    struct timespec main_start;
    FILE *log_handler;
    int sched_policy;
    int sched_prio;
    gd_conn_desc_t conn_desc;

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
    long miss;

} gd_timing_meta_t;

typedef struct _gd_proc_timing_meta_t{
    int mcs;
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
    int kill;
    int iter;
    int miss;

} gd_proc_timing_meta_t;




typedef struct _gd_off_timing_meta_t{
    int ind;
    unsigned long abs_period_time;
    unsigned long abs_start_time;
    unsigned long abs_end_time;
    unsigned long abs_task_start_time;
    unsigned long abs_task_end_time;

    unsigned long rel_period_time;
    unsigned long rel_start_time;
    unsigned long rel_end_time;
    unsigned long rel_task_start_time;
    unsigned long rel_task_end_time;

    unsigned long total_duration;
    unsigned long task_duration;
    char type[30];
    unsigned long period;

} gd_off_timing_meta_t;


#endif