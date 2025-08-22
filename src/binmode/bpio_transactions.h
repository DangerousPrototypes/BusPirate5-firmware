struct bpio_data_request_t {
    bool debug; // Debug flag
    bool start_main; // Start main condition
    bool start_alt;  // Start alternate condition
    uint16_t bytes_write; // Bytes to write
    uint16_t bytes_read; // Bytes to read
    const char *data_buf; // Data buffer 
    bool stop_main; // Stop main condition  
    bool stop_alt;  // Stop alternate condition
};

uint32_t bpio_hw1wire_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read);
uint32_t bpio_hwi2c_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read);
uint32_t bpio_hwspi_transaction(struct bpio_data_request_t *request, flatbuffers_uint8_vec_t data_write, uint8_t *data_read);