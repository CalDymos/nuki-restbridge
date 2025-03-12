#pragma once

// main path for bridge
#define api_path_bridge (char*)"/bridge"

#define api_path_bridge_enable_api (char*)"/enableApi"
#define api_path_bridge_reboot (char*)"/reboot"
#define api_path_bridge_enable_web_server (char*)"/enableWebServer"

// main path for lock
#define api_path_lock (char*)"/lock"

#define api_path_lock_action (char*)"/action"

#define api_path_query_config (char*)"/query/config"
#define api_path_query_lockstate (char*)"/query/lockstate"
#define api_path_query_keypad (char*)"/query/keypad"
#define api_path_query_battery (char*)"/query/battery"

#define api_path_config_action (char*)"/config/action"

#define api_path_keypad_command_action (char*)"/keypad/command/action"
#define api_path_keypad_command_id (char*)"/keypad/command/id"
#define api_path_keypad_command_name (char*)"/keypad/command/name"
#define api_path_keypad_command_code (char*)"/keypad/command/code"
#define api_path_keypad_command_enabled (char*)"/keypad/command/enabled"

#define api_path_timecontrol_action (char*)"/timecontrol/action"

#define api_path_auth_action (char*)"/authorization/action"
