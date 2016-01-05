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
}

os_thread_return_t listener(void* param)
{
    return ((LutronBridge *)param)->telnetListener(param);
}

bool LutronBridge::connect(byte lutronIP[])
{
    Serial.println("lutronConnect - Connecting...");

    if (client.connect(lutronIP, TELNET_PORT))
    {
        Serial.println("lutronConnect - Connected");

        client.println("lutron");
        client.println("integration");

        // wait a couple seconds for the telnet server to catch up
        delay(2000);

        // Setup listener thread to respond to light change events
        telnetListenerThread = new Thread("telnetListener", listener, this);

        // Get the current states of the lights we care about
        initDimmerLevels();

        return true;
    }
    else
    {
        Serial.println("lutronConnect - Connection failed.");
        return false;
    }
}

os_thread_return_t LutronBridge::telnetListener(void* param)
{
    // Extract the current level and save it - format ~OUTPUT,DEVICEID,LEVEL (FLOAT)
    String sVal = LUTRON_RETURN;

    while(true)
    {
        String sResult;
        bool bInput=false;
        while (client.available())
        {
            char c = client.read();
            sResult += c;
            bInput = true;
        }

        if(bInput)
        {
            Serial.println("LutronBridge::telnetListener() - " + sResult);

            // HANDLE INPUT
            int nStart=0;
            do
            {
                // LOOK For ~OUTPUT,NN,NN,NN
                nStart = sResult.indexOf(sVal, nStart);

                if(nStart == -1)
                    break;

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

                if(nCommand == 1)
                {
                    String sDebug = String::format("LutronBridge::telnetListener() - DEVICE=%i, CMD=%i, LEVEL=%.2f", nDevice, nCommand, fLevel);
                    Serial.println(sDebug);

                    // Publish light changed events
                    if(bPublishAll)
                    {
                        String sEventData = String::format("DEVICE=%i,COMMAND=%i,LEVEL=%i", nDevice, nCommand, fLevel);
                        Particle.publish("lutron/device/changed", sEventData);
                    }
                    // Check if we care about this light
                    DEVICE_MAP::iterator it = deviceMap.find(nDevice);
                    if(it != deviceMap.end())
                    {
                        it->second.currentLevel = fLevel;
                        if(changeCB)
                            changeCB(nDevice);
                    }

                    nStart = sResult.indexOf(sVal, nStart);
                }

            } while (nStart != -1);
        }
    }
}

int LutronBridge::initDimmerLevels()
{
    for(DEVICE_MAP::iterator it = deviceMap.begin();
        it != deviceMap.end();
        it++)
    {
        _getDimmer(it->first);
        delay(250);
    }
     /* TODO kill
    _getDimmer(devices[0].id);
    delay(250);
    _getDimmer(devices[1].id);
*/
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
// Internet Published function
int LutronBridge::sendCommand(String sCommand)
{
    Serial.println("LutronBridge::sendCommand - " + sCommand);

    // send command to lutron
    client.println(sCommand);

    return 1;
}
