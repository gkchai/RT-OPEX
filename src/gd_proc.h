#ifndef _GD_PROC_H_
#define _GD_PROC_H_

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

void gd_proc_lte_ul_ofdm();
void gd_proc_lte_ul_demod();
void gd_proc_lte_ul_decode();
void gd_proc_lte_dl();

#endif