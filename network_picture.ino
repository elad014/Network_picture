#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <ESP32Ping.h>
#include <math.h>


class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7796  _panel_instance;
  lgfx::Bus_SPI       _bus_instance;

public:
  LGFX(void)
  {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 20000000;
      cfg.freq_read  = 16000000;
      cfg.spi_3wire  = false;
      cfg.use_lock   = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;

      cfg.pin_sclk = 12;
      cfg.pin_mosi = 11;
      cfg.pin_miso = 13;
      cfg.pin_dc   = 15;
      
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs   = 10;
      cfg.pin_rst  = 14;
      cfg.pin_busy = -1;

      cfg.panel_width  = 320;
      cfg.panel_height = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable = true;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

LGFX tft;
LGFX_Sprite canvas(&tft); 


const char* ssid = "";
const char* password = "";

struct Star {
  bool active;      
  float angle;      
  float radius;     
  float speed;      
  String deviceName; 
};

struct Galaxy {
  bool active;
  bool seen;
  int x;
  int y;
  char ssidName[33]; 
};

Star stars[256]; 
Galaxy galaxies[30]; 
int centerX = 240; 
int centerY = 160;

int lastDiscoveredId = -1;

String getHostNameFromIP(int id) {
  if (id == 15) return "Elad-PC";
  if (id == 42) return "Smart-TV";
  if (id == 100) return "LivingRoom";
  
  return String(id);
}

void scanNetworkTask(void * parameter) {
  for(;;) {
    
    // --- 1. סריקת רשתות זרות (גלקסיות) ---
    int n = WiFi.scanNetworks();
    
    for (int j = 0; j < 30; j++) galaxies[j].seen = false;

    for (int i = 0; i < n; i++) {
       String netSSID = WiFi.SSID(i);
       
       if (netSSID == String(ssid) || netSSID == "") continue;

       bool found = false;
       for (int j = 0; j < 30; j++) {
          if (galaxies[j].active && String(galaxies[j].ssidName) == netSSID) {
             galaxies[j].seen = true;
             found = true;
             break;
          }
       }
       
       if (!found) {
          for (int j = 0; j < 30; j++) {
             if (!galaxies[j].active) {
                strncpy(galaxies[j].ssidName, netSSID.c_str(), 32);
                galaxies[j].ssidName[32] = '\0';

                int gx, gy;
                do {
                   gx = random(20, 460);
                   gy = random(20, 300);
                } while (abs(gx - 240) < 60 && abs(gy - 160) < 60);

                galaxies[j].x = gx;
                galaxies[j].y = gy;
                galaxies[j].active = true;
                galaxies[j].seen = true;
                break;
             }
          }
       }
    }

    for (int j = 0; j < 30; j++) {
       if (galaxies[j].active && !galaxies[j].seen) {
          galaxies[j].active = false;
       }
    }
    
    WiFi.scanDelete(); 

    // --- 2. סריקת הפינג המקומית ---
    for (int i = 1; i < 255; i++) {
      IPAddress ip(10, 0, 0, i);
      bool isAlive = Ping.ping(ip, 1);
      
      if (isAlive) {
        if (!stars[i].active) {
          stars[i].deviceName = getHostNameFromIP(i);
          stars[i].angle = random(0, 360) * (PI / 180.0);
          stars[i].radius = random(40, 140); 
          stars[i].speed = random(5, 35) / 1000.0; 
          stars[i].active = true;
          
          lastDiscoveredId = i; 
        }
      } else {
        stars[i].active = false;
        
        if (lastDiscoveredId == i) {
          lastDiscoveredId = -1;
        }
      }
      
      vTaskDelay(10 / portTICK_PERIOD_MS); 
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS); 
  }
}

void setup(void)
{
  Serial.begin(115200);
  delay(1000);
  
  tft.init();
  tft.setRotation(1); 
  
  canvas.setColorDepth(8);
  canvas.createSprite(tft.width(), tft.height());
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 20);
  tft.print("Initializing Deep Space Scan...");

  for (int i=0; i<256; i++) {
    stars[i].active = false;
    stars[i].deviceName = "";
  }
  for (int j=0; j<30; j++) {
    galaxies[j].active = false;
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  xTaskCreatePinnedToCore(
    scanNetworkTask,   
    "ScanTask",        
    10000,             
    NULL,              
    1,                 
    NULL,              
    0);                
}

void loop(void)
{
  // 1. ניקוי הקנבס
  canvas.fillSprite(TFT_BLACK); 

  canvas.setTextColor(canvas.color565(80, 80, 80));
  canvas.setTextSize(1);
  canvas.setCursor(5, 5);
  canvas.print("Deep Space Scan: 10.0.0.x [ORBITAL + GALAXIES]");

  for (int j = 0; j < 30; j++) {
     if (galaxies[j].active) {
        canvas.fillCircle(galaxies[j].x, galaxies[j].y, 2, TFT_YELLOW);
        canvas.drawPixel(galaxies[j].x - 3, galaxies[j].y, TFT_YELLOW);
        canvas.drawPixel(galaxies[j].x + 3, galaxies[j].y, TFT_YELLOW);
        canvas.drawPixel(galaxies[j].x, galaxies[j].y - 3, TFT_YELLOW);
        canvas.drawPixel(galaxies[j].x, galaxies[j].y + 3, TFT_YELLOW);
        canvas.drawPixel(galaxies[j].x - 2, galaxies[j].y - 2, TFT_YELLOW);
        canvas.drawPixel(galaxies[j].x + 2, galaxies[j].y + 2, TFT_YELLOW);

        canvas.setTextColor(canvas.color565(150, 150, 0)); 
        canvas.setCursor(galaxies[j].x + 6, galaxies[j].y - 4);
        canvas.print(galaxies[j].ssidName);
     }
  }

  for (int i = 1; i < 255; i++) {
    if (stars[i].active) {
      
      stars[i].angle += stars[i].speed;
      if (stars[i].angle > 6.28318) stars[i].angle -= 6.28318;
      
      int newX = centerX + stars[i].radius * cos(stars[i].angle);
      int newY = centerY + stars[i].radius * sin(stars[i].angle);

      canvas.drawLine(centerX, centerY, newX, newY, canvas.color565(40, 40, 40)); 
      canvas.fillCircle(newX, newY, 4, canvas.color565(30, 30, 100)); 
      canvas.fillCircle(newX, newY, 1, TFT_WHITE);                 
      
      // בחירת הצבע לטקסט: אדום אם זה החדש ביותר, אחרת אפור בהיר
      if (i == lastDiscoveredId) {
        canvas.setTextColor(TFT_RED);
      } else {
        canvas.setTextColor(TFT_LIGHTGRAY);
      }
      
      canvas.setCursor(newX + 6, newY - 4);
      canvas.print(stars[i].deviceName);
    }
  }

  canvas.fillCircle(centerX, centerY, 6, TFT_YELLOW);
  canvas.setTextColor(TFT_YELLOW);
  canvas.setTextSize(1);
  canvas.setCursor(centerX - 18, centerY + 10);
  canvas.print("Router");

  canvas.pushSprite(0, 0); 

  delay(40); 
}