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

const int VIDEO_RTSP  = 0;
const int VIDEO_MJPEG = 1;

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
static int mRTSPPort = 0;

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

u32 cbSerialRX(bool ok, u8 cmd, u8 * data, u8 size)
{
    //printf("SerialRX packet : %d (%d)\n", cmd, size);
    if (mWiFi)
        mWiFi->sendResponse(ok, cmd, data, size);
}

const static char *szH264[] = {
    (char*)"baseline",
    (char*)"high",
    (char*)"main"
};

const static char *szEXP[] = {
    (char*)"auto",
    (char*)"antishake",
    (char*)"backlight",
    (char*)"beach",
    (char*)"fireworks",
    (char*)"fixedfps",
    (char*)"night",
    (char*)"nightpreview",
    (char*)"snow",
    (char*)"sports",
    (char*)"spotlight",
    (char*)"verylong"
};

const static char *szAWB[] = {
    (char*)"auto",
    (char*)"cloudy",
    (char*)"flash",
    (char*)"fluorescent",
    (char*)"horizon",
    (char*)"incandescent",
    (char*)"off",
    (char*)"shade",
    (char*)"sun",
    (char*)"tungsten",
};

const static char *szFX[] = {
    (char*)"none",
    (char*)"blur",
    (char*)"cartoon",
    (char*)"colourbalance",
    (char*)"colourpoint",
    (char*)"colourswap",
    (char*)"denoise",
    (char*)"emboss",
    (char*)"film",
    (char*)"gpen",
    (char*)"hatch",
    (char*)"negative",
    (char*)"oilpaint",
    (char*)"pastel",
    (char*)"posterise",
    (char*)"saturation",
    (char*)"sketch",
    (char*)"solarise",
    (char*)"washedout",
    (char*)"watercolour"
};

const static char *szCmdsKill[] = {
    (char*)"pkill -2 -f h264_v4l2_rtspserver",
//    (char*)"fuser -k -n tcp 8554",
    (char*)"pkill uv4l",
	(char*)"pkill -2 -f mjpg_streamer"
};

void killVideo(void)
{
    for (int i = 0; i < sizeof(szCmdsKill) / sizeof(char*); i++) {
        printf("CMD:%s\n", szCmdsKill[i]);
        system(szCmdsKill[i]);
        usleep(500000);
    }
}


const static char *szUV4L =
    (char*)"uv4l --auto-video_nr --sched-rr --driver raspicam --encoding=h264 --nopreview "
    "--vflip --hflip --profile=%s --width=%d --height=%d --framerate=%d --bitrate=%d --awb=%s --exposure=%s "
    "--imgfx=%s --custom-sensor-config=%d";

const static char *szRTSP =
    (char*)"su - pi -c '/home/pi/sw/QuadCopter/h264_v4l2_rtspserver /dev/video0 -P %d "
    "-W %d -H %d -F %d &'";

	
//uv4l --auto-video_nr --sched-rr --driver raspicam --encoding=mjpeg --width=640 --height=480 --hflip --vflip --framerate=30 --nopreview
//./mjpg_streamer -o "output_http.so -w ./www" -i "input_raspicam.so -x 640 -y 480 -fps 25 -vf -hf -quality 70"
//./mjpg_streamer -o "./output_http.so -w ./www" -i "./input_dev.so -dev /dev/video0"


const static char *szMJPGPath = "/home/pi/sw/QuadCopter/mjpg-streamer-experimental";
const static char *szMJPEG = 
	(char*)"su - pi -c '%s/mjpg_streamer "
	"-o \"%s/output_http.so -w %s/www -p %d\" -i \"%s/input_raspicam.so -x %d -y %d -fps %d -vf -hf -quality 70 "
	"-awb %s -ex %s -ifx %s\" &'";
	
int startVideo(int mode, int pro, int width, int height, int bitrate, int fps, int awb, int exp, int fx)
{
    int  ret;
    char szBuf[512];
	int  sensor_config = 0;

/*
	0 for normal mode,
	1 for 1080P30 cropped 1-30fps mode,
	2 for 5MPix 1-15fps mode,
	3 for 5MPix 0.1666-1fps mode,
	4 for 2x2 binned 1296x972 1-42fps mode,
	5 for 2x2 binned 1296x730 1-49fps mode,
	6 for VGA 30-60fps mode,
	7 for VGA 60-90fps mode	
*/

    int port = 7790 + (mRTSPPort++ % 9);
	
	if (mode == VIDEO_RTSP) {
		if (height < 1080)
			sensor_config = 5;
		else if (height < 720)
			sensor_config = 7;

		sprintf(szBuf, szUV4L, szH264[pro], width, height, fps, bitrate,
			szAWB[awb], szEXP[exp], szFX[fx], sensor_config);
		ret = system(szBuf);
		printf("CMD RTSP:%s\n", szBuf);
		
		sprintf(szBuf, szRTSP, port, width, height, fps);
		ret = system(szBuf);
		printf("CMD RTSP:%s\n", szBuf);
	} else {
		sprintf(szBuf, szMJPEG, szMJPGPath, szMJPGPath, szMJPGPath,
				port, szMJPGPath, width, height, fps, szAWB[awb], szEXP[exp], szFX[fx]);
		ret = system(szBuf);
		printf("CMD:%s\n", szBuf);
	}

    return port;
}

typedef struct __attribute__ ((__packed__)) {
	u8  vidmode;
    u16 width;
    u16 height;
    u32 bitrate;
    u8  prof;
    u8  fps;
    u8  awb;
    u8  exp;
    u8  fx;
} ENC_T;

u32 cbWiFiRX(u8 cmd, u8 *data, u8 size)
{
    int state;
    int ret;
    u16 kk;


    //printf("WiFiRX packet : %d (%d)\n", cmd, size);

    switch (cmd) {
        case WiFiProtocol::WIFI_STATUS_CONNECT:
            state = *((int*)data);
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
            break;

        case WiFiProtocol::WIFI_CMD_START_VIDEO: {
            ENC_T *pEnc = (ENC_T *)data;

            int port = startVideo(pEnc->vidmode, pEnc->prof, pEnc->width, pEnc->height,
                pEnc->bitrate, pEnc->fps, pEnc->awb, pEnc->exp, pEnc->fx);
            if (mWiFi)
                mWiFi->sendResponse(TRUE, cmd, (u8*)&port, sizeof(port));
            }
            break;

        case WiFiProtocol::WIFI_CMD_STOP_VIDEO: {
            killVideo();
            if (mWiFi)
                mWiFi->sendResponse(TRUE, cmd, NULL, 0);
            }
            break;

        default:
            if (mMSP)
                mMSP->sendCmd(cmd, data, size);
            break;
    }
}


int main(int argc, char* argv[]) {

    if(signal(SIGINT, signal_handler) == SIG_ERR) {
        printf("could not register signal handler\n");
        return -1;
    }

//    startVideo(1, 1920, 1080, 8000000, 25, 0, 0, 0);

    initPWM();

    mMSP = new SerialProtocol(115200);
    mMSP->setCallback(cbSerialRX);
    mWiFi = new WiFiProtocol(7770);
    if (mWiFi->startServer() < 0) {
        printf("SERVER FAILS!!!\n");
        return -1;
    }

    mWiFi->setCallback(cbWiFiRX);

    while(1) {
        setRGB(mR, mG, mB);
        usleep(mPeriod);
        setRGB(0, 0, 0);
        usleep(mPeriod);
    }
}
