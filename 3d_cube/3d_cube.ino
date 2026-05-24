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
#define MAX_BALLS 500
int nb = 1;
#define CUBE_BOUND 0.85

typedef struct { float x,y,z; } Vec3;

// ─── Framebuffer ────────────────────
uint16_t fb[WIDTH*HEIGHT];

#define FPIX(x,y,c) if((x)>=0&&(x)<WIDTH&&(y)>=0&&(y)<HEIGHT) fb[(y)*WIDTH+(x)]=(c)

void fbClear() {
  for (int i=0; i<WIDTH*HEIGHT; i++) fb[i]=ST7735_BLACK;
}

void fbHLine(int x, int y, int w, uint16_t c) {
  if (y<0||y>=HEIGHT) return;
  if (x<0) { w+=x; x=0; }
  if (x+w>WIDTH) w=WIDTH-x;
  if (w<=0) return;
  uint16_t* p=&fb[y*WIDTH+x];
  while (w--) *p++=c;
}

void fbFillCircle(int cx, int cy, int r, uint16_t c) {
  int x=0, y=r, d=3-2*r;
  while (x<=y) {
    fbHLine(cx-x,cy-y,2*x+1,c);
    fbHLine(cx-x,cy+y,2*x+1,c);
    fbHLine(cx-y,cy-x,2*y+1,c);
    fbHLine(cx-y,cy+x,2*y+1,c);
    if (d<0) d+=4*x+6;
    else { d+=4*(x-y)+10; y--; }
    x++;
  }
}

void fbDrawCircle(int cx, int cy, int r, uint16_t c) {
  int x=0, y=r, d=3-2*r;
  while (x<=y) {
    FPIX(cx+x,cy+y,c); FPIX(cx-x,cy+y,c);
    FPIX(cx+x,cy-y,c); FPIX(cx-x,cy-y,c);
    FPIX(cx+y,cy+x,c); FPIX(cx-y,cy+x,c);
    FPIX(cx+y,cy-x,c); FPIX(cx-y,cy-x,c);
    if (d<0) d+=4*x+6;
    else { d+=4*(x-y)+10; y--; }
    x++;
  }
}

void fbDrawLine(int x0, int y0, int x1, int y1, uint16_t c) {
  int dx=abs(x1-x0), sx=x0<x1?1:-1;
  int dy=-abs(y1-y0), sy=y0<y1?1:-1;
  int err=dx+dy, e2;
  for (;;) {
    FPIX(x0,y0,c);
    if (x0==x1 && y0==y1) break;
    e2=2*err;
    if (e2>=dy) { err+=dy; x0+=sx; }
    if (e2<=dx) { err+=dx; y0+=sy; }
  }
}

void fbFlush() {
  tft.startWrite();
  tft.setAddrWindow(0, 0, WIDTH, HEIGHT);
  tft.writePixels(fb, WIDTH*HEIGHT);
  tft.endWrite();
}

// ─── 3x5 digit font for counter ────
const uint8_t digitBits[10][5] = {
  {0b111, 0b101, 0b101, 0b101, 0b111},  // 0
  {0b010, 0b110, 0b010, 0b010, 0b111},  // 1
  {0b111, 0b001, 0b111, 0b100, 0b111},  // 2
  {0b111, 0b001, 0b111, 0b001, 0b111},  // 3
  {0b101, 0b101, 0b111, 0b001, 0b001},  // 4
  {0b111, 0b100, 0b111, 0b001, 0b111},  // 5
  {0b111, 0b100, 0b111, 0b101, 0b111},  // 6
  {0b111, 0b001, 0b001, 0b001, 0b001},  // 7
  {0b111, 0b101, 0b111, 0b101, 0b111},  // 8
  {0b111, 0b101, 0b111, 0b001, 0b111},  // 9
};

void fbDrawDigit(int x, int y, uint8_t d, uint16_t c) {
  if (d > 9) return;
  for (int row = 0; row < 5; row++) {
    uint8_t bits = digitBits[d][row];
    for (int col = 0; col < 3; col++) {
      if (bits & (0b100 >> col)) FPIX(x+col, y+row, c);
    }
  }
}

