/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         A Carrier Sense Multiple Access (CSMA) MAC layer
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

// #include "csma.h"
#include "contiki.h"
#include "contikiMAC.h"
#include "csma_contikimac.h"
#include "edytee_routing.h"
#include "dev/leds.h"
#include "iotus-api.h"
#include "iotus-core.h"
#include "iotus-netstack.h"
#include "neighbor_discovery.h"
#include "random.h"
#include "piggyback.h"
#include "tree_manager.h"
#include "sys/clock.h"
#include "sys/ctimer.h"
#include "sys/timer.h"
#include "lib/random.h"
#include <string.h>
#include <stdio.h>


#define DEBUG IOTUS_DONT_PRINT//IOTUS_PRINT_IMMEDIATELY
#define THIS_LOG_FILE_NAME_DESCRITOR "contCSMA"
#include "safe-printer.h"

#define PRINTF(...)         SAFE_PRINTF_CLEAN(__VA_ARGS__)
// #define PRINTF(...)         printf(__VA_ARGS__)

/* Constants of the IEEE 802.15.4 standard */

/* macMinBE: Initial backoff exponent. Range 0--CSMA_MAX_BE */
#ifdef CSMA_CONF_MIN_BE
#define CSMA_MIN_BE CSMA_CONF_MIN_BE
#else
#define CSMA_MIN_BE 0
#endif

/* macMaxBE: Maximum backoff exponent. Range 3--8 */
#ifdef CSMA_CONF_MAX_BE
#define CSMA_MAX_BE CSMA_CONF_MAX_BE
#else
#define CSMA_MAX_BE 4
#endif

/* macMaxCSMABackoffs: Maximum number of backoffs in case of channel busy/collision. Range 0--5 */
#ifdef CSMA_CONF_MAX_BACKOFF
#define CSMA_MAX_BACKOFF CSMA_CONF_MAX_BACKOFF
#else
#define CSMA_MAX_BACKOFF 5
#endif

/* macMaxFrameRetries: Maximum number of re-transmissions attampts. Range 0--7 */
#ifdef CSMA_CONF_MAX_FRAME_RETRIES
#define CSMA_MAX_MAX_FRAME_RETRIES CSMA_CONF_MAX_FRAME_RETRIES
#else
#define CSMA_MAX_MAX_FRAME_RETRIES 7
#endif

#if EXP_CONTIKIMAC_802_15_4 == 1
static uint8_t isCoordinator = 0;
// static uint8_t private_nd_control[12];

//Timer for sending neighbor discovery
static struct ctimer sendNDTimer, connectionWathdog;
static clock_time_t backOffDifference, randomAddTime;
static iotus_node_t *gBestNode = NULL;
static uint8_t gCoordinatorRank = 0xFF;

static struct timer NDScanTimer;
typedef enum {
  DATA_LINK_ND_CONNECTION_STATUS_DISCONNECTED,
  DATA_LINK_ND_CONNECTION_STATUS_CONNECTED,
  DATA_LINK_ND_CONNECTION_STATUS_WAITING_REGISTER,
  DATA_LINK_ND_CONNECTION_STATUS_WAITING_ANSWER,
  DATA_LINK_ND_CONNECTION_STATUS_WAITING_CONFIRMATION
} csma_connection_status;

static csma_connection_status gConnectionStatus;
#endif


