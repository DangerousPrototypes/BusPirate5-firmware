// enum to define return codes from nec_get_frame()
typedef enum {
    IR_RX_NO_FRAME = 0,
    IR_RX_FRAME_OK,
    IR_RX_FRAME_ERROR
} ir_rx_status_t;