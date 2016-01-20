#include "gd_sched.h"

//global variables
static pthread_t *trans_threads;
static pthread_t *proc_threads;

static int trans_nthreads;
static int proc_nthreads;

static pthread_mutex_t subframe_mutex[3];
static pthread_cond_t subframe_cond[3];
static int subframe_avail[3];

static int running;

gd_rng_buff_t *rng_buff;


void thread_common(pthread_t th, gd_thread_data_t *tdata){

    int ret;
    struct sched_param param;
    ret = pthread_setaffinity_np(th, sizeof(cpu_set_t),
                        tdata->cpuset);
        if (ret < 0) {
            errno = ret;
            perror("pthread_setaffinity_np");
            exit(-1);
        }

    switch(tdata->sched_policy){

        case SCHED_RR:
        case SCHED_FIFO:
            // fprintf(tdata->log_handler, "# Policy : %s\n",
                // (tdata->sched_policy == SCHED_RR ? "SCHED_RR" : "SCHED_FIFO"));
            param.sched_priority = tdata->sched_prio;
            ret = pthread_setschedparam(th,
                            tdata->sched_policy,
                            &param);
            if (ret != 0) {
                errno = ret;
                perror("pthread_setschedparam");
                exit(-1);
            }
            break;

        case SCHED_OTHER:
            fprintf(tdata->log_handler, "# Policy : SCHED_OTHER\n");
            /* add priority setting */
            tdata->lock_pages = 0; /* forced off for SCHED_OTHER */
            break;
    }

    if (tdata->lock_pages == 1)
    {
        fprintf( tdata->log_handler, "[%d] Locking pages in memory", tdata->ind);
        ret = mlockall(MCL_CURRENT | MCL_FUTURE);
        if (ret < 0) {
            errno = ret;
            perror("mlockall");
            exit(-1);
        }
    }

}


void* trans_main(void* arg){

    gd_thread_data_t *tdata = (gd_thread_data_t *) arg;
    int ind = tdata->ind;

    thread_common(pthread_self(), tdata);


    gd_timing_meta_t *timings;
    long duration_usec = (tdata->duration * 1e6);
    int nperiods = (int) ceil( duration_usec /
            (double) timespec_to_usec(&tdata->period));
    timings = (gd_timing_meta_t*) malloc ( nperiods * sizeof(gd_timing_meta_t));
    gd_timing_meta_t* timing;


    struct timespec t_next, t_deadline, trans_start, trans_end, t_temp, t_now;

    t_next = tdata->main_start;
    unsigned long abs_period_start = timespec_to_usec(&tdata->main_start);
    int period = 0;

    while(running && (period < nperiods)){

        // get current deadline and next period
        t_deadline = timespec_add(&t_next, &tdata->deadline);
        t_next = timespec_add(&t_next, &tdata->period);

        clock_gettime(CLOCK_MONOTONIC, &trans_start);
        /******* Main transport ******/
        // int j;
        // for(j=0; j <50000; j++){}

        gd_trans_read(tdata->conn_desc);

        pthread_mutex_lock(&subframe_mutex[period%3]);
        subframe_avail[period%3]++;
        pthread_cond_signal(&subframe_cond[period%3]);
        pthread_mutex_unlock(&subframe_mutex[period%3]);

        clock_gettime(CLOCK_MONOTONIC, &trans_end);
        /*****************************/

        timing = &timings[period];
        timing->ind = ind;
        timing->period = period;
        timing->abs_period_time = timespec_to_usec(&t_next);
        timing->rel_period_time = timing->abs_period_time - abs_period_start;

        timing->abs_start_time = timespec_to_usec(&trans_start);
        timing->rel_start_time = timing->abs_start_time - abs_period_start;
        timing->abs_end_time = timespec_to_usec(&trans_end);
        timing->rel_end_time = timing->abs_end_time - abs_period_start;
        timing->abs_deadline = timespec_to_usec(&t_deadline);
        timing->rel_deadline = timing->abs_deadline - abs_period_start;
        timing->actual_duration = timing->rel_end_time - timing->rel_start_time;
        timing->slack = timing->rel_deadline - timing->rel_end_time;

        if (timing->slack < 0){
            // printf("Deadline miss. Thread[%d] period[%lu] slack = %ld\n", tdata->ind, period, timing->slack);
        }

        clock_gettime(CLOCK_MONOTONIC, &t_now);

        // check if deadline was missed
        if (timespec_lower(&t_now, &t_next)){
            // sleep for remaining time
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t_next, NULL);
        }

        period ++;
    }
    clock_gettime(CLOCK_MONOTONIC, &t_temp);
    printf("Trans thread [%d] ran for %f s\n", ind, ((float) (timespec_to_usec(&t_temp)-abs_period_start))/1e6);


    fprintf(tdata->log_handler, "#idx\t\tabs_period\t\tabs_deadline\t\tabs_start\t\tabs_end"
                   "\t\trel_period\t\trel_start\t\trel_end\t\tduration\tslack\n");


    int i;
    for (i=0; i < nperiods; i++){
        log_timing(tdata->log_handler, &timings[i]);
    }
    fclose(tdata->log_handler);
    printf("Exit trans thread %d\n", ind);
    pthread_exit(NULL);
}

