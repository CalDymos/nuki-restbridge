class RestDataReceiver
{
public:
    virtual void onRestDataReceived(const char *path, WebServer& server) = 0;
};