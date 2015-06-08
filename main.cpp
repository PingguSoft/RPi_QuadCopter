#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "common.h"
#include "SerialProtocol.h"
#include "WiFiProtocol.h"
#include "pwm.h"

#define MAX(a, b) ( (a) > (b) ? (a) : (b))

const int PERIOD_CH0 = 4000;
const int GPIO_R = 17;
const int GPIO_G = 27;
const int GPIO_B = 18;

static int list_R[] = {255, 255, 255,   0,   0,   0, 255};
static int list_G[] = {  0, 128, 255, 255,   0,   0,   0};
static int list_B[] = {  0,   0,   0,   0, 255, 130, 255};

static int mR  = 0;
static int mG  = 0;
static int mB  = 255;
static int mPeriod = 500000;

static int mRA = 0;
static int mGA = 0;
static int mBA = 0;

static SerialProtocol *mMSP;
static WiFiProtocol   *mWiFi;

void signal_handler(int sig)
{
	printf("CTRL+C !!\n");

    if (mMSP) {
        mMSP->stopServer();
//        delete mMSP;
    }

    if (mWiFi) {
        mWiFi->stopServer();
//        delete mWiFi;
    }

    shutdown();
    exit(0);
}

void setRGB(int r, int g, int b)
{
    int k = PERIOD_CH0 - 10;

    r = (k * r) / 255 / 10;
    g = (k * g) / 255 / 10;
    b = (k * b) / 255 / 10;

    if (r == 0) {
        if (mRA)
            clear_channel_gpio(0, GPIO_R);
        mRA = 0;
    }
    else {
        add_channel_pulse(0, GPIO_R, 0, r);
        mRA = 1;
    }

    if (g == 0) {
        if (mGA)
            clear_channel_gpio(0, GPIO_G);
        mGA = 0;
    }
    else {
        add_channel_pulse(0, GPIO_G, 0, g);
        mGA = 1;
    }

    if (b == 0) {
        if (mBA)
            clear_channel_gpio(0, GPIO_B);
        mBA = 0;
    }
    else {
        add_channel_pulse(0, GPIO_B, 0, b);
        mBA = 1;
    }

}

void initPWM(void)
{
    setup(PULSE_WIDTH_INCREMENT_GRANULARITY_US_DEFAULT, DELAY_VIA_PWM);
    init_channel(0, PERIOD_CH0);
}

u32 cbSerialRX(u8 cmd, u8 * data, u8 size)
{
    printf("SerialRX packet : %d (%d)\n", cmd, size);
    if (mWiFi)
        mWiFi->sendResponse(TRUE, cmd, data, size);
}

u32 cbWiFiRX(u8 cmd, u8 *data, u8 size)
{
    printf("WiFiRX packet : %d (%d)\n", cmd, size);

    if (cmd == 0) {
        int state = *((int*)data);
        if (state == 1) {
            mR = 255;
            mG = 255;
            mB = 0;
        }
        else {
            mR = 0;
            mG = 0;
            mB = 255;
        }
    } else {
        if (mMSP)
            mMSP->sendCmd(cmd, data, size);
    }
}


int main(int argc, char* argv[]) {

    if(signal(SIGINT, signal_handler) == SIG_ERR) {
        printf("could not register signal handler\n");
        return -1;
    }

    initPWM();

    mMSP = new SerialProtocol(115200);
    mMSP->setCallback(cbSerialRX);
    mWiFi = new WiFiProtocol(7777);
    mWiFi->setCallback(cbWiFiRX);

//	mMSP.sendCmd(MSP_IDENT, NULL, 0);
//	mMSP.sendCmd(MSP_STATUS, NULL, 0);

	while(1) {
        setRGB(mR, mG, mB);
        usleep(mPeriod);
        setRGB(0, 0, 0);
        usleep(mPeriod);
	}
}