void* proc_main(void* arg){

    // acquire lock, read subframes and process
    gd_thread_data_t *tdata = (gd_thread_data_t *) arg;
    int id = tdata->ind;
    thread_common(pthread_self(), tdata);
    struct timespec t_offset;
    t_offset = usec_to_timespec(id*1000);
    tdata->main_start = timespec_add(&tdata->main_start, &t_offset);


    struct timespec proc_start, proc_end, t_next, t_deadline;
    gd_timing_meta_t *timings;
    long duration_usec = (tdata->duration * 1e6);
    int nperiods = (int) floor(duration_usec /
            (double) timespec_to_usec(&tdata->period));

    timings = (gd_timing_meta_t*) malloc ( nperiods * sizeof(gd_timing_meta_t));
    gd_timing_meta_t *timing;

    t_next = tdata->main_start;
    unsigned long abs_period_start = timespec_to_usec(&tdata->main_start);
    int period = 0;


    printf("Starting proc thread %d nperiods %d %lu\n", id, nperiods, timespec_to_usec(&t_offset));


    while(running && (period < nperiods)){

        t_deadline = timespec_add(&t_next, &tdata->deadline);
        t_next = timespec_add(&t_next, &tdata->period);

        pthread_mutex_lock(&subframe_mutex[id]);

        // printf("thread [%d] checking trans thread ...\n", id);
        while (!(subframe_avail[id] == trans_nthreads) && running){
            pthread_cond_wait(&subframe_cond[id], &subframe_mutex[id]);
        }
        // printf("thread [%d] got it!\n", id);

        /****** do LTE processing *****/
        clock_gettime(CLOCK_MONOTONIC, &proc_start);
        int j;
        for(j=0; j <100000; j++){}
        clock_gettime(CLOCK_MONOTONIC, &proc_end);
        /*****************************/

        // consume subframe
        subframe_avail[id] = 0;
        pthread_mutex_unlock(&subframe_mutex[id]);

        timing = &timings[period];
        timing->ind = id;
        timing->period = period;
        timing->abs_period_time = timespec_to_usec(&t_next);
        timing->rel_period_time = timing->abs_period_time - abs_period_start;
        timing->abs_start_time = timespec_to_usec(&proc_start);
        timing->rel_start_time = timing->abs_start_time - abs_period_start;
        timing->abs_end_time = timespec_to_usec(&proc_end);
        timing->rel_end_time = timing->abs_end_time - abs_period_start;
        timing->abs_deadline = timespec_to_usec(&t_deadline);
        timing->rel_deadline = timing->abs_deadline - abs_period_start;
        timing->actual_duration = timing->rel_end_time - timing->rel_start_time;
        timing->slack = timing->rel_deadline - timing->rel_end_time;

        // printf("Thread [%d] period [%d]\n", id, period);

        // if (timespec_lower(&proc_end, &t_deadline)){
        //     // sleep for remaining time
        //     clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t_next, NULL);
        // }

        period++;
    }
    printf("Writing to log ... Thread %d\n", id);

    fprintf(tdata->log_handler, "#idx\t\tabs_period\t\tabs_deadline\t\tabs_start\t\tabs_end"
                   "\t\trel_period\t\trel_start\t\trel_end\t\tduration\tslack\n");

    int i;
    for (i=0; i < nperiods; i++){
        log_timing(tdata->log_handler, &timings[i]);
    }
    fclose(tdata->log_handler);
    printf("Exit proc thread %d\n",id);
    pthread_exit(NULL);
}


static void
shutdown(int sig)
{
    int i;
    // notify threads, join them, then exit
    running = 0;

    for (i=0; i<3; i++){
        pthread_mutex_destroy(&subframe_mutex[i]);
        pthread_cond_destroy(&subframe_cond[i]);
    }

    for (i = 0; i < trans_nthreads; i++)
    {
        pthread_join(trans_threads[i], NULL);
    }
    for (i = 0; i < proc_nthreads; i++)
    {
        pthread_join(proc_threads[i], NULL);
    }
    printf("Received shutdown signal ...\n");
    exit(-1);
}



