
// #include <string.h>
// #include <math.h>
// #include <unistd.h>
// #include "SIMULATION/TOOLS/defs.h"
// #include "PHY/types.h"
// #include "PHY/defs.h"
// #include "PHY/vars.h"
// #include "MAC_INTERFACE/vars.h"

// #include "SCHED/defs.h"
// #include "SCHED/vars.h"
// #include "LAYER2/MAC/vars.h"
// #include "OCG_vars.h"
// #include "pthread.h"
// #include <sched.h>

#include "gd_rx.h"



#ifdef XFORMS
#include "PHY/TOOLS/lte_phy_scope.h"
#endif

extern unsigned short dftsizes[33];
extern short *ul_ref_sigs[30][2][33];
//#define AWGN
//#define NO_DCI

// #define BW 7.68
#define BW 15.36
//#define ABSTRACTION
//#define PERFECT_CE

/*
  #define RBmask0 0x00fc00fc
  #define RBmask1 0x0
  #define RBmask2 0x0
  #define RBmask3 0x0
*/
PHY_VARS_eNB *PHY_vars_eNB;
PHY_VARS_UE *PHY_vars_UE;

#define MCS_COUNT 23//added for PHY abstraction

channel_desc_t *eNB2UE[NUMBER_OF_eNB_MAX][NUMBER_OF_UE_MAX];
channel_desc_t *UE2eNB[NUMBER_OF_UE_MAX][NUMBER_OF_eNB_MAX];
//Added for PHY abstraction
node_desc_t *enb_data[NUMBER_OF_eNB_MAX];
node_desc_t *ue_data[NUMBER_OF_UE_MAX];
//double sinr_bler_map[MCS_COUNT][2][16];

extern uint16_t beta_ack[16],beta_ri[16],beta_cqi[16];
//extern  char* namepointer_chMag ;



#ifdef XFORMS
FD_lte_phy_scope_enb *form_enb;
char title[255];
#endif

/*the following parameters are used to control the processing times*/
double t_tx_max = -1000000000; /*!< \brief initial max process time for tx */
double t_rx_max = -1000000000; /*!< \brief initial max process time for rx */
double t_tx_min = 1000000000; /*!< \brief initial min process time for tx */
double t_rx_min = 1000000000; /*!< \brief initial min process time for tx */
int n_tx_dropped = 0; /*!< \brief initial max process time for tx */
int n_rx_dropped = 0; /*!< \brief initial max process time for rx */

void lte_param_init(unsigned char N_tx, unsigned char N_rx,unsigned char transmission_mode,uint8_t extended_prefix_flag,uint8_t N_RB_DL,uint8_t frame_type,uint8_t tdd_config,uint8_t osf)
{

  LTE_DL_FRAME_PARMS *lte_frame_parms;

  // printf("Start lte_param_init\n");
  PHY_vars_eNB = malloc(sizeof(PHY_VARS_eNB));
  PHY_vars_UE = malloc(sizeof(PHY_VARS_UE));
  //PHY_config = malloc(sizeof(PHY_CONFIG));
  mac_xface = malloc(sizeof(MAC_xface));

  randominit(0);
  set_taus_seed(0);

  lte_frame_parms = &(PHY_vars_eNB->lte_frame_parms);

  lte_frame_parms->frame_type         = frame_type;
  lte_frame_parms->tdd_config         = tdd_config;
  lte_frame_parms->N_RB_DL            = N_RB_DL;   //50 for 10MHz and 25 for 5 MHz
  lte_frame_parms->N_RB_UL            = N_RB_DL;
  lte_frame_parms->Ncp                = extended_prefix_flag;
  lte_frame_parms->Ncp_UL             = extended_prefix_flag;
  lte_frame_parms->Nid_cell           = 10;
  lte_frame_parms->nushift            = 0;
  lte_frame_parms->nb_antennas_tx     = N_tx;
  lte_frame_parms->nb_antennas_rx     = N_rx;
  //  lte_frame_parms->Csrs = 2;
  //  lte_frame_parms->Bsrs = 0;
  //  lte_frame_parms->kTC = 0;
  //  lte_frame_parms->n_RRC = 0;
  lte_frame_parms->mode1_flag = (transmission_mode == 1)? 1 : 0;
  lte_frame_parms->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift = 0;//n_DMRS1 set to 0

  init_frame_parms(lte_frame_parms,osf);

  //copy_lte_parms_to_phy_framing(lte_frame_parms, &(PHY_config->PHY_framing));

  PHY_vars_UE->lte_frame_parms = *lte_frame_parms;

  phy_init_lte_top(lte_frame_parms);

  phy_init_lte_ue(PHY_vars_UE,1,0);

  phy_init_lte_eNB(PHY_vars_eNB,0,0,0);

  // printf("Done lte_param_init\n");
}



#define UL_RB_ALLOC 0x1ff;

void runul(void* ptr);

typedef struct tdata{
int argc;
char** argv;
short* iqr;
short* iqi;
} thread_data;



int main(int argc, char **argv){

  pthread_t tid;
  thread_data ttdata;
  ttdata.argc = argc;
  ttdata.argv = argv;

   pthread_attr_t thread_attr;
   int s = 0;
   size_t tmp_size=0;
   s = pthread_attr_init(&thread_attr);
   assert(s==0);
   s = pthread_attr_setstacksize(&thread_attr , 128*PTHREAD_STACK_MIN);
   assert(s==0);
   s = pthread_attr_getstacksize(&thread_attr , &tmp_size );
   assert(s==0);
   s = pthread_attr_setschedpolicy(&thread_attr , SCHED_FIFO);
   assert(s==0);

  struct sched_param tparam;
  tparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
   sched_setscheduler(getpid(), SCHED_FIFO, &tparam);

   pthread_attr_setinheritsched(&thread_attr, PTHREAD_EXPLICIT_SCHED);


   printf("forced stack size of pthread is:%zu\n", tmp_size);

   short* iqr = (short*) malloc(1*2*30720000*sizeof(short)); //1=no_of_frame/1000, 2=BW/5MHz
   short* iqi = (short*) malloc(1*2*30720000*sizeof(short));

   if (iqr == NULL || iqi == NULL){
    printf("Cannot allocate memory!\n");
    exit(-1);
   }


  int ch=0;
  float snr=0;
  int nrb=0;
  int subf=0;
  int rx=0;
  int mcs=0;
  char c;

  FILE* fq = fopen("temp6.txt", "r");
  fscanf(fq, "%d\n", &mcs);
  fscanf(fq, "%f\n", &snr);
  fscanf(fq, "%d\n", &nrb);
  fscanf(fq, "%d\n", &subf);
  fscanf(fq, "%d\n", &ch);
  fscanf(fq, "%d", &rx);


  printf("SNR = %f\n", snr);
  FILE* file_iq;
  char filename_iq[500];
  sprintf(filename_iq, "/mnt/hd3/gkchai/ul/iq_mcs=%d_snr=%f_nrb=%d_subf=%d_ch=%d_rx=%d.dat", mcs, snr, nrb, subf, ch, rx);
  printf("Reading ... %s\n", filename_iq);
  file_iq = fopen(filename_iq, "r");
  int i = 0;
  int j = 0;

  // changed 2*30720000 to 2*3072000 ?
  while (fscanf(file_iq, "%hi\t%hi\n", &iqr[i], &iqi[j]) != EOF && i < 2*30720000){
    i++;
    j++;
   }

   ttdata.iqr = iqr;
   ttdata.iqi = iqi;

  tparam.sched_priority = sched_get_priority_max(SCHED_FIFO);
   s = pthread_attr_setschedparam(&thread_attr , &tparam);
   assert(s==0);

 //  int pcreate = pthread_create (&tid, &thread_attr, (void *) &runul, (void *) &ttdata);
 //  // int pcreate = pthread_create (&tid, NULL, (void *) &runul, (void *) &ttdata);
 //  if (pcreate!=0){
 //    printf("Error initializing thread!\n");
 //    exit(-1);
 //  }

 //  pthread_join(tid, NULL);

 //  fclose(file_iq);
 //  free(iqr);
 //  free(iqi);

 // printf("Thread returned\n");
 //  exit(0);

  runul((void*) &ttdata);

}



