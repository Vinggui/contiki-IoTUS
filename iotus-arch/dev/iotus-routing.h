/**
 * \defgroup description...
 *
 * This...
 *
 * @{
 */

/*
 * iotus-routing.h
 *
 *  Created on: Oct 23, 2017
 *      Author: vinicius
 */



/* The following stuff ends the \defgroup block at the beginning of
   the file: */

/** @} */
#ifndef IOTUS_DEV_ROUTING_H_
#define IOTUS_DEV_ROUTING_H_

#include "iotus-core.h"
#include "packet.h"
#include "platform-conf.h"


struct iotus_routing_protocol_struct {
  void (* start)(void);
  void (* post_start)(void);
  void (* run)(void);
  void (* close)(void);
  iotus_netstack_return (* build_to_send)(iotus_packet_t *packet);
  void (* sent_cb)(iotus_packet_t *packet, iotus_netstack_return returnAns);
  iotus_netstack_return (* receive)(iotus_packet_t *packet);
};

#endif /* IOTUS_DEV_ROUTING_H_ */

/* The following stuff ends the \defgroup block at the beginning of
   the file: */

/** @} */