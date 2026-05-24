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

#define WIDTH 160
#define HEIGHT 128
#define CX (WIDTH/2)
#define CY (HEIGHT/2)
#define NP 22

typedef struct { float x,y,z; } Vec3;

Vec3 cv[8] = {{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
int ee[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};

Vec3 pos[NP], vel[NP];
int pvx[NP], pvy[NP], pvr[NP];  // prev screen pos + radius

float ax=0, ay=0, az=0;
int dir=1;
bool lt=false, first=true;
int el1[12], el2[12], el3[12], el4[12];
float rot[9];

void compRot(float a, float b, float c) {
  float cx=cosf(a),sx=sinf(a),cy=cosf(b),sy=sinf(b),cz=cosf(c),sz=sinf(c);
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
  *sx=CX+(p.x/z)*96; *sy=CY+(p.y/z)*96;
}

uint16_t dimCol(uint16_t c, uint8_t pct) {
  uint8_t r=((c>>8)&0xF8)*pct/255,g=((c>>3)&0xFC)*pct/255,b=((c<<3)&0xF8)*pct/255;
  return tft.color565(r,g,b);
}

void setup() {
  pinMode(TOUCH_PIN, INPUT);
  pixels.begin(); pixels.setBrightness(30); pixels.clear(); pixels.show();
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB); tft.setRotation(1); tft.fillScreen(ST7735_BLACK);

  // Pool all particles at bottom of cube
  for (int i=0; i<NP; i++) {
    pos[i]=(Vec3){
      (float)random(1000)/500-1,           // random X spread
      -0.75 + (float)random(300)/1000,     // near bottom
      (float)random(1000)/500-1            // random Z spread
    };
    pos[i].x*=0.7; pos[i].z*=0.7;
    vel[i]=(Vec3){0,0,0};
    pvx[i]=pvy[i]=pvr[i]=-1;
  }
  for (int i=0; i<12; i++) el1[i]=el2[i]=el3[i]=el4[i]=0;
}

void loop() {
  unsigned long t=millis();

  bool touched=(digitalRead(TOUCH_PIN)==HIGH);
  if (touched && !lt) { dir=-dir; delay(150); }
  lt=touched;

  float da=0.025*dir;
  ax+=da; ay+=da*1.4; az+=da*0.5;
  compRot(ax, ay, az);

  float bound = 0.85; // slightly smaller than cube for visual padding

  // ─── Liquid physics ───
  for (int i=0; i<NP; i++) {
    // Gravity (local -Y)
    vel[i].y -= 0.002f;

    // Viscosity (strong damping = no jitter)
    vel[i].x *= 0.975f;
    vel[i].y *= 0.975f;
    vel[i].z *= 0.975f;

    // ─── Neighbor attraction (cohesion) ───
    float ax=0, ay=0, az=0;
    for (int j=0; j<NP; j++) {
      if (i==j) continue;
      float dx=pos[j].x-pos[i].x, dy=pos[j].y-pos[i].y, dz=pos[j].z-pos[i].z;
      float d2=dx*dx+dy*dy+dz*dz;
      if (d2 < 0.5f && d2 > 0.0001f) {
        float d=sqrtf(d2);
        // Attraction within ~0.7 cube units
        if (d < 0.7f) {
          float f = (0.7f - d) * 0.004f;
          ax += dx*f; ay += dy*f; az += dz*f;
        }
        // Repulsion when very close (surface tension)
        if (d < 0.12f) {
          float f = (0.12f - d) * 0.02f;
          ax -= dx*f; ay -= dy*f; az -= dz*f;
        }
      }
    }
    vel[i].x += ax; vel[i].y += ay; vel[i].z += az;

    // ─── Slight random perturbation (only when there's room) ───
    float spd=vel[i].x*vel[i].x+vel[i].y*vel[i].y+vel[i].z*vel[i].z;
    if (spd < 0.0001f) {
      vel[i].x += (float)random(1000)/8000-0.06f;
      vel[i].z += (float)random(1000)/8000-0.06f;
    }

    // Move
    pos[i].x += vel[i].x;
    pos[i].y += vel[i].y;
    pos[i].z += vel[i].z;

    // ─── Wall collision: LIQUID doesn't bounce — it sticks and slides ───
    // Just clamp to boundary and kill velocity toward wall
    if (pos[i].x > bound) { pos[i].x = bound; if (vel[i].x > 0) vel[i].x *= -0.1f; }
    if (pos[i].x < -bound){ pos[i].x = -bound; if (vel[i].x < 0) vel[i].x *= -0.1f; }
    if (pos[i].y > bound) { pos[i].y = bound; if (vel[i].y > 0) vel[i].y *= -0.1f; }
    if (pos[i].y < -bound){ pos[i].y = -bound; if (vel[i].y < 0) vel[i].y *= -0.15f; } // soft floor
    if (pos[i].z > bound) { pos[i].z = bound; if (vel[i].z > 0) vel[i].z *= -0.1f; }
    if (pos[i].z < -bound){ pos[i].z = -bound; if (vel[i].z < 0) vel[i].z *= -0.1f; }
  }

  // Project cube
  Vec3 tv[8]; int sx[8], sy[8]; float sd[8];
  for (int i=0; i<8; i++) { tv[i]=cv[i]; aRot(&tv[i]); proj(tv[i],&sx[i],&sy[i],&sd[i]); }

  // Project liquid
  Vec3 wp[NP]; int px[NP], py[NP]; float pd[NP];
  for (int i=0; i<NP; i++) { wp[i]=pos[i]; aRot(&wp[i]); proj(wp[i],&px[i],&py[i],&pd[i]); }

  // ─── DRAW ───
  if (first) {
    tft.fillScreen(ST7735_BLACK);
    // Cube edges
    for (int i=0; i<12; i++) {
      int a=ee[i][0], b=ee[i][1];
      float d=(sd[a]+sd[b])/2;
      uint8_t br=(uint8_t)((1.0-(d-0.5)/5.5)*100+155);
      uint16_t ec=dimCol(tft.color565(180,240,255), br);
      tft.drawLine(sx[a],sy[a],sx[b],sy[b], ec);
      if (sx[a]!=sx[b]||sy[a]!=sy[b]) tft.drawLine(sx[a]+1,sy[a],sx[b]+1,sy[b], dimCol(ec,70));
      el1[i]=sx[a]; el2[i]=sy[a]; el3[i]=sx[b]; el4[i]=sy[b];
    }
    // Liquid blobs
    for (int i=0; i<NP; i++) {
      if (pd[i]>0.4 && pd[i]<5.5) {
        int r=2+(1.0/pd[i])*3; if (r<2) r=2; if (r>5) r=5;
        uint8_t br=(uint8_t)((1.0-(pd[i]-0.5)/5.0)*255);
        tft.fillCircle(px[i], py[i], r, dimCol(tft.color565(60,200,255), br));
        pvx[i]=px[i]; pvy[i]=py[i]; pvr[i]=r;
      }
    }
    first=false;
  } else {
    // Erase old liquid blobs
    for (int i=0; i<NP; i++) {
      if (pvr[i]>=0) tft.fillCircle(pvx[i], pvy[i], pvr[i]+1, ST7735_BLACK);
    }
    // Erase old edges
    for (int i=0; i<12; i++) tft.drawLine(el1[i],el2[i],el3[i],el4[i], ST7735_BLACK);

    // Sort edges
    struct E2 { int a,b; float z; };
    struct E2 sed[12];
    for (int i=0; i<12; i++) {
      int a=ee[i][0], b=ee[i][1];
      sed[i]=(struct E2){a,b,(sd[a]+sd[b])/2};
    }
    for (int i=0; i<11; i++)
      for (int j=0; j<11-i; j++)
        if (sed[j].z>sed[j+1].z) { struct E2 t=sed[j]; sed[j]=sed[j+1]; sed[j+1]=t; }

    // Draw bold cube edges
    for (int i=0; i<12; i++) {
      int a=sed[i].a, b=sed[i].b;
      float d=(sd[a]+sd[b])/2;
      uint8_t br=(uint8_t)((1.0-(d-0.5)/5.5)*80+175);
      uint16_t ec=dimCol(tft.color565(180,240,255), br);
      tft.drawLine(sx[a],sy[a],sx[b],sy[b], ec);
      if (sx[a]!=sx[b]||sy[a]!=sy[b]) tft.drawLine(sx[a]+1,sy[a],sx[b]+1,sy[b], dimCol(ec,70));
      el1[i]=sx[a]; el2[i]=sy[a]; el3[i]=sx[b]; el4[i]=sy[b];
    }

    // Draw liquid (particles as soft glowing circles)
    for (int i=0; i<NP; i++) {
      if (pd[i]>0.4 && pd[i]<5.5) {
        int r=2+(1.0/pd[i])*3; if (r<2) r=2; if (r>5) r=5;
        uint8_t br=(uint8_t)((1.0-(pd[i]-0.5)/5.0)*255);
        uint16_t c=dimCol(tft.color565(60,200,255), br);
        // Outer glow
        if (r>=3) tft.drawCircle(px[i], py[i], r, dimCol(c,80));
        // Core fill
        tft.fillCircle(px[i], py[i], r-1, c);
        pvx[i]=px[i]; pvy[i]=py[i]; pvr[i]=r;
      } else { pvx[i]=-1; pvr[i]=-1; }
    }
  }

  // Direction
  tft.fillRect(WIDTH-18,0,18,8,ST7735_BLACK);
  tft.setTextSize(1); tft.setCursor(WIDTH-14,0);
  tft.setTextColor(0x3CEF, ST7735_BLACK);
  tft.print(dir>0 ? ">>" : "<<");

  // LED — deep cyan pulse  
  float p=(sin(t*0.004)+1)*0.5;
  pixels.setPixelColor(0, pixels.Color((uint8_t)(p*20),(uint8_t)(p*100),(uint8_t)(120+p*80)));
  pixels.show();

  delay(22);
}
