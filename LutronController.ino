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
String sDimmerStates;

int nLastPress=0;

// Disconnect reconnect to Telnet every once in a while, currently every
bool bConnected=false;

// This is the code for a periodic reconnect - as I've seen inconsistencies when
// we stay connected for too long

bool bReconnect=false;
/*
Timer   reconnectTimer(20 * 60 * 1000, reconnectTelnet);

void reconnectTelnet() {
    bReconnect=true;
}
*/
typedef struct
{
    int buttonID;
    int ledID;
    int deviceID;
} BUTTON_DATA;

BUTTON_DATA  button1 = { 2, 3, 71 };
BUTTON_DATA  button2 = { 4, 9, 72 };

void setup()
{
    // For my Lutron Bridge I'm doubling up and using a Photon in an InternetButton
    // shield so I can control a few lights.
    b.begin();

    Particle.function("sendCmd", sendCommand);
    Particle.function("setDimmer", setDimmer);
    Particle.function("getDimmer", getDimmer);
    Particle.function("getAll", getAllDimmers);
    Particle.function("setAll", setAllDimmers);
    Particle.function("setButton1", setBtn1);
    Particle.function("setButton2", setBtn2);
    Particle.function("reconnect", reconnectLutron);
    Particle.variable("lastStatus", sLastStatus);
    Particle.variable("dimmerStates", sDimmerStates);

    // Make sure your Serial Terminal app is closed before powering your device
    Serial.begin(9600);

    // initialize LEDs
    ledBreatherThread = new Thread("ledBreather", ledBreather, NULL);

    b.allLedsOff();

    lutron.setNotifyCallback(dimmerChanged);

    // set this flag to initialize all light states
    lutron.m_bMonitor = true;

    bConnected = lutron.connect(lutronIP);

}
// debug: testing reconnect
int reconnectLutron(String sCommand)
{
    bReconnect = true;
}
int getAllDimmers(String sCommand)
{
    sDimmerStates = lutron.getAllDimmers();
    return 1;
}
int setAllDimmers(String sCommand)
{
    return lutron.setAllDimmers(sCommand);
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
    if(bReconnect)
    {
        lutron.disconnect();
        bConnected = bReconnect = false;
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
        LUTRON_DEVICE device = lutron.getDevice(getDeviceIDFromButton(nLastPress));

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

        lutron._setDimmer(getDeviceIDFromButton(nLastPress), device.currentLevel);

    }
    else if (b.buttonOn(3)) // LOWER
    {
        LUTRON_DEVICE device = lutron.getDevice(getDeviceIDFromButton(nLastPress));

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

        lutron._setDimmer(getDeviceIDFromButton(nLastPress), device.currentLevel);

    }

}

void dimmerChanged(int nDeviceID)
{
    // Check if we care about this light
    // LutronBridge will inform us of ALL light events
    if(nDeviceID == button1.deviceID || nDeviceID == button2.deviceID)
    {
        float fLevel = lutron.getDevice(nDeviceID).currentLevel;

        String sEventData = String::format("dimmerChanged() - DEVICE=%i,LEVEL=%.02f", nDeviceID, fLevel);
        //Particle.publish("lutron/device/changed", sEventData);
        Serial.println(sEventData);

        setLEDState(getButtonIDFromDevice(nDeviceID), fLevel, true);
    }
    else
    {
        //String str = String::format("dimmerChanged() - IGNORING DEVICE: %i", nDeviceID);
        //Serial.println(str);
        return;
    }

}
int getDeviceIDFromButton(int button)
{
    if(button1.buttonID == button)
        return button1.deviceID;
    else if(button2.buttonID == button)
        return button2.deviceID;
    else
        return -1;
}
int getButtonIDFromDevice(int device)
{
    if(button1.deviceID == device)
        return button1.buttonID;
    else if(button2.deviceID == device)
        return button2.buttonID;
    else
        return -1;
}

void handleButtonPress(int nButtonID)
{
    Serial.println("loop() - Button Pressed.");

    int deviceID = getDeviceIDFromButton(nButtonID);

    LUTRON_DEVICE   device = lutron.getDevice(deviceID);

    String sDebug = String::format("handleButtonPress() - DeviceID: %i, device.currentLevel: %.02f", deviceID, device.currentLevel);
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

void setLEDState(int nButtonID, float nLightLevel, bool bSoft)
{
    int nLED = 0;

    if(button1.buttonID == nButtonID)
        nLED = button1.ledID;
    else if(button2.buttonID == nButtonID)
        nLED = button2.ledID;
    else
        return;

    if(nLightLevel == 0)
    {
        String sDebug = String::format("setLEDState() - LED %n OFF", nLED);
        Serial.println(sDebug);
        b.ledOff(nLED);
        return;
    }

    String sDebug = String::format("setLEDState() - LED: %i - LEVEL: %.02f", nLED, nLightLevel);
    Serial.println(sDebug);

    if(bSoft)
        softLEDOn(nLED,nLightLevel, nLightLevel, nLightLevel);
    else
        b.ledOn(nLED, nLightLevel, nLightLevel, nLightLevel);

}

void softLEDOn(int nID, int nRed, int nGreen, int nBlue)
{
    int nSteps = 10;
    for(float i=0;i<1;i+=.1)
    {
        b.ledOn(nID, nRed*i, nGreen*i, nBlue*i);
        delay(50);
    }
}


os_thread_return_t ledBreather(void* param)
{
    Serial.println("ledBreather() - Thread Started!");
    while(true)
    {
        //for(int n=1;n<11;n++)
        {
            //int n=1;
            for(int i=0;i<35;i++)
            {
                if(bConnected)
                {
                    b.ledOn(1,0,i,0);
                    b.ledOn(11,0,i,0);
                }
                else
                {
                    b.ledOn(1,i,0,0);
                    b.ledOn(11,i,0,0);
                }

                delay(25);
            }
            for(int i=35;i>0;i--)
            {
                if(bConnected)
                {
                    b.ledOn(1,0,i,0);
                    b.ledOn(11,0,i,0);
                }
                else
                {
                    b.ledOn(1,i,0,0);
                    b.ledOn(11,i,0,0);
                }
                delay(25);
            }
        }
    }
}

int setBtn1(String sCommand)
{
    return button1.deviceID = sCommand.toInt();
}
int setBtn2(String sCommand)
{
    return button2.deviceID = sCommand.toInt();;
}
