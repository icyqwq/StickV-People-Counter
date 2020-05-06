#refer to http://blog.sipeed.com/p/675.html
import sensor
import image
from image import SEARCH_EX, SEARCH_DS
import lcd
import time
import KPU as kpu
from fpioa_manager import fm
import random
from machine import UART
from board import board_info
from fpioa_manager import fm

from Maix import I2S, GPIO
import audio
from Maix import GPIO
from fpioa_manager import *
fm.register(board_info.SPK_SD, fm.fpioa.GPIO0)
spk_sd=GPIO(GPIO.GPIO0, GPIO.OUT)
fm.register(board_info.SPK_DIN,fm.fpioa.I2S0_OUT_D1)
fm.register(board_info.SPK_BCLK,fm.fpioa.I2S0_SCLK)
fm.register(board_info.SPK_LRCLK,fm.fpioa.I2S0_WS)
wav_dev = I2S(I2S.DEVICE_0)

fm.register(34,fm.fpioa.UART1_TX)
fm.register(35,fm.fpioa.UART1_RX)
uart_out = UART(UART.UART1, 1500000, 8, None, 1, timeout=1000, read_buf_len=4096)

def GetRectArea(rect):
    return rect[2] * rect[3]

lcd.init()
lcd.rotation(2)
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)
sensor.run(1)
task = kpu.load(0x300000) # you need put model(face.kfpkg) in flash at address 0x300000
# task = kpu.load("/sd/face.kmodel")
anchor = (1.889, 2.5245, 2.9465, 3.94056, 3.99987, 5.3658, 5.155437, 6.92275, 6.718375, 9.01025)
threshold = 9000
flag = False
count = 0
gap = 0
a = kpu.init_yolo2(task, 0.5, 0.3, 5, anchor)
print('start')



def uart_transfer_img(roi, img):
    for x in range(roi[0], roi[0] + w):
        for y in range(roi[1], roi[1] + h):
            rgb = img.get_pixel(x,y)
            uart_out.write(bytes(rgb))


while(1):
    img = sensor.snapshot()
    code = kpu.run_yolo2(task, img)
    if code:
        for i in code:
            if i.value() > 0.7:
                #print(i)
                area = GetRectArea(i.rect())
                if area > threshold:
                    if flag == False:
                        try:
                            spk_sd.value(1) #Enable the SPK output
                            player = audio.Audio(path = "/flash/ding.wav")
                            player.volume(100)
                            wav_info = player.play_process(wav_dev)
                            wav_dev.channel_config(wav_dev.CHANNEL_1, I2S.TRANSMITTER,resolution = I2S.RESOLUTION_16_BIT, align_mode = I2S.STANDARD_MODE)
                            wav_dev.set_sample_rate(wav_info[1])

                            while True:
                                ret = player.play()
                                if ret == None:
                                    break
                                elif ret==0:
                                    break
                            player.finish()
                            spk_sd.value(0) #Enable the SPK output
                        except:
                            print("audio play err")
                        flag = True
                        count += 1
                        w = i.rect()[2]
                        h = i.rect()[3]
                        if w >= 115:
                            w = 115
                        if h >= 115:
                            h = 115
                        hexlist = [(count>>24)&0xFF,(count>>16)&0xFF,(count>>8)&0xFF,(count)&0xFF]
                        hexlist += [(w>>8)&0xFF,(w)&0xFF]
                        hexlist += [(h>>8)&0xFF,(h)&0xFF]
                        uart_out.write(bytes(hexlist))
                        uart_transfer_img((i.rect()[0], i.rect()[1], w, h), img)
                        print("count = %d, w = %d, h = %d" %(count, i.rect()[2], i.rect()[3]))
                else:
                    flag = False
                a = img.draw_rectangle(i.rect())
    elif gap == 10:
        gap = 0
        flag = False
    else:
        gap += 1
    a = img.draw_string(50, 50, "Count = %d" %count, scale=2)
    a = lcd.display(img)
a = kpu.deinit(task)

