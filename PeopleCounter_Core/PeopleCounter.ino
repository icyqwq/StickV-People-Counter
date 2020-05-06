#include <M5Stack.h>
#include "img.h"
#include "Free_Fonts.h"
#include <list>
#include "FastLED.h"

#define Neopixel_PIN 15
#define NUM_LEDS    10
#define TIME_BASE 1000  //10s (ms)
typedef std::list<uint32_t> Uint32List;

enum
{
    kLedEffectAlarm = 0,
    kLedEffectNormal
};

CRGB leds[NUM_LEDS];
uint8_t gHue = 0;
static TaskHandle_t FastLEDshowTaskHandle = 0;
static TaskHandle_t userTaskHandle = 0;
HardwareSerial VSerial(1);

uint8_t buffer[2000] = {'\0'};
uint32_t count = 0;
uint16_t imgw;
uint16_t imgh;
Uint32List count_min;
uint64_t last_time = 0;

QueueHandle_t xQueue_start_effect_task = xQueueCreate(10, sizeof(uint8_t));
TaskHandle_t xHandle;
uint8_t _info_to_core0;

void LedEffect(uint8_t effect)
{
    _info_to_core0 = effect;
    xQueueSend(xQueue_start_effect_task, &_info_to_core0, 0);
}

void FastLEDshowESP32()
{
    if (userTaskHandle == 0) {
        userTaskHandle = xTaskGetCurrentTaskHandle();
        xTaskNotifyGive(FastLEDshowTaskHandle);
        const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 200 );
        ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
        userTaskHandle = 0;
    }
}

void FastLEDshowTask(void *pvParameters)
{
    uint8_t data = kLedEffectNormal;
    uint8_t birghtness = 255;
    bool direction = false;
    uint8_t i, flag = 0;
    for(;;) 
    {
        xQueueReceive( xQueue_start_effect_task, &data, 0);
        switch(data)
        {
            case kLedEffectNormal:
                flag = 0;
                FastLED.setBrightness(10);
                fill_rainbow(leds, NUM_LEDS, gHue, 7);// rainbow effect
                FastLED.show();// must be executed for neopixel becoming effective
                EVERY_N_MILLISECONDS( 20 ) { gHue++; }
                break;
            case kLedEffectAlarm:
                FastLED.setBrightness(255);
                
                switch(flag)
                {
                    case 0:
                    case 8:
                    case 16:
                    case 24:
                    case 32:
                        for(i = 0; i < NUM_LEDS; i++)
                        {
                            leds[i] = CRGB::Green;
                        }
                        break;
                    case 33:
                        data = kLedEffectNormal;
                    default:
                        for(i = 0; i < NUM_LEDS; i++)
                        {
                            leds[i] = CRGB::Black;
                        }
                }

                FastLED.show();
                EVERY_N_MILLISECONDS(50)
                { 
                    flag++;
                    if(flag == 50)
                    {
                        flag = 0;
                    }
                }
                break;
        }
    }
}

bool VSerialGetData()
{
    if (VSerial.available())
    {
        VSerial.readBytes(buffer, 8);
        count = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | (buffer[3]);
        imgw = (buffer[4] << 8) | (buffer[5]);
        imgh = (buffer[6] << 8) | (buffer[7]);
        Serial.printf("%d,%d,%d\n", count, imgw, imgh);
        VSerial.readBytes(buffer, imgw * imgh);
        /*
        for(size_t i = 0; i < imgw*imgh; i+=3)
        {
            Serial.printf("%02X%02X%02X ", buffer[i], buffer[i+1], buffer[i+2]);
        }
        Serial.printf("\n");*/
        return 1;
    }
    return 0;
}

TFT_eSprite _tft_buffer1 = TFT_eSprite(&M5.Lcd);
TFT_eSprite _tft_buffer2 = TFT_eSprite(&M5.Lcd);

