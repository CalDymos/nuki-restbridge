#include "NetworkDeviceFactory.h"
#include "WifiDevice.h"
#include "EthernetDevice.h"
#include "PreferencesKeys.h"
#include "Logger.h"

// -----------------------------------------------------------------------
// Implementation details — anonymous namespace (identical to original
// NukiNetwork anonymous namespace, just moved to this factory file)
// -----------------------------------------------------------------------

namespace
{
    // Hardware-ID constants — values MUST stay identical to the original
    // NukiNetwork so that existing NVS preferences remain valid.
    constexpr int NETWORK_HARDWARE_WIFI                  = 1;
    constexpr int NETWORK_HARDWARE_LEGACY_LAN8720        = 2;
    constexpr int NETWORK_HARDWARE_M5STACK_W5500         = 3;
    constexpr int NETWORK_HARDWARE_OLIMEX_LAN8720        = 4;
    constexpr int NETWORK_HARDWARE_WT32_LAN8720          = 5;
    constexpr int NETWORK_HARDWARE_M5STACK_POE_TLK110   = 6;
    constexpr int NETWORK_HARDWARE_LILYGO_T_ETH_POE     = 7;
    constexpr int NETWORK_HARDWARE_GL_S10_IP101          = 8;
    constexpr int NETWORK_HARDWARE_ETH01_EVO_DM9051      = 9;
    constexpr int NETWORK_HARDWARE_M5STACK_W5500_S3      = 10;
    constexpr int NETWORK_HARDWARE_CUSTOM                = 11;
    constexpr int NETWORK_HARDWARE_LILYGO_T_ETH_ELITE   = 12;
    constexpr int NETWORK_HARDWARE_WAVESHARE_ESP32_S3_ETH = 13;
    constexpr int NETWORK_HARDWARE_LILYGO_T_ETH_LITE_S3  = 14;
    constexpr int NETWORK_HARDWARE_OLIMEX_LAN8720_WROVER = 20;

    // The eth_clock_mode_t enum is not available on all ESP32 targets.
#if !defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32P4)
    typedef enum
    {
        ETH_CLOCK_GPIO0_IN  = 0,
        ETH_CLOCK_GPIO0_OUT = 1,
        ETH_CLOCK_GPIO16_OUT = 2,
        ETH_CLOCK_GPIO17_OUT = 3
    } eth_clock_mode_t;
#endif

    // -----------------------------------------------------------------------
    // Internal flat config struct (matches NukiNetwork EthernetDeviceConfig)
    // used only within this factory to build EthernetDevice::Config.
    // -----------------------------------------------------------------------
    struct PresetEntry
    {
        int          hardwareId;
        const char*  deviceName;
        bool         useSpi;
        eth_phy_type_t  phyType;
        int32_t      phyAddr;
        int          powerPin;
        int          mdcPin;
        int          mdioPin;
        eth_clock_mode_t clockMode;
        int          csPin;
        int          irqPin;
        int          resetPin;
        int          spiSckPin;
        int          spiMisoPin;
        int          spiMosiPin;
    };

