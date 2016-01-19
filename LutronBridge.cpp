/*

    LutronBridge

    Creates a simple pass through bridge into the Lutron RadioRA2 gateway

    This app will connect through TELNET to your Lutron bridge (set the IP
    address under lutronIP[])

    The app stays connected and passes any string sent through "sendCommand"
    the luton Telnet session.

    For examnple, #OUTPUT,5,1,100.00 will set deviceID 5 to ON

    Created this as a hack to allow for IFTTT connectivity.  Use the Particle
    connector on IFTTT / DO Button / etc. and you can control any LUTRON device

    IMPORTANT NOTE: Requires Particle PHOTON firmware 4.7 which has fixes in TCPClient

    Meant as a temporary workaround until we get the Lutron Smart Brige
    (or whatever they call it)...

    /rowantrollope
*/
// this thread watches for the specific events we care about -
// dimmers changing levels, and stores the new level

#include "LutronBridge.h"

LutronBridge::LutronBridge()
{
    changeCB = NULL;
    bPublishAll = true;
    telnetListenerThread = NULL;
}

os_thread_return_t listener(void* param)
{
    return ((LutronBridge *)param)->telnetListener(param);
}

bool LutronBridge::connect(byte lutronIP[])
{
    Serial.println("lutronConnect - Connecting...");
    client.connect(lutronIP, TELNET_PORT);

    if (client.connected())
    {
        Serial.println("lutronConnect - Connected");

        client.println("lutron");
        client.println("integration");

        // wait a couple seconds for the telnet server to catch up
        delay(1000);

        // Setup listener thread to respond to light change events
        telnetListenerThread = new Thread("telnetListener", listener, this);

        delay(1000);

        // Get the current states of the lights we care about
        // the enums all device ID's from 0 to 90.
        initDimmerLevels(90);

        return true;
    }
    else
    {
        Serial.println("lutronConnect - Connection failed");
        client.stop();
        delay(1000);
        return false;
    }
}
void LutronBridge::disconnect()
{
    client.stop();

    if(telnetListenerThread)
    {
        delete telnetListenerThread;
        telnetListenerThread = NULL;
    }
    return;
}
os_thread_return_t LutronBridge::telnetListener(void* param)
{
    // Extract the current level and save it - format ~OUTPUT,DEVICEID,LEVEL (FLOAT)
    String sVal = LUTRON_RETURN;

    while(true)
    {
        String sResult;
        bool bInput=false;
        Serial.println("LutronBridge::telnetListened - Waiting for server...");
        while (client.available())
        {
            char c = client.read();
            sResult += c;
            bInput = true;
        }

        if(bInput) // Work to do?
        {
            Serial.println("LutronBridge::telnetListened - RECEIVED: " + sResult);

            // HANDLE INPUT
            int nStart=0;
            do
            {
                // LOOK For ~OUTPUT,NN,NN,NN
                nStart = sResult.indexOf(sVal, nStart);

                if(nStart == -1)
                {
                    Serial.println("LutronBridge::telnetListener() - IGNORING - " + sResult);
                    break;
                }
                //Serial.println("LutronBridge::telnetListener() - FOUND OUTPUT - " + sResult);

                // FOUND ~OUTPUT
                // first TOKEN is device ID
                nStart += sVal.length();

                int nEnd = sResult.indexOf(',', nStart);
                String sDevice = sResult.substring(nStart, nEnd);
                int nDevice = sDevice.toInt();

                // next TOKEN is the command, 1, advance past it
                nStart = nEnd + 1;
                nEnd = sResult.indexOf(',', nStart);

                String sCommand = sResult.substring(nStart, nEnd);
                int nCommand = sCommand.toInt();

                // next TOKEN is the level
                nStart = nEnd + 1;
                nEnd = sResult.indexOf('\r\n', nStart);

                String sLevel = sResult.substring(nStart, nEnd);
                float fLevel = sLevel.toFloat();

                // only command we care about for lights is "OUTPUT CHANGED"
                if(nCommand == 1)
                {
                    String sDebug = String::format("LutronBridge::telnetListener() - DEVICE=%i, CMD=%i, LEVEL=%.2f", nDevice, nCommand, fLevel);
                    Serial.println(sDebug);

                    // Publish light changed events
                    if(bPublishAll)
                    {
                        String sEventData = String::format("device=%i&level=%.0f", nDevice, nCommand, fLevel);
                        Particle.publish("lutron/device/changed", sEventData);
                    }
                    // we now STORE all light events in our map...
                    LUTRON_DEVICE device = { nDevice, fLevel, 0 };

                    deviceMap[nDevice] = device;

                    if(changeCB)
                        changeCB(nDevice);

                    nStart = sResult.indexOf(sVal, nStart);
                }

            } while (nStart != -1);
        }
        // throttle read as we seem to be overwhelming something here
        delay(250);
    }
}

