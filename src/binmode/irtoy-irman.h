/**
 * @file irtoy-irman.h
 * @brief IRToy IRMAN infrared protocol interface.
 * @details Provides IRMAN binary mode for infrared communication.
 */

#ifndef IRTOY_IRMAN_H
#define IRTOY_IRMAN_H

extern const char irtoy_irman_name[];

/**
 * @brief Setup IRMAN mode.
 */
void irtoy_irman_setup(void);

/**
 * @brief Cleanup IRMAN mode.
 */
void irtoy_irman_cleanup(void);

/**
 * @brief Service IRMAN mode.
 */
void irtoy_irman_service(void);

#endif // IRTOY_IRMAN_H