#pragma once

#include <ETH.h>
#include <Network.h>
#include <SPI.h>
#include "NetworkDevice.h"

/**
 * @brief Ethernet implementation of NetworkDevice.
 *
 * Supports both SPI Ethernet controllers (e.g. W5500, DM9051) and
 * RMII Ethernet PHYs (e.g. LAN8720) via a single Config struct.
 *
 * The Config is populated by NetworkDeviceFactory::create() from the
 * preset table or the custom-Ethernet preferences, then passed to the
 * constructor. EthernetDevice never reads hardware preferences itself.
 *
 * Static IP retry:
 *   When DHCP is disabled and the hardware uses RMII (not SPI), the IP
 *   cannot always be applied immediately after begin(). EthernetDevice
 *   schedules a retry via _checkIpTs and applies it in update().
 *
 * Critical failure guard:
 *   The extern flag ethCriticalFailure (set/cleared around ETH.begin())
 *   detects crashes that occurred during the previous hardware initialisation
 *   attempt. If set when initialize() is called, the device falls back to
 *   Wi-Fi and reboots.
 */
class EthernetDevice : public NetworkDevice
{
public:
    /**
     * @brief Hardware descriptor populated by NetworkDeviceFactory.
     *
     * For SPI devices (useSpi == true): csPin, irqPin, resetPin, spiSck/Miso/Mosi
     * must be set; mdcPin, mdioPin, powerPin, clockMode are unused.
     *
     * For RMII devices (useSpi == false): mdcPin, mdioPin, powerPin, clockMode
     * must be set; csPin, irqPin, resetPin, spi*Pin are unused (-1).
     */
    struct Config
    {
        /** Human-readable device name for logging and the web UI. */
        String deviceName;

        /**
         * @brief True for SPI-attached controllers (W5500, DM9051 …).
         *        False for RMII PHYs (LAN8720 …).
         */
        bool useSpi = false;

        /** Ethernet PHY type passed to ETH.begin(). */
        eth_phy_type_t phyType = ETH_PHY_LAN8720;

        /** PHY address on the MDIO bus. */
        int32_t phyAddr = 0;

        /** RMII: PHY power-enable pin (-1 if not used). */
        int powerPin = -1;

        /** RMII: MDC (management data clock) pin. */
        int mdcPin = -1;

        /** RMII: MDIO (management data I/O) pin. */
        int mdioPin = -1;

        /** RMII: Reference clock routing / mode. */
        eth_clock_mode_t clockMode = ETH_CLOCK_GPIO17_OUT;

        /** SPI: Chip-select pin. */
        int csPin = -1;

        /** SPI: Interrupt pin (-1 if not used). */
        int irqPin = -1;

        /** SPI: Hardware reset pin (-1 if not used). */
        int resetPin = -1;

        /** SPI: SCK pin. */
        int spiSckPin = -1;

        /** SPI: MISO pin. */
        int spiMisoPin = -1;

        /** SPI: MOSI pin. */
        int spiMosiPin = -1;
    };

    /**
     * @brief Construct an EthernetDevice.
     *
     * @param hostname         mDNS / DHCP hostname.
     * @param preferences      NVS preferences (not owned).
     * @param ipConfiguration  IP settings (not owned).
     * @param config           Hardware descriptor from NetworkDeviceFactory.
     */
    EthernetDevice(const String& hostname,
                   Preferences* preferences,
                   const IPConfiguration* ipConfiguration,
                   const Config& config);

    // -----------------------------------------------------------------------
    // NetworkDevice interface
    // -----------------------------------------------------------------------

    /**
     * @brief Check ethCriticalFailure, initialise ETH hardware, register
     *        event handler, and apply static IP if needed.
     *
     * If ethCriticalFailure is set on entry (indicating a previous crash
     * in ETH.begin()), the device falls back to Wi-Fi and reboots.
     * After hardware init, ethCriticalFailure is set before ETH.begin()
     * and cleared on success — this is the standard crash-guard pattern
     * from the original NukiNetwork.
     */
    void initialize() override;

