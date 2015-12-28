/*

    LutronController



    /rowantrollope
*/
using namespace std;
#include <map>;
#include "InternetButton.h";
#include "LutronBridge.h"

LutronBridge lutron;
byte lutronIP[] = { 10, 0, 0, 201 }; // My RadioRA2 server

InternetButton b = InternetButton();

Thread *ledBreatherThread;
String sLastStatus;
int nLastPress=0;
bool bConnected=false;

struct BUTTON_MAP {
    int nButtonID;
    int nLED;
};

std::map<int,BUTTON_MAP>   mapDevice;
std::map<int,int>          mapButtons;
void setup()
{
    // For my Lutron Bridge I'm doubling up and using a Photon in an InternetButton
    // shield so I can control a few lights.
    b.begin();

    Particle.function("sendCmd", sendCommand);
    Particle.function("setDimmer", setDimmer);
    Particle.function("getDimmer", getDimmer);

    Particle.function("setButton1", setBtn1);
    Particle.function("setButton2", setBtn2);

    Particle.variable("lastStatus", sLastStatus);
    // map our devices to our buttons/LEDs
    mapDevice[71].nButtonID = 2;
    mapDevice[71].nLED = 3;
    mapDevice[72].nButtonID = 4;
    mapDevice[72].nLED = 9;
    mapButtons[2] = 71;
    mapButtons[4] = 72;

    // Make sure your Serial Terminal app is closed before powering your device
    Serial.begin(9600);

    // initialize LEDs
    ledBreatherThread = new Thread("ledBreather", ledBreather, NULL);
    b.allLedsOff();

    // Connect to Lutron server
    LUTRON_DEVICE   device;
    device.onLevel = 90;

    lutron.addDevice(71, device);
    lutron.addDevice(72, device);

    lutron.setNotifyCallback(dimmerChanged);

    bConnected = lutron.connect(lutronIP);

}
int sendCommand(String sCommand)
{
    return lutron.sendCommand(sCommand);
}
int setDimmer(String sDimmer)
{
    return lutron.setDimmer(sDimmer);
}
int getDimmer(String sDimmer)
{
    return lutron.getDimmer(sDimmer);
}
void loop()
{
    if(!bConnected)
    {
        bConnected = lutron.connect(lutronIP);
        delay(3000);
    }

    // Check if any buttons are depressed, turn on the LED then send the
    // command to Lutron bridge
    bool bButtonPressed=false;

    if(b.buttonOn(2))
    {
        nLastPress = 2;
        handleButtonPress(2);
        delay(500);
        return;
    }
    else if (b.buttonOn(4))
    {
        nLastPress = 4;
        handleButtonPress(4);
        delay(500);
        return;
    }
    else if(b.buttonOn(1)) // RAISE
    {
        LUTRON_DEVICE device = lutron.getDevice(mapButtons[nLastPress]);

        if(device.currentLevel == 100)
            return;

        while(b.buttonOn(1) && device.currentLevel < 100)
        {
            delay(200);
            device.currentLevel += 10;
            setLEDState(nLastPress, device.currentLevel, false);
        }

        if (device.currentLevel > 100)
            device.currentLevel = 100;

        lutron._setDimmer(mapButtons[nLastPress], device.currentLevel);

    }
    else if (b.buttonOn(3)) // LOWER
    {
        LUTRON_DEVICE device = lutron.getDevice(mapButtons[nLastPress]);

        if(device.currentLevel == 0)
            return;

        while(b.buttonOn(3) && device.currentLevel > 0)
        {
            delay(200);
            device.currentLevel -= 10;
            setLEDState(nLastPress, device.currentLevel, false);
        }

        if(device.currentLevel < 0)
            device.currentLevel = 0;

        lutron._setDimmer(mapButtons[nLastPress], device.currentLevel);

    }

}

void dimmerChanged(int nID)
{
    Serial.println("dimmerChanged()");

    setLEDState(nID, lutron.getDevice(nID).currentLevel, true);
}

void handleButtonPress(int nButtonID)
{
    Serial.println("loop() - Button Pressed.");

    int             deviceID = mapButtons[nButtonID];
    LUTRON_DEVICE   device = lutron.getDevice(deviceID);

    String sDebug = String::format("handleButtonPress() - DeviceID: %i, device.currentLevel: %i", deviceID, device.currentLevel);
    Serial.println(sDebug);

    if(device.currentLevel > 0) // If light is ON, turn it off
    {

        b.playSong("c7,8");
        lutron._setDimmer(deviceID, 0);
        device.currentLevel = 0;
        lutron.updateDevice(deviceID, device);
    }
    else // Set to ON
    {
        b.playSong("c5,4");
        lutron._setDimmer(deviceID, device.onLevel);
        device.currentLevel = device.onLevel;
        lutron.updateDevice(deviceID, device);
    }
}

void setLEDState(int nDeviceID, float nLightLevel, bool bSoft)
{
    int nLED = mapDevice[nDeviceID].nLED;

    if(nLightLevel == 0)
    {
        String sDebug = String::format("setLEDState() - LED %n OFF", nLED);
        Serial.println(sDebug);
        b.ledOff(nLED);
        return;
    }

    // set the LED level to the dimness level of the light
    float multiplier = nLightLevel / 100;

    int nLevel = 255 * multiplier;

    String sDebug = String::format("setLEDState() - LED: %i - LEVEL: %i", nLED, nLevel);
    Serial.println(sDebug);

    if(bSoft)
        softLEDOn(nLED,nLevel, nLevel, nLevel);
    else
        b.ledOn(nLED,nLevel,nLevel,nLevel);

}

void softLEDOn(int nID, int nRed, int nGreen, int nBlue)
{
    int nSteps = 10;
    for(float i=0;i<1;i+=.1)
    {
        b.ledOn(nID, nRed*i, nGreen*i, nBlue*i);
        delay(25);
    }
}


os_thread_return_t ledBreather(void* param)
{
    Serial.println("ledBreather() - Thread Started!");
    while(true)
    {
        //for(int n=1;n<11;n++)
        {
            int n=1;
            for(int i=0;i<50;i++)
            {
                if(bConnected)
                    b.ledOn(n,0,i,0);
                else
                    b.ledOn(n,i,0,0);

                delay(10);
            }
            for(int i=50;i>0;i--)
            {
                if(bConnected)
                    b.ledOn(n,0,i,0);
                else
                    b.ledOn(n,i,0,0);

                delay(10);
            }
        }
    }
}

// NN,MM
// NN = 1-4 (Button ID)
// MM = Device ID

int setBtn1(String sCommand)
{
    int deviceID = sCommand.toInt();

    LUTRON_DEVICE dev;
    dev.onLevel = 90;
    lutron.addDevice(deviceID, dev);
    mapButtons[2] = deviceID;
    mapDevice[deviceID].nButtonID = 2;
    return lutron._getDimmer(deviceID);
}
int setBtn2(String sCommand)
{
    int deviceID = sCommand.toInt();

    LUTRON_DEVICE dev;
    dev.onLevel = 90;
    lutron.addDevice(deviceID, dev);
    mapButtons[4] = deviceID;
    mapDevice[deviceID].nButtonID = 4;
    return lutron._getDimmer(deviceID);
}
