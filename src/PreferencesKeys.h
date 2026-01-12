#pragma once
#include "Config.h"
#include <Preferences.h>
#include <vector>

// System / Start / Debugging
#define preference_enable_bootloop_reset (char *)"enabtlprst"
#define preference_started_before (char *)"run" // not user-changeable
#define preference_show_secrets (char *)"showSecr"
#define preference_restart_on_disconnect (char *)"restdisc"
#define preference_restart_ble_beacon_lost (char *)"rstbcn"

// Tasks / Buffer
#define preference_task_size_network (char *)"tsksznetw"
#define preference_task_size_nuki (char *)"tsksznuki"
#define preference_buffer_size (char *)"buffsize"

// Debug options
#define preference_enable_debug_mode (char *)"enadbg"
#define preference_debug_command (char *)"dbgCommand"
#define preference_debug_communication (char *)"dbgCommu"
#define preference_debug_connect (char *)"dbgConnect"
#define preference_debug_hex_data (char *)"dbgHexData"
#define preference_debug_readable_data (char *)"dbgReadData"
#define preference_send_debug_info (char *)"pubdbg"

// Network
#define preference_ip_dhcp_enabled (char *)"dhcpena"
#define preference_ip_address (char *)"ipaddr"
#define preference_ip_subnet (char *)"ipsub"
#define preference_ip_gateway (char *)"ipgtw"
#define preference_ip_dns_server (char *)"dnssrv"
#define preference_hostname (char *)"hostname"
#define preference_network_timeout (char *)"nettmout"
#define preference_ntw_reconfigure (char *)"ntwRECONF" // not user-changeable
#define preference_wifi_ssid (char *)"wifiSSID"
#define preference_wifi_pass (char *)"wifiPass"
#define preference_find_best_rssi (char *)"nwbestrssi"
#define preference_rssi_send_interval (char *)"rssipb"
#define preference_time_server (char *)"timeServer"
#define preference_timezone (char *)"timeZone"
#define preference_network_hardware (char *)"nwhw"
#define preference_network_custom_phy (char *)"ntwPHY"
#define preference_network_custom_addr (char *)"ntwADDR"
#define preference_network_custom_irq (char *)"ntwIRQ"
#define preference_network_custom_rst (char *)"ntwRST"
#define preference_network_custom_cs (char *)"ntwCS"
#define preference_network_custom_sck (char *)"ntwSCK"
#define preference_network_custom_miso (char *)"ntwMISO"
#define preference_network_custom_mosi (char *)"ntwMOSI"
#define preference_network_custom_pwr (char *)"ntwPWR"
#define preference_network_custom_mdio (char *)"ntwMDIO"
#define preference_network_custom_mdc (char *)"ntwMDC"
#define preference_network_custom_clk (char *)"ntwCLK"
#define preference_Maintenance_send_interval (char *)"maintpb"

// BLE
#define preference_ble_tx_power (char *)"bleTxPwr"
#define preference_ble_general_timeout (char*)"bleGenTmOt"
#define preference_ble_command_timeout (char*)"bleCmdTmOt"

