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

byte lutronIP[] = { 10, 0, 0, 201 }; // My RadioRA2 server
char myIpString[24];
String sStatus;
TCPClient client;

void setup()
{
    IPAddress myIP = WiFi.localIP();
	sprintf(myIpString, "%d.%d.%d.%d", myIP[0], myIP[1], myIP[2], myIP[3]);

	Particle.variable("devIP", myIpString, STRING);
    Particle.variable("status", sStatus);
    Particle.function("sendCommand", sendCommand);

    // Make sure your Serial Terminal app is closed before powering your device
    Serial.begin(9600);

    connectLutron();
}

void loop()
{
}

int connectLutron()
{
    Serial.println("Connecting...");
    sStatus = "Connecting...";
    if (client.connect(lutronIP, 23))
    {
        Serial.println("Connected");
        sStatus = "Connected";

        client.println("lutron");
        client.println("integration");

        while (client.available()) {
            char c = client.read();
            Serial.print(c);
        }
        Serial.println("Ready.");
        sStatus = "Ready.";
        return 1;
    }
    else
    {
        Serial.println("Connection failed.");
        sStatus = "Connection failed.";
        return 0;
    }
}

int sendCommand(String sCommand)
{
    Serial.println("Recieved Command: " + sCommand);
    client.println(sCommand);
    //Serial.println(sCommand);
    sStatus = sCommand;

    while (client.available()) {
        char c = client.read();
        Serial.print(c);
    }
    return 1;
}