// /*---------------------------------------------------------------------------*/
// static struct neighbor_queue *
// neighbor_queue_from_addr(const linkaddr_t *addr)
// {
//   struct neighbor_queue *n = list_head(neighbor_list);
//   while(n != NULL) {
//     if(linkaddr_cmp(&n->addr, addr)) {
//       return n;
//     }
//     n = list_item_next(n);
//   }
//   return NULL;
// }
/*---------------------------------------------------------------------------*/
static clock_time_t
backoff_period(void)
{
  clock_time_t time;
  /* The retransmission time must be proportional to the channel
     check interval of the underlying radio duty cycling layer. */
  time = active_data_link_protocol->channel_check_interval();

  /* If the radio duty cycle has no channel check interval, we use
   * the default in IEEE 802.15.4: aUnitBackoffPeriod which is
   * 20 symbols i.e. 320 usec. That is, 1/3125 second. */
  if(time == 0) {
    time = MAX(CLOCK_SECOND / 3125, 1);
  }
  return time;
}
/*---------------------------------------------------------------------------*/
static void
transmit_packet_list(void *ptr)
{
  iotus_packet_t *packet = ptr;
  if(packet) {
    uint8_t waitingList = packet_queue_size_by_node(packet_get_next_destination(packet));
      PRINTF("csma: preparing number %d, queue len %u\n", packet->transmissions, waitingList);
    /* Send packets in the neighbor's list */
    active_data_link_protocol->send_list(packet, waitingList);
  }
}
/*---------------------------------------------------------------------------*/
static void
schedule_transmission(iotus_packet_t *packet)
{
  clock_time_t delay;
  int backoff_exponent; /* BE in IEEE 802.15.4 */

  // backoff_exponent = MIN(packet->collisions, CSMA_MAX_BE);
  backoff_exponent = CSMA_MAX_BE;

  /* Compute max delay as per IEEE 802.15.4: 2^BE-1 backoff periods  */
  delay = ((1 << backoff_exponent) - 1) * backoff_period();
  if(delay > 0) {
    /* Pick a time for next transmission */
    delay = random_rand() % delay;
  }
  PRINTF("csma: scheduling transmission in %u ticks, NB=%u, BE=%u\n",
      (unsigned)delay, packet->collisions, backoff_exponent);
  ctimer_set(&packet->transmit_timer, delay, transmit_packet_list, packet);
}
/*---------------------------------------------------------------------------*/
static void
tx_done(int status, iotus_packet_t *packet)
{
  if(status == MAC_TX_OK) {
    PRINTF("csma: rexmit ok %d %p\n", packet->transmissions, packet);
  }
  else if(status == MAC_TX_COLLISION ||
     status == MAC_TX_NOACK) {
    PRINTF("csma: drop %p %u with status %d after %d transmissions, %d collisions\n",
                 packet, packet->pktID ,status, packet->transmissions, packet->collisions);
  }
  else {
    PRINTF("csma: rexmit failed %d %p: %d\n", packet->transmissions, packet, status);
  }

  //remove any future timer or so
  ctimer_stop(&packet->transmit_timer);

  packet_confirm_transmission(packet, status);
}
/*---------------------------------------------------------------------------*/
static void
rexmit(iotus_packet_t *packet)
{
  schedule_transmission(packet);
  /* This is needed to correctly attribute energy that we spent
     transmitting this packet. */
  // queuebuf_update_attr_from_packetbuf(q->buf);
  // COPY PAKET FOR RTx
}
/*---------------------------------------------------------------------------*/
static void
collision(iotus_packet_t *packet, int num_transmissions)
{
  packet->collisions += num_transmissions;

  if(packet->collisions > CSMA_MAX_BACKOFF) {
    packet->collisions = CSMA_MIN_BE;
    /* Increment to indicate a next retry */
    packet->transmissions++;
  }

  if(packet->transmissions >= CSMA_MAX_MAX_FRAME_RETRIES + 1) {
    tx_done(MAC_TX_COLLISION, packet);
  } else {
    PRINTF("csma: rexmit collision %d %p\n", packet->transmissions, packet);
    rexmit(packet);
  }
}

/*---------------------------------------------------------------------------*/
static void
noack(iotus_packet_t *packet, int num_transmissions)
{
  packet->collisions = CSMA_MIN_BE;
  packet->transmissions += num_transmissions;

  if(packet->transmissions >= CSMA_MAX_MAX_FRAME_RETRIES + 1) {
    tx_done(MAC_TX_NOACK, packet);
  } else {
    PRINTF("csma: rexmit noack %d %p\n", packet->transmissions, packet);
    rexmit(packet);
  }
}
/*---------------------------------------------------------------------------*/
static void
tx_ok(iotus_packet_t *packet, int num_transmissions)
{
  packet->collisions = CSMA_MIN_BE;
  packet->transmissions += num_transmissions;
  tx_done(MAC_TX_OK, packet);
}

