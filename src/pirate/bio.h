
void bio_init(void);
void bio_buf_pin_init(uint8_t bio);
void bio_buf_output(uint8_t bio);
void bio_buf_input(uint8_t bio);
void bio_output(uint8_t bio);
void bio_input(uint8_t bio);
void bio_put(uint8_t bio, bool value);
bool bio_get(uint8_t bio);
void bio_set_function(uint8_t bio, uint8_t function);
void bio_buf_test(uint8_t bufio, uint8_t bufdir);

#define BUFDIR_INPUT 0
#define BUFDIR_OUTPUT 1
