/**
 * FIFO in RAM.
 * (C) Juan Schiavoni 2021
 */
#include <stddef.h>
#include "pico/types.h"

/*! \brief Initialice the FIFO.
 *  \ingroup ram_fifo
 *
 * This function don't block until there is space for the data to be insert.
 * Use ram_fifo_is_empty() to check if it is possible to inset to the.
 *
 * \param count quantity of items for FIFO
 * @return true if there is enough dynamic memory, false otherwise
 */
bool ram_fifo_init(size_t count);

/*! \brief Check if the FIFO is empty
 *  \ingroup ram_fifo
 *
 *  @return true if the FIFO has room for more data, false otherwise
 */
bool ram_fifo_is_empty(void);

/*! \brief Push data on to the FIFO.
 *  \ingroup ram_fifo
 *
 * This function don't block until there is space for the data to be insert.
 * Use ram_fifo_is_empty() to check if it is possible to inset to the.
 *
 * \param item A 32 bit value to push on to the FIFO
 * @return true if the FIFO has room for the data, false otherwise
 */
bool ram_fifo_set(uint32_t item);

/*! \brief Pop data from the FIFO.
 *  \ingroup ram_fifo
 *
 * This function don't block until there is data ready to be read
 * Use ram_fifo_is_empty() to check if data is ready to be read.
 *
 * \return 32 bit unsigned data from the FIFO.
 */
uint32_t ram_fifo_get(void);