/*---------------------------------------------------------------------------*/
static void
reset_connection(void *ptr)
{
  ctimer_stop(&sendNDTimer);
  ctimer_stop(&connectionWathdog);
  gConnectionStatus = DATA_LINK_ND_CONNECTION_STATUS_DISCONNECTED;


  leds_off(LEDS_ALL);
  gCoordinatorRank = 0xFF;
  gBestNode = NULL;

  contikiMAC_turn_off(1);
  timer_set(&NDScanTimer, CLOCK_SECOND*CONTIKIMAC_ND_SCAN_TIME);

  edytee_reset_connection();
}

/*---------------------------------------------------------------------------*/
void
csma_packet_sent(iotus_packet_t *packet, int status, int num_transmissions)
{
  if(packet == NULL) {
    PRINTF("csma: pkt not found\n");
    return;
  }
 // printf("vltou %u\n", status);
  if(status == MAC_TX_OK) {
    tx_ok(packet, num_transmissions);
  }
  else if(status == MAC_TX_NOACK) {
    noack(packet, num_transmissions);
  }
  else if(status == MAC_TX_COLLISION) {
    collision(packet, num_transmissions);
  }
  else if(status == MAC_TX_DEFERRED) {

  } else {
    tx_done(status, packet);
  }
}
/*---------------------------------------------------------------------------*/
int8_t
csma_send_packet(iotus_packet_t *packet)
{
  static uint8_t initialized = 0;
  static uint16_t seqno;

  if(!initialized) {
    initialized = 1;
    /* Initialize the sequence number to a random value as per 802.15.4. */
    seqno = random_rand();
  }

  if(seqno == 0) {
    /* PACKETBUF_ATTR_MAC_SEQNO cannot be zero, due to a pecuilarity
       in framer-802154.c. */
    seqno++;
  }
  // packetbuf_set_attr(PACKETBUF_ATTR_MAC_SEQNO, seqno++);
  packet_set_sequence_number(packet, seqno++); 

  schedule_transmission(packet);

  return MAC_TX_DEFERRED;
  // mac_call_sent_callback(sent, ptr, MAC_TX_ERR, 1);
}


/*---------------------------------------------------------------------------*/
static void
control_frames_nd_cb(iotus_packet_t *packet, iotus_netstack_return returnAns)
{
  SAFE_PRINTF_LOG_INFO("nd %p sent %u", packet, returnAns);
  // if(returnAns == MAC_TX_OK) {
  packet_destroy(packet);

  if(returnAns != MAC_TX_OK) {
    if(gConnectionStatus == DATA_LINK_ND_CONNECTION_STATUS_WAITING_ANSWER) {
      reset_connection(NULL);
    }
  }
  // }
}
/*---------------------------------------------------------------------------*/
static void
send_beacon(void *ptr)
{
  clock_time_t backoff = CLOCK_SECOND*CONTIKIMAC_ND_PERIOD_TIME - backOffDifference;//ms
  backOffDifference = (CLOCK_SECOND*((random_rand()%CONTIKIMAC_ND_BACKOFF_TIME)))/1000;

  backoff += backOffDifference;
  ctimer_set(&sendNDTimer, backoff, send_beacon, NULL);

  uint8_t *msg = nd_build_packet_type(ND_PKT_BEACONS);

  iotus_packet_t *packet = iotus_initiate_packet(
                            msg[0],
                            msg,
                            PACKET_PARAMETERS_WAIT_FOR_ACK|PACKET_PARAMETERS_ALLOW_PIGGYBACK,
                            IOTUS_PRIORITY_DATA_LINK,
                            5000,
                            NODES_BROADCAST,
                            control_frames_nd_cb);

  if(NULL == packet) {
    SAFE_PRINTF_LOG_INFO("Packet failed");
    return;
  }

  packet_set_type(packet, IOTUS_PACKET_TYPE_IEEE802154_BEACON);
 
  SAFE_PRINTF_LOG_INFO("Beacon nd %u \n", packet->pktID);
  //active_data_link_protocol->send(packet);
  csma_send_packet(packet);
}

