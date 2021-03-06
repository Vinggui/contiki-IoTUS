/**
 * \defgroup decription...
 *
 * This...
 *
 * @{
 */

/*
 * packet.c
 *
 *  Created on: Nov 14, 2017
 *      Author: vinicius
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dev/leds.h"
#include "global-parameters.h"
#include "iotus-core.h"
#include "iotus-netstack.h"
#include "packet-defs.h"
#include "piggyback.h"
#include "layer-packet-manager.h"
#include "lib/memb.h"
#include "list.h"
#include "packet.h"
#include "packet-default-additional-info.h"
#include "chores.h"
#include "pieces.h"
#include "platform-conf.h"
#include "nodes.h"
#include "random.h"
#include "sys/timer.h"


#define DEBUG IOTUS_DONT_PRINT//IOTUS_PRINT_IMMEDIATELY
#define THIS_LOG_FILE_NAME_DESCRITOR "packet"
#include "safe-printer.h"


// Initiate the lists of module
MEMB(iotus_packet_struct_mem, iotus_packet_t, IOTUS_PACKET_LIST_SIZE);
LIST(gPacketList);
// LIST(gPacketReadyList);

static uint16_t gPacketIDcounter;

// process_event_t event_packet_service = process_alloc_event();

/*---------------------------------------------------------------------*/
/*
 * 
 * \param 
 * \return Packet final size
 */
Boolean
packet_destroy(iotus_packet_t *piece) {
  if(NULL == piece) {
    return FALSE;
  }
  //Try to remove in every list...
  // list_remove(gPacketReadyList, piece);
  SAFE_PRINTF_LOG_INFO("Del pkt %u %p", piece->pktID, piece);
  list_remove(gPacketList, piece);
  pieces_clean_additional_info_list(piece->additionalInfoList);
  //destroy attached piggyback

  //remove any future timer or so
  ctimer_stop(&piece->transmit_timer);

  return pieces_destroy(&iotus_packet_struct_mem, piece);
}

/*---------------------------------------------------------------------*/
/*
 * \brief verify if a certain parameter is defined
 * \param packetPiece Packet to be read.
 * \param param Parameter to be verified
 * \return Boolean.
 */
uint8_t
packet_get_parameter(iotus_packet_t *packetPiece, uint8_t param) {
  if(NULL == packetPiece) {
    SAFE_PRINTF_LOG_ERROR("Null pointer");
    return 0;
  }

  return packetPiece->params & param?1:0;
}


/*---------------------------------------------------------------------*/
/*
 * \brief Allow other services to set a parameter into a msg
 * \param packetPiece Packet to be set.
 * \param param Parameter to be written
 * \return Boolean.
 */
void
packet_set_parameter(iotus_packet_t *packetPiece, uint8_t param) {
  if(NULL == packetPiece) {
    SAFE_PRINTF_LOG_ERROR("Null pointer");
    return;
  }

  /* Encode parameters */
  packetPiece->params |= param;
}

/*---------------------------------------------------------------------*/
/*
 * \brief Allow other services to clear a parameter into a msg
 * \param packetPiece Packet to be set.
 * \param param Parameter to be written
 * \return Boolean.
 */
void
packet_clear_parameter(iotus_packet_t *packetPiece, uint8_t param) {
  if(NULL == packetPiece) {
    SAFE_PRINTF_LOG_ERROR("Null pointer");
    return;
  }

  /* Encode parameters */
  packetPiece->params &= ~param;
}

/*---------------------------------------------------------------------*/
/*
 * Return the pointer to the node of final destination
 * \param packetPiece       The pointer to the packet to be searched.
 * \return                The pointer to the node of the final destination.
 */
iotus_node_t *
packet_get_final_destination(iotus_packet_t *packetPiece)
{
  if(NULL == packetPiece) {
    SAFE_PRINTF_LOG_ERROR("Null pointer");
    return NULL;
  }
  return packetPiece->finalDestinationNode;
}

/*---------------------------------------------------------------------*/
/*
 * Return the pointer to the sender node of this packet
 * \param packetPiece       The pointer to the packet to be searched.
 * \return                  The pointer to the node.
 */
iotus_node_t *
packet_get_prevSource_node(iotus_packet_t *packetPiece)
{
  if(NULL == packetPiece) {
    SAFE_PRINTF_LOG_ERROR("Null pointer");
    return NULL;
  }
  return packetPiece->prevSourceNode;
}

/*---------------------------------------------------------------------*/
/*
 * Return the pointer to the node of next destination
 * \param packetPiece       The pointer to the packet to be searched.
 * \return                The pointer to the node of the final destination.
 */
iotus_node_t *
packet_get_next_destination(iotus_packet_t *packetPiece)
{
  if(NULL == packetPiece) {
    SAFE_PRINTF_LOG_ERROR("Null pointer");
    return NULL;
  }
  return packetPiece->nextDestinationNode;
}

