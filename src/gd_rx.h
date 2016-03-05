
#ifndef GD_RX_H
#define GD_RX_H

void configure(int argc, char **argv, int trials, short* iqr, short* iqi, int mmcs, int nrx, int num_bss);
void configure_runtime(int new_mcs, short* iqr, short* iqi, int id);
void subtask_fft(int l, int id);
void task_fft(int id);
int task_decode(int id);
void task_demod(int id);
int task_all(int id);

#endif