/*---------------------------------------------------------------------------*/
static void
csma_802like_answer_process(void *ptr){
  //ready to request asnwer from router
  if(gBestNode != NULL) {
    // contikiMAC_turn_on();
    contikiMAC_back_on();

    uint8_t nextType[1];
    //make resquest
    nd_set_operation_msg(IOTUS_PRIORITY_DATA_LINK, ND_PKT_ASSOCIANTION_GET, 6, (uint8_t *)"answer");

    //make resquest
    uint8_t *msg = nd_build_packet_type(ND_PKT_ASSOCIANTION_GET);

    iotus_packet_t *packet = iotus_initiate_packet(
                              msg[0],
                              msg,
                              PACKET_PARAMETERS_WAIT_FOR_ACK|PACKET_PARAMETERS_ALLOW_PIGGYBACK,
                              IOTUS_PRIORITY_DATA_LINK,
                              5000,
                              gBestNode,
                              control_frames_nd_cb);

    if(NULL == packet) {
      SAFE_PRINTF_LOG_INFO("Packet failed");
      return;
    }

    packet_set_type(packet, IOTUS_PACKET_TYPE_IEEE802154_COMMAND);
    packet->nextDestinationNode = gBestNode;
   
    //Define this commands as
    nextType[0] = ND_PKT_ASSOCIANTION_GET;
    packet_push_bit_header(8, nextType, packet);

    //active_data_link_protocol->send(packet);
    csma_send_packet(packet);

    ctimer_restart(&connectionWathdog);
    gConnectionStatus = DATA_LINK_ND_CONNECTION_STATUS_WAITING_CONFIRMATION;
  } else {
    SAFE_PRINTF_LOG_WARNING("No router found");
  }
}

/*---------------------------------------------------------------------------*/
static void
csma_802like_register_process(void *ptr)
{
  uint8_t nextType[1];
  // contikiMAC_turn_off(0);
  nd_set_operation_msg(IOTUS_PRIORITY_DATA_LINK, ND_PKT_ASSOCIANTION_REQ, 8, (uint8_t *)"register");

  //make resquest
  uint8_t *msg = nd_build_packet_type(ND_PKT_ASSOCIANTION_REQ);

  iotus_packet_t *packet = iotus_initiate_packet(
                            msg[0],
                            msg,
                            PACKET_PARAMETERS_WAIT_FOR_ACK|PACKET_PARAMETERS_ALLOW_PIGGYBACK,
                            IOTUS_PRIORITY_DATA_LINK,
                            5000,
                            gBestNode,
                            control_frames_nd_cb);

  if(NULL == packet) {
    SAFE_PRINTF_LOG_INFO("Packet failed");
    return;
  }

  packet_set_type(packet, IOTUS_PACKET_TYPE_IEEE802154_COMMAND);
  packet->nextDestinationNode = gBestNode;
 
  //Define this commands as
  nextType[0] = ND_PKT_ASSOCIANTION_REQ;
  packet_push_bit_header(8, nextType, packet);

  gConnectionStatus = DATA_LINK_ND_CONNECTION_STATUS_WAITING_ANSWER;

  // active_data_link_protocol->send(packet);
  csma_send_packet(packet);

  randomAddTime = (CLOCK_SECOND*((random_rand()%CONTIKIMAC_ND_BACKOFF_TIME)))/1000;
  clock_time_t backoff = CLOCK_SECOND + randomAddTime;//ms
  ctimer_set(&sendNDTimer, backoff, csma_802like_answer_process, NULL);
  ctimer_set(&connectionWathdog, CLOCK_SECOND*CONTIKIMAC_WATCHDOG_TIME, reset_connection, NULL);
}