/*---------------------------------------------------------------------*/
Status
packet_set_next_destination(iotus_packet_t *packetPiece, iotus_node_t *node)
{
  if(NULL == packetPiece || NULL == node) {
    SAFE_PRINTF_LOG_ERROR("Null pointer");
    return FAILURE;
  }
  packetPiece->nextDestinationNode = node;
  return SUCCESS;
}
/*---------------------------------------------------------------------*/
Status
packet_set_final_destination(iotus_packet_t *packetPiece, iotus_node_t *node)
{
  if(NULL == packetPiece || NULL == node) {
    SAFE_PRINTF_LOG_ERROR("Null pointer");
    return FAILURE;
  }
  packetPiece->finalDestinationNode = node;
  return SUCCESS;
}
/*---------------------------------------------------------------------*/
Boolean
packet_holds_broadcast(iotus_packet_t *packetPiece)
{
  if(NULL == packetPiece) {
    SAFE_PRINTF_LOG_ERROR("Null pointer");
    return FAILURE;
  }
  return packetPiece->nextDestinationNode == NODES_BROADCAST;
}

/*---------------------------------------------------------------------*/
/**
 * \brief               Set a specific tx channel for a packet transmission
 * @param packetPiece   Packet to set.
 * @param channel       The channel to be used, accordingly to the radios limits.
 * \return              Status of this set.
 */
Status
packet_set_tx_channel(iotus_packet_t *packetPiece, uint8_t channel)
{
  packet_tx_block_input_t  *txBlockInfo = pieces_modify_additional_info_var(
                                            packetPiece->additionalInfoList,
                                            IOTUS_PACKET_INFO_TYPE_RADIO_TX_BLCK,
                                            sizeof(packet_tx_block_input_t),
                                            TRUE);
  if(txBlockInfo == NULL) {
    SAFE_PRINTF_LOG_ERROR("Add info not set");
    return FAILURE;
  }
  //Just set the value into the buffer
  txBlockInfo->channel = channel;
  return SUCCESS;
}

/*---------------------------------------------------------------------*/
/**
 * \brief               Get a specific Tx channel for a packet transmission
 * @param packetPiece   Packet to check.
 * \return              The channel to be used. -1 if operation fails.
 */
int16_t
packet_get_tx_channel(iotus_packet_t *packetPiece)
{
  iotus_additional_info_t *addInfo = pieces_get_additional_info(
                                              packetPiece->additionalInfoList,
                                              IOTUS_PACKET_INFO_TYPE_RADIO_TX_BLCK);
  if(NULL == addInfo) {
    //Found this address type...
    SAFE_PRINTF_LOG_ERROR("Add info not found");
    return -1;
  }

  packet_tx_block_input_t *txBlockInfo = (packet_tx_block_input_t *)pieces_get_data_pointer(addInfo);
  
  return (uint16_t)(txBlockInfo->channel);
}

/*---------------------------------------------------------------------*/
/**
 * \brief               Set a specific tx power for a packet transmission
 * @param packetPiece   Packet to set.
 * @param power         The power in dBm, generally varing from 0 to -25
 * \return              Status of this set.
 */
Status
packet_set_tx_power(iotus_packet_t *packetPiece, int8_t power)
{
  packet_tx_block_input_t  *txBlockInfo = pieces_modify_additional_info_var(
                                            packetPiece->additionalInfoList,
                                            IOTUS_PACKET_INFO_TYPE_RADIO_TX_BLCK,
                                            sizeof(packet_tx_block_input_t),
                                            TRUE);
  if(txBlockInfo == NULL) {
    SAFE_PRINTF_LOG_ERROR("Add info not set");
    return FAILURE;
  }
  //Just set the value into the buffer
  txBlockInfo->txPower = power;
  return SUCCESS;
}

/*---------------------------------------------------------------------*/
/**
 * \brief               Get a specific tx power for a packet transmission
 * @param packetPiece   Packet to check.
 * \return              The power selected, 0xFF for no selection
 */
int8_t
packet_get_tx_power(iotus_packet_t *packetPiece)
{
  iotus_additional_info_t *addInfo = pieces_get_additional_info(
                                          packetPiece->additionalInfoList,
                                          IOTUS_PACKET_INFO_TYPE_RADIO_TX_BLCK);
  if(NULL == addInfo) {
    //Tx power of 120 is not expected
    return 120;
  }

  packet_tx_block_input_t *txBlockInfo = (packet_tx_block_input_t *)pieces_get_data_pointer(addInfo);

  return txBlockInfo->txPower;
}

/*---------------------------------------------------------------------*/
Status
packet_set_sequence_number(iotus_packet_t *packetPiece, uint8_t sequence)
{
  uint8_t *sequenceNum = pieces_modify_additional_info_var(
                              packetPiece->additionalInfoList,
                              IOTUS_PACKET_INFO_TYPE_SEQUENCE_NUMBER,
                              1,
                              TRUE);
  if(sequenceNum == NULL) {
    SAFE_PRINTF_LOG_ERROR("Add info not set");
    return FAILURE;
  }
  *sequenceNum = sequence;
  return SUCCESS;
}