void runul(void* ptr){
// int main(int argc, char **argv){


   int schedType;

   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
     case SCHED_FIFO:
     printf("Pthread Policy is SCHED_FIFO\n");
     break;
     case SCHED_OTHER:
     printf("Pthread Policy is SCHED_OTHER\n");
       break;
     case SCHED_RR:
     printf("Pthread Policy is SCHED_OTHER\n");
     break;
     default:
       printf("Pthread Policy is UNKNOWN\n");
   }


  struct sched_param tparam;
  // int policy = SCHED_FIFO;
  // tparam.sched_priority = sched_get_priority_max(policy);
  // int tret = pthread_setschedparam(pthread_self(), policy, &tparam);
  // if (tret!=0)
  //     printf("Cannot set the thread priority!\n");

  // int policy=0;
  // pthread_getschedparam(pthread_self(), &policy, &tparam);
  // printf("Thread has policy= %d, priority=%d\n", policy,  tparam.sched_priority);

  thread_data* ttdata = (thread_data*) ptr;
  int argc = ttdata->argc;
  char **argv = ttdata->argv;
  short* iqr = ttdata->iqr;
  short* iqi = ttdata->iqi;

  char c;
  int i,j,aa,u;

  int aarx,aatx;
  double channelx,channely;
  double sigma2, sigma2_dB=10,SNR,SNR2,snr0=-2.0,snr1,SNRmeas,rate,saving_bler;
  double input_snr_step=.2,snr_int=30;
  double blerr;

  //int **txdataF, **txdata;

  int **txdata;

  LTE_DL_FRAME_PARMS *frame_parms;
  double **s_re,**s_im,**r_re,**r_im;
  double forgetting_factor=0.0; //in [0,1] 0 means a new channel every time, 1 means keep the same channel
  double iqim=0.0;
  uint8_t extended_prefix_flag=0;
  int cqi_flag=0,cqi_error,cqi_errors,ack_errors,cqi_crc_falsepositives,cqi_crc_falsenegatives;
  int ch_realization;
  int eNB_id = 0;
  int chMod = 0 ;
  int UE_id = 0;
  unsigned char nb_rb=25,first_rb=0,mcs=0,round=0,bundling_flag=1;
  unsigned char l;

  unsigned char awgn_flag = 0 ;
  SCM_t channel_model=Rice1;


  unsigned char *input_buffer,harq_pid;
  unsigned short input_buffer_length;
  unsigned int ret;
  unsigned int coded_bits_per_codeword,nsymb;
  int subframe=3;
  unsigned int tx_lev=0,tx_lev_dB,trials,errs[4]= {0,0,0,0},round_trials[4]= {0,0,0,0};
  uint8_t transmission_mode=1,n_rx=1,n_tx=1;

  FILE *bler_fd=NULL;
  char bler_fname[512];

  FILE *time_meas_fd=NULL;
  char time_meas_fname[512];

  FILE *input_fdUL=NULL,*trch_out_fdUL=NULL;
  //  unsigned char input_file=0;
  short input_val_str, input_val_str2;

  //  FILE *rx_frame_file;
  FILE *csv_fdUL=NULL;

  FILE *fperen=NULL;
  char fperen_name[512];

  FILE *fmageren=NULL;
  char fmageren_name[512];

  FILE *flogeren=NULL;
  char flogeren_name[512];

  /* FILE *ftxlev;
     char ftxlev_name[512];
  */

  char csv_fname[512];
  int n_frames=1000;

  int harq_rounds[n_frames*10];
  int turbo_iter[6*n_frames*10];
  int final_err[n_frames*10];

  int n_ch_rlz = 1;
  int abstx = 0;
  int hold_channel=0;
  channel_desc_t *UE2eNB;

  uint8_t control_only_flag = 0;
  int delay = 0;
  double maxDoppler = 0.0;
  uint8_t srs_flag = 0;

  uint8_t N_RB_DL=25,osf=1;

  uint8_t cyclic_shift = 0;
  uint8_t cooperation_flag = 0; //0 no cooperation, 1 delay diversity, 2 Alamouti
  uint8_t beta_ACK=0,beta_RI=0,beta_CQI=2;
  uint8_t tdd_config=3,frame_type=FDD;

  uint8_t N0=30;
  double tx_gain=1.0;
  double cpu_freq_GHz;
  int avg_iter,iter_trials;

  uint32_t UL_alloc_pdu;
  int s,Kr,Kr_bytes;
  int dump_perf=0;
  int test_perf=0;
  int dump_table =0;

  double effective_rate=0.0;
  char channel_model_input[10];

  uint8_t max_turbo_iterations=4;
  uint8_t llr8_flag=0;
  int nb_rb_set = 0;
  int sf;

  opp_enabled=1; // to enable the time meas

  cpu_freq_GHz = (double)get_cpu_freq_GHz();

  // printf("Detected cpu_freq %f GHz\n",cpu_freq_GHz);


  logInit();

  while ((c = getopt (argc, argv, "hapZbm:n:Y:X:x:s:w:e:q:d:D:O:c:r:i:f:y:c:oA:C:R:g:N:l:S:T:QB:PI:L")) != -1) {
    switch (c) {
    case 'a':
      channel_model = AWGN;
      chMod = 1;
      break;

    case 'b':
      bundling_flag = 0;
      break;

    case 'd':
      delay = atoi(optarg);
      break;

    case 'D':
      maxDoppler = atoi(optarg);
      break;

    case 'm':
      mcs = atoi(optarg);
      break;

    case 'n':
      n_frames = atoi(optarg);
      break;

    case 'Y':
      n_ch_rlz = atoi(optarg);
      break;

    case 'X':
      abstx= atoi(optarg);
      break;

    case 'g':
      sprintf(channel_model_input,optarg,10);

      switch((char)*optarg) {
      case 'A':
        channel_model=SCM_A;
        chMod = 2;
        break;

      case 'B':
        channel_model=SCM_B;
        chMod = 3;
        break;

      case 'C':
        channel_model=SCM_C;
        chMod = 4;
        break;

      case 'D':
        channel_model=SCM_D;
        chMod = 5;
        break;

      case 'E':
        channel_model=EPA;
        chMod = 6;
        break;

      case 'F':
        channel_model=EVA;
        chMod = 7;
        break;

      case 'G':
        channel_model=ETU;
        chMod = 8;
        break;

      case 'H':
        channel_model=Rayleigh8;
        chMod = 9;
        break;

      case 'I':
        channel_model=Rayleigh1;
        chMod = 10;
        break;

      case 'J':
        channel_model=Rayleigh1_corr;
        chMod = 11;
        break;

      case 'K':
        channel_model=Rayleigh1_anticorr;
        chMod = 12;
        break;

      case 'L':
        channel_model=Rice8;
        chMod = 13;
        break;

      case 'M':
        channel_model=Rice1;
        chMod = 14;
        break;

      case 'N':
        channel_model=AWGN;
        chMod = 1;
        break;

      default:
        msg("Unsupported channel model!\n");
        exit(-1);
        break;
      }

      break;

    case 's':
      snr0 = atof(optarg);
      break;

    case 'w':
      snr_int = atof(optarg);
      break;

    case 'e':
      input_snr_step= atof(optarg);
      break;

    case 'x':
      transmission_mode=atoi(optarg);

      if ((transmission_mode!=1) &&
          (transmission_mode!=2)) {
        msg("Unsupported transmission mode %d\n",transmission_mode);
        exit(-1);
      }

      if (transmission_mode>1) {
        n_tx = 1;
      }

      break;

    case 'y':
      n_rx = atoi(optarg);
      break;

    case 'S':
      subframe = atoi(optarg);
      break;

    case 'T':
      tdd_config=atoi(optarg);
      frame_type=TDD;
      break;

    case 'p':
      extended_prefix_flag=1;
      break;

    case 'r':
      nb_rb = atoi(optarg);
      nb_rb_set = 1;
      break;

    case 'f':
      first_rb = atoi(optarg);
      break;

    case 'c':
      cyclic_shift = atoi(optarg);
      break;

    case 'N':
      N0 = atoi(optarg);
      break;

    case 'o':
      srs_flag = 1;
      break;

    case 'i':
      input_fdUL = fopen(optarg,"r");
      printf("Reading in %s (%p)\n",optarg,input_fdUL);

      if (input_fdUL == (FILE*)NULL) {
        printf("Unknown file %s\n",optarg);
        exit(-1);
      }

      //      input_file=1;
      break;

    case 'A':
      beta_ACK = atoi(optarg);

      if (beta_ACK>15) {
        printf("beta_ack must be in (0..15)\n");
        exit(-1);
      }

      break;

    case 'C':
      beta_CQI = atoi(optarg);

      if ((beta_CQI>15)||(beta_CQI<2)) {
        printf("beta_cqi must be in (2..15)\n");
        exit(-1);
      }

      break;

    case 'R':
      beta_RI = atoi(optarg);

      if ((beta_RI>15)||(beta_RI<2)) {
        printf("beta_ri must be in (0..13)\n");
        exit(-1);
      }

      break;

    case 'Q':
      cqi_flag=1;
      break;

    case 'B':
      N_RB_DL=atoi(optarg);
      break;

    case 'P':
      dump_perf=1;
      opp_enabled=1;
      break;

    case 'O':
      test_perf=atoi(optarg);
      //print_perf =1;
      break;

    case 'L':
      llr8_flag=1;
      break;

    case 'I':
      max_turbo_iterations=atoi(optarg);
      break;

    case 'Z':
      dump_table = 1;
      break;

    case 'h':
    default:
      printf("%s -h(elp) -a(wgn on) -m mcs -n n_frames -s snr0 -t delay_spread -p (extended prefix on) -r nb_rb -f first_rb -c cyclic_shift -o (srs on) -g channel_model [A:M] Use 3GPP 25.814 SCM-A/B/C/D('A','B','C','D') or 36-101 EPA('E'), EVA ('F'),ETU('G') models (ignores delay spread and Ricean factor), Rayghleigh8 ('H'), Rayleigh1('I'), Rayleigh1_corr('J'), Rayleigh1_anticorr ('K'), Rice8('L'), Rice1('M'), -d Channel delay, -D maximum Doppler shift \n",
             argv[0]);
      exit(1);
      break;
    }
  }

  lte_param_init(1,n_rx,1,extended_prefix_flag,N_RB_DL,frame_type,tdd_config,osf);

  if (nb_rb_set == 0){
    nb_rb = PHY_vars_eNB->lte_frame_parms.N_RB_UL;
    }

  // printf("nb_rb=%d\n", PHY_vars_eNB->lte_frame_parms.N_RB_UL);
  // printf("1 . rxdataF_comp[0] %p\n",PHY_vars_eNB->lte_eNB_pusch_vars[0]->rxdataF_comp[0][0]);
  // printf("Setting mcs = %d\n",mcs);
  // printf("n_frames = %d\n", n_frames);

  snr1 = snr0+snr_int;
  // printf("SNR0 %f, SNR1 %f\n",snr0,snr1);

  /*
    txdataF    = (int **)malloc16(2*sizeof(int*));
    txdataF[0] = (int *)malloc16(FRAME_LENGTH_BYTES);
    txdataF[1] = (int *)malloc16(FRAME_LENGTH_BYTES);

    txdata    = (int **)malloc16(2*sizeof(int*));
    txdata[0] = (int *)malloc16(FRAME_LENGTH_BYTES);
    txdata[1] = (int *)malloc16(FRAME_LENGTH_BYTES);
  */

  frame_parms = &PHY_vars_eNB->lte_frame_parms;

  txdata = PHY_vars_UE->lte_ue_common_vars.txdata;


  s_re = malloc(2*sizeof(double*));
  s_im = malloc(2*sizeof(double*));
  r_re = malloc(2*sizeof(double*));
  r_im = malloc(2*sizeof(double*));
  //  r_re0 = malloc(2*sizeof(double*));
  //  r_im0 = malloc(2*sizeof(double*));

  nsymb = (PHY_vars_eNB->lte_frame_parms.Ncp == 0) ? 14 : 12;

  coded_bits_per_codeword = nb_rb * (12 * get_Qm(mcs)) * nsymb;

  // rate = (double)dlsch_tbs25[get_I_TBS(mcs)][nb_rb-1]/(coded_bits_per_codeword);
  rate = (double)2*dlsch_tbs25[get_I_TBS(mcs)][25-1]/(coded_bits_per_codeword);


  double freq = 2.79;
  sprintf(bler_fname,"/home/gkchai/gkchai/fall15/garud/log/ul/ULbler_mcs%d_snr%f_nrb%d_subf%d_ch%d_rx=%d_miter=%d.csv",mcs, snr0, nb_rb, subframe, chMod, n_rx, max_turbo_iterations);
  bler_fd = fopen(bler_fname,"w");
  fprintf(bler_fd,"#SNR;mcs;nb_rb;TBS;rate;errors[0];trials[0];errors[1];trials[1];errors[2];trials[2];errors[3];trials[3]\n");

  sprintf(time_meas_fname,"/home/gkchai/gkchai/fall15/garud/log/ul/time_mcs=%d_snr%f_nrb=%d_subf=%d_ch=%d_rx=%d_miter=%d.csv", mcs, snr0, nb_rb, subframe, chMod, n_rx, max_turbo_iterations);
  time_meas_fd = fopen(time_meas_fname,"w");

  printf("Writing to file = %s\n", time_meas_fname);
  printf("Rate = %f (mod %d), coded bits %d\n",rate,get_Qm(mcs),coded_bits_per_codeword);


  /*
    sprintf(ftxlev_name,"txlevel_mcs%d_rb%d_chanMod%d_nframes%d_chanReal%d.m",mcs,nb_rb,chMod,n_frames,n_ch_rlz);
    ftxlev = fopen(ftxlev_name,"a+");
    fprintf(ftxlev,"txlev = [");
    fclose(ftexlv);
  */


  for (i=0; i<2; i++) {
    s_re[i] = malloc(FRAME_LENGTH_COMPLEX_SAMPLES*sizeof(double));
    s_im[i] = malloc(FRAME_LENGTH_COMPLEX_SAMPLES*sizeof(double));
    r_re[i] = malloc(FRAME_LENGTH_COMPLEX_SAMPLES*sizeof(double));
    r_im[i] = malloc(FRAME_LENGTH_COMPLEX_SAMPLES*sizeof(double));
    //    r_re0[i] = malloc(FRAME_LENGTH_COMPLEX_SAMPLES*sizeof(double));
    //    bzero(r_re0[i],FRAME_LENGTH_COMPLEX_SAMPLES*sizeof(double));
    //    r_im0[i] = malloc(FRAME_LENGTH_COMPLEX_SAMPLES*sizeof(double));
    //    bzero(r_im0[i],FRAME_LENGTH_COMPLEX_SAMPLES*sizeof(double));
  }