/*---------------------------------------------------------------------*/
static void
receive_nd_frames(struct packet_piece *packet, uint8_t type, uint8_t size, uint8_t *data)
{
  iotus_node_t *source = packet_get_prevSource_node(packet);
  uint8_t nextType[1];
  nd_node_nogotiating = source;

  if(type == ND_PKT_BEACONS &&
    addresses_self_get_pointer(IOTUS_ADDRESSES_TYPE_ADDR_SHORT)[0] != 1) {
    if(gConnectionStatus == DATA_LINK_ND_CONNECTION_STATUS_CONNECTED) {
      //Ignore msg
    } else if(gConnectionStatus == DATA_LINK_ND_CONNECTION_STATUS_WAITING_ANSWER) {
      //Nothing to do      
    } else if(gConnectionStatus == DATA_LINK_ND_CONNECTION_STATUS_WAITING_CONFIRMATION) {
      //Nothing to do now
    } else {
    // printf("getting\n");
      if(!timer_expired(&NDScanTimer)) {
        uint8_t sourceNodeRank = data[0];

        if(gBestNode == NULL) {
          gBestNode = source;
          // printf("found %p\n", gBestNode);
        } else {
          if(gCoordinatorRank < sourceNodeRank) {
            gBestNode = source;
            // printf("changing\n");
          } else {
          // printf("keeping\n");
          }
        }
      } else {
        //Select the best rank node and make a request
        // printf("t expired %p\n", gBestNode);
        if(gBestNode != NULL) {

          gConnectionStatus = DATA_LINK_ND_CONNECTION_STATUS_WAITING_REGISTER;
          
          randomAddTime = (CLOCK_SECOND*((random_rand()%CONTIKIMAC_ND_BACKOFF_TIME)))/1000;
          clock_time_t backoff = randomAddTime;//ms
          ctimer_set(&sendNDTimer, backoff, csma_802like_register_process, NULL);
        } else {
          //Nothing found. Start over...
          timer_set(&NDScanTimer, CLOCK_SECOND*CONTIKIMAC_ND_SCAN_TIME);
          leds_on(LEDS_RED);

          SAFE_PRINTF_LOG_INFO("No router found");
        }
      }
    }
  } else {
    #if DEBUG != IOTUS_DONT_PRINT
    uint8_t nodeSourceAddress = nodes_get_address(IOTUS_ADDRESSES_TYPE_ADDR_SHORT, source)[0];
    #endif

    if(type == ND_PKT_ASSOCIANTION_REQ) {
      SAFE_PRINTF_LOG_INFO("register cmm from %u\n", nodeSourceAddress);
      //TODO send info to application layer and confirm association
    } else if(type == ND_PKT_ASSOCIANTION_GET) {
      SAFE_PRINTF_LOG_INFO("answer cmm from %u\n", nodeSourceAddress);
      nd_set_operation_msg(IOTUS_PRIORITY_DATA_LINK, ND_PKT_ASSOCIANTION_ANS, 4, (uint8_t *)"join");

      //make resquest
      uint8_t *msg = nd_build_packet_type(ND_PKT_ASSOCIANTION_ANS);

      iotus_packet_t *packet = iotus_initiate_packet(
                                msg[0],
                                msg,
                                PACKET_PARAMETERS_WAIT_FOR_ACK|PACKET_PARAMETERS_ALLOW_PIGGYBACK,
                                IOTUS_PRIORITY_DATA_LINK,
                                5000,
                                source,
                                control_frames_nd_cb);
      if(NULL == packet) {
        SAFE_PRINTF_LOG_INFO("Packet failed");
        return;
      }
      packet_set_type(packet, IOTUS_PACKET_TYPE_IEEE802154_COMMAND);
      packet->nextDestinationNode = source;

      //Define this commands as
      nextType[0] = ND_PKT_ASSOCIANTION_ANS;
      packet_push_bit_header(8, nextType, packet);
     
      // active_data_link_protocol->send(packet);
      csma_send_packet(packet);
    } else if(type == ND_PKT_ASSOCIANTION_ANS) {
      ctimer_stop(&connectionWathdog);
      SAFE_PRINTF_LOG_INFO("join cmm from %u\n", nodeSourceAddress);

      fatherNode = gBestNode;
      uint8_t *rankPointer = pieces_get_additional_info_var(
                                  source->additionalInfoList,
                                  IOTUS_NODES_ADD_INFO_TYPE_TOPOL_TREE_RANK);


      gCoordinatorRank = *rankPointer + 1;
      gConnectionStatus = DATA_LINK_ND_CONNECTION_STATUS_CONNECTED;
      leds_off(LEDS_RED);
      leds_on(LEDS_GREEN);
      // contikiMAC_back_on();
      contikiMAC_turn_on();

      //Now continue routing operation in the case of a router device
      if(treeRouter) {
        printf("coord rank %u\n", gCoordinatorRank);

        nd_set_operation_msg(IOTUS_PRIORITY_DATA_LINK, ND_PKT_BEACONS, 1, &gCoordinatorRank);

        backOffDifference = (CLOCK_SECOND*((random_rand()%CONTIKIMAC_ND_BACKOFF_TIME)))/1000;
        clock_time_t backoff = CLOCK_SECOND*CONTIKIMAC_ND_PERIOD_TIME + backOffDifference;//ms
        ctimer_set(&sendNDTimer, backoff, send_beacon, NULL);
      }
    } else {
      //Nothing to do
    }
  }
}

