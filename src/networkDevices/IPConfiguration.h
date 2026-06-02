#pragma once

#include <Preferences.h>
#include <IPAddress.h>

/**
 * @brief Manages IP configuration settings using the Preferences storage.
 *
 * This class provides access to the stored IP configuration values such as
 * IP address, subnet mask, default gateway, and DNS server. It also handles
 * fallback to DHCP if the configuration is incomplete.
 */
class IPConfiguration
{
public:
    /**
     * @brief Constructs the IPConfiguration and loads stored network settings.
     *
     * If static IP settings are incomplete, the class will enforce DHCP mode.
     *
     * @param preferences Pointer to the Preferences object for persistent storage access.
     */
    explicit IPConfiguration(Preferences *preferences);

    /**
     * @brief Checks if DHCP mode is enabled.
     *
     * @return true if DHCP is enabled, false if static configuration is used.
     */
    bool dhcpEnabled() const;

    /**
     * @brief Gets the stored static IP address.
     *
     * @return IPAddress object containing the IP address.
     */
    const IPAddress ipAddress() const;

    /**
     * @brief Gets the stored subnet mask.
     *
     * @return IPAddress object containing the subnet.
     */
    const IPAddress subnet() const;

    /**
     * @brief Gets the stored default gateway.
     *
     * @return IPAddress object containing the default gateway.
     */
    const IPAddress defaultGateway() const;

    /**
     * @brief Gets the stored DNS server address.
     *
     * @return IPAddress object containing the DNS server address.
     */
    const IPAddress dnsServer() const;

private:
    Preferences *_preferences = nullptr; // Pointer to preferences storage instance

    IPAddress _ipAddress; // Cached IP address
    IPAddress _subnet;    // Cached subnet mask
    IPAddress _gateway;   // Cached default gateway
    IPAddress _dnsServer; // Cached DNS server
};
