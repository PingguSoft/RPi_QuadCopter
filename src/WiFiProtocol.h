/*
 This project is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 see <http://www.gnu.org/licenses/>
*/


#ifndef _WIFI_PROTOCOL_H_
#define _WIFI_PROTOCOL_H_

#include <stdarg.h>
#include "common.h"

#define MAX_PACKET_SIZE 512

class WiFiProtocol
{

public:
    enum {
        WIFI_CODE_STATUS    = 0,
        WIFI_CODE_REDIRECT  = 1,
        WIFI_CODE_CMD       = 2,
    };

    enum {
        WIFI_CMD_START_VIDEO = 10,
        WIFI_CMD_STOP_VIDEO  = 11,
    };
    
    WiFiProtocol(int port, bool redirect=TRUE);
    ~WiFiProtocol();

    int  startServer(void);
    void stopServer(void);
    void sendCmd(u8 cmd, u8 *data, u8 size);
    int  sendPacket(u8 *data, int len);
    void sendResponse(bool ok, u8 cmd, u8 *data, u8 size);
    void setCallback(u32 (*callback)(u8 code, u8 cmd, u8 *data, u8 size));
    bool isRedirect(void)               { return mBoolRedirect; }
    void enableRedirect(bool redirect)  { mBoolRedirect = redirect; }

private:
    typedef enum
    {
        STATE_IDLE,
        STATE_HEADER_START,
        STATE_HEADER_M,
        STATE_HEADER_ARROW,
        STATE_HEADER_SIZE,
        STATE_HEADER_CMD
    } STATE_T;

    //
    void handleRX(u8 *data, int size);
    void evalCommand(u8 cmd, u8 *data, u8 size);
    void sendMSP(u8 sort, u8 bCmd, u8 *data, int len);
    static void* RXThread(void *arg);

    // variables
    pthread_t mThreadRx;
    volatile bool mBoolRedirect;
    volatile bool mBoolFinish;
    int  mSockServer;
    int  mSockClient;
    int  mSockPort;

    u8   mRxPacket[MAX_PACKET_SIZE];

    u8   mState;
    u8   mOffset;
    u8   mDataSize;
    u8   mCheckSum;
    u8   mCmd;
    u32  (*mCallback)(u8 code, u8 cmd, u8 *data, u8 size);
};

#endif