/*---------------------------------------------------------------------------*/
void
csma_control_frame_receive(iotus_packet_t *packet)
{
  //Send this back to the ND service
  if(packet_get_type(packet) == IOTUS_PACKET_TYPE_IEEE802154_BEACON) {
    nd_unwrap_msg(ND_PKT_BEACONS, packet);
  } else if(packet_get_type(packet) == IOTUS_PACKET_TYPE_IEEE802154_COMMAND) {
    //Ths is a command
    uint8_t commandType = packet_unwrap_pushed_byte(packet);
    // printf("commmand %u\n", commandType);
    nd_unwrap_msg(commandType, packet);
  }
}

/*---------------------------------------------------------------------*/
void start_802_15_4_contikimac(void)
{
  gConnectionStatus = DATA_LINK_ND_CONNECTION_STATUS_DISCONNECTED;

  nd_set_layer_operations(IOTUS_PRIORITY_DATA_LINK, ND_PKT_BEACONS);
  nd_set_layer_operations(IOTUS_PRIORITY_DATA_LINK, ND_PKT_ASSOCIANTION_REQ);
  nd_set_layer_operations(IOTUS_PRIORITY_DATA_LINK, ND_PKT_ASSOCIANTION_GET);
  nd_set_layer_operations(IOTUS_PRIORITY_DATA_LINK, ND_PKT_ASSOCIANTION_ANS);
  // nd_set_layer_operations(IOTUS_PRIORITY_DATA_LINK, ND_PKT_ASSOCIANTION_CON);
  nd_set_layer_cb(IOTUS_PRIORITY_DATA_LINK, receive_nd_frames);

  if(treeRouter &&
     addresses_self_get_pointer(IOTUS_ADDRESSES_TYPE_ADDR_SHORT)[0] == 1) {
    //This is the root...
    gCoordinatorRank = 1;

    nd_set_operation_msg(IOTUS_PRIORITY_DATA_LINK, ND_PKT_BEACONS, 1, &gCoordinatorRank);

    backOffDifference = (CLOCK_SECOND*((random_rand()%CONTIKIMAC_ND_BACKOFF_TIME)))/1000;
    clock_time_t backoff = CLOCK_SECOND*CONTIKIMAC_ND_PERIOD_TIME + backOffDifference;//ms
    ctimer_set(&sendNDTimer, backoff, send_beacon, NULL);
  } else {
    timer_set(&NDScanTimer, CLOCK_SECOND*CONTIKIMAC_ND_SCAN_TIME);
  }
  if(IOTUS_PRIORITY_DATA_LINK == iotus_get_layer_assigned_for(IOTUS_CHORE_ONEHOP_BROADCAST)) {
    nd_association_scan_duration = CLOCK_SECOND*CONTIKIMAC_ND_SCAN_TIME;
    nd_beacon_period = CLOCK_SECOND*CONTIKIMAC_ND_PERIOD_TIME;
  }
}