void fbDrawNumber(int x, int y, int num, uint16_t c) {
  if (num > 999) num = 999;
  int d2 = num / 100;
  int d1 = (num / 10) % 10;
  int d0 = num % 10;
  int cx = x;
  if (d2 > 0) {
    fbDrawDigit(cx-8, y, d2, c);
    fbDrawDigit(cx-2, y, d1, c);
    fbDrawDigit(cx+4, y, d0, c);
  } else if (d1 > 0) {
    fbDrawDigit(cx-4, y, d1, c);
    fbDrawDigit(cx+2, y, d0, c);
  } else {
    fbDrawDigit(cx, y, d0, c);
  }
}

// ─── Cube ──────────────────────────
Vec3 cv[8] = {{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
int ee[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};

// ─── Balls ─────────────────────────
Vec3 pos[MAX_BALLS], vel[MAX_BALLS];
float rad[MAX_BALLS];
float bounce[MAX_BALLS];
uint16_t cols[MAX_BALLS];
float hue[MAX_BALLS];

// ─── Projected data (global to avoid stack overflow) ──
Vec3 wp[MAX_BALLS];
int px[MAX_BALLS], py[MAX_BALLS];
float pd[MAX_BALLS];
int order[MAX_BALLS];

// ─── Rotation ──────────────────────
float ax=0, ay=0, az=0;
int dir = 1;
bool lt = false;
float rot[9];
float camDist = 3.0;   // camera distance (zoom)
float focalLen = 96.0;  // focal length (perspective)

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
  float z = p.z + camDist;
  *d = z;
  *sx = CX + (int)(p.x * focalLen / z);
  *sy = CY + (int)(p.y * focalLen / z);
}

uint16_t hsv(float h, float s, float v) {
  h = fmodf(h, 360.0);
  float c = v * s;
  float x = c * (1 - fabsf(fmodf(h/60.0, 2) - 1));
  float m = v - c;
  float r,g,b;
  if (h<60)       {r=c;g=x;b=0;}
  else if (h<120) {r=x;g=c;b=0;}
  else if (h<180) {r=0;g=c;b=x;}
  else if (h<240) {r=0;g=x;b=c;}
  else if (h<300) {r=x;g=0;b=c;}
  else            {r=c;g=0;b=x;}
  return tft.color565((uint8_t)((r+m)*255),(uint8_t)((g+m)*255),(uint8_t)((b+m)*255));
}

void addBall(int i) {
  // Hue determines physical archetype via a weight factor
  // Red (0°):    heavy, slow, big, low bounce   → weight ~1.8
  // Green (120°): medium                         → weight ~1.0
  // Blue (240°):  light, fast, small, high bounce → weight ~0.4
  float h = i * (360.0 / MAX_BALLS);
  float radH = h * (PI / 180.0);
  float weight = 1.1 + cosf(radH) * 0.7;              // 0.4 .. 1.8

  rad[i]    = 0.07 + weight * 0.07;                   // 0.098 .. 0.196
  bounce[i] = 0.88 - weight * 0.22;                   // 0.48 .. 0.79 — heavy balls thud, light balls bounce

  pos[i].x = random(2000)*0.0008 - 0.8;
  pos[i].y = random(2000)*0.0008 - 0.8;
  pos[i].z = random(2000)*0.0008 - 0.8;

  // Light balls get a speed boost, heavy balls are sluggish
  float speedMul = 1.6 - weight * 0.6;                // 0.52 .. 1.36
  vel[i].x = (random(400)*0.0001 - 0.02) * speedMul;
  vel[i].y = (random(400)*0.0001 - 0.02) * speedMul;
  vel[i].z = (random(400)*0.0001 - 0.02) * speedMul;

  hue[i] = h;
  cols[i] = hsv(hue[i], 0.9, 0.85);
}

void setup() {
  pinMode(TOUCH_PIN, INPUT);
  pixels.begin(); pixels.setBrightness(0); pixels.clear(); pixels.show();
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST7735_BLACK);

  for (int i=0; i<nb; i++) addBall(i);
}