/*---------------------------------------------------------------------*/

int16_t
packet_get_sequence_number(iotus_packet_t *packetPiece)
{

  iotus_additional_info_t *addInfo = pieces_get_additional_info(
                                          packetPiece->additionalInfoList,
                                          IOTUS_PACKET_INFO_TYPE_SEQUENCE_NUMBER);
  if(NULL == addInfo) {
    return -1;
  }

  return (uint16_t)(*(pieces_get_data_pointer(addInfo)));
}

/*---------------------------------------------------------------------*/
/**
 * \brief               Set a specific Rx netID for a packet reception
 * @param packetPiece   Packet to set.
 * @param netID         The netID (PanID) to be set.
 * \return              Status of this set.
 */
Status
packet_set_rx_netID(iotus_packet_t *packetPiece, uint16_t netID)
{
  packet_rcv_block_output_t  *rxBlockInfo = pieces_modify_additional_info_var(
                                            packetPiece->additionalInfoList,
                                            IOTUS_PACKET_INFO_TYPE_RADIO_RCV_BLCK,
                                            sizeof(packet_rcv_block_output_t),
                                            TRUE);
  if(rxBlockInfo == NULL) {
    SAFE_PRINTF_LOG_ERROR("Add info not set");
    return FAILURE;
  }
  //Just set the value into the buffer
  rxBlockInfo->networkID = netID;
  return SUCCESS;
}


/*---------------------------------------------------------------------*/
/**
 * \brief               Set a specific Rx link quality for a packet reception
 * @param packetPiece   Packet to set.
 * @param linkQuality   The link quality to be set.
 * @param rssi          The rssi to be set.
 * \return              Status of this set.
 */
Status
packet_set_rx_linkQuality_RSSI(iotus_packet_t *packetPiece, uint8_t linkQuality, uint8_t rssi)
{
  packet_rcv_block_output_t  *rxBlockInfo = pieces_modify_additional_info_var(
                                            packetPiece->additionalInfoList,
                                            IOTUS_PACKET_INFO_TYPE_RADIO_RCV_BLCK,
                                            sizeof(packet_rcv_block_output_t),
                                            TRUE);
  if(rxBlockInfo == NULL) {
    SAFE_PRINTF_LOG_ERROR("Add info not set");
    return FAILURE;
  }
  //Just set the value into the buffer
  rxBlockInfo->linkQuality = linkQuality;
  rxBlockInfo->rssi = rssi;
  return SUCCESS;
}

/*---------------------------------------------------------------------*/
/**
 * \brief               Set a specific Rx link quality for a packet reception
 * @param packetPiece   Packet to set.
 * @param linkQuality   The link quality to be set.
 * @param rssi          The rssi to be set.
 * \return              Status of this set.
 */
packet_rcv_block_output_t *
packet_get_rx_block(iotus_packet_t *packetPiece)
{

  iotus_additional_info_t *addInfo = pieces_get_additional_info(
                                          packetPiece->additionalInfoList,
                                          IOTUS_PACKET_INFO_TYPE_RADIO_RCV_BLCK);
  if(NULL == addInfo) {
    //Found this address type...
    SAFE_PRINTF_LOG_ERROR("Add info not found");
    return NULL;
  }

  packet_rcv_block_output_t *rxBlockInfo = (packet_rcv_block_output_t *)pieces_get_data_pointer(addInfo);
  return rxBlockInfo;
}

/*---------------------------------------------------------------------*/
/*
 * 
 * \param 
 * \return Packet final size
 */
iotus_packet_t *
packet_create_msg(uint16_t payloadSize, const uint8_t* payload,
    iotus_layer_priority priority, uint16_t timeout, Boolean AllowOptimization,
    iotus_node_t *finalDestination)
{
  iotus_packet_t *newMsg = (iotus_packet_t *)pieces_malloc(
                                    &iotus_packet_struct_mem,
                                    sizeof(iotus_packet_t),
                                    payload, payloadSize);
  if(NULL == newMsg) {
    SAFE_PRINTF_LOG_ERROR("Alloc failed.");
    return NULL;
  }
  
  // if(priority != IOTUS_PRIORITY_RADIO) {
    // leds_on(LEDS_BLUE);
    // packetBuildingTime = RTIMER_NOW();
  // }

  timestamp_delay(&(newMsg->timeout), timeout);
  LIST_STRUCT_INIT(newMsg, additionalInfoList);
  
  //this packet will go down the stack, towards the physical layer
  newMsg->firstHeaderBitSize = 0;
  newMsg->lastHeaderSize = 0;
  newMsg->nextDestinationNode = NULL;
  newMsg->prevSourceNode = NULL;
  newMsg->confirm_cb = NULL;
  newMsg->collisions = 0;
  newMsg->transmissions = 0;
  newMsg->pktID = ++gPacketIDcounter;
  newMsg->params = 0;
  //newMsg->iotusHeader = PACKET_IOTUS_HDR_FIRST_BIT;
  newMsg->priority = priority;
  newMsg->type = IOTUS_PACKET_TYPE_IEEE802154_DATA;
  
  newMsg->finalDestinationNode = finalDestination;

  //Set some params into this packet
  if(TRUE == AllowOptimization) {
    packet_set_parameter(newMsg,PACKET_PARAMETERS_IS_NEW_PACKET_SYSTEM);
  }
  if(finalDestination == NODES_BROADCAST) {
    SAFE_PRINT("Broadcast\n");
    newMsg->nextDestinationNode = NODES_BROADCAST;
    //newMsg->iotusHeader |= PACKET_IOTUS_HDR_IS_BROADCAST;
  }
  SAFE_PRINTF_LOG_INFO("Created pktID %u %p", newMsg->pktID, newMsg);
  //Link the message into the list, sorting...
  pieces_insert_timeout_priority(gPacketList, newMsg);
  return newMsg;
}