// Nuki Settings (Lock, Keypad, ...)
#define preference_nuki_id_lock (char *)"nukiId"     // Nuki lock ID (not user-changeable)
#define preference_device_id_lock (char *)"deviceId" // Nuki Bridge ID for Lock (not user-changeable)
#define preference_lock_enabled (char *)"lockena"
#define preference_lock_force_id (char *)"lckForceId"
#define preference_lock_force_doorsensor (char *)"lckForceDrsns"
#define preference_lock_force_keypad (char *)"lckForceKp"
#define preference_acl (char *)"aclLckOpn"
#define preference_conf_lock_basic_acl (char *)"confLckBasAcl"
#define preference_conf_lock_advanced_acl (char *)"confLckAdvAcl"
#define preference_lock_max_keypad_code_count (char *)"maxkpad"     // not user-changeable
#define preference_lock_max_auth_entry_count (char *)"maxauth"      // not user-changeable
#define preference_lock_max_timecontrol_entry_count (char *)"maxtc" // not user-changeable
#define preference_lock_pin_status (char *)"lockpin"                // not user-changeable
#define preference_keypad_check_code_enabled (char *)"kpChkEna"
#define preference_keypad_control_enabled (char *)"kpEnabled"
#define preference_keypad_info_enabled (char *)"kpInfoEnabled"
#define preference_keypad_max_entries (char *)"kpmaxentry"
#define preference_keypad_code_encryption (char *)"kpCodeEnc"
#define preference_keypad_code_multiplier (char *)"kpCodeMul"
#define preference_keypad_code_offset (char *)"kpCodeOff"
#define preference_keypad_code_modulus (char *)"kpCodeMod"
#define preference_auth_control_enabled (char *)"authCtrlEna"
#define preference_auth_max_entries (char *)"authmaxentry"
#define preference_authlog_max_entries (char *)"authmaxlogent"
#define preference_auth_info_enabled (char *)"authInfoEna"
#define preference_timecontrol_max_entries (char *)"tcmaxentry"
#define preference_timecontrol_info_enabled (char *)"tcInfoEnabled"
#define preference_timecontrol_control_enabled (char *)"tcCntrlEnabled"
#define preference_query_interval_lockstate (char *)"lockStInterval"
#define preference_query_interval_configuration (char *)"configInterval"
#define preference_query_interval_battery (char *)"batInterval"
#define preference_query_interval_keypad (char *)"kpInterval"
#define preference_update_time (char *)"updateTime"
#define preference_connect_mode (char *)"nukiConnMode"
#define preference_command_nr_of_retries (char *)"nrRetry"
#define preference_command_retry_delay (char *)"rtryDelay"

// Home Automation Reporting (HAR)
#define preference_har_enabled (char *)"haEna"
#define preference_har_mode (char *)"haMode"
#define preference_har_rest_mode (char *)"haRestMode"
#define preference_har_address (char *)"haAddr"
#define preference_har_port (char *)"haPort"
#define preference_har_user (char *)"haUsr"
#define preference_har_password (char *)"haPwd"

//
#define preference_har_key_state (char *)"haPathState"
#define preference_har_key_remote_access_state (char *)"haPathRemAcSta"
#define preference_har_param_remote_access_state (char *)"haQuerRemAcSta"
#define preference_har_key_wifi_rssi (char *)"haPathWFRssi"
#define preference_har_param_wifi_rssi (char *)"haQueryWFRssi"
#define preference_har_key_uptime (char *)"haPathUpTm"
#define preference_har_param_uptime (char *)"haQueryUpTm"
#define preference_har_key_restart_reason_fw (char *)"haPathRestReFW"
#define preference_har_param_restart_reason_fw (char *)"haQuerRestReFW"
#define preference_har_key_restart_reason_esp (char *)"haPatRestReESP"
#define preference_har_param_restart_reason_esp (char *)"haQueRestReESP"
#define preference_har_key_info_nuki_bridge_version (char *)"haPathNBVer"
#define preference_har_param_info_nuki_bridge_version (char *)"haQueryNBVer"
#define preference_har_key_info_nuki_bridge_build (char *)"haPathNBBuil"
#define preference_har_param_info_nuki_bridge_build (char *)"haQueryNBBuil"
#define preference_har_key_freeheap (char *)"haPathFreeHp"
#define preference_har_param_freeheap (char *)"haQueryFreeHp"
#define preference_har_key_ble_address (char *)"haPathBleAddr"
#define preference_har_param_ble_address (char *)"haQueryBleAddr"
#define preference_har_key_ble_strength (char *)"haPathBleStr"
#define preference_har_param_ble_strength (char *)"haQueryBleStr"
#define preference_har_key_ble_rssi (char *)"haPathBleRssi"
#define preference_har_param_ble_rssi (char *)"haQueryBleRssi"

