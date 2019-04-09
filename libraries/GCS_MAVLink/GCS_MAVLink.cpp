/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/// @file	GCS_MAVLink.cpp

/*
This provides some support code and variables for MAVLink enabled sketches

*/
#include "GCS.h"
#include "GCS_MAVLink.h"

#include <AP_Common/AP_Common.h>
#include <AP_GPS/AP_GPS.h>
#include <AP_HAL/AP_HAL.h>


#ifdef MAVLINK_SEPARATE_HELPERS
// Shut up warnings about missing declarations; TODO: should be fixed on
// mavlink/pymavlink project for when MAVLINK_SEPARATE_HELPERS is defined
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#include "include/mavlink/v2.0/mavlink_helpers.h"
#pragma GCC diagnostic pop
#endif

AP_HAL::UARTDriver	*mavlink_comm_port[MAVLINK_COMM_NUM_BUFFERS];

mavlink_system_t mavlink_system = {7,1};

// mask of serial ports disabled to allow for SERIAL_CONTROL
static uint8_t mavlink_locked_mask;

// routing table
MAVLink_routing GCS_MAVLINK::routing;

// static AP_SerialManager pointer
const AP_SerialManager *GCS_MAVLINK::serialmanager_p;

// snoop function for vehicle types that want to see messages for
// other targets
void (*GCS_MAVLINK::msg_snoop)(const mavlink_message_t* msg) = nullptr;

/*
  lock a channel, preventing use by MAVLink
 */
void GCS_MAVLINK::lock_channel(mavlink_channel_t _chan, bool lock)
{
    if (!valid_channel(chan)) {
        return;
    }
    if (lock) {
        mavlink_locked_mask |= (1U<<(unsigned)_chan);
    } else {
        mavlink_locked_mask &= ~(1U<<(unsigned)_chan);
    }
}

// return a MAVLink variable type given a AP_Param type
uint8_t mav_var_type(enum ap_var_type t)
{
    if (t == AP_PARAM_INT8) {
	    return MAVLINK_TYPE_INT8_T;
    }
    if (t == AP_PARAM_INT16) {
	    return MAVLINK_TYPE_INT16_T;
    }
    if (t == AP_PARAM_INT32) {
	    return MAVLINK_TYPE_INT32_T;
    }
    // treat any others as float
    return MAVLINK_TYPE_FLOAT;
}


/// Read a byte from the nominated MAVLink channel
///
/// @param chan		Channel to receive on
/// @returns		Byte read
///
uint8_t comm_receive_ch(mavlink_channel_t chan)
{
    if (!valid_channel(chan)) {
        return 0;
    }

    return (uint8_t)mavlink_comm_port[chan]->read();
}

/// Check for available transmit space on the nominated MAVLink channel
///
/// @param chan		Channel to check
/// @returns		Number of bytes available
uint16_t comm_get_txspace(mavlink_channel_t chan)
{
    if (!valid_channel(chan)) {
        return 0;
    }
    if ((1U<<chan) & mavlink_locked_mask) {
        return 0;
    }
	int16_t ret = mavlink_comm_port[chan]->txspace();
	if (ret < 0) {
		ret = 0;
	}
    return (uint16_t)ret;
}

/// Check for available data on the nominated MAVLink channel
///
/// @param chan		Channel to check
/// @returns		Number of bytes available
uint16_t comm_get_available(mavlink_channel_t chan)
{
    if (!valid_channel(chan)) {
        return 0;
    }
    if ((1U<<chan) & mavlink_locked_mask) {
        return 0;
    }
    int16_t bytes = mavlink_comm_port[chan]->available();
	if (bytes == -1) {
		return 0;
	}
    return (uint16_t)bytes;
}

/*
  send a buffer out a MAVLink channel
 */
void comm_send_buffer(mavlink_channel_t chan, const uint8_t *buf, uint8_t len)
{
    if (!valid_channel(chan)) {
        return;
    }
    mavlink_comm_port[chan]->write(buf, len);
}

extern const AP_HAL::HAL& hal;

/*
  return true if the MAVLink parser is idle, so there is no partly parsed
  MAVLink message being processed
 */
bool comm_is_idle(mavlink_channel_t chan)
{
	mavlink_status_t *status = mavlink_get_channel_status(chan);
	return status == nullptr || status->parse_state <= MAVLINK_PARSE_STATE_IDLE;
}

//#ifdef XBEE_CONNECT2
Neighbours_Pos* GCS_MAVLINK::update_neighbours_pose(uint16_t index_i)
{
	return &(neighbours_pose[index_i]);
}
	
void GCS_MAVLINK::update_neighbours_mask(uint16_t mask)
{
	neighbours_mask = mask;
}

void GCS_MAVLINK::clear_neighbours_mask(void)
{
	neighbours_mask = 0;
}

void GCS_MAVLINK::init_neighbours_pose(void)
{
	neighbours_pose = new Neighbours_Pos[NEIGHBOUR_NUM];
}

Neighbours_Pos* GCS::update_neighbours_pose(uint16_t index_i)
{
	return chan(0).update_neighbours_pose(index_i);
}
	
void GCS::update_neighbours_mask(uint16_t mask)
{
	chan(0).update_neighbours_mask(mask);
}

void GCS::clear_neighbours_mask(void)
{
	chan(0).clear_neighbours_mask();
}

void GCS::init_neighbours_pose(void)
{
	chan(0).init_neighbours_pose();
}
//#endif

