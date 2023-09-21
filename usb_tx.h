void tx_fifo_init(void);
void tx_fifo_service(void);
void tx_fifo_put(char *c);
void tx_sb_start(uint32_t len);

extern char tx_sb_buf[1024];

//extern queue_t sample_fifo;