    // -----------------------------------------------------------------------
    // Preset table — copied verbatim from NukiNetwork anonymous namespace
    // to ensure identical hardware support.
    // -----------------------------------------------------------------------
    const PresetEntry* findPreset(int hardwareId)
    {
        static const PresetEntry presets[] =
        {
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
            // Backward-compatible alias for the old generic "LAN module" option
            {NETWORK_HARDWARE_LEGACY_LAN8720,
             "LAN module (LAN8720 / Olimex-compatible)",
             false, ETH_PHY_LAN8720, 0, 12, 23, 18, ETH_CLOCK_GPIO17_OUT,
             -1, -1, -1, -1, -1, -1},

            {NETWORK_HARDWARE_OLIMEX_LAN8720,
             "Olimex ESP32-POE/POE-ISO WROOM (LAN8720)",
             false, ETH_PHY_LAN8720, 0, 12, 23, 18, ETH_CLOCK_GPIO17_OUT,
             -1, -1, -1, -1, -1, -1},

            {NETWORK_HARDWARE_OLIMEX_LAN8720_WROVER,
             "Olimex ESP32-POE/POE-ISO WROVER (LAN8720)",
             false, ETH_PHY_LAN8720, 0, 12, 23, 18, ETH_CLOCK_GPIO0_OUT,
             -1, -1, -1, -1, -1, -1},

            {NETWORK_HARDWARE_WT32_LAN8720,
             "WT32-ETH01 (LAN8720)",
             false, ETH_PHY_LAN8720, 1, 16, 23, 18, ETH_CLOCK_GPIO0_IN,
             -1, -1, -1, -1, -1, -1},

            {NETWORK_HARDWARE_M5STACK_POE_TLK110,
             "M5Stack PoESP32 Unit (TLK110)",
             false, ETH_PHY_TLK110, 1, 5, 23, 18, ETH_CLOCK_GPIO0_IN,
             -1, -1, -1, -1, -1, -1},

            {NETWORK_HARDWARE_LILYGO_T_ETH_POE,
             "LilyGO T-ETH-POE (LAN8720)",
             false, ETH_PHY_LAN8720, 0, -1, 23, 18, ETH_CLOCK_GPIO17_OUT,
             -1, -1, -1, -1, -1, -1},

            {NETWORK_HARDWARE_GL_S10_IP101,
             "GL-S10 (IP101)",
             false, ETH_PHY_IP101, 1, 5, 23, 18, ETH_CLOCK_GPIO0_IN,
             -1, -1, -1, -1, -1, -1},
#endif
            // SPI devices — available on all targets
            {NETWORK_HARDWARE_M5STACK_W5500,
             "M5Stack Atom POE (W5500)",
             true, ETH_PHY_W5500, 1, -1, -1, -1, ETH_CLOCK_GPIO0_IN,
             19, -1, -1, 22, 23, 33},

            {NETWORK_HARDWARE_M5STACK_W5500_S3,
             "M5Stack Atom POE S3 (W5500)",
             true, ETH_PHY_W5500, 1, -1, -1, -1, ETH_CLOCK_GPIO0_IN,
             6, -1, -1, 5, 7, 8},

            {NETWORK_HARDWARE_ETH01_EVO_DM9051,
             "ETH01-Evo (DM9051)",
             true, ETH_PHY_DM9051, 1, -1, -1, -1, ETH_CLOCK_GPIO0_IN,
             9, 8, 6, 7, 3, 10},

            {NETWORK_HARDWARE_LILYGO_T_ETH_ELITE,
             "LilyGO T-ETH ELite (W5500)",
             true, ETH_PHY_W5500, 1, -1, -1, -1, ETH_CLOCK_GPIO0_IN,
             45, 14, -1, 48, 47, 21},

            {NETWORK_HARDWARE_WAVESHARE_ESP32_S3_ETH,
             "Waveshare ESP32-S3-ETH / POE-ETH (W5500)",
             true, ETH_PHY_W5500, 1, -1, -1, -1, ETH_CLOCK_GPIO0_IN,
             14, 10, 9, 13, 12, 11},

            {NETWORK_HARDWARE_LILYGO_T_ETH_LITE_S3,
             "LilyGO T-ETH-Lite-ESP32S3 (W5500)",
             true, ETH_PHY_W5500, 1, -1, -1, -1, ETH_CLOCK_GPIO0_IN,
             9, 13, 14, 10, 11, 12},
        };

        for (const auto& p : presets)
        {
            if (p.hardwareId == hardwareId)
                return &p;
        }
        return nullptr;
    }

    // -----------------------------------------------------------------------
    // Custom-Ethernet helper functions (copied verbatim from NukiNetwork)
    // -----------------------------------------------------------------------

    const char* getCustomEthernetDeviceName(int customPhy)
    {
        switch (customPhy)
        {
        case 1:  return "Custom (W5500)";
        case 2:  return "Custom (DM9051)";
        case 3:  return "Custom (KSZ8851SNL)";
        case 4:  return "Custom (LAN8720)";
        case 5:  return "Custom (RTL8201)";
        case 6:  return "Custom (TLK110/IP101)";
        case 7:  return "Custom (DP83848)";
        case 8:  return "Custom (KSZ8041)";
        case 9:  return "Custom (KSZ8081)";
        default: return "Custom Ethernet";
        }
    }

    eth_phy_type_t getCustomEthernetPhyType(int customPhy)
    {
        switch (customPhy)
        {
        case 1:  return ETH_PHY_W5500;
        case 2:  return ETH_PHY_DM9051;
        case 3:  return ETH_PHY_KSZ8851;
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
        case 4:  return ETH_PHY_LAN8720;
        case 5:  return ETH_PHY_RTL8201;
        case 6:  return ETH_PHY_TLK110;
        case 7:  return ETH_PHY_DP83848;
        case 8:  return ETH_PHY_KSZ8041;
        case 9:  return ETH_PHY_KSZ8081;
#endif
        default: return ETH_PHY_W5500;
        }
    }

    eth_clock_mode_t getCustomEthernetClockMode(int clockPreference)
    {
        switch (clockPreference)
        {
        case 0:  return ETH_CLOCK_GPIO0_IN;
        case 1:  return ETH_CLOCK_GPIO0_OUT;
        case 2:  return ETH_CLOCK_GPIO16_OUT;
        case 3:  return ETH_CLOCK_GPIO17_OUT;
        default: return ETH_CLOCK_GPIO17_OUT;
        }
    }

