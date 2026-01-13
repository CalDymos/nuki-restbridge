#include "NukiRetryHandler.h"
#include "Logger.h"

NukiRetryHandler::NukiRetryHandler(std::string reference, int nrOfRetries, int retryDelay)
: _reference(reference),
  _nrOfRetries(nrOfRetries),
  _retryDelay(retryDelay)
{
}

const Nuki::CmdResult NukiRetryHandler::retryComm(std::function<Nuki::CmdResult()> func)
{
    Nuki::CmdResult cmdResult = Nuki::CmdResult::Error;

    int retryCount = 0;

    while(retryCount < _nrOfRetries + 1 && cmdResult != Nuki::CmdResult::Success)
    {
        TaskWdtReset();

        cmdResult = func();

        if (cmdResult != Nuki::CmdResult::Success)
        {
            ++retryCount;

            Log->print(F("[WARNING] "));
            Log->print(_reference.c_str());
            Log->print(": Last command failed, retrying after ");
            Log->print(_retryDelay);
            Log->print(" milliseconds. Retry ");
            Log->print(retryCount);
            Log->print(" of ");
            Log->println(_nrOfRetries);

            TaskWdtDelay(_retryDelay);
        }
    }

    return cmdResult;
}



