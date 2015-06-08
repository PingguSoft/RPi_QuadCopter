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
#include "SerialProtocol.h"

#define MAX_BUF_SIZE  (MAX_PACKET_SIZE + 10)
#define SORT_CMD	  0
#define SORT_RESP_OK  1
#define SORT_RESP_NO  2

void *SerialProtocol::RXThread(void *arg)
{
    u8  buf[64];
    int count;
	fd_set read_fds;
	SerialProtocol *parent = (SerialProtocol*)arg;

	FD_ZERO(&read_fds);
	while (!parent->mBoolFinish) {
		FD_SET(parent->mHandle, &read_fds);
		select(parent->mHandle + 1, &read_fds, NULL, NULL, NULL);

		if (FD_ISSET(parent->mHandle, &read_fds)) {
            ioctl(parent->mHandle, FIONREAD, &count);
            count = (count < sizeof(buf)) ? count : sizeof(buf);
			read(parent->mHandle, buf, count);
			parent->handleRX(buf, count);
		}
	}

    return NULL;
}

SerialProtocol::SerialProtocol(int baud)
{
	struct termios ios;

    mHandle = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY);
    if (mHandle == -1) {
        printf("can not open ios port\n");
        return;
    }

    if (tcgetattr(mHandle, &ios) < 0) {
        perror("Getting configuration");
        return;
    }

    // Set up Serial Configuration
    ios.c_iflag = 0;
    ios.c_oflag = 0;
    ios.c_lflag = 0;
    ios.c_cflag = 0;
    ios.c_cc[VMIN] = 0;
    ios.c_cc[VTIME] = 0;
    ios.c_cflag = B115200 | CS8 | CREAD;
    tcsetattr(mHandle, TCSANOW, &ios);
	mBoolFinish = FALSE;
	mState = STATE_IDLE;
	pthread_create(&mThreadRx, NULL, &RXThread, this);
}

SerialProtocol::~SerialProtocol()
{
    printf("%s\n", __FUNCTION__);
	pthread_join(mThreadRx, NULL);
    close(mHandle);
}

void SerialProtocol::stopServer(void)
{
    mBoolFinish = true;
}

void SerialProtocol::setCallback(u32 (*callback)(u8 cmd, u8 *data, u8 size))
{
    mCallback = callback;
}

void SerialProtocol::sendMSP(u8 sort, u8 bCmd, u8 *data, int len)
{
	int  i;
	int  size = 6;
	int  pos  = 0;
	int  wcount;
	u8	 sig;
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
	wcount = write(mHandle, byteBuf, pos);
}

void SerialProtocol::sendResponse(bool ok, u8 cmd, u8 *data, u8 size)
{
	sendMSP(ok ? SORT_RESP_OK : SORT_RESP_NO, cmd, data, size);
}

void SerialProtocol::sendCmd(u8 cmd, u8 *data, u8 size)
{
	sendMSP(SORT_CMD, cmd, data, size);
}

void SerialProtocol::evalCommand(u8 cmd, u8 *data, u8 size)
{
	if (mCallback)
		(*mCallback)(cmd, data, size);
}

void SerialProtocol::handleRX(u8 *data, int size)
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
                if (ch == '>')
                    mState = STATE_HEADER_ARROW;
                else if (ch == '!')
                    mState = STATE_HEADER_ERR;
                else
                    mState = STATE_IDLE;
    			break;

            case STATE_HEADER_ERR:
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