#ifdef XFORMS
  fl_initialize (&argc, argv, NULL, 0, 0);
  form_enb = create_lte_phy_scope_enb();
  sprintf (title, "LTE PHY SCOPE eNB");
  fl_show_form (form_enb->lte_phy_scope_enb, FL_PLACE_HOTSPOT, FL_FULLBORDER, title);
#endif

  PHY_vars_UE->lte_ue_pdcch_vars[0]->crnti = 14;

  PHY_vars_UE->lte_frame_parms.soundingrs_ul_config_common.srs_BandwidthConfig = 2;
  PHY_vars_UE->lte_frame_parms.soundingrs_ul_config_common.srs_SubframeConfig = 7;
  PHY_vars_UE->soundingrs_ul_config_dedicated[eNB_id].srs_Bandwidth = 0;
  PHY_vars_UE->soundingrs_ul_config_dedicated[eNB_id].transmissionComb = 0;
  PHY_vars_UE->soundingrs_ul_config_dedicated[eNB_id].freqDomainPosition = 0;

  PHY_vars_eNB->lte_frame_parms.soundingrs_ul_config_common.srs_BandwidthConfig = 2;
  PHY_vars_eNB->lte_frame_parms.soundingrs_ul_config_common.srs_SubframeConfig = 7;

  PHY_vars_eNB->soundingrs_ul_config_dedicated[UE_id].srs_ConfigIndex = 1;
  PHY_vars_eNB->soundingrs_ul_config_dedicated[UE_id].srs_Bandwidth = 0;
  PHY_vars_eNB->soundingrs_ul_config_dedicated[UE_id].transmissionComb = 0;
  PHY_vars_eNB->soundingrs_ul_config_dedicated[UE_id].freqDomainPosition = 0;
  PHY_vars_eNB->cooperation_flag = cooperation_flag;
  //  PHY_vars_eNB->eNB_UE_stats[0].SRS_parameters = PHY_vars_UE->SRS_parameters;

  PHY_vars_eNB->pusch_config_dedicated[UE_id].betaOffset_ACK_Index = beta_ACK;
  PHY_vars_eNB->pusch_config_dedicated[UE_id].betaOffset_RI_Index  = beta_RI;
  PHY_vars_eNB->pusch_config_dedicated[UE_id].betaOffset_CQI_Index = beta_CQI;
  PHY_vars_UE->pusch_config_dedicated[eNB_id].betaOffset_ACK_Index = beta_ACK;
  PHY_vars_UE->pusch_config_dedicated[eNB_id].betaOffset_RI_Index  = beta_RI;
  PHY_vars_UE->pusch_config_dedicated[eNB_id].betaOffset_CQI_Index = beta_CQI;

  PHY_vars_UE->ul_power_control_dedicated[eNB_id].deltaMCS_Enabled = 1;

  // printf("PUSCH Beta : ACK %f, RI %f, CQI %f\n",(double)beta_ack[beta_ACK]/8,(double)beta_ri[beta_RI]/8,(double)beta_cqi[beta_CQI]/8);

  UE2eNB = new_channel_desc_scm(PHY_vars_eNB->lte_frame_parms.nb_antennas_tx,
                                PHY_vars_UE->lte_frame_parms.nb_antennas_rx,
                                channel_model,
                                BW,
                                forgetting_factor,
                                delay,
                                0);
  // set Doppler
  UE2eNB->max_Doppler = maxDoppler;

  // NN: N_RB_UL has to be defined in ulsim
  PHY_vars_eNB->ulsch_eNB[0] = new_eNB_ulsch(8,max_turbo_iterations,N_RB_DL,0);
  PHY_vars_UE->ulsch_ue[0]   = new_ue_ulsch(8,N_RB_DL,0);

  // Create transport channel structures for 2 transport blocks (MIMO)
  for (i=0; i<2; i++) {
    PHY_vars_eNB->dlsch_eNB[0][i] = new_eNB_dlsch(1,8,N_RB_DL,0);
    PHY_vars_UE->dlsch_ue[0][i]  = new_ue_dlsch(1,8,MAX_TURBO_ITERATIONS,N_RB_DL,0);

    if (!PHY_vars_eNB->dlsch_eNB[0][i]) {
      printf("Can't get eNB dlsch structures\n");
      exit(-1);
    }

    if (!PHY_vars_UE->dlsch_ue[0][i]) {
      printf("Can't get ue dlsch structures\n");
      exit(-1);
    }

    PHY_vars_eNB->dlsch_eNB[0][i]->rnti = 14;
    PHY_vars_UE->dlsch_ue[0][i]->rnti   = 14;

  }


  switch (PHY_vars_eNB->lte_frame_parms.N_RB_UL) {
  case 6:
    break;

  case 25:
    if (PHY_vars_eNB->lte_frame_parms.frame_type == TDD) {
      ((DCI0_5MHz_TDD_1_6_t*)&UL_alloc_pdu)->type    = 0;
      ((DCI0_5MHz_TDD_1_6_t*)&UL_alloc_pdu)->rballoc = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_UL,first_rb,nb_rb);// 12 RBs from position 8
      // printf("nb_rb %d/%d, rballoc %d (dci %x)\n",nb_rb,PHY_vars_eNB->lte_frame_parms.N_RB_UL,((DCI0_5MHz_TDD_1_6_t*)&UL_alloc_pdu)->rballoc,*(uint32_t *)&UL_alloc_pdu);
      ((DCI0_5MHz_TDD_1_6_t*)&UL_alloc_pdu)->mcs     = mcs;
      ((DCI0_5MHz_TDD_1_6_t*)&UL_alloc_pdu)->ndi     = 1;
      ((DCI0_5MHz_TDD_1_6_t*)&UL_alloc_pdu)->TPC     = 0;
      ((DCI0_5MHz_TDD_1_6_t*)&UL_alloc_pdu)->cqi_req = cqi_flag&1;
      ((DCI0_5MHz_TDD_1_6_t*)&UL_alloc_pdu)->cshift  = 0;
      ((DCI0_5MHz_TDD_1_6_t*)&UL_alloc_pdu)->dai     = 1;
    } else {
      ((DCI0_5MHz_FDD_t*)&UL_alloc_pdu)->type    = 0;
      ((DCI0_5MHz_FDD_t*)&UL_alloc_pdu)->rballoc = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_UL,first_rb,nb_rb);// 12 RBs from position 8
      // printf("nb_rb %d/%d, rballoc %d (dci %x)\n",nb_rb,PHY_vars_eNB->lte_frame_parms.N_RB_UL,((DCI0_5MHz_FDD_t*)&UL_alloc_pdu)->rballoc,*(uint32_t *)&UL_alloc_pdu);
      ((DCI0_5MHz_FDD_t*)&UL_alloc_pdu)->mcs     = mcs;
      ((DCI0_5MHz_FDD_t*)&UL_alloc_pdu)->ndi     = 1;
      ((DCI0_5MHz_FDD_t*)&UL_alloc_pdu)->TPC     = 0;
      ((DCI0_5MHz_FDD_t*)&UL_alloc_pdu)->cqi_req = cqi_flag&1;
      ((DCI0_5MHz_FDD_t*)&UL_alloc_pdu)->cshift  = 0;
    }

    break;

  case 50:
    if (PHY_vars_eNB->lte_frame_parms.frame_type == TDD) {
      ((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->type    = 0;
      ((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->rballoc = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_UL,first_rb,nb_rb);// 12 RBs from position 8
      printf("nb_rb %d/%d, rballoc %d (dci %x)\n",nb_rb,PHY_vars_eNB->lte_frame_parms.N_RB_UL,((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->rballoc,*(uint32_t *)&UL_alloc_pdu);
      ((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->mcs     = mcs;
      ((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->ndi     = 1;
      ((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->TPC     = 0;
      ((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->cqi_req = cqi_flag&1;
      ((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->cshift  = 0;
      ((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->dai     = 1;
    } else {
      ((DCI0_10MHz_FDD_t*)&UL_alloc_pdu)->type    = 0;
      ((DCI0_10MHz_FDD_t*)&UL_alloc_pdu)->rballoc = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_UL,first_rb,nb_rb);// 12 RBs from position 8
      printf("nb_rb %d/%d, rballoc %d (dci %x)\n",nb_rb,PHY_vars_eNB->lte_frame_parms.N_RB_UL,((DCI0_10MHz_FDD_t*)&UL_alloc_pdu)->rballoc,*(uint32_t *)&UL_alloc_pdu);
      ((DCI0_10MHz_FDD_t*)&UL_alloc_pdu)->mcs     = mcs;
      ((DCI0_10MHz_FDD_t*)&UL_alloc_pdu)->ndi     = 1;
      ((DCI0_10MHz_FDD_t*)&UL_alloc_pdu)->TPC     = 0;
      ((DCI0_10MHz_FDD_t*)&UL_alloc_pdu)->cqi_req = cqi_flag&1;
      ((DCI0_10MHz_FDD_t*)&UL_alloc_pdu)->cshift  = 0;
    }

    break;

  case 100:
    if (PHY_vars_eNB->lte_frame_parms.frame_type == TDD) {
      ((DCI0_20MHz_TDD_1_6_t*)&UL_alloc_pdu)->type    = 0;
      ((DCI0_20MHz_TDD_1_6_t*)&UL_alloc_pdu)->rballoc = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_UL,first_rb,nb_rb);// 12 RBs from position 8
      printf("nb_rb %d/%d, rballoc %d (dci %x)\n",nb_rb,PHY_vars_eNB->lte_frame_parms.N_RB_UL,((DCI0_20MHz_TDD_1_6_t*)&UL_alloc_pdu)->rballoc,*(uint32_t *)&UL_alloc_pdu);
      ((DCI0_20MHz_TDD_1_6_t*)&UL_alloc_pdu)->mcs     = mcs;
      ((DCI0_20MHz_TDD_1_6_t*)&UL_alloc_pdu)->ndi     = 1;
      ((DCI0_20MHz_TDD_1_6_t*)&UL_alloc_pdu)->TPC     = 0;
      ((DCI0_20MHz_TDD_1_6_t*)&UL_alloc_pdu)->cqi_req = cqi_flag&1;
      ((DCI0_20MHz_TDD_1_6_t*)&UL_alloc_pdu)->cshift  = 0;
      ((DCI0_20MHz_TDD_1_6_t*)&UL_alloc_pdu)->dai     = 1;
    } else {
      ((DCI0_20MHz_FDD_t*)&UL_alloc_pdu)->type    = 0;
      ((DCI0_20MHz_FDD_t*)&UL_alloc_pdu)->rballoc = computeRIV(PHY_vars_eNB->lte_frame_parms.N_RB_UL,first_rb,nb_rb);// 12 RBs from position 8
      printf("nb_rb %d/%d, rballoc %d (dci %x)\n",nb_rb,PHY_vars_eNB->lte_frame_parms.N_RB_UL,((DCI0_20MHz_FDD_t*)&UL_alloc_pdu)->rballoc,*(uint32_t *)&UL_alloc_pdu);
      ((DCI0_20MHz_FDD_t*)&UL_alloc_pdu)->mcs     = mcs;
      ((DCI0_20MHz_FDD_t*)&UL_alloc_pdu)->ndi     = 1;
      ((DCI0_20MHz_FDD_t*)&UL_alloc_pdu)->TPC     = 0;
      ((DCI0_20MHz_FDD_t*)&UL_alloc_pdu)->cqi_req = cqi_flag&1;
      ((DCI0_20MHz_FDD_t*)&UL_alloc_pdu)->cshift  = 0;
    }

    break;

  default:
    break;
  }


  PHY_vars_UE->PHY_measurements.rank[0] = 0;
  PHY_vars_UE->transmission_mode[0] = 2;
  PHY_vars_UE->pucch_config_dedicated[0].tdd_AckNackFeedbackMode = bundling_flag == 1 ? bundling : multiplexing;
  PHY_vars_eNB->transmission_mode[0] = 2;
  PHY_vars_eNB->pucch_config_dedicated[0].tdd_AckNackFeedbackMode = bundling_flag == 1 ? bundling : multiplexing;
  PHY_vars_UE->lte_frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.groupHoppingEnabled = 1;
  PHY_vars_eNB->lte_frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.groupHoppingEnabled = 1;
  PHY_vars_UE->lte_frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.sequenceHoppingEnabled = 0;
  PHY_vars_eNB->lte_frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.sequenceHoppingEnabled = 0;
  PHY_vars_UE->lte_frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.groupAssignmentPUSCH = 0;
  PHY_vars_eNB->lte_frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.groupAssignmentPUSCH = 0;
  PHY_vars_UE->frame_tx=1;

  for (sf=0; sf<10; sf++) {
    PHY_vars_eNB->proc[sf].frame_tx=1;
    PHY_vars_eNB->proc[sf].subframe_tx=sf;
    PHY_vars_eNB->proc[sf].frame_rx=1;
    PHY_vars_eNB->proc[sf].subframe_rx=sf;
  }

  msg("Init UL hopping UE\n");
  init_ul_hopping(&PHY_vars_UE->lte_frame_parms);
  msg("Init UL hopping eNB\n");
  init_ul_hopping(&PHY_vars_eNB->lte_frame_parms);

  PHY_vars_eNB->proc[subframe].frame_rx = PHY_vars_UE->frame_tx;

  if (ul_subframe2pdcch_alloc_subframe(&PHY_vars_eNB->lte_frame_parms,subframe) > subframe) // allocation was in previous frame
    PHY_vars_eNB->proc[ul_subframe2pdcch_alloc_subframe(&PHY_vars_eNB->lte_frame_parms,subframe)].frame_tx = (PHY_vars_UE->frame_tx-1)&1023;

  PHY_vars_UE->dlsch_ue[0][0]->harq_ack[ul_subframe2pdcch_alloc_subframe(&PHY_vars_eNB->lte_frame_parms,subframe)].send_harq_status = 1;


  //  printf("UE frame %d, eNB frame %d (eNB frame_tx %d)\n",PHY_vars_UE->frame,PHY_vars_eNB->proc[subframe].frame_rx,PHY_vars_eNB->proc[ul_subframe2pdcch_alloc_subframe(&PHY_vars_eNB->lte_frame_parms,subframe)].frame_tx);
  PHY_vars_UE->frame_tx = (PHY_vars_UE->frame_tx-1)&1023;

    printf("**********************here=1**************************\n");


  generate_ue_ulsch_params_from_dci((void *)&UL_alloc_pdu,
                                    14,
                                    ul_subframe2pdcch_alloc_subframe(&PHY_vars_UE->lte_frame_parms,subframe),
                                    format0,
                                    PHY_vars_UE,
                                    SI_RNTI,
                                    0,
                                    P_RNTI,
                                    CBA_RNTI,
                                    0,
                                    srs_flag);

 if (PHY_vars_eNB->ulsch_eNB[0] != NULL)

  generate_eNB_ulsch_params_from_dci((void *)&UL_alloc_pdu,
                                     14,
                                     ul_subframe2pdcch_alloc_subframe(&PHY_vars_eNB->lte_frame_parms,subframe),
                                     format0,
                                     0,
                                     PHY_vars_eNB,
                                     SI_RNTI,
                                     0,
                                     P_RNTI,
                                     CBA_RNTI,
                                     srs_flag);

  PHY_vars_UE->frame_tx = (PHY_vars_UE->frame_tx+1)&1023;

    printf("**********************here=2**************************\n");


  for (ch_realization=0; ch_realization<n_ch_rlz; ch_realization++) {

    /*
      if(abstx){
      int ulchestim_f[300*12];
      int ulchestim_t[2*(frame_parms->ofdm_symbol_size)];
      }
    */

    if(abstx) {
      printf("**********************Channel Realization Index = %d **************************\n", ch_realization);
      saving_bler=1;
    }


    //    if ((subframe>5) || (subframe < 4))
    //      PHY_vars_UE->frame++;

    for (SNR=snr0; SNR<snr1; SNR+=input_snr_step) {


      errs[0]=0;
      errs[1]=0;
      errs[2]=0;
      errs[3]=0;
      round_trials[0] = 0;
      round_trials[1] = 0;
      round_trials[2] = 0;
      round_trials[3] = 0;
      cqi_errors=0;
      ack_errors=0;
      cqi_crc_falsepositives=0;
      cqi_crc_falsenegatives=0;
      round=0;

      //randominit(0);
      printf("**********************here=3**************************\n");


      harq_pid = subframe2harq_pid(&PHY_vars_UE->lte_frame_parms,PHY_vars_UE->frame_tx,subframe);

      avg_iter = 0;
      iter_trials=0;

      reset_meas(&PHY_vars_eNB->phy_proc_rx);
      reset_meas(&PHY_vars_eNB->ofdm_demod_stats);
      reset_meas(&PHY_vars_eNB->ulsch_channel_estimation_stats);
      reset_meas(&PHY_vars_eNB->ulsch_freq_offset_estimation_stats);
      reset_meas(&PHY_vars_eNB->rx_dft_stats);
      reset_meas(&PHY_vars_eNB->ulsch_decoding_stats);
      reset_meas(&PHY_vars_eNB->ulsch_turbo_decoding_stats);
      reset_meas(&PHY_vars_eNB->ulsch_deinterleaving_stats);
      reset_meas(&PHY_vars_eNB->ulsch_demultiplexing_stats);
      reset_meas(&PHY_vars_eNB->ulsch_rate_unmatching_stats);
      reset_meas(&PHY_vars_eNB->ulsch_tc_init_stats);
      reset_meas(&PHY_vars_eNB->ulsch_tc_alpha_stats);
      reset_meas(&PHY_vars_eNB->ulsch_tc_beta_stats);
      reset_meas(&PHY_vars_eNB->ulsch_tc_gamma_stats);
      reset_meas(&PHY_vars_eNB->ulsch_tc_ext_stats);
      reset_meas(&PHY_vars_eNB->ulsch_tc_intl1_stats);
      reset_meas(&PHY_vars_eNB->ulsch_tc_intl2_stats);

      // initialization
      struct list time_vector_rx;
      initialize(&time_vector_rx);
      struct list time_vector_rx_fft;
      initialize(&time_vector_rx_fft);
      struct list time_vector_rx_demod;
      initialize(&time_vector_rx_demod);
      struct list time_vector_rx_dec;
      initialize(&time_vector_rx_dec);

      int super_trials;
      for(super_trials=0; super_trials<10; super_trials++){
      for (trials = 0; trials<n_frames; trials++) {
        //      printf("*");
        //        PHY_vars_UE->frame++;
        //        PHY_vars_eNB->frame++;


        fflush(stdout);
        round=0;

        while (round < 4) {
          PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->round=round;
          PHY_vars_UE->ulsch_ue[0]->harq_processes[harq_pid]->round=round;
           // printf("Trial %d : Round %d ",trials,round);
          round_trials[round]++;

          if (round == 0) {
            //PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->Ndi = 1;
            PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->rvidx = round>>1;
            //PHY_vars_UE->ulsch_ue[0]->harq_processes[harq_pid]->Ndi = 1;
            PHY_vars_UE->ulsch_ue[0]->harq_processes[harq_pid]->rvidx = round>>1;
          } else {
            //PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->Ndi = 0;
            PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->rvidx = round>>1;
            //PHY_vars_UE->ulsch_ue[0]->harq_processes[harq_pid]->Ndi = 0;
            PHY_vars_UE->ulsch_ue[0]->harq_processes[harq_pid]->rvidx = round>>1;
          }


          /////////////////////

            hold_channel = 0;
            flagMag=1;



            aa = 0; int ii=0;
            for(ii=0; ii< PHY_vars_eNB->lte_frame_parms.samples_per_tti; ii++){
                ((short*) &PHY_vars_eNB->lte_eNB_common_vars.rxdata[0][aa][PHY_vars_eNB->lte_frame_parms.samples_per_tti*subframe])[2*ii] = iqr[ii + (4*trials + round)*PHY_vars_eNB->lte_frame_parms.samples_per_tti];
                ((short*) &PHY_vars_eNB->lte_eNB_common_vars.rxdata[0][aa][PHY_vars_eNB->lte_frame_parms.samples_per_tti*subframe])[2*ii +1] = iqi[ii + (4*trials + round)*PHY_vars_eNB->lte_frame_parms.samples_per_tti];
            }



          SNRmeas = 10*log10(((double)signal_energy((int*)&PHY_vars_eNB->lte_eNB_common_vars.rxdata[0][0][160+(PHY_vars_eNB->lte_frame_parms.samples_per_tti*subframe)],
                              OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES/2))/((double)signal_energy((int*)
                                  &PHY_vars_eNB->lte_eNB_common_vars.rxdata[0][0][(PHY_vars_eNB->lte_frame_parms.samples_per_tti<<1) -PHY_vars_eNB->lte_frame_parms.ofdm_symbol_size],
                                  OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES/2)) - 1)+10*log10(PHY_vars_eNB->lte_frame_parms.N_RB_UL/nb_rb);

          // printf("MEASURESD snr = %f, aditional term = %f dB\n", SNRmeas -27.6, 10*log10(PHY_vars_eNB->lte_frame_parms.N_RB_UL/nb_rb));


#ifndef OFDMA_ULSCH
          remove_7_5_kHz(PHY_vars_eNB,subframe<<1);
          remove_7_5_kHz(PHY_vars_eNB,1+(subframe<<1));
#endif

          start_meas(&PHY_vars_eNB->phy_proc_rx);
          start_meas(&PHY_vars_eNB->ofdm_demod_stats);
          lte_eNB_I0_measurements(PHY_vars_eNB,
                                  0,
                                  1);

          for (l=subframe*PHY_vars_UE->lte_frame_parms.symbols_per_tti; l<((1+subframe)*PHY_vars_UE->lte_frame_parms.symbols_per_tti); l++) {

            slot_fep_ul(&PHY_vars_eNB->lte_frame_parms,
                        &PHY_vars_eNB->lte_eNB_common_vars,
                        l%(PHY_vars_eNB->lte_frame_parms.symbols_per_tti/2),
                        l/(PHY_vars_eNB->lte_frame_parms.symbols_per_tti/2),
                        0,
                        0);
          }

          stop_meas(&PHY_vars_eNB->ofdm_demod_stats);

          PHY_vars_eNB->ulsch_eNB[0]->cyclicShift = cyclic_shift;// cyclic shift for DMRS

          if(abstx) {
            namepointer_log2 = &flogeren_name;
            namepointer_chMag = &fmageren_name;
            //namepointer_txlev = &ftxlev;
          }

          start_meas(&PHY_vars_eNB->ulsch_demodulation_stats);
          rx_ulsch(PHY_vars_eNB,
                   subframe,
                   0,  // this is the effective sector id
                   0,  // this is the UE_id
                   PHY_vars_eNB->ulsch_eNB,
                   cooperation_flag);
          stop_meas(&PHY_vars_eNB->ulsch_demodulation_stats);


          ///////

          start_meas(&PHY_vars_eNB->ulsch_decoding_stats);
          ret= ulsch_decoding(PHY_vars_eNB,
                              0, // UE_id
                              subframe,
                              control_only_flag,
                              1,  // Nbundled
                              llr8_flag);
          stop_meas(&PHY_vars_eNB->ulsch_decoding_stats);
          stop_meas(&PHY_vars_eNB->phy_proc_rx);

          if (cqi_flag > 0) {
            cqi_error = 0;

            if (PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->Or1 < 32) {
              for (i=2; i<4; i++) {
                //                printf("cqi %d : %d (%d)\n",i,PHY_vars_eNB->ulsch_eNB[0]->o[i],PHY_vars_UE->ulsch_ue[0]->o[i]);
                if (PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->o[i] != PHY_vars_UE->ulsch_ue[0]->o[i])
                  cqi_error = 1;
              }
            } else {

            }

            if (cqi_error == 1) {
              cqi_errors++;

              if (PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->cqi_crc_status == 1)
                cqi_crc_falsepositives++;
            } else {
              if (PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->cqi_crc_status == 0)
                cqi_crc_falsenegatives++;
            }
          }

          if (PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->o_ACK[0] != PHY_vars_UE->ulsch_ue[0]->o_ACK[0])
            ack_errors++;

          if (ret <= PHY_vars_eNB->ulsch_eNB[0]->max_turbo_iterations) {

            // avg_iter += ret-1;
            avg_iter += ret;
            iter_trials++;
            // turbo_iter[1000*super_trials + 4*trials + round] = ret-1;
            turbo_iter[6000*super_trials + 6*trials + round] = ret;
            harq_rounds[1000*super_trials + trials] = round;
            final_err[1000*super_trials + trials] = 0;

            round=5;

          } else {
            avg_iter += ret-1;
            iter_trials++;

            errs[round]++;
            //      printf("round %d errors %d/%d\n",round,errs[round],trials);

            turbo_iter[6000*super_trials + 6*trials + round] = ret-1;
            harq_rounds[1000*super_trials + trials] = round;
            final_err[1000*super_trials + trials] = 1;




            round++;

          }  // ulsch error

              {

                  double t_rx = (double)PHY_vars_eNB->phy_proc_rx.p_time/cpu_freq_GHz/1000.0;
                  double t_rx_fft = (double)PHY_vars_eNB->ofdm_demod_stats.p_time/cpu_freq_GHz/1000.0;
                  double t_rx_demod = (double)PHY_vars_eNB->ulsch_demodulation_stats.p_time/cpu_freq_GHz/1000.0;
                  double t_rx_dec = (double)PHY_vars_eNB->ulsch_decoding_stats.p_time/cpu_freq_GHz/1000.0;


                  if (t_rx > t_rx_max)
                    t_rx_max = t_rx;

                  if (t_rx < t_rx_min)
                    t_rx_min = t_rx;

                  if (t_rx > 2000)
                    n_rx_dropped++;


                  push_front(&time_vector_rx, t_rx);
                  push_front(&time_vector_rx_fft, t_rx_fft);
                  push_front(&time_vector_rx_demod, t_rx_demod);
                  push_front(&time_vector_rx_dec, t_rx_dec);

              }


        } // round





        //      printf("\n");
        // if ((errs[0]>=100) && (trials>(n_frames/2)))
          // break;

#ifdef XFORMS
        phy_scope_eNB(form_enb,PHY_vars_eNB,0);
#endif
        /*calculate the total processing time for each packet, get the max, min, and number of packets that exceed t>3000us*/


        // if (trials == 100)
        // {

        //         reset_meas(&PHY_vars_eNB->phy_proc_rx);
        //         reset_meas(&PHY_vars_eNB->ofdm_demod_stats);
        //         reset_meas(&PHY_vars_eNB->ulsch_channel_estimation_stats);
        //         reset_meas(&PHY_vars_eNB->ulsch_freq_offset_estimation_stats);
        //         reset_meas(&PHY_vars_eNB->rx_dft_stats);
        //         reset_meas(&PHY_vars_eNB->ulsch_demodulation_stats);
        //         reset_meas(&PHY_vars_eNB->ulsch_decoding_stats);
        //         reset_meas(&PHY_vars_eNB->ulsch_turbo_decoding_stats);
        //         reset_meas(&PHY_vars_eNB->ulsch_deinterleaving_stats);
        //         reset_meas(&PHY_vars_eNB->ulsch_demultiplexing_stats);
        //         reset_meas(&PHY_vars_eNB->ulsch_rate_unmatching_stats);
        //         reset_meas(&PHY_vars_eNB->ulsch_tc_init_stats);
        //         reset_meas(&PHY_vars_eNB->ulsch_tc_alpha_stats);
        //         reset_meas(&PHY_vars_eNB->ulsch_tc_beta_stats);
        //         reset_meas(&PHY_vars_eNB->ulsch_tc_gamma_stats);
        //         reset_meas(&PHY_vars_eNB->ulsch_tc_ext_stats);
        //         reset_meas(&PHY_vars_eNB->ulsch_tc_intl1_stats);
        //         reset_meas(&PHY_vars_eNB->ulsch_tc_intl2_stats);
        // }

        // if (trials >= 100)
        // {

        //     double t_rx = (double)PHY_vars_eNB->phy_proc_rx.p_time/cpu_freq_GHz/1000.0;
        //     double t_rx_fft = (double)PHY_vars_eNB->ofdm_demod_stats.p_time/cpu_freq_GHz/1000.0;
        //     double t_rx_demod = (double)PHY_vars_eNB->ulsch_demodulation_stats.p_time/cpu_freq_GHz/1000.0;
        //     double t_rx_dec = (double)PHY_vars_eNB->ulsch_decoding_stats.p_time/cpu_freq_GHz/1000.0;


        //     if (t_rx > t_rx_max)
        //       t_rx_max = t_rx;

        //     if (t_rx < t_rx_min)
        //       t_rx_min = t_rx;

        //     if (t_rx > 2000)
        //       n_rx_dropped++;


        //     push_front(&time_vector_rx, t_rx);
        //     push_front(&time_vector_rx_fft, t_rx_fft);
        //     push_front(&time_vector_rx_demod, t_rx_demod);
        //     push_front(&time_vector_rx_dec, t_rx_dec);

        // }

      }   //trials
      }   //supertrials

      double table_rx[time_vector_rx.size];
      totable(table_rx, &time_vector_rx);
      // sorttable(table_rx, time_vector_rx.size);
      double table_rx_fft[time_vector_rx_fft.size];
      totable(table_rx_fft, &time_vector_rx_fft);
      // sorttable(table_rx_fft, time_vector_rx_fft.size);
      double table_rx_demod[time_vector_rx_demod.size];
      totable(table_rx_demod, &time_vector_rx_demod);
      // sorttable(table_rx_demod, time_vector_rx_demod.size);
      double table_rx_dec[time_vector_rx_dec.size];
      totable(table_rx_dec, &time_vector_rx_dec);
      // sorttable(table_rx_dec, time_vector_rx_dec.size);



       {
        int n;
        FILE* ft;
        char filename_time[500];
        sprintf(filename_time,"/home/gkchai/gkchai/fall15/garud/log/ul/trial_time_mcs=%d_snr%f_nrb=%d_subf=%d_ch=%d_rx=%d_miter=%d.csv", mcs, snr0, nb_rb, subframe, chMod, n_rx, max_turbo_iterations);
        ft = fopen(filename_time,"w");

        // printf("time_rx_size = %d\n", time_vector_rx.size);
        // for (n=0; n< time_vector_rx.size; n++) {
        //   // printf("%f ", table_rx[n]);
        //   fprintf(ft, "%f;%f;\n", table_rx[n], table_rx_dec[n]);
        // }

        int m;
        int count=time_vector_rx.size-1;
        for (int super_frames = 0; super_frames<10; super_frames++){
          for(n=0; n<n_frames; n++){
            // printf("%d, %d\n", turbo_iter[n*4], harq_rounds[n]);
            for(m=0; m<harq_rounds[1000*super_frames + n]+1; m++){

              fprintf(ft, "%f;%f;%d;%d;%d;%d\n", table_rx[count], table_rx_dec[count], super_frames*1000 + n, turbo_iter[6000*super_frames + n*6 + m], m, final_err[1000*super_frames + n]);
              count--;
             }
          }
        }

        // assert(time_vector_rx.size == count-1);

        fclose(ft);

      }


    // sort table
      qsort(table_rx, time_vector_rx.size, sizeof(double), &compare);
      qsort(table_rx_fft, time_vector_rx_fft.size, sizeof(double), &compare);
      qsort(table_rx_demod, time_vector_rx_demod.size, sizeof(double), &compare);
      qsort(table_rx_dec, time_vector_rx_dec.size, sizeof(double), &compare);



      double rx_median = table_rx[time_vector_rx.size/2];
      double rx_q05 = table_rx[(int)(0.05*time_vector_rx.size)];
      double rx_q95 = table_rx[(int)(0.95*time_vector_rx.size)];
      double rx_max = table_rx[time_vector_rx.size-1];
      double rx_min = table_rx[0];

      double rx_fft_median = table_rx_fft[time_vector_rx_fft.size/2];
      double rx_fft_q05 = table_rx_fft[(int)(0.05*time_vector_rx_fft.size)];
      double rx_fft_q95 = table_rx_fft[(int)(0.95*time_vector_rx_fft.size)];
      double rx_fft_max = table_rx_fft[time_vector_rx_fft.size-1];
      double rx_fft_min = table_rx_fft[0];

      double rx_demod_median = table_rx_demod[time_vector_rx_demod.size/2];
      double rx_demod_q05 = table_rx_demod[(int)(0.05*time_vector_rx_demod.size)];
      double rx_demod_q95 = table_rx_demod[(int)(0.95*time_vector_rx_demod.size)];
      double rx_demod_max = table_rx_demod[time_vector_rx_demod.size-1];
      double rx_demod_min = table_rx_demod[0];

      double rx_dec_median = table_rx_dec[time_vector_rx_dec.size/2];
      double rx_dec_q05 = table_rx_dec[(int)(0.05*time_vector_rx_dec.size)];
      double rx_dec_q95 = table_rx_dec[(int)(0.95*time_vector_rx_dec.size)];
      double rx_dec_max = table_rx_dec[time_vector_rx_dec.size-1];
      double rx_dec_min = table_rx_dec[0];


      double std_phy_proc_rx=0;
      double std_phy_proc_rx_fft=0;
      double std_phy_proc_rx_demod=0;
      double std_phy_proc_rx_dec=0;

      printf("\n**********rb: %d ***mcs : %d  *********SNR = %f dB (%f): TX %d dB (gain %f dB), N0W %f dB, I0 %d dB, delta_IF %d [ (%d,%d) dB / (%d,%d) dB ]**************************\n",
             nb_rb,mcs,SNR,SNR2,
             tx_lev_dB,
             20*log10(tx_gain),
             (double)N0,
             PHY_vars_eNB->PHY_measurements_eNB[0].n0_power_tot_dB,
             get_hundred_times_delta_IF(PHY_vars_UE,eNB_id,harq_pid) ,
             dB_fixed(PHY_vars_eNB->lte_eNB_pusch_vars[0]->ulsch_power[0]),
             dB_fixed(PHY_vars_eNB->lte_eNB_pusch_vars[0]->ulsch_power[1]),
             PHY_vars_eNB->PHY_measurements_eNB->n0_power_dB[0],
             PHY_vars_eNB->PHY_measurements_eNB->n0_power_dB[1]);

      effective_rate = ((double)(round_trials[0])/((double)round_trials[0] + round_trials[1] + round_trials[2] + round_trials[3]));

      printf("Errors (%d/%d %d/%d %d/%d %d/%d), Pe = (%e,%e,%e,%e) => effective rate %f (%3.1f%%,%f,%f), normalized delay %f (%f)\n",
             errs[0],
             round_trials[0],
             errs[1],
             round_trials[1],
             errs[2],
             round_trials[2],
             errs[3],
             round_trials[3],
             (double)errs[0]/(round_trials[0]),
             (double)errs[1]/(round_trials[0]),
             (double)errs[2]/(round_trials[0]),
             (double)errs[3]/(round_trials[0]),
             rate*effective_rate,
             100*effective_rate,
             rate,
             rate*get_Qm(mcs),
             (1.0*(round_trials[0]-errs[0])+2.0*(round_trials[1]-errs[1])+3.0*(round_trials[2]-errs[2])+4.0*(round_trials[3]-errs[3]))/((double)round_trials[0])/
             (double)PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->TBS,
             (1.0*(round_trials[0]-errs[0])+2.0*(round_trials[1]-errs[1])+3.0*(round_trials[2]-errs[2])+4.0*(round_trials[3]-errs[3]))/((double)round_trials[0]));

      if (cqi_flag >0) {
        printf("CQI errors %d/%d,false positives %d/%d, CQI false negatives %d/%d\n",
               cqi_errors,round_trials[0]+round_trials[1]+round_trials[2]+round_trials[3],
               cqi_crc_falsepositives,round_trials[0]+round_trials[1]+round_trials[2]+round_trials[3],
               cqi_crc_falsenegatives,round_trials[0]+round_trials[1]+round_trials[2]+round_trials[3]);
      }

      if (PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->o_ACK[0] > 0)
        printf("ACK/NAK errors %d/%d\n",ack_errors,round_trials[0]+round_trials[1]+round_trials[2]+round_trials[3]);


      fprintf(bler_fd,"%f;%d;%d;%d;%f;%d;%d;%d;%d;%d;%d;%d;%d\n",
              SNR,
              mcs,
              nb_rb,
              PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->TBS,
              rate,
              errs[0],
              round_trials[0],
              errs[1],
              round_trials[1],
              errs[2],
              round_trials[2],
              errs[3],
              round_trials[3]);


      if (dump_perf==1) {

        printf("\n\neNB RX function statistics (per 1ms subframe)\n\n");
        std_phy_proc_rx = sqrt((double)PHY_vars_eNB->phy_proc_rx.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                               2)/PHY_vars_eNB->phy_proc_rx.trials - pow((double)PHY_vars_eNB->phy_proc_rx.diff/PHY_vars_eNB->phy_proc_rx.trials/cpu_freq_GHz/1000,2));
        printf("Total PHY proc rx                  :%f us (%d trials)\n",(double)PHY_vars_eNB->phy_proc_rx.diff/PHY_vars_eNB->phy_proc_rx.trials/cpu_freq_GHz/1000.0,PHY_vars_eNB->phy_proc_rx.trials);
        printf("|__ Statistcs                           std: %fus max: %fus min: %fus median %fus q1 %fus q3 %fus n_dropped: %d packet \n", std_phy_proc_rx, rx_max, t_rx_min, rx_median, rx_q05, rx_q95,
               n_rx_dropped);
        std_phy_proc_rx_fft = sqrt((double)PHY_vars_eNB->ofdm_demod_stats.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                                   2)/PHY_vars_eNB->ofdm_demod_stats.trials - pow((double)PHY_vars_eNB->ofdm_demod_stats.diff/PHY_vars_eNB->ofdm_demod_stats.trials/cpu_freq_GHz/1000,2));
        printf("OFDM_demod time                   :%f us (%d trials)\n",(double)PHY_vars_eNB->ofdm_demod_stats.diff/PHY_vars_eNB->ofdm_demod_stats.trials/cpu_freq_GHz/1000.0,
               PHY_vars_eNB->ofdm_demod_stats.trials);
        printf("|__ Statistcs                           std: %fus median %fus q1 %fus q3 %fus \n", std_phy_proc_rx_fft, rx_fft_median, rx_fft_q05, rx_fft_q95);
        std_phy_proc_rx_demod = sqrt((double)PHY_vars_eNB->ulsch_demodulation_stats.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                                     2)/PHY_vars_eNB->ulsch_demodulation_stats.trials - pow((double)PHY_vars_eNB->ulsch_demodulation_stats.diff/PHY_vars_eNB->ulsch_demodulation_stats.trials/cpu_freq_GHz/1000,2));
        printf("ULSCH demodulation time           :%f us (%d trials)\n",(double)PHY_vars_eNB->ulsch_demodulation_stats.diff/PHY_vars_eNB->ulsch_demodulation_stats.trials/cpu_freq_GHz/1000.0,
               PHY_vars_eNB->ulsch_demodulation_stats.trials);
        printf("|__ Statistcs                           std: %fus max: %fus min: %fus median %fus q1 %fus q3 %fus \n", std_phy_proc_rx_demod, rx_demod_max, rx_demod_min, rx_demod_median, rx_demod_q05, rx_demod_q95);
        std_phy_proc_rx_dec = sqrt((double)PHY_vars_eNB->ulsch_decoding_stats.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                                   2)/PHY_vars_eNB->ulsch_decoding_stats.trials - pow((double)PHY_vars_eNB->ulsch_decoding_stats.diff/PHY_vars_eNB->ulsch_decoding_stats.trials/cpu_freq_GHz/1000,2));
        printf("ULSCH Decoding time (%.2f Mbit/s, avg iter %f)      :%f us (%d trials, max %f)\n",
               PHY_vars_UE->ulsch_ue[0]->harq_processes[harq_pid]->TBS/1000.0,(double)avg_iter/iter_trials,
               (double)PHY_vars_eNB->ulsch_decoding_stats.diff/PHY_vars_eNB->ulsch_decoding_stats.trials/cpu_freq_GHz/1000.0,PHY_vars_eNB->ulsch_decoding_stats.trials,
               (double)PHY_vars_eNB->ulsch_decoding_stats.max/cpu_freq_GHz/1000.0);
        printf("|__ Statistcs                           std: %fus max: %fus min: %fus median %fus q1 %fus q3 %fus \n", std_phy_proc_rx_dec,  rx_dec_max, rx_dec_min, rx_dec_median, rx_dec_q05, rx_dec_q95);
        printf("|__ sub-block interleaving                          %f us (%d trials)\n",
               (double)PHY_vars_eNB->ulsch_deinterleaving_stats.diff/PHY_vars_eNB->ulsch_deinterleaving_stats.trials/cpu_freq_GHz/1000.0,PHY_vars_eNB->ulsch_deinterleaving_stats.trials);
        printf("|__ demultiplexing                                  %f us (%d trials)\n",
               (double)PHY_vars_eNB->ulsch_demultiplexing_stats.diff/PHY_vars_eNB->ulsch_demultiplexing_stats.trials/cpu_freq_GHz/1000.0,PHY_vars_eNB->ulsch_demultiplexing_stats.trials);
        printf("|__ rate-matching                                   %f us (%d trials)\n",
               (double)PHY_vars_eNB->ulsch_rate_unmatching_stats.diff/PHY_vars_eNB->ulsch_rate_unmatching_stats.trials/cpu_freq_GHz/1000.0,PHY_vars_eNB->ulsch_rate_unmatching_stats.trials);
        printf("|__ turbo_decoder(%d bits)                              %f us (%d cycles, %d trials)\n",
               PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->Cminus ? PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->Kminus : PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->Kplus,
               (double)PHY_vars_eNB->ulsch_turbo_decoding_stats.diff/PHY_vars_eNB->ulsch_turbo_decoding_stats.trials/cpu_freq_GHz/1000.0,
               (int)((double)PHY_vars_eNB->ulsch_turbo_decoding_stats.diff/PHY_vars_eNB->ulsch_turbo_decoding_stats.trials),PHY_vars_eNB->ulsch_turbo_decoding_stats.trials);
        printf("    |__ init                                            %f us (cycles/iter %f, %d trials)\n",
               (double)PHY_vars_eNB->ulsch_tc_init_stats.diff/PHY_vars_eNB->ulsch_tc_init_stats.trials/cpu_freq_GHz/1000.0,
               (double)PHY_vars_eNB->ulsch_tc_init_stats.diff/PHY_vars_eNB->ulsch_tc_init_stats.trials/((double)avg_iter/iter_trials),
               PHY_vars_eNB->ulsch_tc_init_stats.trials);
        printf("    |__ alpha                                           %f us (cycles/iter %f, %d trials)\n",
               (double)PHY_vars_eNB->ulsch_tc_alpha_stats.diff/PHY_vars_eNB->ulsch_tc_alpha_stats.trials/cpu_freq_GHz/1000.0,
               (double)PHY_vars_eNB->ulsch_tc_alpha_stats.diff/PHY_vars_eNB->ulsch_tc_alpha_stats.trials*2,
               PHY_vars_eNB->ulsch_tc_alpha_stats.trials);
        printf("    |__ beta                                            %f us (cycles/iter %f,%d trials)\n",
               (double)PHY_vars_eNB->ulsch_tc_beta_stats.diff/PHY_vars_eNB->ulsch_tc_beta_stats.trials/cpu_freq_GHz/1000.0,
               (double)PHY_vars_eNB->ulsch_tc_beta_stats.diff/PHY_vars_eNB->ulsch_tc_beta_stats.trials*2,
               PHY_vars_eNB->ulsch_tc_beta_stats.trials);
        printf("    |__ gamma                                           %f us (cycles/iter %f,%d trials)\n",
               (double)PHY_vars_eNB->ulsch_tc_gamma_stats.diff/PHY_vars_eNB->ulsch_tc_gamma_stats.trials/cpu_freq_GHz/1000.0,
               (double)PHY_vars_eNB->ulsch_tc_gamma_stats.diff/PHY_vars_eNB->ulsch_tc_gamma_stats.trials*2,
               PHY_vars_eNB->ulsch_tc_gamma_stats.trials);
        printf("    |__ ext                                             %f us (cycles/iter %f,%d trials)\n",
               (double)PHY_vars_eNB->ulsch_tc_ext_stats.diff/PHY_vars_eNB->ulsch_tc_ext_stats.trials/cpu_freq_GHz/1000.0,
               (double)PHY_vars_eNB->ulsch_tc_ext_stats.diff/PHY_vars_eNB->ulsch_tc_ext_stats.trials*2,
               PHY_vars_eNB->ulsch_tc_ext_stats.trials);
        printf("    |__ intl1                                           %f us (cycles/iter %f,%d trials)\n",
               (double)PHY_vars_eNB->ulsch_tc_intl1_stats.diff/PHY_vars_eNB->ulsch_tc_intl1_stats.trials/cpu_freq_GHz/1000.0,
               (double)PHY_vars_eNB->ulsch_tc_intl1_stats.diff/PHY_vars_eNB->ulsch_tc_intl1_stats.trials,
               PHY_vars_eNB->ulsch_tc_intl1_stats.trials);
        printf("    |__ intl2+HD+CRC                                    %f us (cycles/iter %f,%d trials)\n",
               (double)PHY_vars_eNB->ulsch_tc_intl2_stats.diff/PHY_vars_eNB->ulsch_tc_intl2_stats.trials/cpu_freq_GHz/1000.0,
               (double)PHY_vars_eNB->ulsch_tc_intl2_stats.diff/PHY_vars_eNB->ulsch_tc_intl2_stats.trials,
               PHY_vars_eNB->ulsch_tc_intl2_stats.trials);
      }


        //fprintf(time_meas_fd,"SNR; MCS; TBS; rate; err0; trials0; err1; trials1; err2; trials2; err3; trials3\n");
        fprintf(time_meas_fd,"%f;%d;%d;%f;%d;%d;%d;%d;%d;%d;%d;%d;",
                SNR,
                mcs,
                PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->TBS,
                rate,
                errs[0],
                round_trials[0],
                errs[1],
                round_trials[1],
                errs[2],
                round_trials[2],
                errs[3],
                round_trials[3]);

        //fprintf(time_meas_fd,"SNR; MCS; TBS; rate; err0; trials0; err1; trials1; err2; trials2; err3; trials3;ND;\n");
        fprintf(time_meas_fd,"%f;%d;%d;%f;%2.1f;%f;%d;%d;%d;%d;%d;%d;%d;%d;%e;%e;%e;%e;%f;%f;",
                SNR,
                mcs,
                PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->TBS,
                rate*effective_rate,
                100*effective_rate,
                rate,
                errs[0],
                round_trials[0],
                errs[1],
                round_trials[1],
                errs[2],
                round_trials[2],
                errs[3],
                round_trials[3],
                (double)errs[0]/(round_trials[0]),
                (double)errs[1]/(round_trials[0]),
                (double)errs[2]/(round_trials[0]),
                (double)errs[3]/(round_trials[0]),
                (1.0*(round_trials[0]-errs[0])+2.0*(round_trials[1]-errs[1])+3.0*(round_trials[2]-errs[2])+4.0*(round_trials[3]-errs[3]))/((double)round_trials[0])/
                (double)PHY_vars_eNB->ulsch_eNB[0]->harq_processes[harq_pid]->TBS,
                (1.0*(round_trials[0]-errs[0])+2.0*(round_trials[1]-errs[1])+3.0*(round_trials[2]-errs[2])+4.0*(round_trials[3]-errs[3]))/((double)round_trials[0]));

        //fprintf(time_meas_fd,"UE_PROC_TX(%d); OFDM_MOD(%d); UL_MOD(%d); UL_ENC(%d); eNB_PROC_RX(%d); OFDM_DEMOD(%d); UL_DEMOD(%d); UL_DECOD(%d);\n",
        fprintf(time_meas_fd,"%d; %d; %d; %d;",

                PHY_vars_eNB->phy_proc_rx.trials,
                PHY_vars_eNB->ofdm_demod_stats.trials,
                PHY_vars_eNB->ulsch_demodulation_stats.trials,
                PHY_vars_eNB->ulsch_decoding_stats.trials
               );
        fprintf(time_meas_fd,"%f;%f;%f;%f;",

                get_time_meas_us(&PHY_vars_eNB->phy_proc_rx),
                get_time_meas_us(&PHY_vars_eNB->ofdm_demod_stats),
                get_time_meas_us(&PHY_vars_eNB->ulsch_demodulation_stats),
                get_time_meas_us(&PHY_vars_eNB->ulsch_decoding_stats)
               );


        //fprintf(time_meas_fd,"eNB_PROC_RX_STD;eNB_PROC_RX_MAX;eNB_PROC_RX_MIN;eNB_PROC_RX_MED;eNB_PROC_RX_Q05;eNB_PROC_RX_Q95;eNB_PROC_RX_DROPPED;\n");
        fprintf(time_meas_fd,"%f;%f;%f;%f;%f;%f;%d;", std_phy_proc_rx, t_rx_max, t_rx_min, rx_median, rx_q05, rx_q95, n_rx_dropped);

        //fprintf(time_meas_fd,"FFT;\n");
        fprintf(time_meas_fd,"%f;%f;%f;%f;%f;%f;", std_phy_proc_rx_fft, rx_fft_max, rx_fft_min, rx_fft_median, rx_fft_q05, rx_fft_q95);

        //fprintf(time_meas_fd,"DEMOD;\n");
        fprintf(time_meas_fd,"%f;%f;%f;%f;%f;%f;", std_phy_proc_rx_demod, rx_demod_max, rx_demod_min, rx_demod_median, rx_demod_q05, rx_demod_q95);

        //fprintf(time_meas_fd,"DEC;\n");
        fprintf(time_meas_fd,"%f;%f;%f;%f;%f;%f\n", std_phy_proc_rx_dec, rx_dec_max, rx_dec_min, rx_dec_median, rx_dec_q05, rx_dec_q95);


        printf("[passed] effective rate : %f  (%2.1f%%,%f)): log and break \n",rate*effective_rate, 100*effective_rate, rate );
        break;


      // if (((double)errs[0]/(round_trials[0]))<1e-2)
      //   break;


      // for (aa=0; aa<PHY_vars_eNB->lte_frame_parms.nb_antennas_rx; aa++) {
      //   fclose(file_iq[aa]);
      // }


    } // SNR


    //


    //write_output("chestim_f.m","chestf",PHY_vars_eNB->lte_eNB_pusch_vars[0]->drs_ch_estimates[0][0],300*12,2,1);
    // write_output("chestim_t.m","chestt",PHY_vars_eNB->lte_eNB_pusch_vars[0]->drs_ch_estimates_time[0][0], (frame_parms->ofdm_symbol_size)*2,2,1);

  }//ch realization


  fclose(bler_fd);
  fclose (time_meas_fd);

  printf("Freeing channel I/O\n");

  for (i=0; i<2; i++) {
    free(s_re[i]);
    free(s_im[i]);
    free(r_re[i]);
    free(r_im[i]);
  }

  free(s_re);
  free(s_im);
  free(r_re);
  free(r_im);

  //  lte_sync_time_free();

  pthread_exit(0);
  // return(0);
}



