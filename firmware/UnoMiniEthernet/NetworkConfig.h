#pragma once
// Example only: select an unused LAN address and a unique locally administered MAC.
// Keep the four IP bytes shared by startup and network recovery.
#define AIR_IP_0 192
#define AIR_IP_1 168
#define AIR_IP_2 1
#define AIR_IP_3 222
#define AIR_GATEWAY 192,168,1,1
#define AIR_NETMASK 255,255,255,0
#define AIR_MAC 0x02,0x00,0x00,0x00,0x00,0x01
