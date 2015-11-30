"""
This example uses lower-level PWM control methods of RPIO.PWM. The default
settings include a subcycle time of 20ms and a pulse-width increment
granularity of 10us.

RPIO Documentation: http://pythonhosted.org/RPIO
"""
import RPIO.PWM as PWM
import time

GPIO_R = 17
GPIO_G = 18
GPIO_B = 27
GPIO_BUZ = 22
CHANNEL = 0

PERIOD_CH0 = 4000    # 4ms = 250Hz
PERIOD_CH1 = 4000    # 4ms = 250Hz

PWM.set_loglevel(PWM.LOG_LEVEL_DEBUG)

PWM.setup(10, 0)
PWM.init_channel(0, PERIOD_CH0)
#PWM.init_channel(1, PERIOD_CH1)
PWM.print_channel(0)
#PWM.print_channel(1)

list_R = [255, 255, 255,   0,   0,   0, 255]
list_G = [  0, 128, 255, 255,   0,   0,   0]
list_B = [  0,   0,   0,   0, 255, 130, 255]

r = 0
g = 0
b = 0
k = PERIOD_CH0 - 10.0

#PWM.add_channel_pulse(1, GPIO_BUZ, 0, 390)
while 1:
    for i in range(0, 7):
        r = int(k * list_R[i] / 255.0) / 10
        g = int(k * list_G[i] / 255.0) / 10
        b = int(k * list_B[i] / 255.0) / 10

        r = max(r, 10)
        g = max(g, 10)
        b = max(b, 10)

        print "------------------------------------------------------"
        print "r=" + repr(list_R[i]) + ", g=" + repr(list_G[i]) + ", b=" + repr(list_B[i])
        print "r=" + repr(r) + ", g=" + repr(g) + ", b=" + repr(b)
        print " "
        PWM.add_channel_pulse(0, GPIO_R, 0, r)
        PWM.add_channel_pulse(0, GPIO_G, 0, g)
        PWM.add_channel_pulse(0, GPIO_B, 0, b)
        time.sleep(0.3)
