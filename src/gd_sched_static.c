#include "gd_sched.h"
#include "gd_rx.h"
short* iqr;
short* iqi;

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))


//global variables
static pthread_t *trans_threads;
static pthread_t *proc_threads;

static int trans_nthreads;
static int proc_nthreads;
static int num_bss;
static int num_ants;
static int num_cores_bs;

static pthread_mutex_t *subframe_mutex;
static pthread_mutex_t *state_mutex;
static pthread_cond_t *subframe_cond;
static struct timespec *common_time;
static struct timespec common_time_ref;
static struct timespec common_time_next;


static int *subframe_avail;
static int running;
static int debug_trans = 1;
static int mcs;
static int var;

gd_rng_buff_t *rng_buff;
int *deadline_miss_flag;
int mcs_data[95];

int *migrate_avail, *migrate_to;
long *state;
int trans_dur_usec = 600;


double decode_time[28] = {15.9,20.7,25.5,32.9,41.8,50.6,59.5,71.4,80.3,92.1,92.1,100.9,114.2,131.9,149.3,162.6,175.9,175.9,189.2,
211.3,224.5,246.4,264.1,293.4,315.5,326.5,352.4,365.4};

double sub_fft_time = 6;
double sub_demod_time = 20;


int N_P;

void thread_common(pthread_t th, gd_thread_data_t *tdata){

    int ret;
    struct sched_param param;
    ret = pthread_setaffinity_np(th, sizeof(cpu_set_t),
                        tdata->cpuset);
        if (ret < 0) {
            errno = ret;
            log_error("pthread_setaffinity_np");
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
                log_error("pthread_setschedparam");
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
            log_error("mlockall");
            exit(-1);
        }
    }

}


void* trans_main(void* arg){

    gd_thread_data_t *tdata = (gd_thread_data_t *) arg;
    int id = tdata->ind;

    thread_common(pthread_self(), tdata);
    unsigned long abs_period_start = timespec_to_usec(&tdata->main_start);


    gd_timing_meta_t *timings;
    long duration_usec = (tdata->duration * 1e6);
    int nperiods = (int) ceil( duration_usec /
            (double) timespec_to_usec(&tdata->period));
    timings = (gd_timing_meta_t*) malloc ( nperiods * sizeof(gd_timing_meta_t));
    gd_timing_meta_t* timing;


    struct timespec t_next, t_deadline, trans_start, trans_end, t_temp, t_now;
    struct timespec trans_dur = usec_to_timespec(trans_dur_usec);

    t_next = tdata->main_start;
    int period = 0;
    int bs_id  = ((int)(id/num_ants));
    int subframe_id;

    while(running && (period < nperiods)){


        subframe_id = period%(num_cores_bs);

        // get current deadline and next period
        t_deadline = timespec_add(&t_next, &tdata->deadline);
        t_next = timespec_add(&t_next, &tdata->period);

        clock_gettime(CLOCK_MONOTONIC, &trans_start);
        /******* Main transport ******/
        if (debug_trans==1) {

            t_temp = timespec_add(&trans_start, &trans_dur);
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t_temp, NULL);

        } else {
            gd_trans_read(tdata->conn_desc);
        }
        /******* Main transport ******/


        pthread_mutex_lock(&subframe_mutex[bs_id*num_cores_bs + subframe_id]);
        // subframe_avail[bs_id*num_cores_bs + subframe_id] = (subframe_avail[bs_id*num_cores_bs + subframe_id]+1)%(num_ants);
        subframe_avail[bs_id*num_cores_bs + subframe_id] ++;
		// printf("subframe_avail:%d %d\n",bs_id*num_cores_bs + subframe_id,subframe_avail[bs_id*num_cores_bs + subframe_id]);


        // hanging fix -- if trans misses a proc, reset the subframe available counter
        if (subframe_avail[bs_id*num_cores_bs + subframe_id] == (num_ants+1)) {
               subframe_avail[bs_id*num_cores_bs + subframe_id] = 1;
        }

        pthread_cond_signal(&subframe_cond[bs_id*num_cores_bs + subframe_id]);
        pthread_mutex_unlock(&subframe_mutex[bs_id*num_cores_bs + subframe_id]);


        clock_gettime(CLOCK_MONOTONIC, &trans_end);
        /*****************************/

        timing = &timings[period];
        timing->ind = id;
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
        timing->miss = (timing->rel_deadline - timing->rel_end_time >= 0) ? 0 : 1;

        if (timing->actual_duration > 1000){
            // log_critical("Transport overload. Thread[%d] Duration= %lu us. Reduce samples or increase threads",
                // tdata->ind, timing->actual_duration);
        }

        clock_gettime(CLOCK_MONOTONIC, &t_now);

        // check if deadline was missed
        if (timespec_lower(&t_now, &t_next)){
            // sleep for remaining time
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t_next, NULL);
        }else{
            // printf("Transport %d is too slow\n", id);
        }

        period ++;

    }
    clock_gettime(CLOCK_MONOTONIC, &t_temp);
    log_notice("Trans thread [%d] ran for %f s", id, ((float) (timespec_to_usec(&t_temp)-abs_period_start))/1e6);


    // fprintf(tdata->log_handler, "#idx\t\tabs_period\t\tabs_deadline\t\tabs_start\t\tabs_end"
                   // "\t\trel_period\t\trel_start\t\trel_end\t\tduration\t\tmiss\n");


    int i;
    for (i=0; i < nperiods; i++){
        // log_timing(tdata->log_handler, &timings[i]);
    }
    // fclose(tdata->log_handler);
    log_notice("Exit trans thread %d", id);

    running = 0;
    for (i=0;i<proc_nthreads;i++) {
            pthread_mutex_lock(&subframe_mutex[i]);
            subframe_avail[i]=-1;
            pthread_cond_signal(&subframe_cond[i]);
            pthread_mutex_unlock(&subframe_mutex[i]);
    }

    pthread_exit(NULL);
}



