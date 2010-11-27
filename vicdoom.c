// doom for the vic-20
// written for cc65
// to compile, cl65 -t vic20 -C vic20-32k-udg.cfg -O vicdoom.c padding.s drawColumnInternalAsm.s -o vicdoom.prg

// todo
// X 1. move angle to sin/cos to logsin/logcos to a separate function and just use those values
// X 2. fix push_out code
// 3. add transparent objects
// 4. finish map and use that instead of the test map
// 5. enemy AI (update only visible enemies, plus enemies in the current sector)
// 5.5. per sector objects (link list)
// 6. add keys and doors
// 7. add health and weapon
// 8. advance levels
// 9. menus
// 10. more optimization?
// 11. use a double buffer scheme that draws to two different sets of characters and just copies the characters over
// 12. optimize push_out code

// memory map:
// see the .cfg file for how to do this
// startup code is $82 bytes long
// to make this work, I needed a "padding.s" that included ".segment "UDG" .res $77F,0"
// 1000-11FF screen
// 1200-13FF startup + random data
// 1400-15FF character font, copied from rom
// 1600-17FF 8x8 bitmapped display character ram
// 1800-19FF back buffer
// 1A00-1DFF texture data
// 1E00-2DFF level data, loaded from disk (not done yet)
// 2E00-7FFF code/data

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dbg.h>

#define POKE(addr,val) ((*(unsigned char *)(addr)) = val)
#define PEEK(addr) (*(unsigned char *)(addr))

// 64 * sin( 64 values [0...2pi) )
static const signed char sinTab[64] =
{
   0, 6, 12, 19, 24, 30, 36, 41, 45, 49, 53, 56, 59, 61, 63, 64,
   64, 64, 63, 61, 59, 56, 53, 49, 45, 41, 36, 30, 24, 19, 12, 6,
   0, -6, -12, -19, -24, -30, -36, -41, -45, -49, -53, -56, -59, -61, -63, -63,
   -64, -64, -63, -61, -59, -56, -53, -49, -45, -41, -36, -30, -24, -19, -12, -6
};

int __fastcall__ muladd88(int x, int y, int z);
unsigned int __fastcall__ div88(unsigned int x, unsigned int y);
void __fastcall__ setCameraAngle(unsigned char a);
void __fastcall__ setCameraX(int x);
void __fastcall__ setCameraY(int y);
void __fastcall__ transformSectorToScreenSpace(char sectorIndex);
signed char __fastcall__ findFirstEdgeInSpan(signed char x_L, signed char x_R);
int __fastcall__ transformxy_withParams(int x, int y);
int __fastcall__ transformy(void);
int __fastcall__ leftShift4ThenDiv(int p, unsigned int q);
unsigned char __fastcall__ getObjectTexIndex(unsigned int halfWidth, unsigned int x);
void __fastcall__ clearFilled(void);
unsigned char __fastcall__ testFilled(signed char col);

unsigned char *secNumVerts;
int __fastcall__ getScreenX(unsigned char i);
int __fastcall__ getTransformedX(unsigned char i);
int __fastcall__ getTransformedY(unsigned char i);

signed char __fastcall__ get_sin(unsigned char angle)
{
  return sinTab[angle & 63];
}

signed char __fastcall__ get_cos(unsigned char angle)
{
  return sinTab[(angle + 16) & 63];
}

void waitforraster(void)
{
    while (PEEK(0x9004) > 16) ;
    while (PEEK(0x9004) < 16) ;
}

typedef struct vertexT
{
  signed char x;
  signed char y;
} vertex;

typedef struct edgeT
{
  signed char tex;
  signed char sector;
  signed char index;
  char len;
} edge;

typedef struct sectorT
{
  char numverts;
  char dummy;
  char verts[7];
  char edges[7];
} sector;

vertex verts[8] =
{
  { -20, -10 },
  { -20, 10 },
  { 20, 10 },
  { 20, -10 },
  { -10, 10 },
  { -10, 20 },
  { 0, 20 },
  { 0, 10 }
};

edge edges[10] =
{
  { 0, -1, -1, 20 },
  { 1, -1, -1, 10 },
  { -1, 1, 1, 10 },
  { 1, -1, -1, 20 },
  { 2, -1, -1, 20 },
  { 0, -1, -1, 40 },
  { 1, -1, -1, 10 },
  { 2, -1, -1, 10 },
  { 0, -1, -1, 10 },
  { -1, 0, 1, 10 }
};

