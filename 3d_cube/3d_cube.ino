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
#define NP 3
#define BALL_R 0.15  // ball radius in cube units

typedef struct { float x, y, z; } Vec3;

Vec3 cubeVerts[8] = {
  {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
  {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}
};
int edges[12][2] = {
  {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},
  {0,4},{1,5},{2,6},{3,7}
};

// ─── Balls in LOCAL space ──────────
Vec3 pos[NP], vel[NP];
uint16_t col[NP];

// ─── State ─────────────────────────
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

uint16_t rainbow(uint8_t hue) {
  uint8_t r,g,b, reg=hue/43, rem=(hue-reg*43)*6;
  switch(reg) {
    case 0: r=255; g=rem; b=0; break;
    case 1: r=255-rem; g=255; b=0; break;
    case 2: r=0; g=255; b=rem; break;
    case 3: r=0; g=255-rem; b=255; break;
    case 4: r=rem; g=0; b=255; break;
    default: r=255; g=0; b=255-rem; break;
  }
  return tft.color565(r,g,b);
}

uint16_t dimColor(uint16_t c, uint8_t pct) {
  uint8_t r=((c>>8)&0xF8)*pct/255, g=((c>>3)&0xFC)*pct/255, b=((c<<3)&0xF8)*pct/255;
  return tft.color565(r,g,b);
}

void setup() {
  pinMode(TOUCH_PIN, INPUT);
  pixels.begin(); pixels.setBrightness(25); pixels.clear(); pixels.show();
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB); tft.setRotation(1); tft.fillScreen(ST7735_BLACK);

  float bound = 1.0 - BALL_R - 0.02;
  for (int i=0; i<NP; i++) {
    pos[i]=(Vec3){random(1000)/500.0*bound-bound, -0.7, random(1000)/500.0*bound-bound};
    vel[i]=(Vec3){(float)random(1000)/5000-0.1, 0, (float)random(1000)/5000-0.1};
    col[i]=rainbow(i*85);
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

  float bound = 1.0 - BALL_R;

  // ─── Physics in LOCAL space ───
  for (int i=0; i<NP; i++) {
    vel[i].y -= 0.0015f;
    vel[i].x *= 0.995f; vel[i].y *= 0.995f; vel[i].z *= 0.995f;

    pos[i].x += vel[i].x;
    pos[i].y += vel[i].y;
    pos[i].z += vel[i].z;

    // Bounce off cube walls (local space = easy!)
    if (pos[i].x > bound) { pos[i].x=bound; vel[i].x=-vel[i].x*0.65f; }
    if (pos[i].x < -bound) { pos[i].x=-bound; vel[i].x=-vel[i].x*0.65f; }
    if (pos[i].y > bound) { pos[i].y=bound; vel[i].y=-vel[i].y*0.65f; }
    if (pos[i].y < -bound) { pos[i].y=-bound; vel[i].y=-vel[i].y*0.65f; }
    if (pos[i].z > bound) { pos[i].z=bound; vel[i].z=-vel[i].z*0.65f; }
    if (pos[i].z < -bound) { pos[i].z=-bound; vel[i].z=-vel[i].z*0.65f; }
  }

  // ─── Render cube ───
  Vec3 tv[8]; int sx[8], sy[8]; float sd[8];
  for (int i=0; i<8; i++) { tv[i]=cubeVerts[i]; aRot(&tv[i]); proj(tv[i], &sx[i], &sy[i], &sd[i]); }

  // ─── Render balls (transform local → world → screen) ───
  Vec3 wpos[NP];
  int px[NP], py[NP]; float pd[NP];
  for (int i=0; i<NP; i++) {
    wpos[i]=pos[i]; aRot(&wpos[i]);
    proj(wpos[i], &px[i], &py[i], &pd[i]);
  }

  // ─── Previous ball screen rects for erasing ───
  static int ppx[NP], ppy[NP], ppr[NP];

  // ─── DRAW ────────
  if (first) {
    tft.fillScreen(ST7735_BLACK);
    // Cube edges
    uint8_t eh = (uint8_t)(t*0.02);
    for (int i=0; i<12; i++) {
      int a=edges[i][0], b=edges[i][1];
      float d=(sd[a]+sd[b])/2;
      uint8_t br=(uint8_t)((1.0-(d-0.5)/5.5)*200+55);
      tft.drawLine(sx[a], sy[a], sx[b], sy[b], dimColor(rainbow(eh+i*20), br));
      el1[i]=sx[a]; el2[i]=sy[a]; el3[i]=sx[b]; el4[i]=sy[b];
    }
    // Balls
    for (int i=0; i<NP; i++) {
      if (pd[i]>0.4 && pd[i]<5.5) {
        int r=2+(1.0/pd[i])*5;
        if (r>7) r=7; if (r<2) r=2;
        uint8_t br=(uint8_t)((1.0-(pd[i]-0.5)/5.0)*255);
        tft.fillCircle(px[i], py[i], r, dimColor(col[i], br));
        ppx[i]=px[i]; ppy[i]=py[i]; ppr[i]=r;
      }
    }
    first=false;
  } else {
    // Erase old balls
    for (int i=0; i<NP; i++) tft.fillCircle(ppx[i], ppy[i], ppr[i]+1, ST7735_BLACK);
    // Erase old edges
    for (int i=0; i<12; i++) tft.drawLine(el1[i], el2[i], el3[i], el4[i], ST7735_BLACK);

    // Draw edges (depth sorted)
    struct E2 { int a,b; float z; };
    struct E2 sed[12];
    for (int i=0; i<12; i++) {
      int a=edges[i][0], b=edges[i][1];
      sed[i]=(struct E2){a,b,(sd[a]+sd[b])/2};
    }
    for (int i=0; i<11; i++)
      for (int j=0; j<11-i; j++)
        if (sed[j].z>sed[j+1].z) {
          struct E2 tmp=sed[j]; sed[j]=sed[j+1]; sed[j+1]=tmp;
        }
    uint8_t eh=(uint8_t)(t*0.02);
    for (int i=0; i<12; i++) {
      int a=sed[i].a, b=sed[i].b;
      float d=(sd[a]+sd[b])/2;
      uint8_t br=(uint8_t)((1.0-(d-0.5)/5.5)*180+75);
      tft.drawLine(sx[a], sy[a], sx[b], sy[b], dimColor(rainbow(eh+i*20), br));
      el1[i]=sx[a]; el2[i]=sy[a]; el3[i]=sx[b]; el4[i]=sy[b];
    }

    // Draw balls (big + depth-bright)
    for (int i=0; i<NP; i++) {
      if (pd[i]>0.4 && pd[i]<5.5) {
        int r=2+(1.0/pd[i])*5;
        if (r>7) r=7; if (r<2) r=2;
        uint8_t br=(uint8_t)((1.0-(pd[i]-0.5)/5.0)*255);
        tft.fillCircle(px[i], py[i], r, dimColor(col[i], br));
        ppx[i]=px[i]; ppy[i]=py[i]; ppr[i]=r;
      }
    }
  }

  // Direction
  tft.fillRect(WIDTH-18, 0, 18, 8, ST7735_BLACK);
  tft.setTextSize(1); tft.setCursor(WIDTH-14, 0);
  tft.setTextColor(0x3AEF, ST7735_BLACK);
  tft.print(dir>0 ? ">>" : "<<");

  // LED
  uint8_t lh=(uint8_t)(t*0.04);
  uint8_t lr,lg,lb, reg=lh/43, rem=(lh-reg*43)*6;
  switch(reg) {
    case 0: lr=255; lg=rem; lb=0; break;
    case 1: lr=255-rem; lg=255; lb=0; break;
    case 2: lr=0; lg=255; lb=rem; break;
    case 3: lr=0; lg=255-rem; lb=255; break;
    case 4: lr=rem; lg=0; lb=255; break;
    default: lr=255; lg=0; lb=255-rem; break;
  }
  pixels.setPixelColor(0, pixels.Color(lr,lg,lb));
  pixels.show();

  delay(20);
}