    // -----------------------------------------------------------------------
    // Build EthernetDevice::Config from custom preferences
    // -----------------------------------------------------------------------

    bool buildCustomEthernetConfig(Preferences* preferences,
                                   EthernetDevice::Config& config)
    {
        const int customPhy = preferences->getInt(preference_network_custom_phy, 0);

        // SPI custom devices: customPhy 1–3
        if (customPhy >= 1 && customPhy <= 3)
        {
            config.deviceName  = getCustomEthernetDeviceName(customPhy);
            config.useSpi      = true;
            config.phyType     = getCustomEthernetPhyType(customPhy);
            config.phyAddr     = preferences->getInt(preference_network_custom_addr, -1);
            config.powerPin    = -1;
            config.mdcPin      = -1;
            config.mdioPin     = -1;
            config.clockMode   = ETH_CLOCK_GPIO0_IN;
            config.csPin       = preferences->getInt(preference_network_custom_cs, -1);
            config.irqPin      = preferences->getInt(preference_network_custom_irq, -1);
            config.resetPin    = preferences->getInt(preference_network_custom_rst, -1);
            config.spiSckPin   = preferences->getInt(preference_network_custom_sck, -1);
            config.spiMisoPin  = preferences->getInt(preference_network_custom_miso, -1);
            config.spiMosiPin  = preferences->getInt(preference_network_custom_mosi, -1);
            return true;
        }

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32P4)
        // RMII custom devices: customPhy 4–9
        if (customPhy >= 4 && customPhy <= 9)
        {
            config.deviceName  = getCustomEthernetDeviceName(customPhy);
            config.useSpi      = false;
            config.phyType     = getCustomEthernetPhyType(customPhy);
            config.phyAddr     = preferences->getInt(preference_network_custom_addr, -1);
            config.powerPin    = preferences->getInt(preference_network_custom_pwr, -1);
            config.mdcPin      = preferences->getInt(preference_network_custom_mdc, -1);
            config.mdioPin     = preferences->getInt(preference_network_custom_mdio, -1);
            config.clockMode   = getCustomEthernetClockMode(
                                     preferences->getInt(preference_network_custom_clk, 0));
            config.csPin       = -1;
            config.irqPin      = -1;
            config.resetPin    = -1;
            config.spiSckPin   = -1;
            config.spiMisoPin  = -1;
            config.spiMosiPin  = -1;
            return true;
        }
#endif
        return false; // unsupported customPhy value
    }

    // -----------------------------------------------------------------------
    // Resolve full Ethernet config (preset or custom)
    // -----------------------------------------------------------------------

    bool resolveEthernetConfig(Preferences* preferences,
                               EthernetDevice::Config& config)
    {
        const int hardwareId = preferences->getInt(preference_network_hardware,
                                                   NETWORK_HARDWARE_WIFI);

        if (hardwareId == NETWORK_HARDWARE_CUSTOM)
            return buildCustomEthernetConfig(preferences, config);

        const PresetEntry* p = findPreset(hardwareId);
        if (!p)
            return false;

        // Copy preset into EthernetDevice::Config
        config.deviceName  = p->deviceName;
        config.useSpi      = p->useSpi;
        config.phyType     = p->phyType;
        config.phyAddr     = p->phyAddr;
        config.powerPin    = p->powerPin;
        config.mdcPin      = p->mdcPin;
        config.mdioPin     = p->mdioPin;
        config.clockMode   = p->clockMode;
        config.csPin       = p->csPin;
        config.irqPin      = p->irqPin;
        config.resetPin    = p->resetPin;
        config.spiSckPin   = p->spiSckPin;
        config.spiMisoPin  = p->spiMisoPin;
        config.spiMosiPin  = p->spiMosiPin;
        return true;
    }

} // anonymous namespace

// -----------------------------------------------------------------------
// Factory entry point
// -----------------------------------------------------------------------

NetworkDevice* NetworkDeviceFactory::create(NetworkDeviceType type,
                                            const String& hostname,
                                            Preferences* preferences,
                                            const IPConfiguration* ipConfiguration)
{
    if (type == NetworkDeviceType::WiFi)
    {
#ifndef CONFIG_IDF_TARGET_ESP32H2
        return new WifiDevice(hostname, preferences, ipConfiguration);
#else
        return nullptr; // Wi-Fi not available on ESP32-H2
#endif
    }

    if (type == NetworkDeviceType::ETH)
    {
        EthernetDevice::Config config;
        if (!resolveEthernetConfig(preferences, config))
        {
            Log->println(F("[ERROR] Unsupported ethernet hardware configuration"));
            return nullptr;
        }
        return new EthernetDevice(hostname, preferences, ipConfiguration, config);
    }

    return nullptr;
}
