#include "gd_rx.h"
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "SIMULATION/TOOLS/defs.h"
#include "PHY/types.h"
#include "PHY/defs.h"
#include "PHY/vars.h"
#include "MAC_INTERFACE/vars.h"

#include "SCHED/defs.h"
#include "SCHED/vars.h"
#include "LAYER2/MAC/vars.h"
#include "OCG_vars.h"
#include <sched.h>


// global variables needed across the tasks
#define BW 15.36
#define UL_RB_ALLOC 0x1ff;


PHY_VARS_eNB **PHY_vars_eNB;
PHY_VARS_UE **PHY_vars_UE;
uint8_t control_only_flag = 0;
uint8_t llr8_flag=0;
int cqi_flag=0,cqi_error,ack_errors;
double **s_re,**s_im,**r_re,**r_im;
unsigned char nb_rb=50,first_rb=0,mcs=27,bundling_flag=1;
uint8_t cyclic_shift = 0;
int subframe=3;
unsigned char harq_pid;
// LTE_DL_FRAME_PARMS *frame_parms;
uint8_t cooperation_flag = 0; //0 no cooperation, 1 delay diversity, 2 Alamouti
// uint32_t UL_alloc_pdu;


void lte_param_init(unsigned char N_tx, unsigned char N_rx,unsigned char transmission_mode,uint8_t extended_prefix_flag,uint8_t N_RB_DL,uint8_t frame_type,uint8_t tdd_config,uint8_t osf, int num_bss)
{

      LTE_DL_FRAME_PARMS *lte_frame_parms[num_bss];
      PHY_vars_eNB = malloc(num_bss*sizeof(PHY_VARS_eNB*));
      PHY_vars_UE = malloc(num_bss*sizeof(PHY_VARS_UE*));

      randominit(0);
      set_taus_seed(0);
      mac_xface = malloc(sizeof(MAC_xface));


      int i = 0;
      for (i = 0; i < num_bss; i++){
          PHY_vars_eNB[i] = malloc(sizeof(PHY_VARS_eNB));
          PHY_vars_UE[i] = malloc(sizeof(PHY_VARS_UE));

          lte_frame_parms[i] = &(PHY_vars_eNB[i]->lte_frame_parms);
          lte_frame_parms[i]->frame_type         = frame_type;
          lte_frame_parms[i]->tdd_config         = tdd_config;
          lte_frame_parms[i]->N_RB_DL            = N_RB_DL;   //50 for 10MHz and 25 for 5 MHz
          lte_frame_parms[i]->N_RB_UL            = N_RB_DL;
          lte_frame_parms[i]->Ncp                = extended_prefix_flag;
          lte_frame_parms[i]->Ncp_UL             = extended_prefix_flag;
          lte_frame_parms[i]->Nid_cell           = 10;
          lte_frame_parms[i]->nushift            = 0;
          lte_frame_parms[i]->nb_antennas_tx     = N_tx;
          lte_frame_parms[i]->nb_antennas_rx     = N_rx;
          lte_frame_parms[i]->mode1_flag = (transmission_mode == 1)? 1 : 0;
          lte_frame_parms[i]->pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift = 0;//n_DMRS1 set to 0

          init_frame_parms(lte_frame_parms[i],osf);
          PHY_vars_UE[i]->lte_frame_parms = *lte_frame_parms[i];
          phy_init_lte_top(lte_frame_parms[i]);
          phy_init_lte_ue(PHY_vars_UE[i],1,0);
          phy_init_lte_eNB(PHY_vars_eNB[i],0,0,0);

    }



}

void subtask_fft(unsigned char l, int id){

    slot_fep_ul(&PHY_vars_eNB[id]->lte_frame_parms,
                &PHY_vars_eNB[id]->lte_eNB_common_vars,
                l%(PHY_vars_eNB[id]->lte_frame_parms.symbols_per_tti/2),
                l/(PHY_vars_eNB[id]->lte_frame_parms.symbols_per_tti/2),
                0,
                0);
}