/*---------------------------------------------------------------------*/
/*
 * \brief Function to get the total size of a packet
 * \param packetPiece Packet to be read.
 * \return Total size.
 */
unsigned int
packet_get_size(iotus_packet_t *packetPiece) {
  return pieces_get_data_size(packetPiece);
}

/*---------------------------------------------------------------------*/
/*
 * \brief Function to push bits into the header
 * \param bitSequenceSize The amount of bit that will be push into.
 * \param bitSeq An array of bytes containing the bits
 * \param packetPiece Msg to apply this push
 * \return Packet final size
 */
uint16_t
packet_push_bit_header(uint16_t bitSequenceSize, const uint8_t *bitSeq,
  iotus_packet_t *packetPiece) {

  /* This operation is only allowed to packets going down the stack (to be transmitted) */
  if(packetPiece->priority == IOTUS_PRIORITY_RADIO) {
    return 0;
  }
  uint16_t i;
  uint16_t newSizeInBYTES = 0;
  uint16_t oldSizeInBYTES = ((packetPiece->firstHeaderBitSize)+7)/8;//round up
  uint16_t packetOldTotalSize = packet_get_size(packetPiece);
  

  //Verify if a new byte is required
  uint16_t freeSpaceInBITS = (oldSizeInBYTES*8)-(packetPiece->firstHeaderBitSize);
  if(freeSpaceInBITS < bitSequenceSize) {
    //Reallocate new buffer for this system
    newSizeInBYTES = (bitSequenceSize - freeSpaceInBITS + 7)/8;//round up


    uint8_t newBuff[newSizeInBYTES + packetOldTotalSize];
    for(i=0;i<newSizeInBYTES;i++) {
      newBuff[i]=0;
    }
    //transfer the old buffer to the new one, backwards!
    memcpy(newBuff+newSizeInBYTES,pieces_get_data_pointer(packetPiece),packetOldTotalSize);

    //Delete the old buffer
#if IOTUS_USING_MALLOC == 0
    //This sequence of free first then alloc makes more sense in constrained devices
    mmem_free(&(packetPiece->data));
    //recreate it...
    if(0 == mmem_alloc(&(packetPiece->data), newSizeInBYTES + packetOldTotalSize)) {
      /* Failed to alloc memory */
      SAFE_PRINTF_LOG_WARNING("Alloc failed");
      //retore old info...
      if(0 == mmem_alloc(&(packetPiece->data), packetOldTotalSize)) {
        SAFE_PRINTF_LOG_ERROR("Recovery Failed!");
        return 0;
      }
      memcpy(pieces_get_data_pointer(packetPiece),newBuff+newSizeInBYTES,packetOldTotalSize);
      return 0;
    }
#else
    //This sequence of free first then alloc makes more sense in constrained devices
    free(pieces_get_data_pointer(packetPiece));
    //recreate it...
    packetPiece->data.ptr = malloc(newSizeInBYTES + packetOldTotalSize);
    if(NULL == packetPiece->data.ptr) {
      /* Failed to alloc memory */
      SAFE_PRINTF_LOG_WARNING("Alloc failed");
      //retore old info...
      packetPiece->data.ptr = malloc(packetOldTotalSize);
      if(NULL == packetPiece->data.ptr) {
        SAFE_PRINTF_LOG_ERROR("Recovery Failed!");
        return 0;
      }
      memcpy(pieces_get_data_pointer(packetPiece), newBuff + newSizeInBYTES, packetOldTotalSize);
      return 0;
    }
    pieces_get_data_size(packetPiece) += newSizeInBYTES;
#endif
    memcpy(pieces_get_data_pointer(packetPiece), newBuff, packetOldTotalSize + newSizeInBYTES);
  }

  //Insert the new bits information
  uint16_t byteToPushToPkt = (newSizeInBYTES + oldSizeInBYTES) - 1 - ((packetPiece->firstHeaderBitSize)/8);
  uint8_t bitToPush = ((packetPiece->firstHeaderBitSize)%8);
  uint16_t byteToReadFromInput = (bitSequenceSize/8);
  if(bitSequenceSize%8){
    // this +1 is because this value is decreased right after.
    byteToReadFromInput++;
  }

  /**
   * Optimization: If we can insert full bytes, then do it instead.
   */
  if(bitToPush == 0) {
    memcpy(pieces_get_data_pointer(packetPiece), bitSeq, byteToReadFromInput);
    SAFE_PRINTF_LOG_INFO("Push bit optmzd");
    return packet_get_size(packetPiece);
  }

  for(i=0; i < bitSequenceSize; i++) {
    //Verify and change the byte to be push into...
    if(bitToPush >= 8) {
      bitToPush = 0;
      byteToPushToPkt--;
    }
    //Read the bits from the source
    uint8_t bitShifted = (1<<(i%8));
    if((i%8) == 0) {
      byteToReadFromInput--;
    }

    uint8_t bitRead = (bitSeq[byteToReadFromInput] & bitShifted);
    if(bitRead == 0) {
      (pieces_get_data_pointer(packetPiece))[byteToPushToPkt] &= ~(1<<bitToPush);
    } else {
      (pieces_get_data_pointer(packetPiece))[byteToPushToPkt] |= (1<<bitToPush);
    }
    bitToPush++;
  }
  packetPiece->firstHeaderBitSize += bitSequenceSize;//counted in bits
  return packet_get_size(packetPiece);
}


