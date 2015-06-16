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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "WiFiProtocol.h"

#define MAX_BUF_SIZE  (MAX_PACKET_SIZE + 10)
#define SORT_CMD      0
#define SORT_RESP_OK  1
#define SORT_RESP_NO  2

void dump(u8 *data, int len)
{
    int i;

    for (i = 0; i < len ; i++) {
        if (i != 0 && (i % 16) == 0)
            printf("\n");
        printf("%02X (%c) ", *data, *data);
        data++;
    }
    printf("\n\n");
}

void *WiFiProtocol::RXThread(void *arg)
{
    u8     buf[64];
    int    count;
    int    clilen;
    int    state;
    fd_set read_fds;
    struct sockaddr_in cli_addr;
    WiFiProtocol *parent = (WiFiProtocol*)arg;

    FD_ZERO(&read_fds);

    while(!parent->mBoolFinish) {
        printf( "waiting for new client...\n" );
        parent->mSockClient = accept(parent->mSockServer, (struct sockaddr*)&cli_addr, (socklen_t*)&clilen);
        if (parent->mSockClient < 0 ) {
            printf("ERROR on accept : %d\n", parent->mSockClient);
        } else {
            state = 1;
            if (parent->mCallback)
                (*parent->mCallback)(WIFI_STATUS_CONNECT, (u8*)&state, sizeof(state));
        }

        printf("A connection has been accepted from %s port:%d\n",
               inet_ntoa((struct in_addr)cli_addr.sin_addr),
               ntohs(cli_addr.sin_port));

        while (!parent->mBoolFinish) {
            FD_SET(parent->mSockClient, &read_fds);
            state = select(parent->mSockClient + 1, &read_fds, NULL, NULL, NULL);

            if (FD_ISSET(parent->mSockClient, &read_fds)) {
                state = ioctl(parent->mSockClient, FIONREAD, &count);
                if (count > 0) {
                    count = (count < sizeof(buf)) ? count : sizeof(buf);
                    //printf("socket read : %d\n", count);
                    count = read(parent->mSockClient, buf, count);
                    //dump(buf, count);
                    parent->handleRX(buf, count);
                } else {
                    break;
                }
            }
        }
        state = 0;
        close(parent->mSockClient);

        if (parent->mCallback)
            (*parent->mCallback)(WIFI_STATUS_CONNECT, (u8*)&state, sizeof(state));
    }

    parent->mSockClient = -1;

    return NULL;
}

WiFiProtocol::WiFiProtocol(int port)
{
    mBoolFinish = FALSE;
    mState = STATE_IDLE;
    mSockPort = port;
}

WiFiProtocol::~WiFiProtocol()
{
    printf("%s\n", __FUNCTION__);
    pthread_join(mThreadRx, NULL);

    if (mSockClient > 0)
        close(mSockClient);
    if (mSockServer > 0)
        close(mSockServer);
}

int WiFiProtocol::startServer(void)
{
    int    err = 0;
    struct sockaddr_in serv_addr;

    mSockServer = socket(AF_INET, SOCK_STREAM, 0);
    if (mSockServer < 0) {
        printf("Socket creation error !!\n");
        return mSockServer;
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port        = htons(mSockPort);
    err = bind(mSockServer, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (err < 0) {
        printf("Binding Error !!\n");
        return err;
    }

    err = listen(mSockServer, 5);
    if (err < 0) {
        printf("Listening Error !!\n");
        return err;
    }
    pthread_create(&mThreadRx, NULL, &RXThread, this);
    return 0;
}

void WiFiProtocol::stopServer(void)
{
    mBoolFinish = true;
}

void WiFiProtocol::setCallback(u32 (*callback)(u8 cmd, u8 *data, u8 size))
{
    mCallback = callback;
}

void WiFiProtocol::sendMSP(u8 sort, u8 bCmd, u8 *data, int len)
{
    int  i;
    int  size = 6;
    int  pos  = 0;
    int  wcount;
    u8   sig;
    u8   pl_size;
    u8   bCheckSum = 0;
    u8   byteBuf[MAX_BUF_SIZE];

    if (data == NULL)
        len = 0;

    if (data != NULL)
        size = 6 + len;

    if (sort == SORT_CMD)
        sig = '<';
    else if (sort == SORT_RESP_OK)
        sig = '>';
    else
        sig = '!';

    byteBuf[pos++] = '$';
    byteBuf[pos++] = 'M';
    byteBuf[pos++] = sig;

    pl_size = (u8)(len & 0xFF);
    byteBuf[pos++] = pl_size;
    bCheckSum ^= pl_size;
    byteBuf[pos++] = bCmd;
    bCheckSum ^= bCmd;

    if (data != NULL) {
        for (i = 0; i < len; i++) {
            byteBuf[pos++] = *data;
            bCheckSum ^= *data++;
        }
    }
    byteBuf[pos++] = bCheckSum;
//  wcount = write(mSockClient, byteBuf, pos);
    wcount = send(mSockClient, byteBuf, pos, MSG_NOSIGNAL);
}

void WiFiProtocol::sendResponse(bool ok, u8 cmd, u8 *data, u8 size)
{
    sendMSP(ok ? SORT_RESP_OK : SORT_RESP_NO, cmd, data, size);
}

void WiFiProtocol::sendCmd(u8 cmd, u8 *data, u8 size)
{
    sendMSP(SORT_CMD, cmd, data, size);
}

void WiFiProtocol::evalCommand(u8 cmd, u8 *data, u8 size)
{
    if (mCallback)
        (*mCallback)(cmd, data, size);
}

void WiFiProtocol::handleRX(u8 *data, int size)
{
    u8  ch;

    for (int i = 0; i < size; i++) {
        ch = *data++;
        switch (mState) {
            case STATE_IDLE:
                if (ch == '$')
                    mState = STATE_HEADER_START;
                break;

            case STATE_HEADER_START:
                mState = (ch == 'M') ? STATE_HEADER_M : STATE_IDLE;
                break;

            case STATE_HEADER_M:
                mState = (ch == '<') ? STATE_HEADER_ARROW : STATE_IDLE;
                break;

            case STATE_HEADER_ARROW:
                if (ch > MAX_PACKET_SIZE) { // now we are expecting the payload size
                    mState = STATE_IDLE;
                    break;
                }
                mDataSize = ch;
                mCheckSum = ch;
                mOffset   = 0;
                mState    = STATE_HEADER_SIZE;
                break;

            case STATE_HEADER_SIZE:
                mCmd       = ch;
                mCheckSum ^= ch;
                mState     = STATE_HEADER_CMD;
                break;

            case STATE_HEADER_CMD:
                if (mOffset < mDataSize) {
                    mCheckSum           ^= ch;
                    mRxPacket[mOffset++] = ch;
                } else {
                    if (mCheckSum == ch)
                        evalCommand(mCmd, mRxPacket, mDataSize);
                    mState = STATE_IDLE;
                }
                break;
        }
    }
}
