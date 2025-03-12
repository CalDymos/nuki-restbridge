#pragma once

enum class NetworkServiceStates
{
    UNDEFINED = 1,
    OK = 0,
    WEBSERVER_NOT_REACHABLE = -1,
    HTTPCLIENT_NOT_REACHABLE = -2,
    BOTH_NOT_REACHABLE = -3
};