#pragma once
#include <Preferences.h>

#define preference_enable_bootloop_reset (char*)"enabtlprst"

#define preference_ip_dhcp_enabled (char *)"dhcpena"
#define preference_ip_address (char *)"ipaddr"
#define preference_ip_subnet (char *)"ipsub"
#define preference_ip_gateway (char *)"ipgtw"
#define preference_ip_dns_server (char *)"dnssrv"

#define preference_network_hardware (char*)"nwhw"

#define preference_wifi_ssid (char *)"wifiSSID"
#define preference_wifi_pass (char *)"wifiPass"
#define preference_find_best_rssi (char *)"nwbestrssi"
#define preference_auth_max_entries (char*)"authmaxentry"

#define preference_disable_network_not_connected (char *)"disNtwNoCon"

#define preference_debug_connect (char*)"dbgConnect"
#define preference_debug_communication (char*)"dbgCommu"
#define preference_debug_readable_data (char*)"dbgReadData"
#define preference_debug_hex_data (char*)"dbgHexData"
#define preference_debug_command (char*)"dbgCommand"

#define preference_connect_mode (char*)"nukiConnMode"

#define preference_task_size_network (char*)"tsksznetw"
#define preference_task_size_nuki (char*)"tsksznuki"
#define preference_buffer_size (char*)"buffsize"

#define preference_webcfgserver_enabled (char *)"webCfgSrvEna"  // enable/disable WebCfgserver 
#define preference_cred_user (char*)"webCfgSrvCrdusr"
#define preference_cred_password (char*)"webCfgSrvCrdpass"
#define preference_cred_session_lifetime (char*)"webCfgSrvCredLf"
#define preference_cred_session_lifetime_remember (char*)"webCfgSrvCredLfRmbr"

#define preference_bypass_proxy (char*)"credBypass"
#define preference_http_auth_type (char*)"httpdAuthType"

#define preference_bypass_secret (char*)"bypassecret"
#define preference_admin_secret (char*)"adminsecret"

#define preference_lock_enabled (char*)"lockena"

#define preference_ha_enabled (char *)"haEna"
#define preference_ha_mode (char *)"haMode"  // "UDP" oder "hTTP POST"
#define preference_ha_address (char *)"haAddr"
#define preference_ha_port (char *)"haPort"
#define preference_ha_user (char *)"haUsr"
#define preference_ha_password (char *)"haPwd"

#define preference_ha_path_state (char *)"haPathState"

#define preference_ha_path_wifi_rssi (char *)"haPathWFRssi"
#define preference_ha_query_wifi_rssi (char *)"haQueryWFRssi"

// publish path and querys for home automation system
#define preference_ha_path_uptime (char *)"haPathUpTm"
#define preference_ha_query_uptime (char *)"haQueryUpTm"
#define preference_ha_path_restart_reason_fw (char *)"haPathRestResFW"
#define preference_ha_query_restart_reason_fw (char *)"haQueryRestResFW"
#define preference_ha_path_restart_reason_esp (char *)"haPathRestResESP"
#define preference_ha_query_restart_reason_esp (char *)"haQueryRestResESP"
#define preference_ha_path_info_nuki_bridge_version (char *)"haPathNBVer"
#define preference_ha_query_info_nuki_bridge_version (char *)"haQueryNBVer"
#define preference_ha_path_info_nuki_bridge_build (char *)"haPathNBBuil"
#define preference_ha_query_info_nuki_bridge_build (char *)"haQueryNBBuil"
#define preference_ha_path_freeheap (char *)"haPathFreeHp"
#define preference_ha_query_freeheap (char *)"haQueryFreeHp"
#define preference_ha_path_ble_address (char *)"haPathBleAddr"
#define preference_ha_query_ble_address (char *)"haQueryBleAddr"
#define preference_ha_path_lock_state (char *)"haPathLckStat"
#define preference_ha_query_lock_state (char *)"haQueryLckStat"
#define preference_ha_path_lockngo_state (char *)"haPathLckNGStat"
#define preference_ha_query_lockngo_state (char *)"haQueryLckNGStat"
#define preference_ha_path_lock_trigger (char *)"haPathLckTrig"
#define preference_ha_query_lock_trigger (char *)"haQueryLckTrig"
#define preference_ha_path_lock_night_mode (char *)"haPathLckNMod"
#define preference_ha_query_lock_night_mode (char *)"haQueryLckNMod"
#define preference_ha_path_lock_completionStatus (char *)"haPathLckCmplStat"
#define preference_ha_query_lock_completionStatus (char *)"haQueryLckCmplStat"
#define preference_ha_path_doorsensor_state (char *)"haPathDoorStat"
#define preference_ha_query_doorsensor_state (char *)"haQueryDoorStat"
#define preference_ha_path_lock_battery_critical (char *)"haPathLckBatCrit"
#define preference_ha_query_lock_battery_critical (char *)"haQueryLckBatCrit"
#define preference_ha_path_lock_battery_level (char *)"haPathLckBatLev"
#define preference_ha_query_lock_battery_level (char *)"haQueryLckBatLev"
#define preference_ha_path_lock_battery_charging (char *)"haPathLckBatChrg"
#define preference_ha_query_lock_battery_charging (char *)"haQueryLckBatChrg"
#define preference_ha_path_keypad_critical (char *)"haPathKeyPCrit"
#define preference_ha_query_keypad_critical (char *)"haQueryKeyPCrit"
#define preference_ha_path_doorsensor_critical (char *)"haPathDoorSCrit"
#define preference_ha_query_doorsensor_critical (char *)"haQueryDoorSCrit"
#define preference_ha_path_remote_access_state (char *)"haPathRemAccStat"
#define preference_ha_query_remote_access_state (char *)"haQueryRemAccStat"
#define preference_ha_path_ble_strength (char *)"haPathBleStr"
#define preference_ha_query_ble_strength (char *)"haQueryBleStr"

