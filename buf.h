void tx_fifo_init(void);
void tx_fifo_service(void);
void tx_fifo_put(char *c);
void tx_sb_start(uint32_t len);

void rx_fifo_init(void);
bool rx_fifo_try_get(char *c);
void rx_fifo_get_blocking(char *c);
bool rx_fifo_try_peek(char *c);
void rx_fifo_peek_blocking(char *c);

void dma_test();

extern char tx_sb_buf[1024];

//extern queue_t sample_fifo;