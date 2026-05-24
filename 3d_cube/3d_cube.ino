#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

#define TFT_CS    5
#define TFT_RST   4
#define TFT_DC    3
#define TFT_MOSI  2
#define TFT_SCLK  1

#define TOUCH_PIN 0
#define LED_PIN   10
#define NUMPIXELS 1

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

#define WIDTH  160
#define HEIGHT 128
#define CX (WIDTH/2)
#define CY (HEIGHT/2)
#define NP 18

typedef struct { float x, y, z; } Vec3;

Vec3 cubeVerts[8] = {
  {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
  {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}
};
int edges[12][2] = {
  {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},
  {0,4},{1,5},{2,6},{3,7}
};

// Liquid particles in LOCAL space
Vec3 pos[NP], vel[NP];
uint16_t lcol[NP];
int prevPx[NP], prevPy[NP];

float ax=0, ay=0, az=0;
int dir=1;
bool lastTouch=false, first=true;
int el1[12], el2[12], el3[12], el4[12];
float rot[9];

void compRot(float ax, float ay, float az) {
  float cx=cosf(ax), sx=sinf(ax), cy=cosf(ay), sy=sinf(ay), cz=cosf(az), sz=sinf(az);
  rot[0]=cy*cz+sx*sy*sz; rot[1]=-cx*sz; rot[2]=sy*cz-sx*cy*sz;
  rot[3]=cy*sz-sx*sy*cz; rot[4]=cx*cz;  rot[5]=sy*sz+sx*cy*cz;
  rot[6]=-cx*sy;         rot[7]=sx;     rot[8]=cx*cy;
}

void aRot(Vec3* p) {
  float x=p->x*rot[0]+p->y*rot[1]+p->z*rot[2];
  float y=p->x*rot[3]+p->y*rot[4]+p->z*rot[5];
  float z=p->x*rot[6]+p->y*rot[7]+p->z*rot[8];
  p->x=x; p->y=y; p->z=z;
}

void proj(Vec3 p, int* sx, int* sy, float* d) {
  float z=p.z+3.0; *d=z;
  *sx=CX+(p.x/z)*96;
  *sy=CY+(p.y/z)*96;
}

uint16_t water(uint8_t hue) {
  // Cool liquid colors: cyan → blue → purple
  uint8_t r,g,b, reg=hue/64;
  uint8_t t=hue&63;
  switch(reg) {
    case 0: r=0; g=t*4; b=200+(t*55/64); break;           // cyan→blue
    case 1: r=t*3; g=255-t*4; b=255-t; break;              // blue→purple  
    case 2: r=200+t*55/64; g=t/2; b=200-t*3; break;        // purple→cyan
    default: r=0; g=180; b=255; break;
  }
  return tft.color565(r,g,b);
}

uint16_t dimCol(uint16_t c, uint8_t pct) {
  uint8_t r=((c>>8)&0xF8)*pct/255, g=((c>>3)&0xFC)*pct/255, b=((c<<3)&0xF8)*pct/255;
  return tft.color565(r,g,b);
}

void setup() {
  pinMode(TOUCH_PIN, INPUT);
  pixels.begin(); pixels.setBrightness(25); pixels.clear(); pixels.show();
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB); tft.setRotation(1); tft.fillScreen(ST7735_BLACK);

  // Spawn liquid at bottom of cube
  for (int i=0; i<NP; i++) {
    pos[i]=(Vec3){
      (float)random(1000)/500-1,           // random X
      -0.7 + (float)random(200)/1000-0.1,  // clustered near bottom
      (float)random(1000)/500-1            // random Z
    };
    pos[i].x*=0.6; pos[i].z*=0.6;
    vel[i]=(Vec3){0, 0, 0};
    lcol[i]=water(random(160));  // blue/cyan/purple range
    prevPx[i]=prevPy[i]=-1;
  }
  for (int i=0; i<12; i++) el1[i]=el2[i]=el3[i]=el4[i]=0;
}

