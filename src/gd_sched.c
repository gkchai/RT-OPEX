#include "gd_sched.h"

//global variables
static pthread_t *trans_threads;
static pthread_t *proc_threads;
static pthread_t *offload_threads;

static int trans_nthreads;
static int proc_nthreads;
static int offload_nthreads;

static pthread_mutex_t subframe_mutex[3];
static pthread_cond_t subframe_cond[3];

static pthread_mutex_t *offload_mutex;
static pthread_cond_t *offload_cond;


static pthread_mutex_t *task_ready_mutex;
static pthread_cond_t *task_ready_cond; //busy wait till offloading thread has task ready
static int * task_ready_flag; // indicates if task is ready for offloading
//0 not ready; 1 task ready; 2 woken up by transport thread -- idling is over


static int *proc_idle; //indicates if processor is idle
static int *proc_trigger; // local trigger for proc from offload
static int *result_ready; //indicates if the result is ready

static int subframe_avail[3];

static int running;

const static int debug_trans = 1;
gd_rng_buff_t *rng_buff;

static int offload_sleep[3];
char exp_str[100];


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

//main body for the offloading thread
void* offload_main(void* arg){

	gd_thread_data_t *tdata = (gd_thread_data_t *) arg;
	int ind = tdata->ind;
	thread_common(pthread_self(), tdata);
    unsigned long abs_period_start = timespec_to_usec(&tdata->main_start);

	struct timespec t_offset;
	t_offset = usec_to_timespec(ind*1000);
	tdata->main_start = timespec_add(&tdata->main_start, &t_offset);


    gd_off_timing_meta_t *timings;
    long duration_usec = (tdata->duration * 1e6);
    int nperiods = (int) floor(duration_usec /
	               (double) timespec_to_usec(&tdata->period));
	nperiods-=500;// probably one period less than the processing thread to avoid hanging threads.

    timings = (gd_off_timing_meta_t*) malloc ( nperiods * sizeof(gd_off_timing_meta_t));
    gd_off_timing_meta_t* timing;
    struct timespec off_start, off_end, off_task_start, off_task_end;
    struct timespec t_current;
    t_current = tdata->main_start;
	int period = 0;



    while(running && (period < nperiods)){

		int terminate_flag = 0;
        pthread_mutex_lock(&offload_mutex[ind]);

        log_debug("offloading thread[%d] is sleeping, proc thread is idle? %d\n",ind,proc_idle[ind]);
		//while processing thread not idle, keep on waiting
		while (proc_trigger[ind]==1) {
			// printf ("offloadiing thread: %d sleeeeps, proc thread is idle? %d\n",ind,proc_idle[ind]);
			offload_sleep[ind]=1;
			pthread_cond_wait(&offload_cond[ind], &offload_mutex[ind]);
			if (proc_idle[ind]==3){
				terminate_flag = 1;
			}
			log_debug ("offloading thread[%d] WAKES UP, proc thread is idle? %d\n",ind,proc_idle[ind]);
			offload_sleep[ind]=0;
			proc_trigger[ind] = 0;
			proc_idle[ind] = 1;
		}
		pthread_mutex_unlock(&offload_mutex[ind]);


        clock_gettime(CLOCK_MONOTONIC, &off_start);

		//after processing thread finishes, do we need offloading thread thread hanging?
		if(terminate_flag) {
			log_notice("offloading thread %d ordered to exit", ind);
			break;
		}

		int my_offload_flag = 0;

		//here we have to wait for task ready from some other processing thread
		pthread_mutex_lock(&task_ready_mutex[ind]);
		task_ready_flag[ind] = 0;
		while (task_ready_flag[ind]==0) {//task not ready
			log_debug ("thread [%d] waiting for offloaded task now, will it ever come?\n",ind);
			pthread_cond_wait(&task_ready_cond[ind], &task_ready_mutex[ind]);
			log_debug ("thread [%d] got a task, flag is %d",ind,task_ready_flag[ind]);
		}
		if (task_ready_flag[ind]==1){
			my_offload_flag = 1;
			log_debug("let's do some offloading :) thread[%d], is loc proc idle[%d]?",ind,proc_idle[ind]);
		}

		if (task_ready_flag[ind]==2){

			log_debug("offload thread: %d woken up by trans thread --venture",ind);
		}
		if (task_ready_flag[ind]==3) {
			terminate_flag = 1;
		}
		pthread_mutex_unlock(&task_ready_mutex[ind]);

		if(terminate_flag) {
			log_notice("offloading thread %d ordered to exit", ind);
			break;
		}



        clock_gettime(CLOCK_MONOTONIC, &off_task_start);

		//do we need a lock? -- yes
		result_ready[ind] = 0;
		//also we need a condition here so that this thread is not offloaded
		//the signal will come from the thread that has tasks to offload
		//might also come from the transport threads as well, without the data


        // work only if task ready == 1
        if (my_offload_flag==1){

            int flag = 0;
            int j;
    		for (j=0; j<500000 ;j++){

                if (j%1000 == 0){

                    //bug-fix: exit should be atomic
                    if(proc_idle[ind]==0) { // this one is not locked
        				log_debug("had to drop the offloaded task -- sadly");
                        proc_idle[ind]=0;
                        pthread_mutex_lock(&task_ready_mutex[ind]);
                        task_ready_flag[ind]=0; //same as above :)
                        pthread_mutex_unlock(&task_ready_mutex[ind]);
                        flag=1;
                        clock_gettime(CLOCK_MONOTONIC, &off_task_end);
                        j=500001;
        			}
                }
    		}


    		if (flag==0) {
                clock_gettime(CLOCK_MONOTONIC, &off_task_end);

    			result_ready[ind] =1; // this indicates the task was not dropped, and we have computed the final result
                proc_idle[ind]=0;
                pthread_mutex_lock(&task_ready_mutex[ind]);
                task_ready_flag[ind]=0; //same as above :)
                pthread_mutex_unlock(&task_ready_mutex[ind]);

                // wait for subframe mutex
                pthread_mutex_lock(&subframe_mutex[ind]);
                log_debug("offload [%d] waiting for subframe mutex...", ind);
                while (!(subframe_avail[ind] == trans_nthreads)){
                    pthread_cond_wait(&subframe_cond[ind], &subframe_mutex[ind]);
                }

                if (subframe_avail[ind]==-1){
                   log_debug(" offload thread [%d] got it!", ind);
                    break;
                }

                // consume subframe
                subframe_avail[ind] = 0;

                pthread_mutex_unlock(&subframe_mutex[ind]);


            }
        }


        clock_gettime(CLOCK_MONOTONIC, &off_end);

        // signal proc
        pthread_mutex_lock(&offload_mutex[ind]);
        proc_trigger[ind]=1;
        pthread_cond_signal(&offload_cond[ind]);
        pthread_mutex_unlock(&offload_mutex[ind]);



        timing = &timings[period];
        timing->ind = ind;
        timing->period = period;
        timing->abs_period_time = timespec_to_usec(&t_current);
        timing->rel_period_time = timing->abs_period_time - abs_period_start;
        timing->abs_start_time = timespec_to_usec(&off_start);
        timing->rel_start_time = timing->abs_start_time - abs_period_start;
        timing->abs_end_time = timespec_to_usec(&off_end);
        timing->rel_end_time = timing->abs_end_time - abs_period_start;
        timing->abs_task_start_time = timespec_to_usec(&off_task_start);
        timing->rel_task_start_time = timing->abs_task_start_time - abs_period_start;
        timing->abs_task_end_time = timespec_to_usec(&off_task_end);
        timing->rel_task_end_time = timing->abs_task_end_time - abs_period_start;
        timing->total_duration = timing->rel_end_time - timing->rel_start_time;
        timing->task_duration = timing->rel_task_end_time - timing->rel_task_start_time;

        // update to next period
        t_current = timespec_add(&t_current, &tdata->period);
        period ++;
    }

    pthread_mutex_lock(&offload_mutex[ind]);
    pthread_cond_signal(&offload_cond[ind]);
    pthread_mutex_unlock(&offload_mutex[ind]);



    log_notice("Writing to log ... offload thread %d", ind);

    // fprintf(tdata->log_handler, "#idx\t\tabs_period\t\tabs_start\t\tabs_end"
    //             "\t\tabs_task_start\t\tabs_task_end"
    //               "\t\trel_period\t\trel_start\t\trel_end\t\trel_task_start\t\trel_task_end"
    //               "\t\ttotal_duration\t\ttask_duration\n");

    fprintf(tdata->log_handler, "#idx"
                  "\t\trel_period\t\trel_start\t\trel_end\t\trel_task_start\t\trel_task_end"
                  "\t\ttotal_duration\t\ttask_duration\n");

    int i;
    for (i=0; i < nperiods-3; i++){
        fprintf(tdata->log_handler,
        // "%d\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\n",
        "%d\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\t\t%lu\n",
        timings[i].ind,
        // timings[i].abs_period_time,
        // timings[i].abs_start_time,
        // timings[i].abs_end_time,
        // timings[i].abs_task_start_time,
        // timings[i].abs_task_end_time,
        timings[i].rel_period_time,
        timings[i].rel_start_time,
        timings[i].rel_end_time,
        timings[i].rel_task_start_time,
        timings[i].rel_task_end_time,
        timings[i].total_duration,
        timings[i].task_duration
        );
    }
    fclose(tdata->log_handler);
    log_notice("Exit offloading thread %d", ind);
    pthread_exit(NULL);
}


