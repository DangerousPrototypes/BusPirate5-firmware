/**
 * @file irtoy-air.h
 * @brief IRToy AIR infrared protocol interface.
 * @details Provides AIR binary mode for infrared communication.
 */

#ifndef IRTOY_AIR_H
#define IRTOY_AIR_H

extern const char irtoy_air_name[];

/**
 * @brief Setup AIR mode.
 */
void irtoy_air_setup(void);

/**
 * @brief Cleanup AIR mode.
 */
void irtoy_air_cleanup(void);

/**
 * @brief Service AIR mode.
 */
void irtoy_air_service(void);

#endif // IRTOY_AIR_H