/*---------------------------------------------------------------------*/
/*
 * \brief Function to append full bytes headers into the tail (inversed)
 * \param bytesSize The amount of bytes that will be appended.
 * \param byteSeq An array of bytes in its normal sequence
 * \param packetPiece Msg to apply this append
 * \return Packet final size, 0 if failed.
 */
uint16_t
packet_append_last_header(uint16_t byteSequenceSize, const uint8_t *headerToAppend,
  iotus_packet_t *packetPiece) {
  int i;//Verify if the msg piece already has something

  /* This operation is only allowed to packets going down the stack (to be transmitted) */
  if(packetPiece->priority == IOTUS_PRIORITY_RADIO) {
    return 0;
  } else if(headerToAppend == NULL ||
     packetPiece == NULL) {
    printf("NULL pointer\n");
    return 0;
  }

  uint16_t packetOldTotalSize = packet_get_size(packetPiece);
  uint16_t packetNewTotalSize = packetOldTotalSize + byteSequenceSize;
  //Reallocate new buffer for this system
  uint8_t newBuff[packetNewTotalSize];

/* this was malloc before...
  if(newBuff == NULL) {
    SAFE_PRINTF_LOG_ERROR("allocate memory");
    return 0;
  }
*/

  //transfer the old buffer to the new one! (left to the right)
  memcpy(newBuff, pieces_get_data_pointer(packetPiece), packetOldTotalSize);

  //Delete the old buffer
#if IOTUS_USING_MALLOC == 0
  //This sequence of free first then alloc makes more sense in constrained devices
  mmem_free(&(packetPiece->data));
  //recreate it...
  if(0 == mmem_alloc(&(packetPiece->data), packetNewTotalSize)) {
    /* Failed to alloc memory */
    SAFE_PRINTF_LOG_WARNING("Alloc failed");
    //retore old info...
    if(0 == mmem_alloc(&(packetPiece->data), packetOldTotalSize)) {
      SAFE_PRINTF_LOG_ERROR("Recovery Failed!");
      return 0;
    }
    memcpy(pieces_get_data_pointer(packetPiece),newBuff,packetOldTotalSize);
    return 0;
  }
#else
  //This sequence of free first then alloc makes more sense in constrained devices
  free(pieces_get_data_pointer(packetPiece));
  //recreate it...
  packetPiece->data.ptr = malloc(packetNewTotalSize);
  if(NULL == packetPiece->data.ptr) {
    /* Failed to alloc memory */
    SAFE_PRINTF_LOG_WARNING("Alloc failed");
    //retore old info...
    packetPiece->data.ptr = malloc(packetOldTotalSize);
    if(NULL == packetPiece->data.ptr) {
      SAFE_PRINTF_LOG_ERROR("Recovery Failed!");
      return 0;
    }
    memcpy(pieces_get_data_pointer(packetPiece),newBuff,packetOldTotalSize);
    return 0;
  }
  pieces_get_data_size(packetPiece) += byteSequenceSize;
#endif
  memcpy(pieces_get_data_pointer(packetPiece),newBuff,packetOldTotalSize);

  //update size
  packetPiece->lastHeaderSize += byteSequenceSize;
  
  //Insert the new bytes, backwards...
  for(i=0; i < byteSequenceSize; i++) {
    pieces_get_data_pointer(packetPiece)[packetNewTotalSize - i - 1] = headerToAppend[i];
  }

  return packet_get_size(packetPiece);
}

/*---------------------------------------------------------------------*/
/*
 * \brief  Function to read any byte of a message, given its position
 * \param bytePos The position of the byte
 * \param packetPiece Packet to be read.
 * \return Byte read.
 */