void* trans_main(void* arg){

    gd_thread_data_t *tdata = (gd_thread_data_t *) arg;
    int ind = tdata->ind;

    thread_common(pthread_self(), tdata);
    unsigned long abs_period_start = timespec_to_usec(&tdata->main_start);


    gd_timing_meta_t *timings;
    long duration_usec = (tdata->duration * 1e6);
    int nperiods = (int) ceil( duration_usec /
            (double) timespec_to_usec(&tdata->period));
    timings = (gd_timing_meta_t*) malloc ( nperiods * sizeof(gd_timing_meta_t));
    gd_timing_meta_t* timing;


    struct timespec t_next, t_deadline, trans_start, trans_end, t_temp, t_now;

    t_next = tdata->main_start;
    int period = 0;

    while(running && (period < nperiods)){

        // get current deadline and next period
        t_deadline = timespec_add(&t_next, &tdata->deadline);
        t_next = timespec_add(&t_next, &tdata->period);

        clock_gettime(CLOCK_MONOTONIC, &trans_start);
        /******* Main transport ******/
		if (debug_trans) {
			int j;
			for(j=0; j <50000; j++){}
		} else {
	        gd_trans_read(tdata->conn_desc);
		}

        pthread_mutex_lock(&subframe_mutex[period%3]);
        subframe_avail[period%3]++;

	// hanging fix -- if trans misses a proc, reset the subframe available counter
       	if (subframe_avail[period%3] == (trans_nthreads+1)) {
               subframe_avail[period%3] = 1;
		}

		//something wrong here perhaps?
    	if (subframe_avail[period%3] == (trans_nthreads)) {
			log_debug ("signaling the offloading thread[%d] to sleep again",(period%3));
			proc_idle[period%3] = 0;
			pthread_mutex_lock(&task_ready_mutex[period%3]);
			task_ready_flag[period%3]=2;
			pthread_cond_signal(&task_ready_cond[period%3]);
			pthread_mutex_unlock(&task_ready_mutex[period%3]);
		}



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

        if (timing->actual_duration > 1000){
            log_critical("Transport overload. Thread[%d] Duration= %lu us. Reduce samples or increase threads",
                tdata->ind, timing->actual_duration);
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
    log_notice("Trans thread [%d] ran for %f s", ind, ((float) (timespec_to_usec(&t_temp)-abs_period_start))/1e6);


    fprintf(tdata->log_handler, "#idx\t\tabs_period\t\tabs_deadline\t\tabs_start\t\tabs_end"
                   "\t\trel_period\t\trel_start\t\trel_end\t\tduration\t\tslack\n");


    int i;
    for (i=0; i < nperiods; i++){
        log_timing(tdata->log_handler, &timings[i]);
    }
    fclose(tdata->log_handler);
    log_notice("Exit trans thread %d", ind);


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
    t_offset = usec_to_timespec(id*1000);
    tdata->main_start = timespec_add(&tdata->main_start, &t_offset);


    struct timespec proc_start, proc_end, t_next, t_deadline;
    gd_timing_meta_t *timings;
    long duration_usec = (tdata->duration * 1e6);
    int nperiods = (int) floor(duration_usec /
            (double) timespec_to_usec(&tdata->period));

    //nperiods reduce a little to prevents trans finishing before proc; ugly fix
    nperiods-=500;

    timings = (gd_timing_meta_t*) malloc ( nperiods * sizeof(gd_timing_meta_t));
    gd_timing_meta_t *timing;

    t_next = tdata->main_start;
    int period = 0;


    log_notice("Starting proc thread %d nperiods %d %lu", id, nperiods, timespec_to_usec(&t_offset));


    while(running && (period < nperiods)){

        t_deadline = timespec_add(&t_next, &tdata->deadline);
        t_next = timespec_add(&t_next, &tdata->period);

	//Processing thread is going to sleep; wake up the offloading thread
	//an assumption here is that the offloading thread is going to finish before
	//processing thread wakes up again

//		printf ("proc thread: %d sleeps",id);
		int terminate_flag = 0;


		pthread_mutex_lock(&offload_mutex[id]);
		log_debug ("offloading thread: %d sleeping %d ",id,offload_sleep[id]);
		proc_trigger[id]=0; //now the processing thread is idle
		pthread_cond_signal(&offload_cond[id]);
		pthread_mutex_unlock(&offload_mutex[id]);


        if (strcmp(exp_str, "offload") == 0){

            pthread_mutex_lock(&offload_mutex[id]);
            log_debug("proc thread: %d is waiting for local offoad\n",id);
            //while processing thread not idle, keep on waiting
            while (proc_trigger[id]==0) {
                // printf ("offloadiing thread: %d sleeeeps, proc thread is idle? %d\n",ind,proc_idle[ind]);
                pthread_cond_wait(&offload_cond[id], &offload_mutex[id]);
            }
//            proc_idle[id]=0;
//
            log_debug("proc thread[%d] is ready to process its data\n",id);
//
            pthread_mutex_unlock(&offload_mutex[id]);

        }


        if (subframe_avail[id]==-1){
           log_debug("proc thread [%d] got it!", id);
            break;
        }



        /****** do LTE processing *****/
        clock_gettime(CLOCK_MONOTONIC, &proc_start);
        int j;
		int flag = 0;

		int offload_ind = (id+1)%offload_nthreads;

        for(j=0; j <100000; j++){
			//here, we might want to check if offloading thread is running
			if (j == 1000) {//just an initial condition for the check if offloading thread is NOT sleeping
				pthread_mutex_lock(&offload_mutex[offload_ind]);
				if (offload_sleep[offload_ind]==0 && !flag) {
					log_debug("the offloading thread of id: %d is awake",offload_ind);
					flag = 1; //print only once :)
				}
				pthread_mutex_unlock(&offload_mutex[offload_ind]);
				int temp = rand()%10;
				if (flag == 1&& temp >-1) { // don't offload everytime for testing purposes
					flag++;
					//we can offload some tasks here
					pthread_mutex_lock(&task_ready_mutex[offload_ind]);
					log_debug("sending task to be offloaded from: %d to: %d",id,offload_ind);
					task_ready_flag[offload_ind]=1;
					pthread_cond_signal(&task_ready_cond[offload_ind]);
					pthread_mutex_unlock(&task_ready_mutex[offload_ind]);

				}
			}

			//at some point here, we check for result ready flag
		}
		//check if result is ready
        clock_gettime(CLOCK_MONOTONIC, &proc_end);
        /*****************************/
       log_debug("proc thread [%d] just finished its processing", id);

			pthread_mutex_lock(&offload_mutex[id]);
            proc_trigger[id]=0;
            pthread_mutex_unlock(&offload_mutex[id]);



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

        // printf("Thread [%d] period [%d]", id, period);


        period++;
    }
    log_notice("Writing to log ... proc thread %d", id);

    fprintf(tdata->log_handler, "#idx\t\tabs_period\t\tabs_deadline\t\tabs_start\t\tabs_end"
                   "\t\trel_period\t\trel_start\t\trel_end\t\tduration\t\tslack\n");

    int i;
    for (i=0; i < nperiods; i++){
        log_timing(tdata->log_handler, &timings[i]);
    }
    fclose(tdata->log_handler);
    log_notice("Exit proc thread %d",id);

//let all hanging offloading threads terminate at the time being
	pthread_mutex_lock(&offload_mutex[id]);
	proc_idle[id]=3; //now the processing thread is idle
	pthread_cond_signal(&offload_cond[id]);
	pthread_mutex_unlock(&offload_mutex[id]);

//let all hanging offloading threads terminate at the time being
	pthread_mutex_lock(&task_ready_mutex[(id+1)%proc_nthreads]);
	task_ready_flag[(id+1)%proc_nthreads]=3; //now the processing thread is idle
	pthread_cond_signal(&task_ready_cond[(id+1)%proc_nthreads]);
	pthread_mutex_unlock(&task_ready_mutex[(id+1)%proc_nthreads]);

    pthread_exit(NULL);
}


static void
gd_shutdown(int sig)
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
    log_notice("Received shutdown signal ...\n");
    exit(-1);
}



int main(int argc, char** argv){


	srand(time(NULL));

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
    int duration = 50; //secs
    int priority = 99;
    int sched = SCHED_FIFO;
    sprintf(exp_str, "offload");

    char c;
    while ((c = getopt (argc, argv, "h::n:s:d:p:S:e:")) != -1) {
        switch (c) {
            case 'n':
              num_nodes = atoi(optarg);
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
              switch((char)*optarg){

                  case 'P':
                    sprintf(exp_str, "plain");
                    break;

                  case 'O':
                    sprintf(exp_str, "offload");
                    break;

                  default:
                    log_error("Unsupported exp!\n");
                    exit(-1);
                    break;
              }
              break;

            case 'D':
              debug_trans = atoi(optarg);
              break;


            case 'h':
            default:
              printf("%s -h(elp) -n num_radios -s num_samples -d duration(s) -p priority(1-99) -S sched (R/F/O) -e experiment ('P'plain /'O' offload) -D transport debug(0 or 1)\n\nExample usage: sudo ./gd.o -n 4 -s 1000 -d 10 -p 99 -S F -e P -D 0\n",
                     argv[0]);
              exit(1);
              break;

        }
    }



    double complex *buffer = (double complex*) malloc(num_samples*sizeof(double complex));

    policy_to_string(sched, tmp_str_a);


    trans_nthreads = num_nodes;
    proc_nthreads = 3;
    offload_nthreads = 3;

    for (i=0; i<3; i++){
        pthread_mutex_init(&subframe_mutex[i], NULL);
        pthread_cond_init(&subframe_cond[i], NULL);
    }

	//perhaps move to another function or file for readability
	/*******variables, flags, mutexes, conditions to assist offloading*********/
	offload_mutex = malloc(offload_nthreads*sizeof(pthread_mutex_t));
	offload_cond = malloc(offload_nthreads*sizeof(pthread_cond_t));

	task_ready_mutex = malloc(offload_nthreads*sizeof(pthread_mutex_t));
	task_ready_cond = malloc(offload_nthreads*sizeof(pthread_cond_t));

	task_ready_flag = malloc(offload_nthreads*sizeof(int));

    result_ready = malloc (offload_nthreads*sizeof(int));

	for (i=0;i<offload_nthreads;i++) {

		pthread_mutex_init(&offload_mutex[i], NULL);
        pthread_cond_init(&offload_cond[i], NULL);

		pthread_mutex_init(&task_ready_mutex[i], NULL);
        pthread_cond_init(&task_ready_cond[i], NULL);

		task_ready_flag[i] = 0;
		result_ready[i] = 0;
	}

	/**************************************************************************/
    trans_threads = malloc(trans_nthreads*sizeof(pthread_t));
    gd_thread_data_t *trans_tdata;
    trans_tdata = malloc(trans_nthreads*sizeof(gd_thread_data_t));

    proc_threads = malloc(proc_nthreads*sizeof(pthread_t));
    gd_thread_data_t *proc_tdata;
    proc_tdata = malloc(proc_nthreads*sizeof(gd_thread_data_t));

    offload_threads = malloc(offload_nthreads*sizeof(pthread_t));
    gd_thread_data_t *offload_tdata;
    offload_tdata = malloc(offload_nthreads*sizeof(gd_thread_data_t));

    proc_idle = malloc (proc_nthreads*sizeof(int));
    proc_trigger = malloc (proc_nthreads*sizeof(int));


    for (i=0; i<3; i++){
        subframe_avail[i] = 0;
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
        trans_tdata[i].sched_policy = SCHED_RR;
        trans_tdata[i].deadline = usec_to_timespec(500);
        trans_tdata[i].period = usec_to_timespec(1000);

        sprintf(tmp_str, "../log/exp%s_samp%d_trans%d_prior%d_sched%s_nant%d_nproc%d.log",
            exp_str, num_samples, i, priority, tmp_str_a, trans_nthreads, proc_nthreads);
        trans_tdata[i].log_handler = fopen(tmp_str, "w");
        trans_tdata[i].sched_prio = priority;
        trans_tdata[i].cpuset = malloc(sizeof(cpu_set_t));
        CPU_SET( 20 +i%2, trans_tdata[i].cpuset);

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
        proc_tdata[i].deadline = usec_to_timespec(3000);
        proc_tdata[i].period = usec_to_timespec(3000);

        sprintf(tmp_str, "../log/exp%s_samp%d_proc%d_prior%d_sched%s_nant%d_nproc%d.log",
            exp_str, num_samples, i, priority,tmp_str_a, trans_nthreads, proc_nthreads);
        proc_tdata[i].log_handler = fopen(tmp_str, "w");
        proc_tdata[i].sched_prio = priority;
        proc_tdata[i].cpuset = malloc(sizeof(cpu_set_t));
        CPU_SET( 8+2*i, proc_tdata[i].cpuset);

		proc_idle[i] = 0;
        proc_trigger[i] = 0;

    }



    for(i= 0; i < offload_nthreads; i++){

        offload_tdata[i].ind = i;
        offload_tdata[i].duration = duration; //probably we have to revise
        offload_tdata[i].sched_policy = sched;
        offload_tdata[i].deadline = usec_to_timespec(3000);
        offload_tdata[i].period = usec_to_timespec(3000);

		//we can assume offloading thread count is always equal to proc thread count?
        sprintf(tmp_str, "../log/exp%s_samp%d_offload%d_prior%d_sched%s_nant%d_nproc%d.log",
            exp_str, num_samples, i, priority, tmp_str_a, trans_nthreads, proc_nthreads);
        offload_tdata[i].log_handler = fopen(tmp_str, "w");
        offload_tdata[i].sched_prio = priority;
        offload_tdata[i].cpuset = malloc(sizeof(cpu_set_t));
        CPU_SET( 8+2*i, offload_tdata[i].cpuset); //pin the offloading and processing threads on the same cores
        offload_sleep[i] = 1;

    }


    log_notice("Starting trans threads");

    struct timespec t_start;
    // starting time
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    // start threads
    for(i = 0; i < trans_nthreads; i++){
        trans_tdata[i].main_start = t_start;
        thread_ret = pthread_create(&trans_threads[i], NULL, trans_main, &trans_tdata[i]);
        if (thread_ret){
            log_error("Cannot start thread");
            exit(-1);
        }
	}

    struct timespec t_sleep = usec_to_timespec(200000);
    t_sleep = timespec_add(&t_start, &t_sleep);


	// nanosleep((const struct timespec[]){{0, 1000000000L}}, NULL);
    // clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t_sleep, NULL);

    log_notice("Starting proc threads");

    for(i= 0; i < proc_nthreads; i++){
        proc_tdata[i].main_start = t_start;
        thread_ret = pthread_create(&proc_threads[i], NULL, proc_main, &proc_tdata[i]);
        if (thread_ret){
            log_error("Cannot start thread");
            exit(-1);
        }
    }


    if (strcmp(exp_str, "offload") == 0){
        log_notice("Starting offloading threads");
        for(i= 0; i < offload_nthreads; i++){
            offload_tdata[i].main_start = t_start;
            thread_ret = pthread_create(&offload_threads[i], NULL, offload_main, &offload_tdata[i]);
            if (thread_ret){
                log_error("Cannot start thread");
                exit(-1);
            }
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

    if (strcmp(exp_str, "offload") == 0){

        for (i = 0; i < offload_nthreads; i++)
        {
            pthread_join(offload_threads[i], NULL);
        }
    }

	//let's not wait for the offloading threads to finish, no need I guess. -- maybe change later
    return 0;
}
