
/**
 * \defgroup decription...
 *
 * This...
 *
 * @{
 */

/*
 * timestamp.c
 *
 *  Created on: Feb 27, 2018
 *      Author: vinicius
 */

#include <stdio.h>
#include <stdlib.h>
#include "iotus-core.h"
#include "platform-conf.h"
#include "clock.h"
#include "timestamp.h"

#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else /* DEBUG */
#define PRINTF(...)
#endif /* DEBUG */

/*---------------------------------------------------------------------*/
/*
 * \brief Get the elapsed time between the provided values and now. Max of +-30s.
 * \param time          The pointer of the timestamp to be set.
 * \param delta         Add or subtract time (in ms) to the actual momment.
 */
void
timestamp_delay(timestamp_t *time, int16_t delta)
{
  if(delta > 0) {
    *time += (delta*CLOCK_CONF_SECOND)/1000U;
  } else {
    *time -= (delta*CLOCK_CONF_SECOND)/1000U;
  }
}

/*--------------------------------------------------------------------
-*/

/*
 * \brief Gets which timestamp is bigger then the other.
 * \param time_1    The pointer of the timestamp 1 to be calculated.
 * \param time_2    The pointer of the timestamp 2 to be calculated.
 * \retval 0           Both inputs 1 and 2 are equal.
 * \retval 1           Inputs 1 > 2.
 * \retval 2           Inputs 1 < 2.
 */
uint8_t
timestamp_greater_then(timestamp_t *time_1, timestamp_t *time_2)
{
  if(*time_1 == *time_2) {
    return 0;
  } else if(*time_1 > *time_2) {
    return 1;
  } else {
    return 2;
  }
}
/*---------------------------------------------------------------------*/
/*
 * \brief Get the time difference between the provided values 1 and 2.
 * \param time_1    The pointer of the timestamp 1 to be calculated.
 * \param time_2    The pointer of the timestamp 2 to be calculated.
 * \return          The elasped time in milliseconds between time 1 and 2
                    (input 1 has to be greater than 2, return 0 otherwise).
 */
uint16_t
timestamp_difference(timestamp_t *time_1, timestamp_t *time_2)
{
  /* We have to consider every kind of clock implementation.
   * It means that clock_time() can return epoch values or easily wraped 2 bytes counter
   */
  uint8_t greaterValue = timestamp_greater_then(time_1,time_2);
  unsigned long difference = 0;//This must be zero!
  if(greaterValue == 1) {
    difference = ((*time_1 - *time_2)*TIMESTAMP_GRANULARITY)/CLOCK_CONF_SECOND;
  }
  return difference;
}
/*---------------------------------------------------------------------*/

/*
 * \brief Get the elapsed time between the provided values and now.
 * \param seconds    The pointer of the timestamp.
 * \return           The elasped time in milliseconds until now.
 */
uint16_t
timestamp_elapsed(timestamp_t *time)
{
  timestamp_t now = clock_time();
  return timestamp_difference(&now,time);
}
/*---------------------------------------------------------------------*/

/*
 * \brief Get the elapsed time between the provided values and now.
 * \param seconds    The pointer of the timestamp.
 * \return           The elasped time in milliseconds for this next time.
 */
uint16_t
timestamp_remainder(timestamp_t *time)
{
  timestamp_t now = clock_time();
  return timestamp_difference(time,&now);
}
/*---------------------------------------------------------------------*/

/* The following stuff ends the \defgroup block at the beginning of
   the file: */

/** @} */