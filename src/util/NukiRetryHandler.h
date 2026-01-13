#pragma once
#include <functional>
#include "NukiDataTypes.h"
#include "TaskUtils.h"

class NukiRetryHandler
{
public:
    NukiRetryHandler(std::string reference, int nrOfRetries, int retryDelay);

    const Nuki::CmdResult retryComm(std::function<Nuki::CmdResult ()> func);


private:

    std::string _reference;
    int _nrOfRetries = 0;
    int _retryDelay = 0;
};
