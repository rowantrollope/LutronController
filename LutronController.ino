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
int nLightID[4] = { 0, 71, 0, 72 }; // When InternetButton N is pressed, this Light ID will get a command sent
float fLightLevel[4]; // The current LEVEL for that dimmer
float fOnLightLevel[4] = { 90, 90, 90, 90 }; // The level when the light is set to ON

String sDimmerReadTemplate = "?OUTPUT,%i,1"; // variable is device id
String sDimmerWriteTemplate = "#OUTPUT,%i,1,%.2f"; // first variable is the device id, second is dimmer level
String sOverrideCmd[4];

void setup()
{
    // For my Lutron Bridge I'm doubling up and using a Photon in an InternetButton
    // shield so I can control a few lights.
    b.begin();

    Particle.function("lutronSendCommand", lutronSendCommand);
    Particle.function("lutronSetDimmer", lutronSetDimmer);
    Particle.function("lutronGetDimmer", lutronGetDimmer);

    Particle.variable("lightID1", nLightID[0]);
    Particle.variable("lightID2", nLightID[1]);
    Particle.variable("lightID3", nLightID[2]);
    Particle.variable("lightID4", nLightID[3]);

    Particle.variable("lightLevel1", fLightLevel[0]);
    Particle.variable("lightLevel1", fLightLevel[1]);
    Particle.variable("lightLevel1", fLightLevel[2]);
    Particle.variable("lightLevel1", fLightLevel[3]);

    Particle.variable("overrideCmd", sOverrideCmd[0]);
    Particle.variable("overrideCmd", sOverrideCmd[1]);
    Particle.variable("overrideCmd", sOverrideCmd[2]);
    Particle.variable("overrideCmd", sOverrideCmd[3]);

    String sOverrideCmd[4];

    // Make sure your Serial Terminal app is closed before powering your device
    Serial.begin(9600);

    lutronConnect();
    lutonInitDimmerLevels();

    b.allLedsOff();
    for(int i=0;i<4;i++)
    {
        // set the LED level to the dimness level of the light
        int nLevel = 255 * (fLightLevel[i] / 100);
        if(nLevel/3<=0)
            b.ledOff(i*3);
        else
            b.ledOn(i*3, nLevel/3, nLevel/3, nLevel/3);
        //_lutronSetDimmer(nLightID[i], fOnLightLevel[i]);
    }

}

void loop()
{
    // Check if any buttons are depressed, turn on the LED then send the
    // command to Lutron bridge

    // TODO: Support dimming- When off, click turns to on level, click and hold gradually raises light

    // TODO: Maintain state: Listen for feedback on telnet, watching for any of our lights, then setting state internally

    for(int i=0;i<4;i++)
    {
        if(b.buttonOn(i+1))
        {
            Serial.println("Button Pressed.");
            if( fLightLevel[i] > 0) // If light is ON, turn it off
            {
                _lutronSetDimmer(nLightID[i], 0);

                b.ledOff(i*3);

                fLightLevel[i] = 0;
            }
            else // Set to ON
            {
                _lutronSetDimmer(nLightID[i], fOnLightLevel[i]);

                // set the LED level to the dimness level of the light
                int nLevel = 255 * (fOnLightLevel[i] / 100);
                if(nLevel / 3 >= 0)
                    b.ledOn(i*3, nLevel/3, nLevel/3, nLevel/3);
                else
                    b.ledOff(i*3);

                fLightLevel[i] = fOnLightLevel[i];
            }
        }
    }
    delay(500);
}

int lutronConnect()
{
    Serial.println("Connecting...");

    if (client.connect(lutronIP, 23))
    {
        Serial.println("Connected");

        client.println("lutron");
        client.println("integration");

        while (client.available()) {
            char c = client.read();
            Serial.print(c);
        }
        Serial.println("Ready.");
        return 1;
    }
    else
    {
        Serial.println("Connection failed.");
        return 0;
    }
}

int lutonInitDimmerLevels()
{
    for(int i=0;i<4;i++)
        fLightLevel[i] = _lutronGetDimmer(nLightID[i]);

    return 1;
}

// Internet Published Function ... REQUIRES FORMAT: NN,MM where NN == DeviceID, MM == DimLevel 0-100
int lutronSetDimmer(String sDimmer)
{
    Serial.println("Received Command: " + sDimmer);

    int nPos = sDimmer.indexOf(',');

    if(nPos == -1)
        return -1;

    int nDimmerID = sDimmer.substring(0,nPos).toInt();
    float fLevel = sDimmer.substring(nPos+1).toFloat();

    if(_lutronSetDimmer(nDimmerID, fLevel).length())
        return 1;
    else
        return -1;
}

String _lutronSetDimmer(int nDimmer, float fLevel)
{
    String sCommand = String::format(sDimmerWriteTemplate, nDimmer, fLevel);
    return(_lutronSendCommand(sCommand));
}

// Get level of dimmer, sDimmer is the Device ID (number), returns the dim level (0-100)
int lutronGetDimmer(String sDimmer)
{
    return (int) _lutronGetDimmer(sDimmer.toInt());
}

// Internal Function ...
float _lutronGetDimmer(int nLightID)
{
    String sCommand = String::format(sDimmerReadTemplate, nLightID);

    // Ask lutron the dimmer level
    String sReturn = _lutronSendCommand(sCommand);

    // Extract the current level and save it - format ~OUTPUT,DEVICEID,LEVEL (FLOAT)
    int nPos = sReturn.lastIndexOf(',');
    if(nPos == -1)
    {
        Serial.println("Error gettng dim level: " + sReturn + " Expected ~OUTPUT,ID,LEVEL");
        return 0; // So we don't cause problems, we'll just return 0...
    }

    String sLevel = sReturn.substring(nPos + 1);
    if(sLevel.length() > 0)
    {
        Serial.println("Lutron returned: " + sReturn);
        //Serial.println("Dimmer: " + nLightID + ", Level: " + sLevel);
        return sLevel.toFloat();
    }
    else
    {
        Serial.println("_lutronGetDimmer ERROR");
        return -1;
    }

}

// Internet Published function
int lutronSendCommand(String sCommand)
{
    _lutronSendCommand(sCommand);
    return 1;
}

// Internal function which returns the feedback
String _lutronSendCommand(String sCommand)
{
    Serial.println("Recieved Command: " + sCommand);

    // send command to lutron
    client.println(sCommand);

    String sResult;
    // give a bit of time to respond
    delay(500);
    while (client.available())
    {
        char c = client.read();
        Serial.print(c);
        sResult += c;
    }

    Serial.print("_lutronSendCommand - returned: ");
    Serial.println(sResult);

    return sResult;
}