void loop() {
  unsigned long t = millis();

  // Touch
  bool touched = (digitalRead(TOUCH_PIN)==HIGH);
  if (touched && !lt) {
    dir = -dir;
    if (nb < MAX_BALLS) { addBall(nb); nb++; }
  }
  lt = touched;

  // ─── Physics ──────────────────────
  const float gravity = 0.005;
  const float damping = 0.997;

  for (int i=0; i<nb; i++) {
    vel[i].y -= gravity;
    vel[i].x *= damping;
    vel[i].y *= damping;
    vel[i].z *= damping;
  }

  for (int i=0; i<nb; i++) {
    for (int j=i+1; j<nb; j++) {
      float dx=pos[i].x-pos[j].x, dy=pos[i].y-pos[j].y, dz=pos[i].z-pos[j].z;
      float d2=dx*dx+dy*dy+dz*dz;
      float minDist=rad[i]+rad[j];
      if (d2 < minDist*minDist && d2 > 0.000001f) {
        float d=sqrtf(d2);
        float nx=dx/d, ny=dy/d, nz=dz/d;
        float overlap=minDist-d;
        float mi=rad[j]*rad[j]*rad[j]/(rad[i]*rad[i]*rad[i]+rad[j]*rad[j]*rad[j]);
        float mj=1-mi;
        pos[i].x+=nx*overlap*mi; pos[i].y+=ny*overlap*mi; pos[i].z+=nz*overlap*mi;
        pos[j].x-=nx*overlap*mj; pos[j].y-=ny*overlap*mj; pos[j].z-=nz*overlap*mj;
        float rv=(vel[i].x-vel[j].x)*nx+(vel[i].y-vel[j].y)*ny+(vel[i].z-vel[j].z)*nz;
        if (rv<0) { float jImp=-rv*1.3f; vel[i].x-=jImp*mi*nx; vel[i].y-=jImp*mi*ny; vel[i].z-=jImp*mi*nz; vel[j].x+=jImp*mj*nx; vel[j].y+=jImp*mj*ny; vel[j].z+=jImp*mj*nz; }
      }
    }
  }

  for (int i=0; i<nb; i++) {
    pos[i].x+=vel[i].x; pos[i].y+=vel[i].y; pos[i].z+=vel[i].z;
    float b=CUBE_BOUND-rad[i];
    if (pos[i].x>b)      {pos[i].x=b;      vel[i].x=-fabsf(vel[i].x)*bounce[i];}
    else if (pos[i].x<-b) {pos[i].x=-b;     vel[i].x= fabsf(vel[i].x)*bounce[i];}
    if (pos[i].y>b)      {pos[i].y=b;      vel[i].y=-fabsf(vel[i].y)*bounce[i];}
    else if (pos[i].y<-b) {pos[i].y=-b;     vel[i].y= fabsf(vel[i].y)*bounce[i];}
    if (pos[i].z>b)      {pos[i].z=b;      vel[i].z=-fabsf(vel[i].z)*bounce[i];}
    else if (pos[i].z<-b) {pos[i].z=-b;     vel[i].z= fabsf(vel[i].z)*bounce[i];}
  }

  // Color cycling
  for (int i=0; i<nb; i++) {
    hue[i] += 1.0;
    if (hue[i]>=360) hue[i]-=360;
    cols[i] = hsv(hue[i], 0.9, 0.85);
  }

  // Dynamic camera — orbits with breathing zoom, wobble, varying speed
  float tSec = t * 0.001;
  camDist = 2.6 + 0.8 * sinf(tSec * 0.13);
  focalLen = 85.0 + 25.0 * sinf(tSec * 0.17 + 1.0);
  float speed = 0.020 + 0.018 * sinf(tSec * 0.09);
  ax += speed * dir * (1.0 + 0.7 * sinf(tSec * 0.19));
  ay += speed * dir * (1.0 + 0.6 * cosf(tSec * 0.23));
  az += speed * dir * (0.6 + 0.5 * sinf(tSec * 0.11));
  compRot(ax,ay,az);

  // Project
  Vec3 tv[8]; int sx[8],sy[8]; float sd[8];
  for (int i=0; i<8; i++) { tv[i]=cv[i]; aRot(&tv[i]); proj(tv[i],&sx[i],&sy[i],&sd[i]); }
  for (int i=0; i<nb; i++) { wp[i]=pos[i]; aRot(&wp[i]); proj(wp[i],&px[i],&py[i],&pd[i]); }

  // ─── RENDER to framebuffer ────────
  fbClear();

  // Sort edges back-to-front
  struct {int a,b;float z;} sed[12];
  for (int i=0;i<12;i++) {int a=ee[i][0],b=ee[i][1];sed[i]=(typeof(sed[0])){a,b,(sd[a]+sd[b])*0.5};}
  for (int i=0;i<11;i++)
    for (int j=0;j<11-i;j++)
      if (sed[j].z>sed[j+1].z) {typeof(sed[0])t=sed[j];sed[j]=sed[j+1];sed[j+1]=t;}

  // Edges — rainbow, cycling, 3px
  for (int i=0;i<12;i++) {
    int a=sed[i].a, b=sed[i].b;
    float d=(sd[a]+sd[b])*0.5;
    uint8_t br=(uint8_t)((1.0-(d-0.5)/5.5)*80+175);
    float eh = fmodf(i*30.0 + t*0.15, 360.0);
    uint16_t ec = hsv(eh, 0.8, br/255.0);
    uint16_t ed = hsv(eh, 0.5, br*0.6/255.0);
    fbDrawLine(sx[a],sy[a],sx[b],sy[b],ec);
    fbDrawLine(sx[a]+1,sy[a],sx[b]+1,sy[b],ec);
    fbDrawLine(sx[a]-1,sy[a],sx[b]-1,sy[b],ed);
  }

  // Sort balls back-to-front (bubble sort)
  for (int i=0;i<nb;i++) order[i]=i;
  for (int i=0;i<nb-1;i++)
    for (int j=0;j<nb-1-i;j++)
      if (pd[order[j]]<pd[order[j+1]]) {int t=order[j];order[j]=order[j+1];order[j+1]=t;}

  for (int si=0;si<nb;si++) {
    int i=order[si];
    if (pd[i]<=0.4||pd[i]>=5.5) continue;
    int vr=2+(int)(rad[i]*18.0/pd[i]);
    if (vr<2) vr=2; if (vr>7) vr=7;

    fbDrawCircle(px[i],py[i],vr+1,tft.color565(20,20,50));
    fbFillCircle(px[i],py[i],vr,((cols[i]>>1)&0x7BEF));
    fbFillCircle(px[i],py[i],vr-1,cols[i]);
    if (vr>=4) fbFillCircle(px[i]-1,py[i]-1,vr-3,tft.color565(255,255,255));
  }

  // Direction indicator (top-right corner)
  for (int y=0;y<9;y++)
    for (int x=WIDTH-18;x<WIDTH;x++)
      FPIX(x,y,ST7735_BLACK);
  uint16_t dc = tft.color565(100,220,255);
  int ax = WIDTH-12, ay = 4;
  if (dir>0) {
    for (int r=0; r<4; r++)
      for (int c=0; c<=r; c++)
        FPIX(ax+r, ay-2+c, dc);
  } else {
    for (int r=0; r<4; r++)
      for (int c=0; c<=r; c++)
        FPIX(ax+3-r, ay-2+c, dc);
  }

  // Ball counter (bottom-right corner)
  for (int y=HEIGHT-8; y<HEIGHT; y++)
    for (int x=WIDTH-22; x<WIDTH; x++)
      FPIX(x,y,ST7735_BLACK);
  fbDrawNumber(WIDTH-8, HEIGHT-7, nb, tft.color565(100,220,255));

  // ─── FLUSH to display in one shot ──
  fbFlush();

  delay(8);
}
