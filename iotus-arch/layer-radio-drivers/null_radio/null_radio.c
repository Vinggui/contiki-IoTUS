/**
 * \defgroup description...
 *
 * This...
 *
 * @{
 */

/*
 * null_radio.c
 *
 *  Created on: Nov 18, 2017
 *      Author: vinicius
 */
#include <stdio.h>
#include <stdlib.h>
#include "iotus-core.h"
#include "iotus-radio.h"
#include "packet.h"
#include "platform-conf.h"

#define DEBUG IOTUS_PRINT_IMMEDIATELY
#define THIS_LOG_FILE_NAME_DESCRITOR "null-radio"
#include "safe-printer.h"

static iotus_address_type iotus_used_address_type;

static radio_result_t
get_value(radio_param_t param, radio_value_t *value)
{
  if(!value) {
    return RADIO_RESULT_INVALID_VALUE;
  }
  switch(param) {
  case RADIO_PARAM_POWER_MODE:
  case RADIO_PARAM_CHANNEL:
  case RADIO_PARAM_RX_MODE:
  case RADIO_PARAM_TX_MODE:
  case RADIO_PARAM_TXPOWER:
  case RADIO_PARAM_CCA_THRESHOLD:
  case RADIO_PARAM_RSSI:
  case RADIO_CONST_CHANNEL_MIN:
  case RADIO_CONST_CHANNEL_MAX:
  case RADIO_CONST_TXPOWER_MIN:
  case RADIO_CONST_TXPOWER_MAX:
  case RADIO_CONST_ADDRESSES_OPTIONS:
  case RADIO_PARAM_ADDRESS_USE_TYPE:
  default:
    return RADIO_RESULT_NOT_SUPPORTED;
  }
}


static radio_result_t
set_value(radio_param_t param, radio_value_t value)
{
  int i;
  switch(param) {
  case RADIO_PARAM_CHANNEL:
  case RADIO_PARAM_RX_MODE:
  case RADIO_PARAM_TX_MODE:
  case RADIO_PARAM_TXPOWER:
  case RADIO_PARAM_CCA_THRESHOLD:
  case RADIO_PARAM_ADDRESS_USE_TYPE:
  default:
    return RADIO_RESULT_NOT_SUPPORTED;
  }
}

static int
send(iotus_packet_t *packet)
{
  return 1;
}

static void
start(void)
{
  SAFE_PRINTF_CLEAN("\tNull Radio\n");
}


static void
run(void)
{
}

static void
close(void)
{
}

const struct iotus_radio_driver_struct null_radio_radio_driver = {
  start,
  NULL,//post_start
  run,
  close,
  NULL,//cc2420_prepare,
  NULL,//cc2420_transmit,
  send,
  NULL,//cc2420_read,
  NULL,//cc2420_cca,
  NULL,//cc2420_receiving_packet,
  NULL,//pending_packet,
  NULL,//cc2420_on,
  NULL,//cc2420_off,
  get_value,
  set_value,
  NULL,//get_object,
  NULL,//set_object
};

/* The following stuff ends the \defgroup block at the beginning of
   the file: */

/** @} */
