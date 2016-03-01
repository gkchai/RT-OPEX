
#ifndef GD_RX_H
#define GD_RX_H

void configure(int argc, char **argv, int trials, short* iqr, short* iqi, int mmcs, int nrx);
void subtask_fft(unsigned char l);
void task_fft();
int task_decode();
void task_demod();
int task_all();

#endif