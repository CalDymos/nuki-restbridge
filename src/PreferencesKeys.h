#pragma once
#include "Config.h"
#include <Preferences.h>

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
#define preference_auth_max_entries (char *)"authmaxentry"
#define preference_authlog_max_entries (char *)"authmaxlogentry"
#define preference_auth_info_enabled (char *)"authInfoEna"
#define preference_access_level (char *)"accLvl"
#define preference_timecontrol_max_entries (char *)"tcmaxentry"
#define preference_timecontrol_info_enabled (char *)"tcInfoEnabled"
#define preference_timecontrol_control_enabled (char*)"tcCntrlEnabled"
#define preference_auth_control_enabled (char*)"authCtrlEna"
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
#define preference_har_key_remote_access_state (char *)"haPathRemAccStat"
#define preference_har_param_remote_access_state (char *)"haQueryRemAccStat"
#define preference_har_key_wifi_rssi (char *)"haPathWFRssi"
#define preference_har_param_wifi_rssi (char *)"haQueryWFRssi"
#define preference_har_key_uptime (char *)"haPathUpTm"
#define preference_har_param_uptime (char *)"haQueryUpTm"
#define preference_har_key_restart_reason_fw (char *)"haPathRestResFW"
#define preference_har_param_restart_reason_fw (char *)"haQueryRestResFW"
#define preference_har_key_restart_reason_esp (char *)"haPathRestResESP"
#define preference_har_param_restart_reason_esp (char *)"haQueryRestResESP"
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

#define preference_har_key_lock_state (char *)"haPathLckStat"
#define preference_har_param_lock_state (char *)"haQueryLckStat"
#define preference_har_key_lockngo_state (char *)"haPathLckNGStat"
#define preference_har_param_lockngo_state (char *)"haQueryLckNGStat"
#define preference_har_key_lock_trigger (char *)"haPathLckTrig"
#define preference_har_param_lock_trigger (char *)"haQueryLckTrig"
#define preference_har_key_lock_night_mode (char *)"haPathLckNMod"
#define preference_har_param_lock_night_mode (char *)"haQueryLckNMod"
#define preference_har_key_lock_completionStatus (char *)"haPathLckCmplStat"
#define preference_har_param_lock_completionStatus (char *)"haQueryLckCmplStat"

#define preference_har_key_doorsensor_state (char *)"haPathDoorStat"
#define preference_har_param_doorsensor_state (char *)"haQueryDoorStat"
#define preference_har_key_doorsensor_critical (char *)"haPathDoorSCrit"
#define preference_har_param_doorsensor_critical (char *)"haQueryDoorSCrit"
#define preference_har_key_keypad_critical (char *)"haPathKeyPCrit"
#define preference_har_param_keypad_critical (char *)"haQueryKeyPCrit"

#define preference_har_key_lock_battery_critical (char *)"haPathLckBatCrit"
#define preference_har_param_lock_battery_critical (char *)"haQueryLckBatCrit"
#define preference_har_key_lock_battery_level (char *)"haPathLckBatLev"
#define preference_har_param_lock_battery_level (char *)"haQueryLckBatLev"
#define preference_har_key_lock_battery_charging (char *)"haPathLckBatChrg"
#define preference_har_param_lock_battery_charging (char *)"haQueryLckBatChrg"

#define preference_har_key_battery_voltage (char *)"haPathBatteryVolt"
#define preference_har_param_battery_voltage (char *)"haQueryBatteryVolt"
#define preference_har_key_battery_drain (char *)"haPathBatteryDrain"
#define preference_har_param_battery_drain (char *)"haQueryBatteryDrain"
#define preference_har_key_battery_max_turn_current (char *)"haPathBattMaxTurnCur"
#define preference_har_param_battery_max_turn_current (char *)"haQueryBattMaxTurnCur"
#define preference_har_key_battery_lock_distance (char *)"haPathBattLockDist"
#define preference_har_param_battery_lock_distance (char *)"haQueryBattLockDist"

// API / Web Configurator
#define preference_api_enabled (char *)"ApiEna"
#define preference_api_port (char *)"ApiPort"
#define preference_api_token (char *)"ApiToken"
#define preference_config_from_api (char *)"nhCntrlEnabled"

#define preference_webcfgserver_enabled (char *)"webCfgSrvEna"
#define preference_cred_user (char *)"webCfgSrvCrdusr"
#define preference_cred_password (char *)"webCfgSrvCrdpass"
#define preference_cred_session_lifetime (char *)"webCfgSrvCredLf"
#define preference_cred_session_lifetime_remember (char *)"webCfgSrvCredLfRmbr"
#define preference_http_auth_type (char *)"httpdAuthType"
#define preference_bypass_proxy (char *)"credBypass"
#define preference_admin_secret (char *)"adminsecret"

// Logging
#define preference_log_max_file_size (char *)"logMaxFileSize"
#define preference_log_max_msg_len (char *)"logMaxMsgLen"
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
  preferences->begin("nukiBridge", false);

  bool firstStart = !preferences->getBool(preference_started_before);

  if (firstStart)
  {
    Serial.println("First start, setting preference defaults");

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
    preferences->putBool(preference_find_best_rssi, true);
    preferences->putBool(preference_connect_mode, true);

    preferences->putInt(preference_buffer_size, CHAR_BUFFER_SIZE);
    preferences->putInt(preference_task_size_network, NETWORK_TASK_SIZE);
    preferences->putInt(preference_task_size_nuki, NUKI_TASK_SIZE);

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