sector sectors[2] =
{
  { 6, -1, { 0, 1, 4, 7, 2, 3 }, { 0, 1, 2, 3, 4, 5 } },
  { 4, -1, { 4, 5, 6, 7 }, { 6, 7, 8, 9 } }
};

char spanStackSec[10];
signed char spanStackL[10];
signed char spanStackR[10];

typedef struct xfvertexT
{
  short x;
  short y;
  short screenx;
  short dummy;
} xfvertex;

typedef struct objtypeT
{
  char texture;
  char solid;
  char width;
  char dummy;
} objtype;

objtype objtypes[5] =
{
  { 3, 1, 4 }, // imp
  { 4, 1, 8 }, // caco
  { 5, 1, 8 }, // pinky
  { 3, 0, 2 }, // medkit
  { 4, 0, 1 }  // clip
};

typedef struct objectT
{
  short x;
  short y;
  char angle;
  char type;
  char sector;
  char dummy;
} object;

object player = { -19*256, 2*256, 8, 0, 0 };
object *camera = &player;

object objects[5] =
{
  { 0, -256*2, 0, 0, 0 },
  { 256*10, 256*2, 0, 1, 0 },
  { -256*5, 256*15, 0, 2, 1 },
  { -256*10, 0, 0, 3, 3, 0 },
  { -256*10, 256, 0, 4, 0 }
};

// transformed sector verts
xfvertex xfverts[8];

#define SCREENWIDTH 32
#define HALFSCREENWIDTH (SCREENWIDTH/2)
#define SCREENHEIGHT 64
#define HALFSCREENHEIGHT (SCREENHEIGHT/2)
#define PIXELSPERMETER 4
#define TEXWIDTH 16
#define TEXHEIGHT 32
#define INNERCOLLISIONRADIUS 512
#define OUTERCOLLISIONRADIUS 515
#define COLLISIONDELTA (OUTERCOLLISIONRADIUS - INNERCOLLISIONRADIUS)

unsigned char frame = 0;

void __fastcall__ drawColumn(char textureIndex, char texI, signed char curX, short curY, unsigned short h);

void drawWall(char sectorIndex, char curEdgeIndex, char nextEdgeIndex, signed char x_L, signed char x_R)
{
  sector *sec = &sectors[sectorIndex];
  edge *curEdge = &edges[sec->edges[curEdgeIndex]];
  char textureIndex = curEdge->tex;

  // intersect the view direction and the edge
  // http://local.wasp.uwa.edu.au/~pbourke/geometry/lineline2d/
  int x1 = getTransformedX(curEdgeIndex);
  int y1 = getTransformedY(curEdgeIndex);
  int dx = getTransformedX(nextEdgeIndex) - x1;
  int dy = getTransformedY(nextEdgeIndex) - y1;

  //int x3 = 0;
  //int y3 = 0;
  //int y4 = 256*1;

  char edgeLen = curEdge->len;
  int x4;
  int denom;
  signed char curX;
  int numer;
  unsigned int t;
  unsigned int texI;
  unsigned int curY;
  unsigned int h;

  // add 128 to correct for sampling in the center of the column
  x4 = (256*x_L + 128)/HALFSCREENWIDTH;
  for (curX = x_L; curX < x_R; ++curX)
  {
     if (testFilled(curX) == 0)
     {
        //x4 = (256*curX + 128)/HALFSCREENWIDTH;
        x4 += 16;

        // denom = dx - x4 * dy / 256;
        denom = muladd88(-x4, dy, dx);
        if (denom > 0)
        {
           // numer = x4 * ((long)y1) / 256 - x1;
           numer = muladd88(x4, y1, -x1);
           // t = 256 * numer / denom;
           t = div88(numer, denom);
           if (t > 256) t = 256;
           // curY = y1 + t * dy / 256;
           curY = muladd88(t, dy, y1);
           // perspective transform
           // Ys = Yw * (Ds/Dw) ; Ys = screenY, Yw = worldY, Ds = dist to screen, Dw = dist to point
           // h = (SCREENHEIGHT/16)*512/(curY/16);

           h = div88(128, curY);

           texI = t * edgeLen / 64; // 256/PIXELSPERMETER
           texI &= 15; // 16 texel wide texture

           // can look up the yStep (and starting texY) too
           // each is a 512 byte table - hooray for wasting memory
           // on the other hand, since I've already decided to waste 2k on a multiply table, I might as well use another 2k for lookups where appropriate
           drawColumn(textureIndex, texI, curX, curY, h);
        }
     }
  }
}

