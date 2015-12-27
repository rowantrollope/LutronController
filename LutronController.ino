/*

    LutronController

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

#include "InternetButton.h";

byte lutronIP[] = { 10, 0, 0, 201 }; // My RadioRA2 server
char myIpString[24];
TCPClient client;
InternetButton b = InternetButton();
int nLightID[4] = { 34, 71, 47, 72 }; // When InternetButton N is pressed, this Light ID will get a command sent
float fLightLevel[4]; // The current LEVEL for that dimmer
float fOnLightLevel[4] = { 90, 90, 90, 90 }; // The level when the light is set to ON

String sDimmerReadTemplate = "?OUTPUT,%i,1"; // variable is device id
String sDimmerWriteTemplate = "#OUTPUT,%i,1,%.2f"; // first variable is the device id, second is dimmer level
String sOverrideCmd[4];
int nDimFactor=3;
Thread *ledBreatherThread;
Thread *telnetListenerThread;

void setup()
{
    // For my Lutron Bridge I'm doubling up and using a Photon in an InternetButton
    // shield so I can control a few lights.
    b.begin();

    Particle.function("sendCmd", lutronSendCommand);
    Particle.function("setDimmer", lutronSetDimmer);
    Particle.function("getDimmer", lutronGetDimmer);
    Particle.function("setSwitch", setSwitch);

    // Make sure your Serial Terminal app is closed before powering your device
    Serial.begin(9600);

    // initialize LEDs
    ledBreatherThread = new Thread("ledBreather", ledBreather, NULL);
    b.allLedsOff();

    // Connect to Lutron server
    lutronConnect();
    delay(2000);

    // Setup listener thread to respond to light change events
    telnetListenerThread = new Thread("telnetListener", telnetListener, NULL);

    // Get the current states of the lights we care about
    lutronInitDimmerLevels();

}

void setLEDState(int nID, float nLightLevel)
{
    if(nLightLevel == 0)
    {
        String sDebug = String::format("setLEDState() - LED %n OFF", nID);
        Serial.println(sDebug);
        b.ledOff(nID*3);
        return;
    }

    // set the LED level to the dimness level of the light
    float multiplier = nLightLevel / 100;

    int nLevel = 255 * multiplier;

    //nLevel = nLevel / nDimFactor;

    String sDebug = String::format("setLEDState() - LED: %i - LEVEL: %i", nID, nLevel);
    Serial.println(sDebug);

    softLEDOn(nID*3,nLevel, nLevel, nLevel);

    //b.ledOn(nID*3, nLevel, nLevel, nLevel);

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

void loop()
{
    // Check if any buttons are depressed, turn on the LED then send the
    // command to Lutron bridge
    bool bButtonPressed=false;
    // TODO: Support dimming- When off, click turns to on level, click and hold gradually raises light

    // TODO: Maintain state: Listen for feedback on telnet, watching for any of our lights, then setting state internally

    for(int i=0;i<4;i++)
    {
        if(b.buttonOn(i+1))
        {
            Serial.println("loop() - Button Pressed.");
            if( fLightLevel[i] > 0) // If light is ON, turn it off
            {
                b.playSong("g2,2");
                _lutronSetDimmer(nLightID[i], 0);

                fLightLevel[i] = 0;

            }
            else // Set to ON
            {
                b.playSong("c5,4");
                _lutronSetDimmer(nLightID[i], fOnLightLevel[i]);

                fLightLevel[i] = fOnLightLevel[i];

                /* NOW Handled by listener
                b.ledOff(i*3);
                // set the LED level to the dimness level of the light

                int nLevel = 255 * (fOnLightLevel[i] / 100);
                if(nLevel / nDimFactor >= 0)
                    b.ledOn(i*3, nLevel/nDimFactor, nLevel/nDimFactor, nLevel/nDimFactor);
                else
                    b.ledOff(i*3);
                */

            }
            delay(500);
            return;
        }
    }
}

os_thread_return_t ledBreather(void* param)
{
    Serial.println("ledBreather() - Thread Started!");
    while(true)
    {
        for(int i=0;i<150;i++)
        {
            b.ledOn(1,0,i,0);
            delay(10);
        }
        for(int i=150;i>0;i--)
        {
            b.ledOn(1,0,i,0);
            delay(10);
        }
    }
}