void DrawBlodLine(uint16_t x, uint16_t y, uint16_t lx, uint16_t ly, uint16_t color, uint8_t blod)
{
    uint8_t i = 0;
    _tft_buffer2.fillCircle(x, y, blod, TFT_WHITE);
    _tft_buffer2.fillCircle(lx, ly, blod, TFT_WHITE);
    _tft_buffer2.drawLine(x, y, lx, ly, TFT_WHITE);
    for(i = 1; i <= blod; i++)
    {
        _tft_buffer2.drawLine(x-i, y, lx-i, ly, TFT_WHITE);
        _tft_buffer2.drawLine(x+i, y, lx+i, ly, TFT_WHITE);
        _tft_buffer2.drawLine(x, y-i, lx, ly-i, TFT_WHITE);
        _tft_buffer2.drawLine(x, y+i, lx, ly+i, TFT_WHITE);
    }
}

void UpdateGUI(void)
{
    char temp[100];
    _tft_buffer1.fillSprite(M5.Lcd.color565(127, 127, 127));
    _tft_buffer2.pushImage(0, 0, 320, 125, (uint16_t *)img_screen);

    sprintf(temp, "Total: %d people", count);
    _tft_buffer1.drawString(temp, 160, 40, GFXFF);

    uint32_t max_count = 0;
    for( Uint32List::iterator i = count_min.begin(); i != count_min.end(); i++)
    {
        if(*i > max_count) max_count = *i;
    }
    uint16_t y_gap;
    if(max_count <= 1)
    {
        y_gap = 0;
    }
    else
    {
        y_gap = 120.0f / max_count;
    }
    uint16_t x_gap;
    uint16_t x = 0, lx = 0;
    uint16_t y = 120, ly = 120;
    uint16_t counter = 0;
    //Serial.printf("x_gap = %d, y_gap = %d, max_count = %d, size = %d\n", x_gap, y_gap, max_count, count_min.size());
    for( Uint32List::iterator i = count_min.begin(); i != count_min.end(); i++)
    {
        counter++;
        if(count_min.size() - counter > 0) 
            x_gap = (320 - x) / (count_min.size() - counter);
        else 
            x = 319;
        
        ly = y;
        y = 120 - ((*i) * y_gap);
        //Serial.printf("x = %d, lx = %d, y = %d, ly = %d\n", x, lx, y, ly);
        DrawBlodLine(x, y, lx, ly, TFT_WHITE, 1);
        lx = x;
        x += x_gap;
    }
    

    _tft_buffer1.pushSprite(0, 0);
    _tft_buffer2.pushSprite(0, 115);
    last_time = millis();
}

void setup()
{
    M5.begin();
    FastLED.addLeds<NEOPIXEL,Neopixel_PIN>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(10);
    disableCore0WDT();
    xTaskCreatePinnedToCore(FastLEDshowTask, "FastLEDshowTask", 1024, NULL, 2, NULL, 0);
    LedEffect(kLedEffectNormal);

    VSerial.begin(1500000, SERIAL_8N1, 22, 21);
    M5.Lcd.setFreeFont(FF18);
    M5.Lcd.setTextDatum(TC_DATUM);
    _tft_buffer1.setColorDepth(16);
    _tft_buffer1.createSprite(320, 115);
    _tft_buffer1.setTextDatum(TC_DATUM);
    _tft_buffer1.setFreeFont(FF18);
    _tft_buffer1.setTextColor(TFT_WHITE);
    _tft_buffer2.setColorDepth(16);
    _tft_buffer2.createSprite(320, 125);
    _tft_buffer2.setTextDatum(TC_DATUM);
    _tft_buffer2.setFreeFont(FF18);
    _tft_buffer2.setTextColor(TFT_WHITE);
    count_min.push_back(0);
    UpdateGUI();
}

void loop()
{
    while(true)
    {
        if(millis() - last_time > TIME_BASE)
        {
            if(count_min.size() > 500)
            {
                count_min.pop_front();
            }
            count_min.push_back(0);
            break;
        }
        if(VSerialGetData())
        {
            LedEffect(kLedEffectAlarm);
            count_min.back()++;
        }
    }

    UpdateGUI();
}