uint8_t
packet_read_byte(uint16_t bytePos, iotus_packet_t *packetPiece)
{
  if(packet_get_size(packetPiece) <= bytePos) {
    return 0;
  }
  return pieces_get_data_pointer(packetPiece)[bytePos];
}

/*---------------------------------------------------------------------*/
/*
 * \brief  Function to read any byte of a message backward, given its position
 * \param bytePos The position of the byte
 * \param packetPiece Packet to be read.
 * \return Byte read.
 */
uint8_t
packet_read_byte_backward(uint16_t bytePos, iotus_packet_t *packetPiece)
{
  if(packet_get_size(packetPiece) <= bytePos) {
    return 0;
  }

  int32_t pos;
  pos = pieces_get_data_size(packetPiece) -bytePos -1 -packetPiece->lastHeaderSize;
  if(pos < 0) {
    return 0;
  }

  return pieces_get_data_pointer(packetPiece)[pos];
}

/*---------------------------------------------------------------------*/
/*
 * \brief  Function to set any byte of a message backward, given its position
 * \param byte The value to set in
 * \param bytePos The position of the byte
 * \param packetPiece Packet to be read.
 * \return Byte read.
 */
void
packet_set_byte_backward(uint8_t byte, uint16_t bytePos, iotus_packet_t *packetPiece)
{
  if(packet_get_size(packetPiece) <= bytePos) {
    return 0;
  }

  int32_t pos;
  pos = pieces_get_data_size(packetPiece) -bytePos -1 -packetPiece->lastHeaderSize;
  if(pos < 0) {
    return 0;
  }

  pieces_get_data_pointer(packetPiece)[pos] = byte;
}

/*---------------------------------------------------------------------*/
/*
 * \brief  Function to extract a chunk of bytes from the data buffer
 * \param buff Buffer to be used to save extracted bytes
 * \param bytePos The position to start extraction
 * \param size Size of extraction
 * \param packetPiece Packet to be read.
 * \return Byte read.
 */
void
packet_extract_data_bytes(uint8_t *buff, uint16_t bytePos, uint16_t size, iotus_packet_t *packetPiece)
{
  if(packet_get_size(packetPiece) <= (bytePos+size)) {
    return;
  }
// printf("Buffer: ");
  uint8_t i, byteToReplace;
  for(i=bytePos; i<packet_get_payload_size(packetPiece); i++) {
    if(i < bytePos + size) {
      buff[i-bytePos] = packet_get_payload_data(packetPiece)[i];
      // printf("%c", buff[i-bytePos]);
    }

    if(i+size < packet_get_payload_size(packetPiece)) {
      byteToReplace = packet_get_payload_data(packetPiece)[i+size];
    } else {
      byteToReplace = 0;
    }

    packet_get_payload_data(packetPiece)[i] = byteToReplace;
  }

// printf("\n");
  packetPiece->lastHeaderSize += size;
}

/*---------------------------------------------------------------------*/
/*
 * \brief  Read bytes appended in an incoming packet.
 * \param packetPiece Packet to be read.
 * \param buf         The buf to save the read data into.
 * \param num         The number of byes to be read.
 * \return Byte read.
 */
Status
packet_unwrap_appended_byte(iotus_packet_t *packetPiece, uint8_t *buf, uint16_t num)
{
  if(buf == NULL || packetPiece == NULL) {
    return FAILURE;
  }
  uint16_t i;
  int32_t pos;
  for(i=0;i<num;i++) {
    pos = pieces_get_data_size(packetPiece) -1 - packetPiece->lastHeaderSize;
    if(pos < 0) {
      return FAILURE;
    }

    *buf = pieces_get_data_pointer(packetPiece)[pos];
    buf++;
    packetPiece->lastHeaderSize++;
  }
  return SUCCESS;
}

/*---------------------------------------------------------------------*/
/*
 * \brief  Read one byte pushed in an incoming packet.
 * \param packetPiece Packet to be read.
 * \return Byte read.
 */
uint8_t
packet_unwrap_pushed_byte(iotus_packet_t *packetPiece)
{
  uint16_t byteToRead;
  byteToRead = packetPiece->firstHeaderBitSize/8;

  packetPiece->firstHeaderBitSize += 8;
  return pieces_get_data_pointer(packetPiece)[byteToRead];
}

/*---------------------------------------------------------------------*/
/*
 * \brief  Read bits pushed in an incoming packet. Max of 32 bit a at time.
 * \param 
 * \param packetPiece Packet to be read.
 * \return The sequence of Bit read.
 */