// this function requests the level for every deviceID between 0 and nMax,
// the telnetListener will catch the response and initialize the object
// in our object map
// This is less than ideal, and really we should only call getDimmer for the devices
// we know exist, but there doesn't seem to be a way to request the list of
// valid devices...
int LutronBridge::initDimmerLevels(int nMax)
{
    /*
    for(DEVICE_MAP::iterator it = deviceMap.begin();
        it != deviceMap.end();
        it++)
    {
        _getDimmer(it->first);
        delay(250);
    }
    */

    for (int i=0;i<nMax;i++)
    {
        _getDimmer(i);
        delay(50);
    }
    return 1;
}
void LutronBridge::addDevice(int nDeviceID, LUTRON_DEVICE device)
{
    deviceMap[nDeviceID] = device;
}
void LutronBridge::updateDevice(int nDeviceID, LUTRON_DEVICE device)
{
    deviceMap[nDeviceID] = device;
}
LUTRON_DEVICE LutronBridge::getDevice(int nDeviceID)
{
    return deviceMap[nDeviceID];
}
bool LutronBridge::deviceExists(int nDeviceID)
{
    return deviceMap.find(nDeviceID) != deviceMap.end();
}

// Internet Published Function ... REQUIRES FORMAT: NN,MM where NN == DeviceID, MM == DimLevel 0-100
int LutronBridge::setDimmer(String sDimmer)
{
    Serial.println("LutronBridge::setDimmer - Received Command: " + sDimmer);

    int nPos = sDimmer.indexOf(',');

    if(nPos == -1)
        return -1;

    int nDimmerID = sDimmer.substring(0,nPos).toInt();
    float fLevel = sDimmer.substring(nPos+1).toFloat();

    return _setDimmer(nDimmerID, fLevel);
}

int LutronBridge::_setDimmer(int nDimmer, float fLevel)
{
    String sCommand = String::format(LUTRON_WRITE, nDimmer, fLevel);
    return sendCommand(sCommand);
}

// Get level of dimmer, sDimmer is the Device ID (number), returns the dim level (0-100)
int LutronBridge::getDimmer(String sDimmer)
{
    return _getDimmer(sDimmer.toInt());
}
int LutronBridge::_getDimmer(int nDimmer)
{
    String sCommand = String::format(LUTRON_READ, nDimmer);

    // Ask lutron the dimmer level
    return sendCommand(sCommand);
}

// returns a string with all dimmer states
String LutronBridge::getAllDimmers()
{
    String states;

    for(DEVICE_MAP::iterator it = deviceMap.begin();
        it != deviceMap.end();
        it++)
    {
        states += String::format("D=%i&L=%.0f\r\n", it->first, it->second.currentLevel);
    }

    Particle.publish("lutron/alldevices/state", states);
    Serial.println("LutronBridge::getAllDimmers()");
    Serial.println(states);

    return states;
}

int LutronBridge::setAllDimmers(String sCommand)
{
    String  sDeviceToken="D=";
    String  sLevelToken="L=";
    String  sResult = sCommand;

    // HANDLE INPUT
    int nStart=0;
    int nEnd=0;
    do
    {
        // LOOK For D= (NN&L=NN)
        nStart = sResult.indexOf(sDeviceToken, nStart);

        if(nStart == -1)
            break;

        // FOUND D=
        // first TOKEN is device ID
        nStart += sDeviceToken.length();

        nEnd = sResult.indexOf('&', nStart);
        String sDevice = sResult.substring(nStart, nEnd);
        int nDevice = sDevice.toInt();

        // LOOK for L=
        nStart = sResult.indexOf(sLevelToken, nStart);

        if(nStart == -1)
            break;

        nStart += sLevelToken.length();
        nEnd = sResult.indexOf('\r\n', nStart);

        String sLevel = sResult.substring(nStart, nEnd);
        float fLevel = sLevel.toFloat();

        nStart = nEnd + 2;

        _setDimmer(nDevice, fLevel);

    } while (nStart != -1);

    return 1;
}
// Internet Published function
int LutronBridge::sendCommand(String sCommand)
{
    // send command to lutron
    if(client.connected())
    {
        Serial.println("LutronBridge::sendCommand STARTED - " + sCommand);
        client.println(sCommand);
        Serial.println("LutronBridge::sendCommand RETURNED");
        return 1;
    }
    else
    {
        Serial.println("LutronBridge::sendCommand - CLIENT DISCONNECTED");
        return -1;
    }
}