void loop() {
  unsigned long t=millis();

  bool touched=(digitalRead(TOUCH_PIN)==HIGH);
  if (touched && !lastTouch) { dir=-dir; delay(150); }
  lastTouch=touched;

  float da=0.03*dir;
  ax+=da; ay+=da*1.5; az+=da*0.5;
  compRot(ax, ay, az);

  float bound = 0.92;

  // ─── Liquid physics ───
  for (int i=0; i<NP; i++) {
    // Gravity (local -Y = towards bottom face)
    vel[i].y -= 0.0018f;

    // Damping (viscosity)
    vel[i].x *= 0.988f;
    vel[i].y *= 0.988f;
    vel[i].z *= 0.988f;

    // Cohesion: slight attraction toward center of mass
    float cx=0, cy=0, cz=0;
    for (int j=0; j<NP; j++) { cx+=pos[j].x; cy+=pos[j].y; cz+=pos[j].z; }
    cx/=NP; cy/=NP; cz/=NP;
    vel[i].x += (cx-pos[i].x)*0.0003f;
    vel[i].y += (cy-pos[i].y)*0.0003f;
    vel[i].z += (cz-pos[i].z)*0.0003f;

    // Surface tension: slight repulsion when too close
    for (int j=i+1; j<NP; j++) {
      float dx=pos[i].x-pos[j].x, dy=pos[i].y-pos[j].y, dz=pos[i].z-pos[j].z;
      float dist=dx*dx+dy*dy+dz*dz;
      if (dist<0.04f && dist>0.001f) {
        float f=0.00015f/sqrtf(dist);
        vel[i].x+=dx*f; vel[i].y+=dy*f; vel[i].z+=dz*f;
        vel[j].x-=dx*f; vel[j].y-=dy*f; vel[j].z-=dz*f;
      }
    }

    // Thermal jitter (only when not resting)
    float speed=vel[i].x*vel[i].x+vel[i].y*vel[i].y+vel[i].z*vel[i].z;
    if (speed<0.00001f) {
      vel[i].x+=(float)random(1000)/5000-0.1f;
      vel[i].z+=(float)random(1000)/5000-0.1f;
    }

    // Move
    pos[i].x+=vel[i].x; pos[i].y+=vel[i].y; pos[i].z+=vel[i].z;

    // Bounce
    if (pos[i].x>bound) { pos[i].x=bound; vel[i].x*=-0.55f; }
    if (pos[i].x<-bound){ pos[i].x=-bound; vel[i].x*=-0.55f; }
    if (pos[i].y>bound) { pos[i].y=bound; vel[i].y*=-0.55f; }
    if (pos[i].y<-bound){ pos[i].y=-bound; vel[i].y*=-0.45f; }  // softer floor bounce
    if (pos[i].z>bound) { pos[i].z=bound; vel[i].z*=-0.55f; }
    if (pos[i].z<-bound){ pos[i].z=-bound; vel[i].z*=-0.55f; }
  }

  // Project
  Vec3 tv[8]; int sx[8], sy[8]; float sd[8];
  for (int i=0; i<8; i++) { tv[i]=cubeVerts[i]; aRot(&tv[i]); proj(tv[i],&sx[i],&sy[i],&sd[i]); }

  Vec3 wp[NP]; int px[NP], py[NP]; float pd[NP];
  for (int i=0; i<NP; i++) { wp[i]=pos[i]; aRot(&wp[i]); proj(wp[i],&px[i],&py[i],&pd[i]); }

  // ─── DRAW ───
  if (first) {
    tft.fillScreen(ST7735_BLACK);
    uint8_t eh=(uint8_t)(t*0.015);
    for (int i=0; i<12; i++) {
      int a=edges[i][0], b=edges[i][1];
      float d=(sd[a]+sd[b])/2;
      uint8_t br=(uint8_t)((1.0-(d-0.5)/5.5)*160+95);
      tft.drawLine(sx[a],sy[a],sx[b],sy[b], dimCol(water(eh+i*15), br));
      el1[i]=sx[a]; el2[i]=sy[a]; el3[i]=sx[b]; el4[i]=sy[b];
    }
    for (int i=0; i<NP; i++) {
      if (pd[i]>0.5 && pd[i]<5.5) {
        tft.drawPixel(px[i], py[i], lcol[i]);
        prevPx[i]=px[i]; prevPy[i]=py[i];
      }
    }
    first=false;
  } else {
    // Erase
    for (int i=0; i<NP; i++) if (prevPx[i]>=0) tft.drawPixel(prevPx[i], prevPy[i], ST7735_BLACK);
    for (int i=0; i<12; i++) tft.drawLine(el1[i],el2[i],el3[i],el4[i], ST7735_BLACK);

    // Cube edges
    struct E2 { int a,b; float z; };
    struct E2 sed[12];
    for (int i=0; i<12; i++) {
      int a=edges[i][0], b=edges[i][1];
      sed[i]=(struct E2){a,b,(sd[a]+sd[b])/2};
    }
    for (int i=0; i<11; i++)
      for (int j=0; j<11-i; j++)
        if (sed[j].z>sed[j+1].z) { struct E2 t=sed[j]; sed[j]=sed[j+1]; sed[j+1]=t; }
    uint8_t eh=(uint8_t)(t*0.015);
    for (int i=0; i<12; i++) {
      int a=sed[i].a, b=sed[i].b;
      float d=(sd[a]+sd[b])/2;
      uint8_t br=(uint8_t)((1.0-(d-0.5)/5.5)*140+115);
      tft.drawLine(sx[a],sy[a],sx[b],sy[b], dimCol(water(eh+i*15), br));
      el1[i]=sx[a]; el2[i]=sy[a]; el3[i]=sx[b]; el4[i]=sy[b];
    }

    // Liquid droplets (pixel + soft glow)
    for (int i=0; i<NP; i++) {
      if (pd[i]>0.4 && pd[i]<5.5) {
        uint8_t br=(uint8_t)((1.0-(pd[i]-0.5)/5.0)*255);
        uint16_t c=dimCol(lcol[i], br);
        tft.drawPixel(px[i], py[i], c);
        // Subtle neighbor glow for close particles
        if (pd[i]<3.5) {
          uint16_t dc=dimCol(lcol[i], br*0.4);
          int nx=px[i]+1, ny=py[i]+1;
          if (nx<WIDTH) tft.drawPixel(nx, py[i], dc);
          if (ny<HEIGHT) tft.drawPixel(px[i], ny, dc);
        }
        prevPx[i]=px[i]; prevPy[i]=py[i];
      } else { prevPx[i]=-1; }
    }
  }

  // Direction
  tft.fillRect(WIDTH-18, 0, 18, 8, ST7735_BLACK);
  tft.setTextSize(1); tft.setCursor(WIDTH-14, 0);
  tft.setTextColor(0x3CEF, ST7735_BLACK);
  tft.print(dir>0 ? ">>" : "<<");

  // LED — liquid blue/cyan pulse
  float pulse=(sin(t*0.005)+1)*0.5;
  pixels.setPixelColor(0, pixels.Color(
    (uint8_t)(pulse*30), (uint8_t)(pulse*120), (uint8_t)(180+pulse*75)
  ));
  pixels.show();

  delay(18);
}