uint32_t
packet_unwrap_pushed_bit(iotus_packet_t *packetPiece, int8_t num)
{
  if(packetPiece == NULL || num > 32 || num < 1) {
    return 0;
  }
  uint32_t result;
  uint8_t bitMask;
  uint16_t byteToRead;

  /**
   * Adding this remainder is necessary for correct read of the whole requested
   * num size.
   */
  byteToRead = packetPiece->firstHeaderBitSize/8;

  //Create a mask for the first byte of this reading
  bitMask = (1<<(8-packetPiece->firstHeaderBitSize%8))-1;

  //Do the first read
  result = 0;
  result |= (uint32_t)pieces_get_data_pointer(packetPiece)[byteToRead];
  result &= bitMask;
  /**
   * bitMask won`t be used anymore... reusing it for counting...
   */
  bitMask = 8-packetPiece->firstHeaderBitSize%8;
  num -= bitMask;
  packetPiece->firstHeaderBitSize += bitMask;

  while(num > 0) {
    result = (result<<8);
    byteToRead++;
    result |= (uint32_t)pieces_get_data_pointer(packetPiece)[byteToRead];
    num -= 8;
    packetPiece->firstHeaderBitSize += 8;
  }
  /* If num is negative... */
  if(num) {
    result = result>>(-num);
    packetPiece->firstHeaderBitSize += num;
  }
  return result;
}

/*---------------------------------------------------------------------*/
uint16_t
packet_get_payload_size(iotus_packet_t *packetPiece)
{
  uint16_t hdrSize;
  hdrSize = packetPiece->firstHeaderBitSize/8;
  if(packetPiece->firstHeaderBitSize%8 != 0) {
    hdrSize++;
  }
  return packet_get_size(packetPiece) - hdrSize - packetPiece->lastHeaderSize;
}

/*---------------------------------------------------------------------*/
uint8_t *
packet_get_payload_data(iotus_packet_t *packetPiece)
{
  uint16_t hdrSize;
  hdrSize = packetPiece->firstHeaderBitSize/8;
  if(packetPiece->firstHeaderBitSize%8 != 0) {
    hdrSize++;
  }
  return (pieces_get_data_pointer(packetPiece) + hdrSize);
}

/*---------------------------------------------------------------------*/
void
packet_parse(iotus_packet_t *packetPiece) {
  if(packetPiece == NULL) {
    return;
  }

  if(IOTUS_PRIORITY_RADIO == packetPiece->priority) {
    //Radio priority means that this pkt is going up the stack, towards app layer
    packetPiece->firstHeaderBitSize = 0;

    /**
     * Find the first set bit in this packet and jump to the next bit
     * of the iotus dynamic header.
     */
    uint8_t i;
    i=packet_read_byte(0,packetPiece);
    while(i>0) {
      i/=2;
      packetPiece->firstHeaderBitSize++;
    }
    packetPiece->firstHeaderBitSize = 9-packetPiece->firstHeaderBitSize;
    //Now that we found the first bit of the packet, we can ignore it.
    uint8_t byteMapToReset = 1<< (8-packetPiece->firstHeaderBitSize);
    
    pieces_get_data_pointer(packetPiece)[0] &= ~(byteMapToReset);

    //Get the dynamic header now
    //packetPiece->iotusHeader = packet_unwrap_pushed_bit(packetPiece,PACKET_IOTUS_HDR_FIRST_BIT_POS-1);
    
    //if(packetPiece->iotusHeader & PACKET_IOTUS_HDR_IS_BROADCAST) {
    //  packetPiece->nextDestinationNode = NODES_BROADCAST;
    //}
  }
}

/*---------------------------------------------------------------------*/
Boolean
packet_has_space(iotus_packet_t *packetPiece, uint16_t space)
{
  if(packetPiece == NULL) {
    return FALSE;
  }

  uint16_t freeSpace;
  freeSpace = iotus_packet_dimensions.totalSize
              -packet_get_size(packetPiece)
              -space;
  if(freeSpace >= 0) {
    return TRUE;
  }
  return FALSE;
}

/*---------------------------------------------------------------------*/
/**
 * \brief   Select one packet and initiate the stack signaling of packet to be sent.
 * \param num   The number of packets to be polled and ready to transmit.
 */
// void
// packet_poll_by_priority(uint8_t num)
// {
//   iotus_packet_t *packet, *packetSelected;
//   unsigned long minTimeout, packetTimeout;

//   minTimeout = -1;
//   //Get the packet with lowest priority and nearest timeout
//   packetSelected = list_head(gPacketList);
//   for(packet = packetSelected; packet != NULL; packet = list_item_next(packet)) {
//     if(packet->priority != IOTUS_PRIORITY_RADIO &&
//        packet->priority <= packetSelected->priority) {
//       packetTimeout = timestamp_remainder(&packet->timeout);
//       if(packetTimeout < minTimeout) {
//         minTimeout = packetTimeout;
//         packetSelected = packet;
//       }
//     }
//   }

//   if(packetSelected != NULL) {
//     packet_send(packetSelected);
//   }
// }

/*---------------------------------------------------------------------*/
/**
 * \brief   Select one packet and initiate the stack signaling of packet to be sent.
 * \param num   The number of packets to be polled and ready to transmit.
 */
// void
// packet_poll_by_node(iotus_node_t *node, uint8_t num)
// {
//   iotus_packet_t *packet, *packetSelected;
//   unsigned long minTimeout, packetTimeout;