void* proc_main(void* arg){

    // acquire lock, read subframes and process
    gd_thread_data_t *tdata = (gd_thread_data_t *) arg;
    int id = tdata->ind;
    thread_common(pthread_self(), tdata);
    unsigned long abs_period_start = timespec_to_usec(&tdata->main_start);
    struct timespec t_offset;
    struct timespec proc_start, proc_end, t_next, t_deadline;
    gd_proc_timing_meta_t *timings;
    long duration_usec = (tdata->duration * 1e6);
    int nperiods = (int) floor(duration_usec /
            (double) timespec_to_usec(&tdata->period));

    //nperiods reduce a little to prevents trans finishing before proc; ugly fix
    nperiods-=500;

    timings = (gd_proc_timing_meta_t*) malloc ( nperiods * sizeof(gd_proc_timing_meta_t));
    gd_proc_timing_meta_t *timing;

    t_next = tdata->main_start;
    int period = 0;
    int deadline_miss=0;

    long time_deadline, proc_actual_time, avail_time;
    struct timespec t_temp, t_now;

    int bs_id = (int)(id/num_cores_bs);
    int subframe_id =  id%(num_cores_bs);

    t_offset = usec_to_timespec(subframe_id*1000);
    t_deadline = timespec_add(&tdata->main_start, &t_offset);

    log_notice("Starting proc thread %d nperiods %d %lu", id, nperiods, timespec_to_usec(&t_offset));
    log_notice("checking subframe mutex %d", bs_id*num_cores_bs + subframe_id);
    int kill = 0;
    int ret= 0;
    int curr_mcs = 0;
    while(running && (period < nperiods)){


        // wait for the transport thread to wake me
        pthread_mutex_lock(&subframe_mutex[id]);
        while (!(subframe_avail[id] == num_ants)){
                    pthread_cond_wait(&subframe_cond[id], &subframe_mutex[id]);
        }
		subframe_avail[id]=0;
        pthread_mutex_unlock(&subframe_mutex[id]);

        /****** do LTE processing *****/
        clock_gettime(CLOCK_MONOTONIC, &proc_start);
        clock_gettime(CLOCK_MONOTONIC, &t_now);

        t_next = timespec_add(&t_now, &tdata->deadline);
        t_deadline = timespec_add(&t_deadline, &tdata->period);

        if (var){
            curr_mcs = mcs_data[period%95];
        }else{
            curr_mcs = mcs;
        }


        configure_runtime(curr_mcs, (short*)(iqr + 15360*curr_mcs), (short*)(iqi + 15360*curr_mcs), bs_id);


        task_fft(bs_id);
        task_demod(bs_id);
        clock_gettime(CLOCK_MONOTONIC, &t_now);

        // // check if there is enough time to decode else kill
        if (timespec_to_usec(&t_next) - (50 + timespec_to_usec(&t_now) + 3*decode_time[curr_mcs]) < 0.0){
            kill = 1;
            ret = -1;
        }else{
            kill = 0;
            ret = task_decode(bs_id);
        }


        // ret = task_all(bs_id);

        clock_gettime(CLOCK_MONOTONIC, &proc_end);
        clock_gettime(CLOCK_MONOTONIC, &t_now);

        while (timespec_to_usec(&t_now) <=  timespec_to_usec(&t_next)-50){
                            clock_gettime(CLOCK_MONOTONIC, &t_now);
        }

        timing = &timings[period];
        timing->mcs = curr_mcs;
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
        timing->miss = (timing->rel_deadline > timing->rel_end_time ? 0 : 1)| kill;
        timing->iter = ret;
        timing->kill = kill;
        timing->migrated = 0;
        period++;
    }

    log_notice("Writing to log ... proc thread %d", id);
    fprintf(tdata->log_handler, "#MCS\ttabs_period\tabs_deadline\tabs_start\tabs_end"
                   "\trel_period\trel_start\trel_end\tduration\tmiss\titer\tkill\tmigrated\n");

    int i;
    for (i=0; i < nperiods; i++){
        proc_log_timing(tdata->log_handler, &timings[i]);
    }

    fclose(tdata->log_handler);
    log_notice("Exit proc thread %d",id);
    pthread_exit(NULL);
}


