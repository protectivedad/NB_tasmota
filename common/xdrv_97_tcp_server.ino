/*
  xdrv_97_tcp_server.ino - TCP server

  Copyright (C) 2021  Nicolas Bernaerts

  This TCP server sends raw TIC message on a TCP port
  It is then possible to publish the teleinfo stream on your LAN
  You can start and stop the server from the console : 
    - start TCP server on port 8888 : tcp_start 8888
    - stop TCP server               : tcp_stop

  From any linux or raspberry, you can retrieve the teleinfo stream with
    # nc 192.168.x.x 8888

  Version history :
    04/08/2021 - v1.0   - Creation

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FIRMWARE_SAFEBOOT
#ifdef USE_TCPSERVER

#define XDRV_97                   97

#define TCP_CLIENT_MAX            2              // maximum number of TCP connexion
#define TCP_DATA_MAX              64             // buffer size

// TCP - MQTT commands
const char kTCPServerCommands[] PROGMEM = "tcp_" "|" "help" "|" "start" "|" "stop";
void (* const TCPServerCommand[])(void) PROGMEM = { &CmndTCPHelp, &CmndTCPStart, &CmndTCPStop };

/****************************************\
 *        Class TeleinfoTCPServer
\****************************************/


class TCPServer
{
public:
  TCPServer ();
  bool start (int port);
  bool stop ();
  void send (char to_send);
  void manage_client ();

private:
  WiFiServer *server;                       // TCP server pointer
  WiFiClient client[TCP_CLIENT_MAX];        // TCP clients
  uint8_t    client_next;                   // next client slot
  char       buffer[TCP_DATA_MAX];          // data transfer buffer
  uint8_t    buffer_index;                  // buffer current index
};

TCPServer::TCPServer ()
{
  server = nullptr; 
  client_next = 0;
  buffer_index = 0;
}

bool TCPServer::stop ()
{
  bool cmd_done = false;
  int  index; 

  // if already running, stop previous server
  if (server != nullptr)
  {
    // kill server
    server->stop ();
    delete server;
    server = nullptr;

    // stop all clients
    for (index = 0; index < TCP_CLIENT_MAX; index++) client[index].stop ();

    // validate and log
    cmd_done = true;
    AddLog (LOG_LEVEL_INFO, PSTR ("TCP: Stopping TCP server"));
  }

  return cmd_done;
}

bool TCPServer::start (int port)
{
  bool cmd_done = false;

  // if port defined, start new server
  if (port > 0)
  {
    // if server already started, stop it
    if (server != nullptr) stop ();

    // create and start server
    server = new WiFiServer (port);
    server->begin ();
    server->setNoDelay (true);

    // reset buffer index
    buffer_index = 0;

    // validate and log
    cmd_done = true;
    AddLog (LOG_LEVEL_INFO, PSTR ("TCP: Starting TCP server on port %d"), port);
  }

  return cmd_done;
}

// TCP server data send
void TCPServer::send (char to_send)
{
  int index;

  // if TCP server is active, append character to TCP buffer
  if (server != nullptr)
  {
    // append caracter to buffer
    buffer[buffer_index++] = to_send;

    // if end of line or buffer size reached
    if ((buffer_index == sizeof (buffer)) || (to_send == 13))
    {
      // send data to connected clients
      for (index = 0; index < TCP_CLIENT_MAX; index++)
        if (client[index]) client[index].write (buffer, buffer_index);

      // reset buffer index
      buffer_index = 0;
    }
  } 
}

// TCP server client connexion management
void TCPServer::manage_client ()
{
  int index;

  // check for a new client connection
  if (server != nullptr)
  {
    if (server->hasClient ())
    {
      // find an empty slot
      for (index = 0; index < TCP_CLIENT_MAX; index++)
        if (!client[index])
        {
          client[index] = server->available ();
          break;
        }

      // if no empty slot, kill oldest one
      if (index >= TCP_CLIENT_MAX)
      {
        index = client_next++ % TCP_CLIENT_MAX;
        client[index].stop ();
        client[index] = server->available ();
      }
    }
  }
}

// TCP server instance
TCPServer tcp_server;

/***********************************************************\
 *                      Commands
\***********************************************************/

// TCP stream help
void CmndTCPHelp ()
{
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: TCP Server commands :"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tcp_start xxxx = start stream on port xxxx"));
  AddLog (LOG_LEVEL_INFO, PSTR (" - tcp_stop       = stop stream"));
  ResponseCmndDone();
}

// Start TCP server
void CmndTCPStart (void)
{
  bool result = false;

  if ((XdrvMailbox.data_len > 0) && (XdrvMailbox.payload > 0)) result = tcp_server.start (XdrvMailbox.payload);
 
  if (result) ResponseCmndDone (); else ResponseCmndFailed ();
}

// Stop TCP server
void CmndTCPStop (void)
{
  bool result = false;

  result = tcp_server.stop ();

  if (result) ResponseCmndDone (); else ResponseCmndFailed ();
}

/***********************************************************\
 *                      Functions
\***********************************************************/

// init main status
void TCPInit ()
{
  // log help command
  AddLog (LOG_LEVEL_INFO, PSTR ("HLP: tcp_help to get help on TCP Server commands"));
}

/***********************************************************\
 *                      Interface
\***********************************************************/

bool Xdrv97 (uint8_t function)
{
  bool result = false;

  // main callback switch
  switch (function)
  { 
    case FUNC_INIT:
      TCPInit ();
      break;
    case FUNC_COMMAND:
      result = DecodeCommand (kTCPServerCommands, TCPServerCommand);
      break;
    case FUNC_LOOP:
      tcp_server.manage_client ();
      break;
  }
  
  return result;
}

#endif    // USE_TCPSERVER
#endif    // FIRMWARE_SAFEBOOT