int main(){

    int i,j;
    int thread_ret;
    char tmp_str[100], tmp_str_a[100];

    // options
    int node_ids[4] =  {0, 1, 2, 3};
    // int node_ids[16] =  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
    int num_nodes = 4;
    int node_socks[num_nodes];
    int host_id = 200;
    int num_samples = 1*1e6*1e-3; // samples in subframe = (samples/sec)*1ms
    int duration = 10; //secs
    int priority = 99;
    double complex *buffer = (double complex*) malloc(num_samples*sizeof(double complex));
    int sched = SCHED_FIFO;


    policy_to_string(sched, tmp_str_a);


    trans_nthreads = num_nodes;
    proc_nthreads = 3;

    for (i=0; i<3; i++){
        pthread_mutex_init(&subframe_mutex[i], NULL);
        pthread_cond_init(&subframe_cond[i], NULL);
    }


    trans_threads = malloc(trans_nthreads*sizeof(pthread_t));
    gd_thread_data_t *trans_tdata;
    trans_tdata = malloc(trans_nthreads*sizeof(gd_thread_data_t));

    proc_threads = malloc(proc_nthreads*sizeof(pthread_t));
    gd_thread_data_t *proc_tdata;
    proc_tdata = malloc(proc_nthreads*sizeof(gd_thread_data_t));

    for (i=0; i<3; i++){
        subframe_avail[i] = 0;
    }

    /* install a signal handler for proper shutdown */
    signal(SIGQUIT, shutdown);
    signal(SIGTERM, shutdown);
    signal(SIGHUP, shutdown);
    signal(SIGINT, shutdown);

    running = 1;
    gd_trans_initialize(node_socks, num_nodes);
    gd_trans_trigger();

    for(i= 0; i < trans_nthreads; i++){

        trans_tdata[i].ind = i;
        trans_tdata[i].duration = duration;
        trans_tdata[i].sched_policy = sched;
        trans_tdata[i].deadline = usec_to_timespec(500);
        trans_tdata[i].period = usec_to_timespec(1000);

        sprintf(tmp_str, "../log/trans%d_prior%d_sched%s_nant%d_nproc%d.log", i, priority,
                                    tmp_str_a, trans_nthreads, proc_nthreads);
        trans_tdata[i].log_handler = fopen(tmp_str, "w");
        trans_tdata[i].sched_prio = priority;
        trans_tdata[i].cpuset = malloc(sizeof(cpu_set_t));
        CPU_SET( 5, trans_tdata[i].cpuset);

        trans_tdata[i].conn_desc.node_id = i;
        trans_tdata[i].conn_desc.node_sock = node_socks[i];
        trans_tdata[i].conn_desc.host_id = host_id;
        trans_tdata[i].conn_desc.num_samples = num_samples;
        trans_tdata[i].conn_desc.start_sample = 0;
        trans_tdata[i].conn_desc.buffer = buffer;
        trans_tdata[i].conn_desc.buffer_id = 1;
    }

    for(i= 0; i < proc_nthreads; i++){

        proc_tdata[i].ind = i;
        proc_tdata[i].duration = duration;
        proc_tdata[i].sched_policy = sched;
        proc_tdata[i].deadline = usec_to_timespec(2000);
        proc_tdata[i].period = usec_to_timespec(3000);

        sprintf(tmp_str, "../log/proc%d_prior%d_sched%s_nant%d_nproc%d.log", i, priority,
        tmp_str_a, trans_nthreads, proc_nthreads);
        proc_tdata[i].log_handler = fopen(tmp_str, "w");
        proc_tdata[i].sched_prio = priority;
        proc_tdata[i].cpuset = malloc(sizeof(cpu_set_t));
        CPU_SET( 8+2*i, proc_tdata[i].cpuset);
    }


    printf("Starting trans threads\n");

    struct timespec t_start;
    // starting time
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    // start threads
    for(i = 0; i < trans_nthreads; i++){
        trans_tdata[i].main_start = t_start;
        thread_ret = pthread_create(&trans_threads[i], NULL, trans_main, &trans_tdata[i]);
        if (thread_ret){
            perror("Cannot start thread");
            exit(-1);
        }
    }

    printf("Starting proc threads\n");

    for(i= 0; i < proc_nthreads; i++){
        proc_tdata[i].main_start = t_start;
        thread_ret = pthread_create(&proc_threads[i], NULL, proc_main, &proc_tdata[i]);
        if (thread_ret){
            perror("Cannot start thread");
            exit(-1);
        }
    }

    for (i = 0; i < trans_nthreads; i++)
    {
        pthread_join(trans_threads[i], NULL);
    }

    for (i = 0; i < proc_nthreads; i++)
    {
        pthread_join(proc_threads[i], NULL);
    }

    return 0;
}
