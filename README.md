# LutronController
IoT Gateway for Lutron Radio RA2

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
