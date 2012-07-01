/*
  (P)ortable (A)rduino (N)etwork (T)ool
  
  See README for more information or email info@inditech.org
*/

//#include <LiquidCrystal.h>    // Adafruit modified LiquidCrystal library for i2c backpack
#include <LiquidTWI.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Dns.h>
#include <utility/w5100.h>

// https://github.com/mcherry/I2Ceeprom
#include <I2Ceeprom.h>
#include <Wire.h>

// sourced from http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1285859555
// written by Blake Foster - http://www.blake-foster.com/contact.php
// modified by Mike Cherry <mcherry@inditech.org> to reduce the amount of bytes return on success/fail
// modified version: https://github.com/mcherry/ICMPPing
#include <ICMPPing.h>

// For reading/processing chip temperature
// #include <avr/io.h>

#define PANT_VERSION "3.3.1.10"
#define INFO_URL "goo.gl/HzHKn"
/*
#define AUTHOR "Mike Cherry"
#define COAUTHOR "Eric Brundick"
#define COAUTHOR2 "Blake Foster"
#define EMAIL1 "mcherry@"
#define EMAIL2 "inditech.org"
#define COEMAIL1 "spirilis@"
#define COEMAIL2 "linux.com"
#define COEMAIL3 "blfoster"
#define COEMAIL4 "@vassar.edu"
*/

#define LONG_DELAY 1750
#define MEDIUM_DELAY 500
#define SHORT_DELAY 250
#define MICRO_DELAY 150

#define PROMPT ">"
#define CANCEL "Canceled        "
#define STATUS_OK "."
#define STATUS_GO "o"
#define STATUS_FAIL "x"
#define BLANK "                "

#define buttonUp 0
#define buttonDown 1
#define buttonSelect 2
#define buttonBack 3

#define EEPROM_PAGESIZE 16

boolean buttons[4];

// Ethernet mac address
byte mac[6] = { 0x90, 0xA2, 0xDA, 0x0D, 0x61, 0x42 };

// internet server to ping and another to perform dns lookup
byte internetIp[4] = { 4, 2, 2, 2 };
char internetServer[11] = "google.com";

int ethernetActive = 0;
int ethernetFailed = 0;
int sentPackets = 0;

char pingBuffer[20];

// buffers to be reused for misc message printing
char line0[17];
char line1[17];

// for the current on-screen menu
int CurrentPage = 0;
int CurrentMenuItem = 0;
int CursorPosition = 0;

//int lastPingTotalPackets = 0;

unsigned long netsize;
unsigned long current;
byte subnet[4];

// keep track of a few metrics
IPAddress myLocalIp;
IPAddress mySubnetMask;

// the icmp ping library seems to not be reliable if using any other socket
SOCKET pingSocket = 3;

// initialize lcd & ethernet
//LiquidCrystal lcd(0);
LiquidTWI lcd(0);
EthernetClient client;

// initialize 1024Mbit EEPROM chip at address 0x50 using 128 byte pages
I2Ceeprom eeprom(0x50, 1024000, 128);

// print a message to a given column and row, optionally clearing the screen
void lcdPrint(int column, int row, char message[], boolean clrscreen = false)
{
  if (clrscreen == true) lcd.clear();
  
  lcd.setCursor(column, row);
  lcd.print(message);
}

