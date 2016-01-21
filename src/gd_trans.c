#include "gd_trans.h"

void gd_trans_initialize(int* node_socks, int num_nodes){

    nodes_initialize(node_socks, num_nodes);
}

void gd_trans_trigger(){

    sendTrigger();
}

void gd_trans_read(gd_conn_desc_t conn){

    readIQ(conn.buffer, conn.start_sample, conn.num_samples, conn.node_sock, conn.node_id, conn.buffer_id, conn.host_id);
}

void gd_trans_write(gd_conn_desc_t conn){

    writeIQ(conn.buffer, conn.start_sample, conn.num_samples, conn.node_sock, conn.node_id, conn.buffer_id, conn.host_id);
}