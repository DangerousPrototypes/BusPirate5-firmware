/**
 * @file falaio.h
 * @brief Follow-Along Logic Analyzer I/O interface.
 * @details Provides binary mode interface for FALA data streaming.
 */

#ifndef FALAIO_H
#define FALAIO_H

extern const char falaio_name[];

/**
 * @brief Setup FALA I/O mode.
 */
void falaio_setup(void);

/**
 * @brief Display FALA setup message.
 */
void falaio_setup_message(void);

/**
 * @brief Cleanup FALA I/O mode.
 */
void falaio_cleanup(void);

/**
 * @brief Notify FALA data available.
 */
void falaio_notify(void);

/**
 * @brief Service FALA I/O.
 */
void falaio_service(void);

#endif // FALAIO_H