void drawObjectInSector(char o, int vx, int vy, signed char x_L, signed char x_R)
{
  // perspective transform (see elsewhere for optimization)
  //int h = (SCREENHEIGHT/16) * 512 / (vy/16);
  unsigned int h = div88(128, vy);
  unsigned int w = h/3;
  char textureIndex = objtypes[objects[o].type].texture;
  int sx;
  int leftX;
  int rightX;
  int startX;
  int endX;
  signed char curX;
  char texI;
  if (w > 0)
  {
     //sx = vx / (vy / HALFSCREENWIDTH);
     sx = leftShift4ThenDiv(vx, vy);
     leftX = sx - w;
     rightX = sx + w;
     startX = leftX;
     endX = rightX;
     if (startX < x_L) startX = x_L;
     if (endX > x_R) endX = x_R;
     if (startX < x_R && endX > x_L)
     {
        for (curX = startX; curX < endX; ++curX)
        {
           if (testFilled(curX) == 0)
           {
              // compensate for pixel samples being mid column
              //texI = TEXWIDTH * (2*(curX - leftX) + 1) / (4 * w);
              texI = getObjectTexIndex(w, curX - leftX);
              if ((frame & 4) != 0) texI = (TEXWIDTH - 1) - texI;
              drawColumn(textureIndex, texI, curX, vy, h);
           }
        }
     }
  }
}

typedef struct objxyT
{
  char o;
  short x;
  unsigned short y;
  char dummy;
  short dummy2;
} objxy;

objxy unsorted[12];
char sorted[12];

void drawObjectsInSector(char sectorIndex, signed char x_L, signed char x_R)
{
  object *obj;
  int vx, vy;
  objxy *objInst;
  char o, i, j;
  char count = 0;

  // loop through the objects
  for (o = 0; o < 3; ++o)
  {
     obj = &objects[o];

     if (obj->sector == sectorIndex)
     {
        // inverse transform
        vx = transformxy_withParams(obj->x, obj->y);
        vy = transformy();
        
        if (vy > 0)
        {
           sorted[count] = count;

           objInst = &unsorted[count];
           objInst->x = vx;
           objInst->y = vy;
           objInst->o = o;

           ++count;
        }
     }
  }

  if (count > 0)
  {
	  // sort
	  for (i = 0; i < count - 1; ++i)
	  {
		 for (j = i + 1; j < count; ++j)
		 {
			if (unsorted[sorted[i]].y > unsorted[sorted[j]].y)
			{
			   o = sorted[j];
			   sorted[j] = sorted[i];
			   sorted[i] = o;
			}
		 }
	  }

	  // draw
	  for (i = 0; i < count; ++i)
	  {
		 objInst = &unsorted[sorted[i]];
		 drawObjectInSector(objInst->o, objInst->x, objInst->y, x_L, x_R);
	  }
	}
}

void drawSpans()
{
  signed char stackTop = 0;
  char sectorIndex;
  signed char x_L, x_R;
  sector *sec;
  signed char firstEdge;
  char curEdge;
  signed char curX;
  char nextEdge;
  short nextX;
  signed char thatSector;

  clearFilled();

  spanStackSec[0] = camera->sector;
  spanStackL[0] = -HALFSCREENWIDTH;
  spanStackR[0] = HALFSCREENWIDTH;

  while (stackTop >= 0)
  {
     sectorIndex = spanStackSec[stackTop];
     x_L = spanStackL[stackTop];
     x_R = spanStackR[stackTop];
     --stackTop;

     // STEP 1 - draw objects belonging to this sector!
     // fill in the table of written columns as we progress
     drawObjectsInSector(sectorIndex, x_L, x_R);

     //POKE(0x900f, 11);
     transformSectorToScreenSpace(sectorIndex);
     //POKE(0x900f, 13);

     firstEdge = findFirstEdgeInSpan(x_L, x_R);
     // didn't find a first edge - must be behind
     if (firstEdge == -1)
     {
        continue;
     }
     
     // now fill the span buffer with these edges

     sec = &sectors[sectorIndex];
     curEdge = firstEdge;
     curX = x_L;
     while (curX < x_R)
     {
        // update the edge
        nextEdge = curEdge + 1;
        if (nextEdge == sec->numverts) nextEdge = 0;
        nextX = getScreenX(nextEdge);
        if (nextX < curX || nextX > x_R) nextX = x_R;

        thatSector = edges[sec->edges[curEdge]].sector;
        if (thatSector != -1)
        {
           // come back to this
           ++stackTop;
           spanStackSec[stackTop] = thatSector;
           spanStackL[stackTop] = curX;
           spanStackR[stackTop] = nextX;
        }
        else
        {
           drawWall(sectorIndex, curEdge, nextEdge, curX, nextX);
        }
        curX = nextX;
        curEdge = nextEdge;
     }
  }
}