    /**
     * @brief Retry static IP configuration for RMII devices.
     *
     * RMII ETH may not accept the static IP immediately after begin().
     * When _checkIpTs is set, update() waits for the delay, then calls
     * ETH.config() if the currently assigned IP still doesn't match.
     * Called every network-task loop by NukiNetwork::update().
     */
    void update() override;

    /**
     * @brief Restart to re-detect Ethernet hardware (ReconfigureETH reason).
     */
    void reconfigure() override;

    /**
     * @brief Suppress restart-on-disconnect watchdog.
     *
     * Called during OTA to prevent a watchdog reboot mid-flash.
     */
    void disableAutoRestarts() override;

    // -----------------------------------------------------------------------
    // Status
    // -----------------------------------------------------------------------

    /**
     * @brief True once ARDUINO_EVENT_ETH_GOT_IP has fired.
     */
    bool isConnected() const override;

    /**
     * @brief Always returns false — Ethernet has no AP mode.
     */
    bool isApOpen() const override { return false; }

    /**
     * @brief Always returns true — Ethernet LAN is always accessible.
     *
     * HAR and REST API may be available on the LAN even without a WAN route.
     */
    bool networkGateOpen() const override { return true; }

    // -----------------------------------------------------------------------
    // Network information
    // -----------------------------------------------------------------------

    /** @brief Return ETH.localIP() as a string. */
    String localIP() const override;

    /**
     * @brief Returns 127 (= "not available").
     *
     * HarClient::update() skips RSSI reporting when signalStrength() == 127,
     * which is the correct behaviour for a wired Ethernet link.
     */
    int8_t signalStrength() const override { return 127; }

    /** @brief Return NetworkDeviceType::ETH. */
    NetworkDeviceType type() const override;

    /** @brief Return Config::deviceName. */
    String deviceName() const override;

private:
    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Call ETH.begin() with parameters from _config.
     *
     * For SPI: calls SPI.begin() first, then ETH.begin() with SPI instance.
     * For RMII: calls ETH.begin() with MDC/MDIO/power/clock parameters.
     *
     * @return true  if ETH.begin() reported success.
     * @return false if ETH.begin() failed or the platform is unsupported.
     */
    bool beginEthernetDevice();

    /**
     * @brief Ethernet network event handler.
     *
     * Registered via Network.onEvent() in initialize().
     * Handles: ETH_START, ETH_CONNECTED, ETH_GOT_IP, ETH_GOT_IP6,
     *          ETH_LOST_IP, ETH_DISCONNECTED, ETH_STOP.
     *
     * Mirrors the original NukiNetwork::onNetworkEvent() Ethernet section.
     */
    void onNetworkEvent(arduino_event_id_t event, arduino_event_info_t info);

    /**
     * @brief Called on any disconnection event (LOST_IP / DISCONNECTED / STOP).
     *
     * If preference_restart_on_disconnect is true and the device has been up
     * for at least 60 s, triggers a RestartOnDisconnectWatchdog reboot.
     */
    void onDisconnected();

    // -----------------------------------------------------------------------
    // Members
    // -----------------------------------------------------------------------

    /** Hardware descriptor provided by NetworkDeviceFactory at construction. */
    Config _config;

    /** True after ETH.begin() returned true. */
    bool _hardwareInitialized = false;

    /**
     * @brief Timestamp (espMillis()) after which to retry static IP config.
     *
     * -1 means no retry is pending. Set during initialize() for RMII devices
     * when DHCP is disabled; cleared in update() once the IP is applied.
     */
    int64_t _checkIpTs = -1;

    /** True once ARDUINO_EVENT_ETH_GOT_IP has fired. */
    bool _connected = false;

    /**
     * @brief Intermediate flag set by ARDUINO_EVENT_ETH_CONNECTED.
     *
     * Preserved from the original NukiNetwork implementation.
     */
    bool _ethConnected = false;

    /**
     * @brief When false, onDisconnected() skips the restart-on-disconnect
     *        watchdog. Set by disableAutoRestarts() during OTA.
     */
    bool _autoRestartEnabled = true;
};