void subtask_demod(unsigned char l, int id){
    //TODO: implement this


}

void task_fft(int id){
    unsigned char l;
    for (l=subframe*PHY_vars_UE[id]->lte_frame_parms.symbols_per_tti; l<((1+subframe)*PHY_vars_UE[id]->lte_frame_parms.symbols_per_tti); l++) {
            // printf("l = %d\n",l);
            subtask_fft(l, id);
        }
}

void task_demod(int id){
    rx_ulsch(PHY_vars_eNB[id], subframe, 0, 0, PHY_vars_eNB[id]->ulsch_eNB, cooperation_flag);
}


int task_decode(int id){
    return ulsch_decoding(PHY_vars_eNB[id],0, subframe, control_only_flag, 1, llr8_flag);
}


void configure(int argc, char **argv, int trials, short* iqr, short* iqi, int mmcs, int nrx, int num_bss){

    mcs = mmcs;
    /**************************************************************************/
    char c;
    int i, j,u;
    double snr0=-2.0,snr1,rate;
    double input_snr_step=.2,snr_int=30;
    double forgetting_factor=0.0; //in [0,1] 0 means a new channel every time, 1 means keep the same channel
    uint8_t extended_prefix_flag=0;
    int eNB_id = 0;
    int chMod = 0 ;
    int UE_id = 0;
    unsigned char l;
    int **txdata;
    unsigned char awgn_flag = 0 ;
    SCM_t channel_model=Rice1;
    unsigned int coded_bits_per_codeword,nsymb;
    uint8_t transmission_mode=1,n_rx=nrx,n_tx=1;
    FILE *input_fdUL=NULL;
    short input_val_str, input_val_str2;
    int n_frames=1000;
    int n_ch_rlz = 1;
    int abstx = 0;
    channel_desc_t *UE2eNB;
    int delay = 0;
    double maxDoppler = 0.0;
    uint8_t srs_flag = 0;
    uint8_t N_RB_DL=50,osf=1;
    uint8_t beta_ACK=0,beta_RI=0,beta_CQI=2;
    uint8_t tdd_config=3,frame_type=FDD;
    uint8_t N0=30;
    double tx_gain=1.0;
    double cpu_freq_GHz;
    int s;
    int dump_perf=0;
    int test_perf=0;
    int dump_table =0;
    double effective_rate=0.0;
    char channel_model_input[10];
    uint8_t max_turbo_iterations=4;
    int nb_rb_set = 0;
    int sf;
  /***************************************************************************/

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

  lte_param_init(1,n_rx,1,extended_prefix_flag,N_RB_DL,frame_type,tdd_config,osf, num_bss);

  int loop = 0;

  for (loop = 0; loop < num_bss; loop++){




                  if (nb_rb_set == 0){
                    nb_rb = PHY_vars_eNB[loop]->lte_frame_parms.N_RB_UL;
                    }


                  // frame_parms = &PHY_vars_eNB[loop]->lte_frame_parms;
                  txdata = PHY_vars_UE[loop]->lte_ue_common_vars.txdata;

                  nsymb = (PHY_vars_eNB[loop]->lte_frame_parms.Ncp == 0) ? 14 : 12;
                  coded_bits_per_codeword = nb_rb * (12 * get_Qm(mcs)) * nsymb;
                  rate = (double)2*dlsch_tbs25[get_I_TBS(mcs)][25-1]/(coded_bits_per_codeword);

                  PHY_vars_UE[loop]->lte_ue_pdcch_vars[0]->crnti = 14;

                  PHY_vars_UE[loop]->lte_frame_parms.soundingrs_ul_config_common.srs_BandwidthConfig = 2;
                  PHY_vars_UE[loop]->lte_frame_parms.soundingrs_ul_config_common.srs_SubframeConfig = 7;
                  PHY_vars_UE[loop]->soundingrs_ul_config_dedicated[eNB_id].srs_Bandwidth = 0;
                  PHY_vars_UE[loop]->soundingrs_ul_config_dedicated[eNB_id].transmissionComb = 0;
                  PHY_vars_UE[loop]->soundingrs_ul_config_dedicated[eNB_id].freqDomainPosition = 0;

                  PHY_vars_eNB[loop]->lte_frame_parms.soundingrs_ul_config_common.srs_BandwidthConfig = 2;
                  PHY_vars_eNB[loop]->lte_frame_parms.soundingrs_ul_config_common.srs_SubframeConfig = 7;

                  PHY_vars_eNB[loop]->soundingrs_ul_config_dedicated[UE_id].srs_ConfigIndex = 1;
                  PHY_vars_eNB[loop]->soundingrs_ul_config_dedicated[UE_id].srs_Bandwidth = 0;
                  PHY_vars_eNB[loop]->soundingrs_ul_config_dedicated[UE_id].transmissionComb = 0;
                  PHY_vars_eNB[loop]->soundingrs_ul_config_dedicated[UE_id].freqDomainPosition = 0;
                  PHY_vars_eNB[loop]->cooperation_flag = cooperation_flag;
                  //  PHY_vars_eNB[loop]->eNB_UE_stats[0].SRS_parameters = PHY_vars_UE[loop]->SRS_parameters;

                  PHY_vars_eNB[loop]->pusch_config_dedicated[UE_id].betaOffset_ACK_Index = beta_ACK;
                  PHY_vars_eNB[loop]->pusch_config_dedicated[UE_id].betaOffset_RI_Index  = beta_RI;
                  PHY_vars_eNB[loop]->pusch_config_dedicated[UE_id].betaOffset_CQI_Index = beta_CQI;
                  PHY_vars_UE[loop]->pusch_config_dedicated[eNB_id].betaOffset_ACK_Index = beta_ACK;
                  PHY_vars_UE[loop]->pusch_config_dedicated[eNB_id].betaOffset_RI_Index  = beta_RI;
                  PHY_vars_UE[loop]->pusch_config_dedicated[eNB_id].betaOffset_CQI_Index = beta_CQI;

                  PHY_vars_UE[loop]->ul_power_control_dedicated[eNB_id].deltaMCS_Enabled = 1;


                  UE2eNB = new_channel_desc_scm(PHY_vars_eNB[loop]->lte_frame_parms.nb_antennas_tx,
                                                PHY_vars_UE[loop]->lte_frame_parms.nb_antennas_rx,
                                                channel_model,
                                                BW,
                                                forgetting_factor,
                                                delay,
                                                0);
                  // set Doppler
                  UE2eNB->max_Doppler = maxDoppler;

                  // NN: N_RB_UL has to be defined in ulsim
                  PHY_vars_eNB[loop]->ulsch_eNB[0] = new_eNB_ulsch(8,max_turbo_iterations,N_RB_DL,0);
                  PHY_vars_UE[loop]->ulsch_ue[0]   = new_ue_ulsch(8,N_RB_DL,0);

                  // Create transport channel structures for 2 transport blocks (MIMO)
                  for (i=0; i<2; i++) {
                    PHY_vars_eNB[loop]->dlsch_eNB[0][i] = new_eNB_dlsch(1,8,N_RB_DL,0);
                    PHY_vars_UE[loop]->dlsch_ue[0][i]  = new_ue_dlsch(1,8,MAX_TURBO_ITERATIONS,N_RB_DL,0);

                    if (!PHY_vars_eNB[loop]->dlsch_eNB[0][i]) {
                      printf("Can't get eNB dlsch structures\n");
                      exit(-1);
                    }

                    if (!PHY_vars_UE[loop]->dlsch_ue[0][i]) {
                      printf("Can't get ue dlsch structures\n");
                      exit(-1);
                    }

                    PHY_vars_eNB[loop]->dlsch_eNB[0][i]->rnti = 14;
                    PHY_vars_UE[loop]->dlsch_ue[0][i]->rnti   = 14;

                  }


                  switch (PHY_vars_eNB[loop]->lte_frame_parms.N_RB_UL) {
                  case 6:
                    break;

                  case 50:
                    if (PHY_vars_eNB[loop]->lte_frame_parms.frame_type == TDD) {
                      ((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->type    = 0;
                      ((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->rballoc = computeRIV(PHY_vars_eNB[loop]->lte_frame_parms.N_RB_UL,first_rb,nb_rb);// 12 RBs from position 8
                      printf("nb_rb %d/%d, rballoc %d (dci %x)\n",nb_rb,PHY_vars_eNB[loop]->lte_frame_parms.N_RB_UL,((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->rballoc,*(uint32_t *)&UL_alloc_pdu);
                      ((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->mcs     = mcs;
                      ((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->ndi     = 1;
                      ((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->TPC     = 0;
                      ((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->cqi_req = cqi_flag&1;
                      ((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->cshift  = 0;
                      ((DCI0_10MHz_TDD_1_6_t*)&UL_alloc_pdu)->dai     = 1;
                    } else {
                      ((DCI0_10MHz_FDD_t*)&UL_alloc_pdu)->type    = 0;
                      ((DCI0_10MHz_FDD_t*)&UL_alloc_pdu)->rballoc = computeRIV(PHY_vars_eNB[loop]->lte_frame_parms.N_RB_UL,first_rb,nb_rb);// 12 RBs from position 8
                      printf("nb_rb %d/%d, rballoc %d (dci %x)\n",nb_rb,PHY_vars_eNB[loop]->lte_frame_parms.N_RB_UL,((DCI0_10MHz_FDD_t*)&UL_alloc_pdu)->rballoc,*(uint32_t *)&UL_alloc_pdu);
                      ((DCI0_10MHz_FDD_t*)&UL_alloc_pdu)->mcs     = mcs;
                      ((DCI0_10MHz_FDD_t*)&UL_alloc_pdu)->ndi     = 1;
                      ((DCI0_10MHz_FDD_t*)&UL_alloc_pdu)->TPC     = 0;
                      ((DCI0_10MHz_FDD_t*)&UL_alloc_pdu)->cqi_req = cqi_flag&1;
                      ((DCI0_10MHz_FDD_t*)&UL_alloc_pdu)->cshift  = 0;
                    }

                    break;


                  default:
                    break;
                  }


                  PHY_vars_UE[loop]->PHY_measurements.rank[0] = 0;
                  PHY_vars_UE[loop]->transmission_mode[0] = 2;
                  PHY_vars_UE[loop]->pucch_config_dedicated[0].tdd_AckNackFeedbackMode = bundling_flag == 1 ? bundling : multiplexing;
                  PHY_vars_eNB[loop]->transmission_mode[0] = 2;
                  PHY_vars_eNB[loop]->pucch_config_dedicated[0].tdd_AckNackFeedbackMode = bundling_flag == 1 ? bundling : multiplexing;
                  PHY_vars_UE[loop]->lte_frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.groupHoppingEnabled = 1;
                  PHY_vars_eNB[loop]->lte_frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.groupHoppingEnabled = 1;
                  PHY_vars_UE[loop]->lte_frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.sequenceHoppingEnabled = 0;
                  PHY_vars_eNB[loop]->lte_frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.sequenceHoppingEnabled = 0;
                  PHY_vars_UE[loop]->lte_frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.groupAssignmentPUSCH = 0;
                  PHY_vars_eNB[loop]->lte_frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.groupAssignmentPUSCH = 0;
                  PHY_vars_UE[loop]->frame_tx=1;

                  for (sf=0; sf<10; sf++) {
                    PHY_vars_eNB[loop]->proc[sf].frame_tx=1;
                    PHY_vars_eNB[loop]->proc[sf].subframe_tx=sf;
                    PHY_vars_eNB[loop]->proc[sf].frame_rx=1;
                    PHY_vars_eNB[loop]->proc[sf].subframe_rx=sf;
                  }

                  msg("Init UL hopping UE\n");
                  init_ul_hopping(&PHY_vars_UE[loop]->lte_frame_parms);
                  msg("Init UL hopping eNB\n");
                  init_ul_hopping(&PHY_vars_eNB[loop]->lte_frame_parms);

                  PHY_vars_eNB[loop]->proc[subframe].frame_rx = PHY_vars_UE[loop]->frame_tx;

                  if (ul_subframe2pdcch_alloc_subframe(&PHY_vars_eNB[loop]->lte_frame_parms,subframe) > subframe) // allocation was in previous frame
                    PHY_vars_eNB[loop]->proc[ul_subframe2pdcch_alloc_subframe(&PHY_vars_eNB[loop]->lte_frame_parms,subframe)].frame_tx = (PHY_vars_UE[loop]->frame_tx-1)&1023;

                  PHY_vars_UE[loop]->dlsch_ue[0][0]->harq_ack[ul_subframe2pdcch_alloc_subframe(&PHY_vars_eNB[loop]->lte_frame_parms,subframe)].send_harq_status = 1;

                  PHY_vars_UE[loop]->frame_tx = (PHY_vars_UE[loop]->frame_tx-1)&1023;
                  printf("**********************here=1**************************\n");
                  generate_ue_ulsch_params_from_dci((void *)&UL_alloc_pdu,
                                                    14,
                                                    ul_subframe2pdcch_alloc_subframe(&PHY_vars_UE[loop]->lte_frame_parms,subframe),
                                                    format0,
                                                    PHY_vars_UE[loop],
                                                    SI_RNTI,
                                                    0,
                                                    P_RNTI,
                                                    CBA_RNTI,
                                                    0,
                                                    srs_flag);

                 if (PHY_vars_eNB[loop]->ulsch_eNB[0] != NULL)
                  generate_eNB_ulsch_params_from_dci((void *)&UL_alloc_pdu,
                                                     14,
                                                     ul_subframe2pdcch_alloc_subframe(&PHY_vars_eNB[loop]->lte_frame_parms,subframe),
                                                     format0,
                                                     0,
                                                     PHY_vars_eNB[loop],
                                                     SI_RNTI,
                                                     0,
                                                     P_RNTI,
                                                     CBA_RNTI,
                                                     srs_flag);

                  PHY_vars_UE[loop]->frame_tx = (PHY_vars_UE[loop]->frame_tx+1)&1023;
                  printf("**********************here=2**************************\n");

                    int round = 0;

                    harq_pid = subframe2harq_pid(&PHY_vars_UE[loop]->lte_frame_parms,PHY_vars_UE[loop]->frame_tx,subframe);
                     // fflush(stdout);

                    PHY_vars_eNB[loop]->ulsch_eNB[0]->harq_processes[harq_pid]->round=round;
                    PHY_vars_UE[loop]->ulsch_ue[0]->harq_processes[harq_pid]->round=round;
                    PHY_vars_eNB[loop]->ulsch_eNB[0]->harq_processes[harq_pid]->rvidx = round>>1;
                    PHY_vars_UE[loop]->ulsch_ue[0]->harq_processes[harq_pid]->rvidx = round>>1;


                    /////////////////////
                    int aa = 0; int ii=0;
                    for(ii=0; ii< PHY_vars_eNB[loop]->lte_frame_parms.samples_per_tti; ii++){
                        ((short*) &PHY_vars_eNB[loop]->lte_eNB_common_vars.rxdata[0][aa][PHY_vars_eNB[loop]->lte_frame_parms.samples_per_tti*subframe])[2*ii] = iqr[ii];
                        ((short*) &PHY_vars_eNB[loop]->lte_eNB_common_vars.rxdata[0][aa][PHY_vars_eNB[loop]->lte_frame_parms.samples_per_tti*subframe])[2*ii +1] = iqi[ii];
                    }

                        printf("Loaded %d IQ samples\n", PHY_vars_eNB[loop]->lte_frame_parms.samples_per_tti);

                      lte_eNB_I0_measurements(PHY_vars_eNB[loop], 0, 1);
                      PHY_vars_eNB[loop]->ulsch_eNB[0]->cyclicShift = cyclic_shift;// cyclic shift for DMRS

                      remove_7_5_kHz(PHY_vars_eNB[loop],subframe<<1);
                      remove_7_5_kHz(PHY_vars_eNB[loop],1+(subframe<<1));


        }


}



//configure the processing in runtime
void configure_runtime(int new_mcs, short* iqr, short* iqi, int id){




  ((DCI0_10MHz_FDD_t*)&UL_alloc_pdu)->mcs     = new_mcs;
  generate_ue_ulsch_params_from_dci((void *)&UL_alloc_pdu,
                                    14,
                                    ul_subframe2pdcch_alloc_subframe(&PHY_vars_UE[id]->lte_frame_parms,subframe),
                                    format0,
                                    PHY_vars_UE[id],
                                    SI_RNTI,
                                    0,
                                    P_RNTI,
                                    CBA_RNTI,
                                    0,
                                    0);

 if (PHY_vars_eNB[id]->ulsch_eNB[0] != NULL)
  generate_eNB_ulsch_params_from_dci((void *)&UL_alloc_pdu,
                                     14,
                                     ul_subframe2pdcch_alloc_subframe(&PHY_vars_eNB[id]->lte_frame_parms,subframe),
                                     format0,
                                     0,
                                     PHY_vars_eNB[id],
                                     SI_RNTI,
                                     0,
                                     P_RNTI,
                                     CBA_RNTI,
                                     0);




  int ii = 0;
  for(ii=0; ii< PHY_vars_eNB[id]->lte_frame_parms.samples_per_tti; ii++){
    ((short*) &PHY_vars_eNB[id]->lte_eNB_common_vars.rxdata[0][0][PHY_vars_eNB[id]->lte_frame_parms.samples_per_tti*subframe])[2*ii] = iqr[ii];
    ((short*) &PHY_vars_eNB[id]->lte_eNB_common_vars.rxdata[0][0][PHY_vars_eNB[id]->lte_frame_parms.samples_per_tti*subframe])[2*ii +1] = iqi[ii];
    }

  remove_7_5_kHz(PHY_vars_eNB[id],subframe<<1);
  remove_7_5_kHz(PHY_vars_eNB[id],1+(subframe<<1));

}


int task_all(int id){

    opp_enabled=1; // to enable the time meas
    // cpu_freq_GHz = (double)get_cpu_freq_GHz();
    // printf("Detected cpu_freq %f GHz\n",cpu_freq_GHz);
    ack_errors=0;
    task_fft(id);
    task_demod(id);
    unsigned int ret = task_decode(id);
    if (PHY_vars_eNB[id]->ulsch_eNB[0]->harq_processes[harq_pid]->o_ACK[0] != PHY_vars_UE[id]->ulsch_ue[0]->o_ACK[0])
    {ack_errors++;}

    if (ret <= PHY_vars_eNB[id]->ulsch_eNB[0]->max_turbo_iterations) {
    // printf("Decoding success!\n")
        return 1;
    } else {
    // printf("Decoding error!\n");
        return 0;
    }  // ulsch error

}


// int main (int argc, char** argv){
//     configure(argc, argv);
//     short* iqr = (short*) malloc(1*2*30720*sizeof(short)); //1=no_of_frame/1000, 2=BW/5MHz
//     short* iqi = (short*) malloc(1*2*30720*sizeof(short));
//     if (iqr == NULL || iqi == NULL){
//         printf("Cannot allocate memory!\n");
//         exit(-1);
//     }
//     task_all(0, iqr, iqi);
// }