unsigned int sqrt(unsigned long x)
{
  unsigned long m = 0x40000000;
  unsigned long y = 0;
  unsigned long b;
  while (m != 0)
  {
     b = y | m;
     y = y >> 1;
     if (x >= b)
     {
        x -= b;
        y = y | m;
     }
     m = m >> 2;
  }
  return ((unsigned int)y);
}

// THIS IS THE NEXT TARGET OF OPTIMIZATION!

int push_out(object *obj)
{
  // probably a good idea to check the edges we can cross first
  // if any of them teleport us, move, then push_out in the new sector

  char curSector = obj->sector;
  sector *sec = &sectors[curSector];
  char i, ni;
  edge *curEdge;
  vertex *v1;
  vertex *v2;
  long ex;
  long ey;
  long px, py;
  long height;
  long edgeLen;
  long dist;
  long distanceToPush;
  long dx, dy;
  
  // see which edge the new coordinate is behind
  for (i = 0; i < sec->numverts; ++i)
  {
     ni = (i + 1);
     if (ni == sec->numverts) ni = 0;
     curEdge = &edges[sec->edges[i]];
     v1 = &verts[sec->verts[i]];
     v2 = &verts[sec->verts[ni]];
     ex = ((long)v2->x) - v1->x;
     ey = ((long)v2->y) - v1->y;
     px = obj->x - 256*v1->x;
     py = obj->y - 256*v1->y;
     // need to precalc 65536/edge.len
     edgeLen = curEdge->len;
     height = (px * ey - py * ex) / edgeLen;
     if (height < INNERCOLLISIONRADIUS)
     {
        // check we're within the extents of the edge
        dist = (px * ex + py * ey)/edgeLen;
        if (dist > 0 && dist < 256*edgeLen)
        {
           if (curEdge->sector != -1)
           {
              if (height < 0)
              {
                 obj->sector = curEdge->sector;
                 return 1;
              }
           }
           else
           {
              // try just pushing out
              distanceToPush = OUTERCOLLISIONRADIUS - height;
              obj->x += distanceToPush * ey / edgeLen;
              obj->y -= distanceToPush * ex / edgeLen;
              return 1;
           }
        }
        else if (dist > -INNERCOLLISIONRADIUS
          && dist < 256*edgeLen + INNERCOLLISIONRADIUS)
        {
          if (dist <= 0)
			{
			   height = sqrt(px * px + py * py);
			   distanceToPush = INNERCOLLISIONRADIUS - height;
			   if (distanceToPush > 0)
			   {
				  distanceToPush += COLLISIONDELTA;
				  obj->x += distanceToPush * px / height;
				  obj->y += distanceToPush * py / height;
				  return 1;
			   }
			}
			else
			{
			   dx = obj->x - 256*v2->x;
			   dy = obj->y - 256*v2->y;
			   height = sqrt(dx * dx + dy * dy);
			   distanceToPush = INNERCOLLISIONRADIUS - height;
			   if (distanceToPush > 0)
			   {
				  distanceToPush += COLLISIONDELTA;
				  obj->x += distanceToPush * dx / height;
				  obj->y += distanceToPush * dy / height;
				  return 1;
			   }
			}
		}
     }
  }
  return 0;
}

void clearSecondBuffer(void);
void copyToPrimaryBuffer(void);

char keys;
char counter = 0;
char turnSpeed = 0;
char shells = 40;
char armor = 0;
char health = 100;
char shotgunStage = 0;
char changeLookTime = 7;
char lookDir = 0;
  