#define preference_publish_config (char*)"nhPubConfig"
#define preference_config_from_api (char*)"nhCntrlEnabled"

#define preference_hostname (char *)"hostname"

#define preference_started_before (char *)"run"

#define preference_api_port (char *)"ApiPort"
#define preference_api_enabled (char *)"ApiEna"
#define preference_api_Token (char *)"ApiToken"

#define preference_config_Auth (char *)"ConfAuth"
#define preference_device_id_lock (char *)"deviceId"
#define preference_restart_on_disconnect (char *)"restdisc"
#define preference_publish_debug_info (char*)"pubdbg"
#define preference_callback_key_prefix (char *)"cbUrl"

#define preference_rssi_publish_interval (char *)"rssipb"          // seconds
#define preference_network_timeout (char*)"nettmout"
#define preference_Maintenance_publish_interval (char *)"maintpb"  // seconds
#define preference_restart_ble_beacon_lost (char *)"rstbcn"

#define preference_authlog_max_entries (char*)"authmaxentry"
#define preference_keypad_max_entries (char*)"kpmaxentry"
#define preference_timecontrol_max_entries (char*)"tcmaxentry"

#define preference_keypad_check_code_enabled (char*)"kpChkEna"
#define preference_lock_force_id (char*)"lckForceId"
#define preference_lock_force_doorsensor (char*)"lckForceDrsns"
#define preference_lock_force_keypad (char*)"lckForceKp"

#define preference_query_interval_lockstate (char *)"lockStInterval"
#define preference_query_interval_configuration (char *)"configInterval"
#define preference_query_interval_battery (char *)"batInterval"
#define preference_query_interval_keypad (char *)"kpInterval"

#define preference_keypad_control_enabled (char *)"kpEnabled"
#define preference_command_nr_of_retries (char *)"nrRetry"
#define preference_command_retry_delay (char *)"rtryDelay"
#define preference_lock_max_keypad_code_count (char *)"maxkpad"
#define preference_access_level (char *)"accLvl"
#define preference_time_server (char*)"timeServer"

#define preference_auth_info_enabled (char*)"authInfoEna"

#define preference_ntw_reconfigure (char *)"ntwRECONF"

#define preference_log_max_file_size (char *)"logMaxFileSize"
#define preference_log_max_msg_len (char *)"logMaxMsgLen"
#define preference_log_filename (char *)"logFile"
#define preference_log_backup_enabled (char *)"logBckEna"
#define preference_log_backup_ftp_server (char *)"logBckSrv"
#define preference_log_backup_ftp_dir (char *)"logBckdir"
#define preference_log_backup_ftp_user (char *)"logBckUsr"
#define preference_log_backup_ftp_pwd (char *)"logBckPwd"
#define preference_log_backup_file_index (char *)"logBckFileId"

#define preference_acl (char *)"aclLckOpn"
#define preference_conf_lock_basic_acl (char *)"confLckBasAcl"
#define preference_conf_lock_advanced_acl (char *)"confLckAdvAcl"

#define preference_ble_tx_power (char*)"bleTxPwr"
#define preference_show_secrets (char*)"showSecr"

#define preference_keypad_info_enabled (char*)"kpInfoEnabled"
#define preference_publish_authdata (char*)"pubAuth"
#define preference_conf_info_enabled (char*)"cnfInfoEnabled"
#define preference_timecontrol_info_enabled (char*)"tcInfoEnabled"

#define preference_update_time (char*)"updateTime"
#define preference_enable_debug_mode (char*)"enadbg"

//NOT USER CHANGEABLE
#define preference_lock_max_auth_entry_count (char*)"maxauth"
#define preference_lock_max_timecontrol_entry_count (char*)"maxtc"
#define preference_nuki_id_lock (char*)"nukiId"
#define preference_lock_pin_status (char*)"lockpin"

inline bool initPreferences(Preferences *&preferences) {
  preferences = new Preferences();
  preferences->begin("nukiBridge", false);

  bool firstStart = !preferences->getBool(preference_started_before);

  if (firstStart) {
    // first start, set defaults
    preferences->putBool(preference_started_before, true);
    preferences->putBool(preference_lock_enabled, true);
    uint32_t aclPrefs[17] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    preferences->putBytes(preference_acl, (byte *)(&aclPrefs), sizeof(aclPrefs));
    uint32_t basicLockConfigAclPrefs[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    preferences->putBytes(preference_conf_lock_basic_acl, (byte *)(&basicLockConfigAclPrefs), sizeof(basicLockConfigAclPrefs));
    uint32_t advancedLockConfigAclPrefs[25] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    preferences->putBytes(preference_conf_lock_advanced_acl, (byte *)(&advancedLockConfigAclPrefs), sizeof(advancedLockConfigAclPrefs));

    preferences->putString(preference_time_server, "pool.ntp.org");

    preferences->putInt(preference_api_port, 8080);
  }

  return firstStart;
}