#define preference_har_key_lock_state (char *)"haPathLckSta"
#define preference_har_param_lock_state (char *)"haQueryLckSta"
#define preference_har_key_lockngo_state (char *)"haPatLckNGSta"
#define preference_har_param_lockngo_state (char *)"haQuerLckNGSta"
#define preference_har_key_lock_trigger (char *)"haPathLckTrig"
#define preference_har_param_lock_trigger (char *)"haQueryLckTrig"
#define preference_har_key_lock_night_mode (char *)"haPathLckNMod"
#define preference_har_param_lock_night_mode (char *)"haQueryLckNMod"
#define preference_har_key_lock_completionStatus (char *)"haPatLckCmpSta"
#define preference_har_param_lock_completionStatus (char *)"haQueLckCmpSta"

#define preference_har_key_doorsensor_state (char *)"haPathDoorSta"
#define preference_har_param_doorsensor_state (char *)"haQuerDoorSta"
#define preference_har_key_doorsensor_critical (char *)"haPatDoorSCrit"
#define preference_har_param_doorsensor_critical (char *)"haQueDoorSCrit"
#define preference_har_key_keypad_critical (char *)"haPathKeyPCrit"
#define preference_har_param_keypad_critical (char *)"haQuerKeyPCrit"

#define preference_har_key_lock_battery_critical (char *)"haPatLckBatCri"
#define preference_har_param_lock_battery_critical (char *)"haQueLckBatCri"
#define preference_har_key_lock_battery_level (char *)"haPatLckBatLev"
#define preference_har_param_lock_battery_level (char *)"haQueLckBatLev"
#define preference_har_key_lock_battery_charging (char *)"haPatLckBatChr"
#define preference_har_param_lock_battery_charging (char *)"haQueLckBatChr"

#define preference_har_key_battery_voltage (char *)"haPatBatVolt"
#define preference_har_param_battery_voltage (char *)"haQuerBatVolt"
#define preference_har_key_battery_drain (char *)"haPathBatDrain"
#define preference_har_param_battery_drain (char *)"haQueryBatDrai"
#define preference_har_key_battery_max_turn_current (char *)"haPatBatMaxTCu"
#define preference_har_param_battery_max_turn_current (char *)"haQueBatMaxTCu"
#define preference_har_key_battery_lock_distance (char *)"haPatBatLckDst"
#define preference_har_param_battery_lock_distance (char *)"haQueBatLckDst"

// API / Web Configurator
#define preference_api_enabled (char *)"ApiEna"
#define preference_api_port (char *)"ApiPort"
#define preference_api_token (char *)"ApiToken"
#define preference_config_from_api (char *)"nhCntrlEnabled"

#define preference_webcfgserver_enabled (char *)"webCfgSrvEna"
#define preference_cred_user (char *)"webCfgSrvCrdus"
#define preference_cred_password (char *)"webCfgSrvCrpas"
#define preference_cred_session_lifetime (char *)"webCfgSrvCrLf"
#define preference_cred_session_lifetime_remember (char *)"webCfgSrvCrLfR"
#define preference_http_auth_type (char *)"httpdAuthType"
#define preference_bypass_proxy (char *)"credBypass"
#define preference_admin_secret (char *)"adminsecret"

// Logging
#define preference_log_max_file_size (char *)"logMaxFileSize"
#define preference_log_level (char *)"loglvl"
#define preference_log_backup_enabled (char *)"logBckEna"
#define preference_log_backup_ftp_server (char *)"logBckSrv"
#define preference_log_backup_ftp_dir (char *)"logBckdir"
#define preference_log_backup_ftp_user (char *)"logBckUsr"
#define preference_log_backup_ftp_pwd (char *)"logBckPwd"
#define preference_log_backup_file_index (char *)"logBckFileId" // not user-changeable

