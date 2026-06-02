#pragma once

#include <Preferences.h>
#include "NetworkDevice.h"
#include "NetworkDeviceType.h"
#include "IPConfiguration.h"

/**
 * @brief Creates concrete NetworkDevice instances from stored preferences.
 *
 * The factory encapsulates all hardware-ID constants and Ethernet preset
 * configurations that were previously embedded in the anonymous namespace
 * of NukiNetwork.cpp. Moving them here keeps NukiNetwork free of low-level
 * hardware knowledge.
 *
 * Usage:
 * @code
 *     _device = NetworkDeviceFactory::create(
 *                   _networkDeviceType, _hostname,
 *                   _preferences, _ipConfiguration);
 * @endcode
 *
 * Hardware-ID values stored in NVS (preference_network_hardware) are kept
 * identical to the original NukiNetwork implementation so that existing
 * user preferences are not invalidated.
 */
class NetworkDeviceFactory
{
public:
    /**
     * @brief Create a concrete NetworkDevice for the given type.
     *
     * For Wi-Fi: returns a new WifiDevice.
     * For Ethernet: resolves the hardware config from preferences (preset
     *               table or custom config), then returns a new EthernetDevice.
     *               Returns nullptr and logs an error if the hardware ID is
     *               unsupported.
     *
     * @param type             Device type (WiFi or ETH).
     * @param hostname         mDNS / DHCP hostname to pass to the device.
     * @param preferences      NVS preferences pointer (not transferred).
     * @param ipConfiguration  IP settings pointer (not transferred).
     * @return NetworkDevice*  Heap-allocated device, or nullptr on failure.
     */
    static NetworkDevice* create(NetworkDeviceType type,
                                 const String& hostname,
                                 Preferences* preferences,
                                 const IPConfiguration* ipConfiguration);
};