int main()
{
  int i, x, y;

  POKE(0x900E, 16*6 + (PEEK(0x900E)&15)); // blue aux color
  POKE(0x900F, 8 + 5); // green border, and black screen

  for (i = 0; i < 512; ++i)
  {
      // fill the screen with spaces
	  POKE(0x1000 + i, 32);
	  // set the color memory
	  POKE(0x9400 + i, 2); // main colour red, multicolour
  }
  // write an 8x8 block for the graphics
  // into the middle of the screen
  for (x = 0; x < 8; ++x)
  {
    for (y = 0; y < 8; ++y)
    {
      POKE(0x1000 + (x + 7) + 22*(y + 4), 64 + 8*x + y);
      POKE(0x9400 + (x + 7) + 22*(y + 4), 8 + 2);
    }
  }
  // set the character set to $1400
  POKE(0x9005, 13 + (PEEK(0x9005)&240));
  textcolor(6);
  gotoxy(0,17);
  cprintf("######################");
  gotoxy(0,19);
  cprintf("######################");
  textcolor(2);
  gotoxy(0,18);
  cprintf("knee deep in the dead!");
  gotoxy(0,21);
  textcolor(3);
  cprintf(" &40");
  textcolor(5);
  cprintf(" :000 ");
  textcolor(7);
  cprintf("()");
  textcolor(2);
  cprintf(" /100");
  textcolor(6);
  cprintf(" ;;;");
  gotoxy(10,20);
  textcolor(7);
  cprintf("$%%");
  gotoxy(10,22);
  cprintf("*+");

  while (1)
  {
	  // note: XXXXYZZZ (X = screen, Y = reverse mode, Z = border)
	  POKE(0x900F, 8 + 5); // green border, and black screen

 	  // query the keyboard line containing <Ctrl>ADGJL;<Right>
	  POKE(0x9120, 0xFB);
	  keys = PEEK(0x9121);
	  if ((keys & 16) == 0)
	  {
	    if (turnSpeed < 2)
	    {
    	    turnSpeed++;
    	}
	    player.angle -= turnSpeed;
	  }
	  else if ((keys & 32) == 0)
	  {
	    if (turnSpeed < 2)
	    {
    	    turnSpeed++;
    	}
	    player.angle += turnSpeed;
	  }
	  else
	  {
	    turnSpeed = 0;
	  }
	  player.angle &= 63;
	  setCameraAngle(player.angle);
	  if ((keys & 2) == 0)
	  {
		player.x -= 4*get_cos(player.angle);
		player.y += 4*get_sin(player.angle);
	  }
	  if ((keys & 4) == 0)
	  {
		player.x += 4*get_cos(player.angle);
		player.y -= 4*get_sin(player.angle);
	  }

	  // query the keyboard line containing <Left>WRYIP*<Ret>
	  POKE(0x9120, 0xFD);
	  keys = PEEK(0x9121);
	  if ((keys & 2) == 0)
	  {
		player.x += 8*get_sin(player.angle);
		player.y += 8*get_cos(player.angle);
	  }
	  if (shotgunStage > 0) shotgunStage--;
	  if ((keys & 16) == 0)
	  {
		// pressed fire
		if (shells > 0 && shotgunStage == 0)
		{
		  shells--;
		  gotoxy(2,21);
		  textcolor(3);
		  cprintf("%02d", shells);
		  POKE(0x900F, 8+1);
		  shotgunStage = 7;
		}
	  }

	  // query the keyboard line containing <CBM>SFHK:=<F3>
	  POKE(0x9120, 0xDF);
	  keys = PEEK(0x9121);
	  if ((keys & 2) == 0)
	  {
		player.x -= 8*get_sin(player.angle);
		player.y -= 8*get_cos(player.angle);
	  }
#if 1
      POKE(0x900f, 11);
	  if (push_out(&player))
	  {
		push_out(&player);
	  }
      POKE(0x900f, 13);
#endif      
      setCameraX(player.x);
      setCameraY(player.y);

      clearSecondBuffer();
	  // draw to second buffer
	  drawSpans();
	  // this takes about 30 raster lines
	  copyToPrimaryBuffer();
	  
	  ++frame;
	  frame &= 7;
	  
	  --changeLookTime;
	  if (changeLookTime == 0)
	  {
	    lookDir = 1 - lookDir;
	    gotoxy(10,21);
		textcolor(7);
		if (lookDir == 0)
		{
  	      changeLookTime = 12;
  		  cprintf("[\\");
  		}
  		else
  		{
  	      changeLookTime = 6;
  		  cprintf("()");
  		}
      }
	  
	  counter++;
	  POKE(0x1000, counter);
	}  
  
  return EXIT_SUCCESS;
}