inline bool initPreferences(Preferences *&preferences)
{
  preferences = new Preferences();
  preferences->begin(PREFERENCE_NAME, false);

  bool firstStart = !preferences->getBool(preference_started_before);

  if (firstStart)
  {
    Serial.println(F("First start, setting preference defaults"));

    preferences->putBool(preference_started_before, true);
    preferences->putBool(preference_lock_enabled, true);
    uint32_t aclPrefs[17] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    preferences->putBytes(preference_acl, (byte *)(&aclPrefs), sizeof(aclPrefs));
    uint32_t basicLockConfigAclPrefs[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    preferences->putBytes(preference_conf_lock_basic_acl, (byte *)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
    uint32_t advancedLockConfigAclPrefs[25] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    preferences->putBytes(preference_conf_lock_advanced_acl, (byte *)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));

    preferences->putString(preference_time_server, "pool.ntp.org");

    preferences->putString(preference_bypass_proxy, "");
    preferences->putInt(preference_api_port, 8080);
    preferences->putBool(preference_ip_dhcp_enabled, true);
    preferences->putBool(preference_enable_bootloop_reset, false);
    preferences->putBool(preference_show_secrets, false);
    preferences->putBool(preference_find_best_rssi, true);
    preferences->putBool(preference_keypad_info_enabled, false);
    preferences->putBool(preference_timecontrol_info_enabled, false);  
    preferences->putBool(preference_connect_mode, true);

    preferences->putInt(preference_buffer_size, CHAR_BUFFER_SIZE);
    preferences->putInt(preference_task_size_network, NETWORK_TASK_SIZE);
    preferences->putInt(preference_task_size_nuki, NUKI_TASK_SIZE);
    preferences->putInt(preference_ble_general_timeout, 10000);
    preferences->putInt(preference_ble_command_timeout, 3000);

    preferences->putInt(preference_authlog_max_entries, MAX_AUTHLOG);
    preferences->putInt(preference_keypad_max_entries, MAX_KEYPAD);
    preferences->putInt(preference_timecontrol_max_entries, MAX_TIMECONTROL);

    preferences->putInt(preference_rssi_send_interval, 60);
    preferences->putInt(preference_network_timeout, 60);

    preferences->putInt(preference_command_nr_of_retries, 3);
    preferences->putInt(preference_command_retry_delay, 100);
    preferences->putInt(preference_restart_ble_beacon_lost, 60);

    preferences->putInt(preference_query_interval_lockstate, 1800);
    preferences->putInt(preference_query_interval_configuration, 3600);
    preferences->putInt(preference_query_interval_battery, 1800);
    preferences->putInt(preference_query_interval_keypad, 1800);

    preferences->putInt(preference_http_auth_type, 0);
    preferences->putInt(preference_cred_session_lifetime, 3600);
    preferences->putInt(preference_cred_session_lifetime_remember, 720);
  }

  return firstStart;
}

//Todo: check all types PreferencesKeyRegistry

// Registry for all known preferences keys used by this firmware.
//
// Notes:
// - _keys contains *all* preference_* keys defined in this header (useful for backup/restore or diagnostics).
// - Keys listed in _redact should be treated as sensitive and never be shown in plain text.
// - The typed lists (_boolPrefs/_intPrefs/_bytePrefs/_uintPrefs/_uint64Prefs) are for code paths that need
//   to load/save preferences with the correct Preferences::* API.
//   The default for _keys is the String type.
class PreferencesKeyRegistry
{
private:
    const std::vector<char*> _keys =
    {
        preference_enable_bootloop_reset, preference_started_before, preference_show_secrets,
        preference_restart_on_disconnect, preference_restart_ble_beacon_lost, preference_task_size_network,
        preference_task_size_nuki, preference_buffer_size, preference_enable_debug_mode, preference_debug_command,
        preference_debug_communication, preference_debug_connect, preference_debug_hex_data,
        preference_debug_readable_data, preference_send_debug_info, preference_ip_dhcp_enabled,
        preference_ip_address, preference_ip_subnet, preference_ip_gateway, preference_ip_dns_server,
        preference_hostname, preference_network_timeout, preference_ntw_reconfigure, preference_wifi_ssid,
        preference_wifi_pass, preference_find_best_rssi, preference_rssi_send_interval, preference_time_server,
        preference_timezone, preference_network_hardware, preference_network_custom_phy,
        preference_network_custom_addr, preference_network_custom_irq, preference_network_custom_rst,
        preference_network_custom_cs, preference_network_custom_sck, preference_network_custom_miso,
        preference_network_custom_mosi, preference_network_custom_pwr, preference_network_custom_mdio,
        preference_network_custom_mdc, preference_network_custom_clk, preference_Maintenance_send_interval,
        preference_ble_tx_power, preference_nuki_id_lock, preference_device_id_lock, preference_lock_enabled,
        preference_lock_force_id, preference_lock_force_doorsensor, preference_lock_force_keypad, preference_acl,
        preference_conf_lock_basic_acl, preference_conf_lock_advanced_acl, preference_lock_max_keypad_code_count,
        preference_lock_max_auth_entry_count, preference_lock_max_timecontrol_entry_count,
        preference_lock_pin_status, preference_keypad_check_code_enabled, preference_keypad_control_enabled,
        preference_keypad_info_enabled, preference_keypad_max_entries, preference_keypad_code_encryption,
        preference_keypad_code_multiplier, preference_keypad_code_offset, preference_keypad_code_modulus,
        preference_auth_control_enabled, preference_auth_max_entries, preference_authlog_max_entries,
        preference_auth_info_enabled, preference_timecontrol_max_entries,
        preference_timecontrol_info_enabled, preference_timecontrol_control_enabled,
        preference_query_interval_lockstate, preference_query_interval_configuration,
        preference_query_interval_battery, preference_query_interval_keypad, preference_update_time,
        preference_connect_mode, preference_command_nr_of_retries, preference_command_retry_delay,
        preference_har_enabled, preference_har_mode, preference_har_rest_mode, preference_har_address,
        preference_har_port, preference_har_user, preference_har_password, preference_har_key_state,
        preference_har_key_remote_access_state, preference_har_param_remote_access_state,
        preference_har_key_wifi_rssi, preference_har_param_wifi_rssi, preference_har_key_uptime,
        preference_har_param_uptime, preference_har_key_restart_reason_fw, preference_har_param_restart_reason_fw,
        preference_har_key_restart_reason_esp, preference_har_param_restart_reason_esp,
        preference_har_key_info_nuki_bridge_version, preference_har_param_info_nuki_bridge_version,
        preference_har_key_info_nuki_bridge_build, preference_har_param_info_nuki_bridge_build,
        preference_har_key_freeheap, preference_har_param_freeheap, preference_har_key_ble_address,
        preference_har_param_ble_address, preference_har_key_ble_strength, preference_har_param_ble_strength,
        preference_har_key_ble_rssi, preference_har_param_ble_rssi, preference_har_key_lock_state,
        preference_har_param_lock_state, preference_har_key_lockngo_state, preference_har_param_lockngo_state,
        preference_har_key_lock_trigger, preference_har_param_lock_trigger, preference_har_key_lock_night_mode,
        preference_har_param_lock_night_mode, preference_har_key_lock_completionStatus,
        preference_har_param_lock_completionStatus, preference_har_key_doorsensor_state,
        preference_har_param_doorsensor_state, preference_har_key_doorsensor_critical,
        preference_har_param_doorsensor_critical, preference_har_key_keypad_critical,
        preference_har_param_keypad_critical, preference_har_key_lock_battery_critical,
        preference_har_param_lock_battery_critical, preference_har_key_lock_battery_level,
        preference_har_param_lock_battery_level, preference_har_key_lock_battery_charging,
        preference_har_param_lock_battery_charging, preference_har_key_battery_voltage,
        preference_har_param_battery_voltage, preference_har_key_battery_drain,
        preference_har_param_battery_drain, preference_har_key_battery_max_turn_current,
        preference_har_param_battery_max_turn_current, preference_har_key_battery_lock_distance,
        preference_har_param_battery_lock_distance, preference_api_enabled, preference_api_port,
        preference_api_token, preference_config_from_api, preference_webcfgserver_enabled, preference_cred_user,
        preference_cred_password, preference_cred_session_lifetime, preference_cred_session_lifetime_remember,
        preference_http_auth_type, preference_bypass_proxy, preference_admin_secret, preference_log_max_file_size,
        preference_log_level, preference_log_backup_enabled, preference_log_backup_ftp_server,
        preference_log_backup_ftp_dir, preference_log_backup_ftp_user, preference_log_backup_ftp_pwd,
        preference_log_backup_file_index, preference_ble_general_timeout, preference_ble_command_timeout
    };

    const std::vector<char*> _redact =
    {
        preference_wifi_pass, preference_nuki_id_lock, preference_har_user, preference_har_password,
        preference_api_token, preference_cred_user, preference_cred_password, preference_bypass_proxy,
        preference_admin_secret, preference_log_backup_ftp_user, preference_log_backup_ftp_pwd
    };

    const std::vector<char*> _boolPrefs =
    {
        preference_enable_bootloop_reset, preference_started_before, preference_show_secrets,
        preference_restart_on_disconnect, preference_enable_debug_mode, preference_debug_command,
        preference_debug_communication, preference_debug_connect, preference_debug_hex_data,
        preference_debug_readable_data, preference_send_debug_info, preference_ip_dhcp_enabled,
        preference_ntw_reconfigure, preference_find_best_rssi, preference_lock_enabled, preference_lock_force_id,
        preference_lock_force_doorsensor, preference_lock_force_keypad, preference_keypad_check_code_enabled,
        preference_keypad_control_enabled, preference_keypad_info_enabled, preference_keypad_code_encryption,
        preference_auth_control_enabled, preference_auth_info_enabled, preference_timecontrol_info_enabled,
        preference_timecontrol_control_enabled, preference_update_time, preference_connect_mode,
        preference_har_enabled, preference_api_enabled, preference_config_from_api,
        preference_webcfgserver_enabled, preference_log_backup_enabled
    };

    const std::vector<char*> _bytePrefs =
    {
        preference_acl, preference_conf_lock_basic_acl, preference_conf_lock_advanced_acl
    };

    const std::vector<char*> _intPrefs =
    {
        preference_restart_ble_beacon_lost, preference_task_size_network, preference_task_size_nuki,
        preference_buffer_size, preference_network_timeout, preference_rssi_send_interval,
        preference_network_hardware, preference_network_custom_phy, preference_network_custom_addr,
        preference_network_custom_irq, preference_network_custom_rst, preference_network_custom_cs,
        preference_network_custom_sck, preference_network_custom_miso, preference_network_custom_mosi,
        preference_network_custom_pwr, preference_network_custom_mdio, preference_network_custom_mdc,
        preference_network_custom_clk, preference_ble_tx_power, preference_lock_max_keypad_code_count,
        preference_lock_max_auth_entry_count, preference_lock_max_timecontrol_entry_count,
        preference_lock_pin_status, preference_keypad_max_entries, preference_auth_max_entries,
        preference_authlog_max_entries, preference_timecontrol_max_entries, preference_query_interval_lockstate,
        preference_query_interval_configuration, preference_query_interval_battery,
        preference_query_interval_keypad, preference_command_nr_of_retries, preference_command_retry_delay,
        preference_har_mode, preference_har_port, preference_api_port, preference_cred_session_lifetime,
        preference_cred_session_lifetime_remember, preference_http_auth_type, preference_log_max_file_size,
        preference_log_level, preference_log_backup_file_index, preference_Maintenance_send_interval,
        preference_ble_general_timeout, preference_ble_command_timeout
    };

    const std::vector<char*> _uintPrefs =
    {
        preference_nuki_id_lock, preference_device_id_lock, preference_keypad_code_multiplier,
        preference_keypad_code_offset, preference_keypad_code_modulus
    };

    const std::vector<char*> _uint64Prefs =
    {
        // (none)
    };

public:
    const std::vector<char*> getPreferencesKeys() const { return _keys; }
    const std::vector<char*> getPreferencesRedactedKeys() const { return _redact; }

    const std::vector<char*> getPreferencesBoolKeys() const { return _boolPrefs; }
    const std::vector<char*> getPreferencesByteKeys() const { return _bytePrefs; }
    const std::vector<char*> getPreferencesIntKeys() const { return _intPrefs; }
    const std::vector<char*> getPreferencesUIntKeys() const { return _uintPrefs; }
    const std::vector<char*> getPreferencesUInt64Keys() const { return _uint64Prefs; }

};