// ping a host and return number of packets lost
// send 10 pings by default
int pingHost(IPAddress ip, char label[], int pings = 10)
{
  byte thisIp[] = { ip[0], ip[1], ip[2], ip[3] };
  
  int packetLoss = 0;
  int packets = 0;
  int pingNo = 0;
  int hasHadPacketLoss = 0;
  int pingloop = 0;
  int linePos = 0;
  
  lcdPrint(0, 0, label, true);
   
  sprintf(line1, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  lcdPrint(0, 1, line1);
   
  // clear line 1 after giving the user time to read which IP is being pinged
  delay(LONG_DELAY);
  lcdPrint(0, 1, BLANK);
  
  sentPackets = 0;
  
  if (pings == 0)
  {
    pings = 2;
    pingloop = 1;
  }
  
  for (pingNo = 0; pingNo < pings; pingNo++)
  {
    ICMPPing ping(pingSocket);
     
    readButtons();
      
    if (buttons[buttonBack] == HIGH)
    {
      lcdPrint(0, 1, CANCEL);
       
      delay(MEDIUM_DELAY);
        
      if (pingloop == 1)
      {
        break;
      }
        
      return packetLoss;
    }
     
    // send a single ping with a 4 second timeout, returning the result in pingBuffer
    ping(4, thisIp, pingBuffer);
      
    if (strncmp(pingBuffer, "T", 1) == 0)
    {
      // packet timeout results in printing a 'x' on-screen
      lcdPrint(linePos, 1, STATUS_FAIL);
      packetLoss++;
        
      hasHadPacketLoss = 1;
    }
    else
    {
      // successful packet results in printing a '.' on-screen
      if (pingloop == 1)
      {
        lcdPrint(linePos, 1, STATUS_GO);
        hasHadPacketLoss = 0;
      }
      else
      {
        lcdPrint(linePos, 1, STATUS_OK);
      }
    }
     
    // wait minimum between pings
    delay(MEDIUM_DELAY);
      
    if (pingloop == 1)
    {
      if (hasHadPacketLoss == 0) lcdPrint(linePos, 1, STATUS_OK);
        
      pingNo = 0;
    }
      
    sentPackets++;
    linePos++;
      
    if (linePos == 16) linePos = 0;
  }
  
  // successful number of packets received
  packets = (sentPackets - packetLoss);
   
  sprintf(line1, "%d/%d received", packets, sentPackets);
  lcdPrint(0, 1, line1);
  
  delay(LONG_DELAY);

  return packetLoss;
}

// test if name resolution works using dns server obtained via dhcp
int dnsTest()
{
  DNSClient dnsLookup;
  IPAddress ipResolved;
  
  dnsLookup.begin(Ethernet.dnsServerIP());
  int err = dnsLookup.getHostByName(internetServer, ipResolved);
  
  if (err == 1)
  {
    sprintf(line0, "%d.%d.%d.%d", ipResolved[0], ipResolved[1], ipResolved[2], ipResolved[3]);
    
    lcdPrint(0, 0, internetServer, true);
    lcdPrint(0, 1, line0);
  }
  else
  {
    lcdPrint(0, 0, "DNS failed", true);
  }
  
  delay(LONG_DELAY);
  
  return err;
}

// run all available tests
void testNetwork()
{
  int gwPingLoss, dnsPingLoss, extPingLoss, err;
  int dnsFailed, gwFailed, extFailed;
  
  gwPingLoss = pingHost(Ethernet.gatewayIP(), "Ping gateway");   
  dnsPingLoss = pingHost(Ethernet.dnsServerIP(), "Ping DNS");

  err = dnsTest();
     
  extPingLoss = pingHost(internetIp, "Ping ext host");
  
  // basic test only sends 10 pings. if number of lost packets is 10, 100% packet loss
  if (gwPingLoss == 10) gwFailed = 1;
  if (dnsPingLoss == 10) dnsFailed = 1;
  if (extPingLoss == 10) extFailed = 1;
     
  // figure out if we are missing any packets and show the user
  if ((gwPingLoss > 0) || (dnsPingLoss > 0) || (extPingLoss > 0))
  {
    int line = 0;
    
    lcdPrint(0, 0, "Network OK", true);
    lcdPrint(0, 1, "w/packet loss");
         
    delay(LONG_DELAY);
    lcd.clear();
         
    if (gwPingLoss > 0)
    {
      sprintf(line0, "GW %d0%% Loss", gwPingLoss);
      lcdPrint(0, line, line0);
      line = 1;
    }
         
    if (dnsPingLoss > 0)
    {
      sprintf(line0, "DNS %d0%% Loss", dnsPingLoss);
      lcdPrint(0, line, line0);
      line = 1;
    }
    
    if (extPingLoss > 0)
    {
      if (line > 1)
      {
        delay(LONG_DELAY);
        lcd.clear();
           
        line = 0;
      }
           
      sprintf(line0, "EXT %d0%% Loss", extPingLoss);
      lcdPrint(0, line-1, line0);
    }
    
    delay(LONG_DELAY);
  }
}

// determine how many pages based on how many menu items we have
int PageCount(int MenuItems)
{
  int pages = 1;
  
  if (MenuItems > 2)
  {
    pages = (MenuItems / 2);
    
    if (MenuItems % 2 != 0) pages++;
  }
  
  return pages;
}

// display the first page of a menu
void printMenu(char *Menu[], int MenuItems)
{
  CursorPosition = 0;
  CurrentPage = 0;
  CurrentMenuItem = 0;
  
  lcd.clear();
  
  lcdPrint(0, CursorPosition, PROMPT);
  lcdPrint(2, 0, Menu[CurrentMenuItem]);
  
  if ((CurrentMenuItem + 1) < (MenuItems)) lcdPrint(2, 1, Menu[CurrentMenuItem + 1]);
  
  delay(SHORT_DELAY);
}

// advance the cursor (PROMPT) to the next menu item
void CursorNext(char *Menu[], int MenuItems)
{
  if ((CursorPosition == 0))
  {
    // we are at the begining of a menu
    if ((CurrentMenuItem + 1) < (MenuItems))
    {
      lcdPrint(0, 0, " ");
      lcdPrint(0, 1, PROMPT);
      
      CursorPosition++;
      CurrentMenuItem++;
    }
  }
  else
  {
    // we are already on line 1 of the currently displayed menu or
    // we are displaying pages if info
    // so we need to advance to the next page
    if (CurrentPage < (PageCount(MenuItems) - 1))
    {
      lcd.clear();
      
      CurrentPage++;
      CurrentMenuItem++;
      
      CursorPosition = 0;
         
      lcdPrint(0, CursorPosition, PROMPT);
      lcdPrint(2, 0, Menu[CurrentMenuItem]);
      
      // more than 1 item to display on this page
      if ((CurrentMenuItem + 1) < (MenuItems)) lcdPrint(2, 1, Menu[CurrentMenuItem + 1]);
    }
  }
  
  delay(SHORT_DELAY);
}

// move the cursor (PROMPT) to the previous menu item
void CursorPrevious(char *Menu[], int MenuItems)
{
  if ((CursorPosition == 1))
  {
    // we are already on line 1 of the display, so just move the cursor up 1 item
    if (CurrentMenuItem > 0)
    {
      lcdPrint(0, 1, " ");
      lcdPrint(0, 0, PROMPT);
      
      CursorPosition--;
      CurrentMenuItem--;
    }
  }
  else
  {
    if (CurrentPage > 0)
    {
      // we are already at line 0 but there is another page before this one
      // so clear the screen and move to the previous page
      lcd.clear();

      CurrentPage--;
      CurrentMenuItem--;

      CursorPosition = 1;
      
      lcdPrint(2, 0, Menu[CurrentMenuItem - 1]);
      lcdPrint(2, 1, Menu[CurrentMenuItem]);

      lcdPrint(0, 1, PROMPT);
    }
  }
  
  delay(SHORT_DELAY);
}

// display diagnostics menu items
void diagMenu()
{
  char *DiagMenu[5] = { "All Tests", "Ping Gateway", "Ping DNS", "DNS Resolve", "Ping Ext Host" };
  
  printMenu(DiagMenu, 5);
  
  while (1)
  {
    readButtons();
     
    // back button
    if (buttons[buttonBack] == HIGH) return;
     
    // up button  
    if (buttons[buttonUp] == HIGH) CursorPrevious(DiagMenu, 5);
    
    // down button
    if (buttons[buttonDown] == HIGH) CursorNext(DiagMenu, 5);
     
    // select button
    if (buttons[buttonSelect] == HIGH)
    {
      int gwPingLoss, dnsPingLoss, err, extPingLoss;
      switch (CurrentMenuItem)
      {
        case 0:
          testNetwork();
          break;
          
        case 1:
          gwPingLoss = pingHost(Ethernet.gatewayIP(), "Ping gateway", 0);
          break;
        
        case 2:
          dnsPingLoss = pingHost(Ethernet.dnsServerIP(), "Ping DNS", 0);
          break;
          
        case 3:
          err = dnsTest();
          break;
          
        case 4:
          extPingLoss = pingHost(internetIp, "Ping ext host", 0);
          break;
      }
        
      printMenu(DiagMenu, 5);
    }
  }
}

// display info screen
// can scroll through the info with up/down
void infoMenu()
{
  // info from DHCP
  IPAddress myGwIp = Ethernet.gatewayIP();
  IPAddress myDnsIp = Ethernet.dnsServerIP();
  
  // starting out at the begining of the display
  int menuPosition = 0;
  int buttonClick = 0;
  int menuItems = 4;
  
  while (1)
  {
    // if a button was clicked previously, reset it to 'unclicked'
    if (buttonClick == 1) buttonClick = 0;
    
    switch (menuPosition)
    {
      case 0:
        sprintf(line0, "IP Addr");
        sprintf(line1, "%d.%d.%d.%d", myLocalIp[0], myLocalIp[1], myLocalIp[2], myLocalIp[3]);
        break;
      
      case 1:
        sprintf(line0, "Gateway");
        sprintf(line1, "%d.%d.%d.%d", myGwIp[0], myGwIp[1], myGwIp[2], myGwIp[3]);
        break;
        
      case 2:
        sprintf(line0, "DNS");
        sprintf(line1, "%d.%d.%d.%d", myDnsIp[0], myDnsIp[1], myDnsIp[2], myDnsIp[3]);
        break;
    
      case 3:
        sprintf(line0, "Subnet Mask");
        sprintf(line1, "%d.%d.%d.%d", mySubnetMask[0], mySubnetMask[1], mySubnetMask[2], mySubnetMask[3]);
        break;
      
      /*
      case 4:
        sprintf(line0, "MAC Addr");
        sprintf(line1, "%02x%02x.%02x%02x.%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        break;
        
      case 5:
        sprintf(line0, "Processor Temp");
        sprintf(line1, "%i F", chipTempF());
        break;
      */
    }
    
    // display output compiled above
    lcdPrint(0, 0, line0, true);
    lcdPrint(0, 1, line1);
    
    // small delay to prevent buttons from repeating too fast
    delay(SHORT_DELAY);
    
    while (buttonClick == 0)
    {
      // as long as no button has been pressed, read the state of all buttons
      readButtons();
      
      // down button
      if (buttons[buttonDown] == HIGH)
      {
        if (menuPosition < (menuItems - 1))
        {
          // advance which info page we are on if we arent on the last page
          menuPosition++;
          buttonClick = 1;
        }
      }
      
      // up button
      if (buttons[buttonUp] == HIGH)
      {
        if (menuPosition > 0)
        {
          // decrease which page we are on if we arent on the first page
          menuPosition--;
          buttonClick = 1;
        }
      }
      
      // back button
      if (buttons[buttonBack] == HIGH) return;
    }
  }
}

char *eepromGetLine(int line, byte page_size = EEPROM_PAGESIZE)
{
  unsigned long lineaddr = 0;
  char newbuffer[page_size+1];
  byte a;
  
  if (line > 0)
  {
    //lineaddr = (line * page_size) + line;
    lineaddr = (line * page_size);
  }
  
  for (int b = 0; b <= page_size; b++)
  {
    a = eeprom.ReadByte(lineaddr);
    newbuffer[b] = (char)a;
    
    lineaddr++;
  }
  
  /*
  Serial.print("line = ");
  Serial.println(line);
  
  Serial.print("addr = ");
  Serial.println(addr);
  
  Serial.print("'");
  Serial.print(newbuffer);
  Serial.println("'");
  */
  return newbuffer;
}

void showEeprom(int menuItems, int offset = 0)
{
  int menuPosition = 0;
  int buttonClick = 0;
  
  char buffer0[17];
  char buffer1[17];
  
  while (1)
  {
    if (buttonClick == 1) buttonClick = 0;
    
    strcpy(buffer0, eepromGetLine(menuPosition));
    lcdPrint(offset, 0, buffer0, true);
    
    if ((menuPosition+1) < menuItems)
    {
      strcpy(buffer1, eepromGetLine(menuPosition+1));
      lcdPrint(offset, 1, buffer1);
    }
    
    delay(SHORT_DELAY);
    
    while (buttonClick == 0)
    {
      readButtons();
      
      // down button
      if (buttons[buttonDown] == HIGH)
      {
        if (menuPosition < (menuItems - 1))
        {
          menuPosition++;
          buttonClick = 1;
        }
      }
      
      // up button
      if (buttons[buttonUp] == HIGH)
      {
        if (menuPosition > 0)
        {
          menuPosition--;
          buttonClick = 1;
        }
      }
      
      // back button
      if (buttons[buttonBack] == HIGH) return;
    }
  }
}

// display about pages
void aboutMenu()
{
  int menuPosition = 0;
  int buttonClick = 0;
  //int menuItems = 7;
  int menuItems = 2;
  
  while (1)
  {
    if (buttonClick == 1) buttonClick = 0;
    
    switch (menuPosition)
    {
      case 0:
        sprintf(line0, "PANT Version");
        sprintf(line1, "%s", PANT_VERSION);
        break;
      
      case 1:
        sprintf(line0, "More Info");
        sprintf(line1, INFO_URL);
        break;
      
      /*  
        sprintf(line0, "Designed by");
        sprintf(line1, AUTHOR);
        break;
        
      case 2:
        sprintf(line0, EMAIL1);
        sprintf(line1, EMAIL2);
        break;
        
      case 3:
        sprintf(line0, "Other code by");
        sprintf(line1, COAUTHOR);
        break;
        
      case 4:
        sprintf(line0, COEMAIL1);
        sprintf(line1, COEMAIL2);
        break;
        
      case 5:
        sprintf(line0, "Other code by");
        sprintf(line1, COAUTHOR2);
        break;
        
      case 6:
        sprintf(line0, COEMAIL3);
        sprintf(line1, COEMAIL4);
        break;
      */
    }
    
    lcdPrint(0, 0, line0, true);
    lcdPrint(0, 1, line1);
    
    delay(SHORT_DELAY);
    
    while (buttonClick == 0)
    {
      readButtons();
      
      // down button
      if (buttons[buttonDown] == HIGH)
      {
        if (menuPosition < (menuItems - 1))
        {
          menuPosition++;
          buttonClick = 1;
        }
      }
      
      // up button
      if (buttons[buttonUp] == HIGH)
      {
        if (menuPosition > 0)
        {
          menuPosition--;
          buttonClick = 1;
        }
      }
      
      // back button
      if (buttons[buttonBack] == HIGH) return;
    }
  }
}

// calculate subnet based on subnet mask and ip address
// written by Eric Brundick <spirilis@linux.com>
// modified by Mike Cherry <mcherry@inditech.org>
void iplist_define(byte ipaddr[], byte subnetmask[])
{
  int i;
  
  for (i=31; i>=0; i--)
  {
    if (subnetmask[i/8] & (1 << (7-i%8)))
    {
      current = 0;
     
      netsize = ((unsigned long)1) << (31-i);
     
      for (int a = 0; a <= 3; a++) subnet[a] = ipaddr[a] & subnetmask[a];
      
      return;
    }
  }
}

// get the next IP address in the range
// written by Eric Brundick <spirilis@linux.com>
// modified by Mike Cherry <mcherry@inditech.org>
boolean iplist_next(byte nextip[])
{
  if (current < netsize) {
    for (int a = 0; a <= 3; a++) nextip[a] = subnet[a];
 
    if (current & 0x000000FF)
    {
      nextip[3] |= (byte) (current & 0x000000FF);
    }
    if (current & 0x0000FF00)
    {
      nextip[2] |= (byte) ((current & 0x0000FF00) >> 8);
    }
    if (current & 0x00FF0000)
    {
      nextip[1] |= (byte) ((current & 0x00FF0000) >> 16);
    }
   
    if (current & 0xFF000000)
    {
      nextip[0] |= (byte) ((current & 0xFF000000) >> 24);
    }
    
    current++;
    
    // dont want to ping the network or broadcast address
    if (nextip[3] == 0x00 || nextip[3] == 0xFF)
    {
      return iplist_next(nextip);
    }
 
    return true;
  }
 
  return false;
}

// enable user to enter an IP address
// pressing back with cursor under first octet returns an ip with 0 for the first octet (canceled)
// pressing select with cursor under last octet returns the ip that was input (selected ip)
IPAddress ipInput(IPAddress ip, char label[], boolean isNetmask = false)
{
  int octetPos = 0;
  int netMaskMax = 254;
  
  if (isNetmask == true)
  {
    netMaskMax = 255;
  }
  
  //byte ip[] = { startip[0], startip[1], startip[2], startip[3] };
  
  sprintf(line1, "%03d.%03d.%03d.%03d", ip[0], ip[1], ip[2], ip[3]);
  lcdPrint(0, 0, label, true);
  lcdPrint(0, 1, line1);
  
  //lcdPrint(2, 1, "^");
  lcd.setCursor(2, 1);
  lcd.cursor();
  lcd.blink();
  
  delay(SHORT_DELAY);
  
  while (1)
  {
    readButtons();
    
    if (buttons[buttonUp] == HIGH)
    {
      if (ip[octetPos] < netMaskMax)
      {
        ip[octetPos]++;
      }
      else
      {
        if (octetPos == 0)
        {
          ip[octetPos] = 1;
        }
        else
        {
          ip[octetPos] = 0;
        }
      }
      
      sprintf(line1, "%03d", ip[octetPos]);
      lcdPrint((octetPos+2*(octetPos+1)+octetPos)-2, 1, line1);
      
      delay(MICRO_DELAY);
    }
    
    if (buttons[buttonDown] == HIGH)
    {
      if (ip[octetPos] > 0)
      {
        ip[octetPos]--;
      }
      else
      {
        ip[octetPos] = netMaskMax;
      }
      
      sprintf(line1, "%03d", ip[octetPos]);
      lcdPrint((octetPos+2*(octetPos+1)+octetPos)-2, 1, line1);
      delay(MICRO_DELAY);
    }
    
    if (buttons[buttonSelect] == HIGH)
    {
      if (octetPos == 3)
      {
        lcd.noBlink();
        lcd.noCursor();
        return ip;
      }
      
      if (octetPos < 4)
      {
        if (octetPos != 3)
        {
          octetPos++;
          //lcdPrint(0, 1, BLANK);  
        }
        
        //lcdPrint(octetPos+2*(octetPos+1)+octetPos, 1, "^");
        lcd.setCursor(octetPos+2*(octetPos+1)+octetPos, 1);
        delay(SHORT_DELAY);
      }
    }
    
    if (buttons[buttonBack] == HIGH)
    {
      if (octetPos == 0)
      {
        ip[0] = 0;
        
        lcd.noBlink();
        lcd.noCursor();
        
        return ip;
      }
      
      octetPos--;
      
      //lcdPrint(0, 1, BLANK);
      //lcdPrint(octetPos+2*(octetPos+1)+octetPos, 1, "^");
      lcd.setCursor(octetPos+2*(octetPos+1)+octetPos, 1);
      //lcd.cursor();
      
      delay(SHORT_DELAY);
    }
  }
}

// discover hosts responding to ping on the network
// based on netmask and ip address
int hostDiscovery()
{
  boolean iptest, hostcheck;
  
  unsigned long hostcount = 0;
  unsigned long pingedhosts = 0;
  unsigned long startaddr = 0;
  
  byte validIp[4];
  
  char buffer0[17];
  //char buffer1[17];
  
  // convert IPAddress's into byte arrays
  byte IPAsByte[] = { myLocalIp[0], myLocalIp[1], myLocalIp[2], myLocalIp[3] };
  //byte NMAsByte[] = { mySubnetMask[0], mySubnetMask[1], mySubnetMask[2], mySubnetMask[3] };
  byte NMAsByte[4];
  
  IPAddress subnetmask_default = ipInput(mySubnetMask, "Netmask", true);
  if (subnetmask_default[0] == 0)
  {
    return 0;
  }
  else
  {
    NMAsByte[0] = subnetmask_default[0];
    NMAsByte[1] = subnetmask_default[1];
    NMAsByte[2] = subnetmask_default[2];
    NMAsByte[3] = subnetmask_default[3];
  }
  
  
  // setup the netrange list
  iplist_define(IPAsByte, NMAsByte);
  
  lcdPrint(0, 0, "Found", true);
  
  // try to get the first ip in the range
  while (iplist_next(validIp) == true)
  {
    // process back button to cancel search
    readButtons();
    
    if (buttons[buttonBack] == HIGH)
    {
      lcdPrint(0, 1, CANCEL);
       
      delay(MEDIUM_DELAY);
     
      break;
    }
    
    sprintf(line1, "%d.%d.%d.%d", validIp[0], validIp[1], validIp[2], validIp[3]);
    sprintf(buffer0, "%-16s", line1);
    
    lcdPrint(0, 1, buffer0);
  
    ICMPPing ping(pingSocket);
    // ping a host
    ping(1, validIp, pingBuffer);
  
    if (strncmp(pingBuffer, "T", 1) != 0)
    {
      // timeout
      hostcount++;
      sprintf(line0, "%d", hostcount);
      lcdPrint(6, 0, line0);
      
      //sprintf(buffer0, "%d.%d.%d.%d", validIp[0], validIp[1], validIp[2], validIp[3]);
      //sprintf(buffer1, "%-16s", buffer0);
      
      if (startaddr <= 8000)
      {
        eeprom.WritePage(startaddr, (byte *)buffer0, EEPROM_PAGESIZE);
        startaddr += EEPROM_PAGESIZE;
      }
    }
    
    pingedhosts++;      
  }
  
  // show total number of found hosts
  sprintf(line0, "Found %d of", hostcount);
  sprintf(line1, "%d hosts", pingedhosts);
  
  lcdPrint(0, 0, line0, true);
  lcdPrint(0, 1, line1);

  delay(LONG_DELAY);
  
  //showEeprom(hostcount);
  // wait for user to press back button  
  /*
  while (1)
  {
    readButtons();
    
    if (buttons[buttonBack] == HIGH) return;
  }
  */
  
  //delay(MICRO_DELAY);
  return hostcount;
}

int portScanner()
{
  IPAddress ip;
  int addr = 0;
  //char buffer[17];
  
  //byte portList[1024];
  
  int portCount = 0;
  
  ip = ipInput(myLocalIp, "IP Addr");
  if (ip[0] == 0) return 0;
  
  sprintf(line0, "%02d.%02d.%02d.%02d", ip[0], ip[1], ip[2], ip[3]);
  lcdPrint(0, 0, line0, true);
  
  for (int a = 0; a <= 1023; a++)
  {
    readButtons();
    
    if (buttons[buttonBack] == HIGH)
    {
      lcdPrint(0, 1, CANCEL);
       
      delay(MEDIUM_DELAY);
      
      break;
    }
    
    sprintf(line1, "Trying port %d", a+1);
    lcdPrint(0, 1, line1);
  
    if (client.connect(ip, a+1))
    {
      client.stop();
      
      sprintf(line0, "Port %d", a+1);
      sprintf(line1, "%-16s", line0);
      
      //Serial.print("'");
      //Serial.print(line1);
      //Serial.println("'");
      //portList[a] = 1;
      
      eeprom.WritePage(addr, (byte *)line1, EEPROM_PAGESIZE);
      //addr += 17;
      addr += EEPROM_PAGESIZE;
      
      portCount++;
    }
  }
  
  sprintf(line0, "Found %d", portCount, true);
  lcdPrint(0, 0, line0, true);
  lcdPrint(0, 1, "open ports");
  
  //if (portCount > 0)
  //{
    //delay(LONG_DELAY);
    
    //showEeprom(portCount);
  //}
  
  //while (1)
  //{
  //  readButtons();
    
  //  if (buttons[buttonBack] == HIGH) return;
  //}
  delay(LONG_DELAY);
  return portCount;
}

// display main menu
void mainMenu()
{
  //char *MainMenu[6] = { "Information", "Diagnostics", "Host Discovery", "Port Scanner", "Restart", "About" };
  char *MainMenu[5] = { "Information", "Diagnostics", "Host Discovery", "Port Scanner", "About" };
  unsigned long hostcount = 0;
  unsigned long portCount = 0;
  
  printMenu(MainMenu, 5);
  
  while (1)
  {
    readButtons();
     
    // up button
    if (buttons[buttonUp] == HIGH) CursorPrevious(MainMenu, 5);
    
    // down button
    if (buttons[buttonDown] == HIGH) CursorNext(MainMenu, 5);
    
    // select button
    if (buttons[buttonSelect] == HIGH)
    {
      switch (CurrentMenuItem)
      {
        case 0:
          infoMenu();
          break;
          
        case 1:
          diagMenu();
          break;
          
        case 2:
          hostcount = hostDiscovery();
          
          if (hostcount > 0)
          {
            showEeprom(hostcount);
          }
          
          break;
          
        case 3:
          portCount = portScanner();
          
          if (portCount > 0)
          {
            showEeprom(portCount);
          }
          
          break;
          
        //case 4:
        //  softReset();
        //  break;
          
        case 4:
          aboutMenu();
          break;
      }
         
      printMenu(MainMenu, 5);
    }
  }
}

// display main menu
/*
void superSecretMenu()
{
  char *SuperSecretMenu[2] = { "Drain Battery", "Restart" };
  
  printMenu(SuperSecretMenu, 2);
  
  while (1)
  {
    readButtons();
     
    // up button
    if (buttons[buttonUp] == HIGH) CursorPrevious(SuperSecretMenu, 2);
    
    // down button
    if (buttons[buttonDown] == HIGH) CursorNext(SuperSecretMenu, 2);
    
    // select button
    if (buttons[buttonSelect] == HIGH)
    {
      switch (CurrentMenuItem)
      {
        case 0:
          drainBattery();
          break;
          
        case 1:
          softReset();
          break;
      }
         
      printMenu(SuperSecretMenu, 2);
    }
  }
}

void drainBattery()
{
  int a = 0;
  char b[2] = " ";
  
  sprintf(line0, BLANK);
  
  while (1)
  {
    readButtons();
     
    // back
    if (buttons[buttonBack] == HIGH) return;
    
    b[0] = random(32, 255);
    lcdPrint(random(0, 16), a, b);
    
    a = 1 - a;
    
    delay(MICRO_DELAY);
  }
}
*/

/*
// this is some reworked code originally sourced from
// http://www.avdweb.nl/arduino/hardware-interfacing/temperature-measurement.html
int readAdc()
{ 
  ADCSRA |= _BV(ADSC); // start the conversion 
  while (bit_is_set(ADCSRA, ADSC)); // ADSC is cleared when the conversion finishes   
  return (ADCL | (ADCH << 8)); // combine bytes
}

// this is some reworked code originally sourced from
// http://www.avdweb.nl/arduino/hardware-interfacing/temperature-measurement.html
int chipTempF()
{
  // Calibration values, set in decimals
  static const float offset = 335.2; // change this!
  static const float gain = 1.06154;

  static const int samples = 1000; // must be >= 1000, else the gain setting has no effect

  // Compile time calculations
  static const long offsetFactor = offset * samples;
  static const int divideFactor = gain * samples/10; // deci = 1/10
  
  long averageTemp = 0;
  
  ADMUX = 0xC8;
  delay(10);
  readAdc();
  
  for (int i=0; i<samples; i++) averageTemp += readAdc();
  averageTemp -= offsetFactor;
  
  return ((9 * (averageTemp / divideFactor) + 1600) / 5) / 10;
}
*/

// read the state of the buttons, HIGH == pressed
void readButtons()
{
  buttons[buttonUp] = digitalRead(A3);
  buttons[buttonDown] = digitalRead(A0);
  buttons[buttonSelect] = digitalRead(A2);
  buttons[buttonBack] = digitalRead(A1);
}

void setup()
{
  //delay(LONG_DELAY*2);
  
  // set the button pins to input
  pinMode(buttonUp, INPUT);
  pinMode(buttonDown, INPUT);
  pinMode(buttonSelect, INPUT);
  pinMode(buttonBack, INPUT);

  //char data[] = "Port 666";
  //char somedata1[17];
  
  //sprintf(somedata1, "%-16s", data);
  
  // initialize 16x2 lcd
  lcd.begin(16, 2);
  
  //Serial.begin(9600);
  
  /*
  int records = 4;
  int addr = 0;
  byte c;
  char newbuffer[17];
  
  for (int a = 0; a <= (records-1); a++)
  {
    for (int b = 0; b <= 16; b++)
    {
      c = eeprom.ReadByte(addr);
      newbuffer[b] = (char)c;
      
      addr++;
    }
    
    Serial.println(newbuffer);
    
    Serial.print("* addr = ");
    Serial.println(addr);
  }
  */
  
  //for (int a = 0; a <= 5; a++)
  //{
  //  strcpy(line0, eepromGetLine(a));
  //  Serial.println(line0);
  //}
  
  //eeprom.WritePage(0, (byte *)somedata1, sizeof(somedata1));
  /*
  int addr = 0;
  //byte b = eeprom.ReadByte(0);
  byte b;
  
  Serial.print("'");
  
  for (int a = 0; a <= 16; a++)
  {
    b = eeprom.ReadByte(a);
    
    newbuffer[a] = (char)b;
  }
  
  Serial.print(newbuffer);
  */
  
  /*
  while (b != 0)
  {
    //strcat(buffer3, (char *)b);
    Serial.print((char)b);
    
    addr++;
    
    b = eeprom.ReadByte(addr);
  }
  */
  
  //Serial.println("'");
  
  // detect ultra super secret button combo
  /*
  readButtons();
  if ((buttons[buttonUp] == LOW) && (buttons[buttonDown] == HIGH) && (buttons[buttonSelect] == LOW) && (buttons[buttonBack] == HIGH))
  {
    int a;
    
    lcdPrint(0, 0, " PC LOAD LETTER", true);
    lcdPrint(0, 1, "[");
    lcdPrint(15, 1, "]");
    
    for (a = 0; a < 15; a++)
    {
      lcdPrint(a+1, 1, "*");
      delay(75);
    }
    
    readButtons();
    if ((buttons[buttonUp] == LOW) && (buttons[buttonDown] == LOW) && (buttons[buttonSelect] == LOW) && (buttons[buttonBack] == HIGH))
    {
      superSecretMenu();
    }
  }
  */
}

void loop()
{ 
  if (ethernetActive == 0)
  {
    // ethernet isnt active so lets get this show on the road
    lcdPrint(0, 0, "Requesting IP", true);
     
    if (Ethernet.begin(mac) != 0)
    {
      // ethernet is active now
      ethernetActive = 1;
      
      // waiting 1 second to let ethernet completely initialize
      // ive seen this in other code so i put it here to be safe
      delay(1000);
      
      myLocalIp = Ethernet.localIP();
      mySubnetMask = Ethernet.subnetMask();
      
      // set connection timeout and retry count
      // 0x07D0 == 2000
      // 0x320  == 800
      // 0x1F4  == 500
      W5100.setRetransmissionTime(0x1F4);
      W5100.setRetransmissionCount(1);
    }
    else
    {
      // ethernet failed to initialize, we will keep trying
      lcdPrint(0, 1, "No DHCP");
    }
  }
  else
  {
    // if everything seems good to go, display the main menu
    mainMenu(); 
  }
}

//void softReset()
//{
//  asm volatile ("  jmp 0");
//}
