#include "Logger.h"

#ifdef DEBUG
Print* Log = nullptr;
#else
EmptyPrint* Log = nullptr;
#endif