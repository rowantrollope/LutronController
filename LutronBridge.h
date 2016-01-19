/*
 PArticle Library for connecting to Lutron RadioRA2 server

 Created December 2015
 by Rowan Trollope - hosted on http://github.com/rowantrollope/LutronController

 */

using namespace std;

#include <map>

#include "application.h"

#ifndef LutronBridge_h
#define LutronBridge_h

struct LUTRON_DEVICE
{
    int id;
    float currentLevel;
    float onLevel;
};

typedef void (*notifyFunc)(int id);
typedef std::map<int, LUTRON_DEVICE> DEVICE_MAP;

class LutronBridge {

public:

    LutronBridge();

    bool connect(byte lutronIP[]);
    void disconnect();
    
    os_thread_return_t telnetListener(void* param);

    int
        initDimmerLevels(int nMax),
        setDimmer(String sDimmer),
        _setDimmer(int nDimmer, float fLevel),
        getDimmer(String sDimmer),
        _getDimmer(int nDimmer),
        sendCommand(String sCommand),
        setAllDimmers(String sCommand);

    String getAllDimmers();

    void setNotifyCallback(notifyFunc func) { changeCB = func; };

    void addDevice(int nDeviceID, LUTRON_DEVICE device);
    LUTRON_DEVICE getDevice(int nDeviceID);
    void updateDevice(int nDeviceID, LUTRON_DEVICE device);
    bool deviceExists(int nDeviceID);

private:

    Thread *telnetListenerThread;

    DEVICE_MAP deviceMap;
    notifyFunc changeCB;
    TCPClient client;
    bool    bPublishAll;
};

//----------------

#define LUTRON_READ "?OUTPUT,%i,1"
#define LUTRON_WRITE "#OUTPUT,%i,1,%.2f"
#define LUTRON_RETURN "~OUTPUT,"
#define TELNET_PORT 23




#endif // LutronBridge_h
