/**
 * @file bpio.h
 * @brief Binary Protocol I/O (BPIO) mode interface.
 * @details Provides FlatBuffers-based binary protocol mode.
 */

/**
 * @brief BPIO mode handler.
 */
void dirtyproto_mode(void);

extern const char dirtyproto_mode_name[];

/**
 * @brief Change protocol mode.
 * @param mode_name    New mode name
 * @param mode_config  Mode configuration
 * @return             true on success
 */
bool mode_change_new(const char *mode_name, bpio_mode_configuration_t *mode_config);

/**
 * @brief Setup BPIO mode.
 */
void dirtyproto_mode_setup(void);