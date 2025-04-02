#pragma once

enum class NetworkServiceState
{
    OK = 0,                     // Everything is working
    ERROR_REST_API_SERVER = -1, // Bridge REST API not reachable
    ERROR_HAR_CLIENT = -2,      // HAR (Home Automation Reporting) client not reachable
    ERROR_BOTH = -3,            // Both services not reachable
    UNKNOWN = 1                 // Undefined or initial state
};