static void
gd_shutdown(int sig)
{
    int i;
    // notify threads, join them, then exit
    running = 0;

    for (i=0; i<proc_nthreads; i++){
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

    log_notice("Received shutdown signal ...\n");
    exit(-1);
}


int main(int argc, char** argv){


    deadline_miss_flag = (int *)malloc(100*sizeof(int));


    srand(time(NULL));
    int i,j;
    int thread_ret;
    char tmp_str[200], tmp_str_a[200];

    // options
    int node_ids[4] =  {0, 1, 2, 3};
    // int node_ids[16] =  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
    int num_nodes = 4;
    int node_socks[num_nodes];
    int host_id = 200;
    int num_samples = 1*1e6*1e-3; // samples in subframe = (samples/sec)*1ms
    int duration = 50; //secs
    int priority = 10;
    int sched = SCHED_FIFO;
    mcs = 20;
    int lmax  = 1500;
    int deadline = 1500;
    num_bss = 4;
    num_ants = 1; // antennas per radio
    N_P = 8;
    int snr = 30;
    var = 1;

    char c;
    while ((c = getopt (argc, argv, "h::M:A:L:s:d:p:S:e:D:Z:N:m:R:")) != -1) {
        switch (c) {
            case 'M':
              num_bss = atoi(optarg);
              break;

            case 'A':
              num_ants = atoi(optarg);
              break;

            case 's':
              num_samples = atoi(optarg);
              break;

            case 'd':
              duration = atoi(optarg);
              break;

            case 'p':
              priority = atoi(optarg);
              if (priority > 99){
                log_error("Unsupported priority!\n");
                exit(-1);
              }
              break;

            case 'Z':
              N_P = atoi(optarg);
              break;

            case 'S':
              switch((char)*optarg){

                  case 'R':
                    sched = SCHED_RR;
                    break;

                  case 'F':
                    sched = SCHED_FIFO;
                    break;

                  case 'O':
                    sched = SCHED_OTHER;
                    break;

                  default:
                    log_error("Unsupported scheduler!\n");
                    exit(-1);
                    break;
              }
              break;

            case 'e':
              var = atoi(optarg);
              break;

            case 'D':
              debug_trans = atoi(optarg);
              break;

            case 'L':
                lmax = atoi(optarg);
                break;

            case 'N':
                snr = atoi(optarg);
                break;

            case 'R':
                trans_dur_usec = atoi(optarg);
                break;

            case 'm':
                mcs = atoi(optarg);
                break;


            case 'h':
            default:
              printf("%s -h(elp) -M num_bss -A num_ants  -L lmax -s num_samples -d duration(s) -p priority(1-99) -S sched (R/F/O) -e experiment (0 fixed_mcs /1 var_mcs) -D transport debug(0 or 1) -m MCS\n\nExample usage: sudo ./gd_lte_static.o -M 4 -A 1 -L 2000 -s 1000 -d 10 -p 10 -S F -e 1 -D 1 -N 30 -m 20 -R 600\n",
                     argv[0]);
              exit(1);
              break;

        }
    }

    num_nodes = num_bss*num_ants;

    // calculate the number of cores to support the given radios
    num_cores_bs = ceil((double)lmax/1000);  // each bs
    proc_nthreads = num_cores_bs*num_bss; // total

    assert(num_cores_bs == 1 || num_cores_bs == 2 || num_cores_bs == 3);
    log_notice("Scheduler will run with %d cores for each of %d BSs. Total cores = %d\n",num_cores_bs, num_bss, proc_nthreads)


    FILE *fp;
    i = 0;
    fp = fopen("/home/gkchai/gkchai/win16/garud/src/mcs.txt", "r");
    while (fscanf(fp, "%d\n", &mcs_data[i])!= EOF && i < 95){
        i++;
    }
    fclose(fp);

    /**************************************************************************/
    iqr = (short*) malloc(28*1*15360*sizeof(short)); //1=no_of_frame/1000, 2=BW/5MHz
    iqi = (short*) malloc(28*1*15360*sizeof(short));

    FILE* file_iq;
    char filename_iq[500];

    // load samples for all mcs
    j = 0;

    for (i=0; i<28; i++){

        sprintf(filename_iq, "/mnt/hd3/gkchai/ul/iq_mcs=%d_snr=%f_nrb=%d_subf=%d_ch=%d_rx=%d.dat", i, (float)snr, 50, 3, 1, 0);
        // printf("Reading ... %s\n", filename_iq);
        file_iq = fopen(filename_iq, "r");

        int k = 0;
        // changed 1*15360000 to 1*1536000 ?
        while (fscanf(file_iq, "%hi\t%hi\n", &iqr[j], &iqi[j]) != EOF && k < 1*15360){
            k++;
            j++;
        }
        fclose(file_iq);
    }


    // configure the baseband
    configure(0, NULL, 0, iqr, iqi, mcs, num_ants, num_bss);

    /**************************************************************************/


    double my_complex *buffer = (double my_complex*) malloc(num_samples*sizeof(double my_complex));
    policy_to_string(sched, tmp_str_a);
    trans_nthreads = num_nodes;

    /**************************************************************************/
    trans_threads = malloc(trans_nthreads*sizeof(pthread_t));
    gd_thread_data_t *trans_tdata;
    trans_tdata = malloc(trans_nthreads*sizeof(gd_thread_data_t));

    proc_threads = malloc(proc_nthreads*sizeof(pthread_t));
    gd_thread_data_t *proc_tdata;
    proc_tdata = malloc(proc_nthreads*sizeof(gd_thread_data_t));


    subframe_mutex = (pthread_mutex_t*)malloc(proc_nthreads*sizeof(pthread_mutex_t));
    state_mutex = (pthread_mutex_t*)malloc(proc_nthreads*sizeof(pthread_mutex_t));
    subframe_cond = (pthread_cond_t*)malloc(proc_nthreads*sizeof(pthread_cond_t));
    common_time = (struct timespec*)malloc((num_cores_bs)*sizeof(struct timespec));
    subframe_avail = (int *)malloc(proc_nthreads*sizeof(int));
    state = (long *)malloc(proc_nthreads*sizeof(long));
    migrate_avail = (int *)malloc(proc_nthreads*sizeof(int));
    migrate_to = (int *)malloc(proc_nthreads*sizeof(int));



    for (i=0; i<proc_nthreads; i++){
        subframe_avail[i] = 0;
        pthread_mutex_init(&subframe_mutex[i], NULL);
        pthread_mutex_init(&state_mutex[i], NULL);
        pthread_cond_init(&subframe_cond[i], NULL);
    }

    /* install a signal handler for proper shutdown */
    signal(SIGQUIT, gd_shutdown);
    signal(SIGTERM, gd_shutdown);
    signal(SIGHUP, gd_shutdown);
    signal(SIGINT, gd_shutdown);

    running = 1;
    gd_trans_initialize(node_socks, num_nodes);
    gd_trans_trigger();


    for(i= 0; i < trans_nthreads; i++){

        trans_tdata[i].ind = i;
        trans_tdata[i].duration = duration;
        trans_tdata[i].sched_policy = sched;
        trans_tdata[i].deadline = usec_to_timespec(500);
        trans_tdata[i].period = usec_to_timespec(1000);

        // sprintf(tmp_str, "/home/gkchai/gkchai/win16/garud/log_static/exp%d_samp%d_trans%d_prior%d_sched%s_nbss%d_nants%d_ncores%d_Lmax%d_mcs%d_delay%d.log",
        //     var, num_samples, i, priority, tmp_str_a, num_bss, num_ants, num_cores_bs, lmax, mcs, trans_dur_usec);
        // trans_tdata[i].log_handler = fopen(tmp_str, "w");
        trans_tdata[i].sched_prio = priority;
        trans_tdata[i].cpuset = malloc(sizeof(cpu_set_t));
        CPU_SET( (26 +i)%32, trans_tdata[i].cpuset);

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
        proc_tdata[i].deadline = usec_to_timespec(num_cores_bs*1000 - trans_dur_usec);
        proc_tdata[i].period = usec_to_timespec(2000);
        sprintf(tmp_str, "/home/gkchai/gkchai/win16/garud/log_static/exp%d_samp%d_proc%d_prior%d_sched%s_nbss%d_nants%d_ncores%d_Lmax%d_snr%d_mcs%d_delay%d.log",
            var, num_samples, i, priority,tmp_str_a, num_bss, num_ants, num_cores_bs, lmax, snr, mcs, trans_dur_usec);

        proc_tdata[i].log_handler = fopen(tmp_str, "w");
        proc_tdata[i].sched_prio = priority;
        proc_tdata[i].cpuset = malloc(sizeof(cpu_set_t));
        CPU_SET( 2+i, proc_tdata[i].cpuset);
    }

    struct timespec t_start;
    // starting time
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    log_notice("Starting trans threads");
    // start threads
    for(i = 0; i < trans_nthreads; i++){
        trans_tdata[i].main_start = t_start;
        thread_ret = pthread_create(&trans_threads[i], NULL, trans_main, &trans_tdata[i]);
        if (thread_ret){
            log_error("Cannot start thread");
            exit(-1);
        }
    }


    log_notice("Starting proc threads");
    for(i= 0; i < proc_nthreads; i++){
        proc_tdata[i].main_start = t_start;
        thread_ret = pthread_create(&proc_threads[i], NULL, proc_main, &proc_tdata[i]);
        if (thread_ret){
            log_error("Cannot start thread");
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
