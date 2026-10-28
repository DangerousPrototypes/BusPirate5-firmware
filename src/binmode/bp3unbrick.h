/*
 * This file is part of the Bus Pirate project (http://code.google.com/p/the-bus-pirate/).
 *
 * Written and maintained by the Bus Pirate project.
 *
 * To the extent possible under law, the project has
 * waived all copyright and related or neighboring rights to Bus Pirate. This
 * work is published from United States.
 *
 * For details see: http://creativecommons.org/publicdomain/zero/1.0/.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/**
 * @file bp3unbrick.h
 * @brief Binmode that writes bootloader v4.5 to a Bus Pirate v3 over its ICSP header.
 */

#ifndef BP3UNBRICK_H
#define BP3UNBRICK_H

extern const char bp3unbrick_name[];

/**
 * @brief Binmode setup: resets the state. The work runs in bp3unbrick_run().
 */
void bp3unbrick_setup(void);

/**
 * @brief The whole job, run once when the binmode command selects this binmode:
 *        checks, erase, write, verify, then the default binmode comes back.
 */
void bp3unbrick_run(void);

/**
 * @brief Binmode cleanup: releases the pins and the power supply this binmode took.
 */
void bp3unbrick_cleanup(void);

#endif