//   if(node == NULL) {
//     return;
//   }

//   minTimeout = -1; //make it the max value...
//   //Get the packet with lowest priority and nearest timeout
//   packetSelected = list_head(gPacketList);
//   for(packet = packetSelected; packet != NULL; packet = list_item_next(packet)) {
//     if(packet_get_next_destination(packet) == node) {
//       packetTimeout = timestamp_remainder(&packet->timeout);
//       if(packetTimeout < minTimeout) {
//         minTimeout = packetTimeout;
//         packetSelected = packet;
//       }
//     }
//   }

//   if(packetSelected != NULL) {
//     packet_send(packetSelected);
//   }
// }


/*---------------------------------------------------------------------*/
/**
 * \brief         If a packet get deferred by the mac layer, this is the function
 *                that should be called to continue the process of its transmission.
 * \param packet  The packet to be continued.
 */
// void
// packet_continue_deferred_packet(iotus_packet_t *packet)
// {
//   iotus_netstack_return returnAns;
//   returnAns = active_data_link_protocol->send(packet);
//   return_packet_on_layers(packet, returnAns);
//   packet_destroy(packet);
// }

/*---------------------------------------------------------------------*/
/**
 * \brief   Deliver the received packet up in the stack, at most to the application layer.
 * \param packet   The packet to be processed up to the stack.
 */
// void
// packet_deliver_upstack(iotus_packet_t *packet)
// {
//   if(packet == NULL ||
//      packet->priority != IOTUS_PRIORITY_RADIO) {
//     return;
//   }

//   iotus_netstack_return returnAns;
//   returnAns = RX_ERR_DROPPED;
//   //Call the return functions of each layer
//   if(active_data_link_protocol->receive != NULL) {
//     returnAns = active_data_link_protocol->receive(packet);
//   }
//   if(returnAns == RX_SEND_UP_STACK) {
//     if(active_routing_protocol->receive != NULL) {
//       returnAns = active_routing_protocol->receive(packet);
//     }
//   }
//   if(returnAns == RX_SEND_UP_STACK) {
//     if(active_transport_protocol->receive != NULL) {
//       returnAns = active_transport_protocol->receive(packet);
//     }
//   }
//   if(returnAns == RX_SEND_UP_STACK) {
//     if(gApplicationPacketHandler != NULL) {
//       gApplicationPacketHandler(packet);
//     }
//   }

//   packet_destroy(packet);
// }

/*---------------------------------------------------------------------*/
void
packet_set_confirmation_cb(iotus_packet_t *packet, packet_sent_cb func_cb)
{
  if(packet != NULL && func_cb != NULL) {
    packet->confirm_cb = func_cb;
  }
}

/*---------------------------------------------------------------------*/
void
packet_confirm_transmission(iotus_packet_t *packet, iotus_netstack_return status)
{
  if(packet != NULL) {
    if (packet->confirm_cb != NULL) {
      (packet->confirm_cb)(packet, status);
      return;
    }
  }
  packet_destroy(packet);
}


/*---------------------------------------------------------------------*/
/**
 * \brief   Count the number of packets on the queue to be sent to a certain node.
 * \param node Node to be searched.
 */
uint8_t
packet_queue_size_by_node(iotus_node_t *node)
{
  iotus_packet_t *packet;
  uint8_t counter = 0;

  if(node == NULL) {
    return 0;
  }

  packet = list_head(gPacketList);
  for(; packet != NULL; packet = list_item_next(packet)) {
    if(packet_get_next_destination(packet) == node) {
      counter++;
    }
  }

  return counter;
}

/*---------------------------------------------------------------------*/
/**
 * \brief   Return the first packet after "from" of the list with target equal to node.
 * \param node Node to be searched.
 * \param from The packet at which it should start from.
 */
iotus_packet_t *
packet_get_queue_by_node(iotus_node_t *node, iotus_packet_t *from)
{
  iotus_packet_t *packet;

  if(node == NULL) {
    return 0;
  }

  if(from == NULL) {
    packet = list_head(gPacketList);
  } else {
    packet = list_item_next(from);
  }
  for(; packet != NULL; packet = list_item_next(packet)) {
    if(packet_get_next_destination(packet) == node) {
      return packet;
    }
  }

  return NULL;
}

/*---------------------------------------------------------------------*/
/*
 * \brief Default function required from IoTUS, to initialize, run and finish this service
 */
void
iotus_signal_handler_packet(iotus_service_signal signal, void *data)
{
  if(IOTUS_START_SERVICE == signal) {

    #if IOTUS_USING_MALLOC == 0
    SAFE_PRINT("\tService Packet:MMEM\n");
    #else
    SAFE_PRINT("\tService Packet:Malloc\n");
    #endif
    
    // Initiate the lists of module
    list_init(gPacketList);

    gPacketIDcounter = 0;

  } else if (IOTUS_END_SERVICE == signal){

  }
}

/* The following stuff ends the \defgroup block at the beginning of
   the file: */

/** @} */
