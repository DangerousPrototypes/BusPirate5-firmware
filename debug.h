
struct _debug_uart
{ 
    uart_inst_t (*const uart);
    uint8_t rx_pin;    
    uint8_t tx_pin;
    int irq;
};

extern struct _debug_uart debug_uart[];
extern int debug_uart_number;

void debug_uart_init(int uart_number, bool dbrx, bool dbtx, bool terminal_label);
void debug_tx(char c);
bool debug_rx(char *c);