/**
 * \defgroup description...
 *
 * This...
 *
 * @{
 */

/*
 * packet-default-additional-info.h
 *
 *  Created on: Nov 15, 2017
 *      Author: vinicius
 */

#ifndef IOTUS_ARCH_SERVICES_PACKET_DEFAULT_ADDITIONAL_INFO_H_
#define IOTUS_ARCH_SERVICES_PACKET_DEFAULT_ADDITIONAL_INFO_H_

#include "nodes.h"

//The field infoPiece can have more valuer then these, but these are the standard
enum packet_additionalInfoList_types {
  IOTUS_PACKET_INFO_TYPE_RESERVED = 0,

  IOTUS_PACKET_INFO_TYPE_P2P_ADDRESSES,
  IOTUS_PACKET_INFO_TYPE_SOURCE_ADDRESS,
  IOTUS_PACKET_INFO_TYPE_DEST_ADDRESS,
  IOTUS_PACKET_INFO_TYPE_SEQUENCE_NUMBER,
  IOTUS_PACKET_INFO_TYPE_RETX_ATTEMPTS_DID,

  //Values generally sensed and available from the radio after receiving a packet
  IOTUS_PACKET_INFO_TYPE_RADIO_RCV_BLCK,
  //Value generally expected by the radio to transmit a packet
  IOTUS_PACKET_INFO_TYPE_RADIO_TX_BLCK,

  IOTUS_PACKET_INFO_TYPE___N,
};

/* Application layer has twice the range, so that it can include sub-layers of protocols */
#define IOTUS_PACKET_INFO_TYPE_RANGE_PER_LAYER     ((256-IOTUS_PACKET_INFO_TYPE___N)/5)
#define IOTUS_PACKET_INFO_DATA_LINK_TYPES_BEGIN     (IOTUS_PACKET_INFO_TYPE___N)
#define IOTUS_PACKET_INFO_ROUTING_TYPES_BEGIN       (IOTUS_PACKET_INFO_TYPE_RANGE_PER_LAYER+IOTUS_PACKET_INFO_DATA_LINK_TYPES_BEGIN)
#define IOTUS_PACKET_INFO_TRANSPORT_TYPES_BEGIN     (IOTUS_PACKET_INFO_TYPE_RANGE_PER_LAYER+IOTUS_PACKET_INFO_ROUTING_TYPES_BEGIN)
#define IOTUS_PACKET_INFO_APPLICATION_TYPES_BEGIN   (IOTUS_PACKET_INFO_TYPE_RANGE_PER_LAYER+IOTUS_PACKET_INFO_TRANSPORT_TYPES_BEGIN)

/**
 * This struct is preceded by the type IOTUS_PACKET_INFO_TYPE_RADIO_RCV_BLCK
 */
typedef struct __attribute__ ((__packed__)) packet_rcv_additional_info {
  uint16_t networkID;
  int8_t linkQuality;
  int8_t rssi; //got from CCA
} packet_rcv_block_output_t;

/**
 * This struct is preceded by the type IOTUS_PACKET_INFO_TYPE_RADIO_TX_BLCK
 */
typedef struct __attribute__ ((__packed__)) packet_tx_additional_info {
  uint8_t txPower;
  uint8_t channel;
} packet_tx_block_input_t;

/* 
 * This struct is preceded by the type IOTUS_PACKET_INFO_TYPE_P2P_ADDRESSES
 * Used to inform end to end communications
 */
typedef struct __attribute__ ((__packed__)) packet_e2e_nodes_addit_info {
  uint8_t *sourceNode;
  uint8_t *finaLNode;
} packet_addresses_block_t;

#endif /* IOTUS_ARCH_SERVICES_PACKET_DEFAULT_ADDITIONAL_INFO_H_*/

/* The following stuff ends the \defgroup block at the beginning of
   the file: */

/** @} */