// this thread watches for the specific events we care about -
// dimmers changing levels, and stores the new level
os_thread_return_t telnetListener(void* param)
{
    // Extract the current level and save it - format ~OUTPUT,DEVICEID,LEVEL (FLOAT)
    String sVal = "~OUTPUT,";

    while(true)
    {
        String sResult;
        bool    bInput=false;
        while (client.available())
        {
            char c = client.read();
            //Serial.print(c);
            sResult += c;
            bInput=true;
        }

        if(bInput)
        {
            Serial.println("telnetListener() - " + sResult);

            // HANDLE INPUT
            int nStart=0;
            do
            {
                // LOOK For ~OUTPUT,NN,NN,NN
                nStart = sResult.indexOf(sVal, nStart);

                if(nStart == -1)
                    break;

                // FOUND ~OUTPUT
                // Serial.println(String::format("telnetListener() - FOUND ~OUTPUT Command", nStart));

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
                    String sDebug = String::format("telnetListener() - DEVICE=%i, CMD=%i, LEVEL=%.2f", nDevice, nCommand, fLevel);
                    Serial.println(sDebug);

                    // Check if we care about this light
                    for(int i=0;i<4;i++)
                        if(nLightID[i] == nDevice)
                        {
                            fLightLevel[i] = fLevel;
                            setLEDState(i, fLightLevel[i]);
                        }

                    nStart = sResult.indexOf(sVal, nStart);
                }

            } while (nStart != -1);
        }
    }
}
int lutronConnect()
{
    Serial.println("lutronConnect - Connecting...");

    if (client.connect(lutronIP, 23))
    {
        Serial.println("lutronConnect - Connected");

        client.println("lutron");
        client.println("integration");

        /* Now handled by listenerThread

        delay(2000);
        while (client.available()) {
            char c = client.read();
            Serial.print(c);
        }
        */
        return 1;
    }
    else
    {
        Serial.println("lutronConnect - Connection failed.");
        return 0;
    }
}

int lutronInitDimmerLevels()
{
    for(int i=0;i<4;i++)
    {
        _lutronGetDimmer(nLightID[i]);
        delay(250);
    }

    return 1;
}

// Internet Published Function ... REQUIRES FORMAT: NN,MM where NN == DeviceID, MM == DimLevel 0-100
int lutronSetDimmer(String sDimmer)
{
    Serial.println("lutronSetDimmer - Received Command: " + sDimmer);

    int nPos = sDimmer.indexOf(',');

    if(nPos == -1)
        return -1;

    int nDimmerID = sDimmer.substring(0,nPos).toInt();
    float fLevel = sDimmer.substring(nPos+1).toFloat();

    return _lutronSetDimmer(nDimmerID, fLevel);
}

int _lutronSetDimmer(int nDimmer, float fLevel)
{
    String sCommand = String::format(sDimmerWriteTemplate, nDimmer, fLevel);
    return lutronSendCommand(sCommand);
}

// Get level of dimmer, sDimmer is the Device ID (number), returns the dim level (0-100)
int lutronGetDimmer(String sDimmer)
{
    return _lutronGetDimmer(sDimmer.toInt());
}
int _lutronGetDimmer(int nDimmer)
{
    String sCommand = String::format(sDimmerReadTemplate, nDimmer);

    // Ask lutron the dimmer level
    return lutronSendCommand(sCommand);
}
// Internet Published function
int lutronSendCommand(String sCommand)
{
    Serial.println("lutronSendCommand - " + sCommand);

    // send command to lutron
    client.println(sCommand);

    return 1;
}

// NN,MM
// NN = 1-4 (Button ID)
// MM = Device ID

int setSwitch(String sCommand)
{
    int nPos = sCommand.indexOf(',');

    if(nPos == -1)
        return -1;

    int nButtonID = sCommand.substring(0,nPos).toInt();
    int nDeviceID = sCommand.substring(nPos+1).toInt();
    nLightID[nButtonID] = nDeviceID;

}
