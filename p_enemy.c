// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// $Log:$
//
// DESCRIPTION:
//	Enemy thinking, AI.
//	Action Pointer Functions
//	that are associated with states/frames. 
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <conio.h>

#include "playSound.h"
#include "p_enemy.h"
#include "player.h"
#include "mapAsm.h"

#define fixed_t int
#define boolean char
#define false 0
#define true 1
#define FRACUNIT 256

// player radius for movement checking
#define PLAYERRADIUS	(16*FRACUNIT) // for reference
#define MELEERANGE		(64*FRACUNIT)

#define INNERCOLLISIONRADIUS 512

#define sfx_pistol SOUND_PISTOL
#define sfx_claw SOUND_CLAW

#define MF_JUSTHIT 1
#define MF_JUSTATTACKED 2
#define MF_SHOOTABLE 4
#define MF_AMBUSH 8
#define MF_SOLID 16
#define MF_WASSEENTHISFRAME 32

#define MT_TROOPSHOT 6

typedef struct
{
   char speed;
   char seesound;
   char activesound;
   char painsound;
   char meleesound;
   char missilesound;
   
   char spawnhealth;
   char painchance;

   char spawnstate;
   char chasestate;
   char painstate;
   char meleestate;
   char shootstate;
   char deathstate;
}
mobjInfo_t;

// states are specific to enemy types
#define STATE_POSLOOK 0
#define STATE_POSCHASE 1
#define STATE_POSPAIN 2
#define STATE_POSSHOOT 3
#define STATE_POSFALL 4

#define MOBJINFO_POSSESSED 0

typedef struct
{
   char allocated;
   char mobjIndex;
   int x;
   int y;
   signed char momx;
   signed char momy;
   char sector;
   char movedir;
   char flags;
   char reactiontime;
   signed char movecount;
   signed char health;
   char infoType;
   char stateIndex;
}
mobj_t;

// TODO: fill out
mobjInfo_t mobjinfo[1] =
{
  { 4, -1, SOUND_GURGLE, SOUND_POPAIN, -1, SOUND_PISTOL, 30, 50,
    STATE_POSLOOK, STATE_POSCHASE, STATE_POSPAIN, -1, STATE_POSSHOOT, STATE_POSFALL }
};

void A_Look(mobj_t *);
void A_Chase(mobj_t *);
void A_Flinch(mobj_t *);
void A_Melee(mobj_t *);
void A_Shoot(mobj_t *);
void A_Fall(mobj_t *);

// actions are global
#define ACTION_LOOK 0
#define ACTION_CHASE 1
#define ACTION_FLINCH 2
#define ACTION_MELEE 3
#define ACTION_SHOOT 4
#define ACTION_FALL 5

typedef struct
{
   char texture;
   char actionIndex;
}
mobjState_t;

mobjState_t states[] =
{
  { 5, ACTION_LOOK },
  { 5, ACTION_CHASE },
  { 7, ACTION_FLINCH },
  { 6, ACTION_SHOOT },
  { 7, ACTION_FALL }
};

void callAction(mobj_t *obj)
{
   gotoxy(1,0);
   switch (states[obj->stateIndex].actionIndex)
   {
   case ACTION_LOOK:
     cputs("look. ");
     A_Look(obj);
     break;
   case ACTION_CHASE:
     cputs("chase. ");
     A_Chase(obj);
     break;
   case ACTION_FLINCH:
     cputs("flinch. ");
     A_Flinch(obj);
     break;
   case ACTION_MELEE:
     cputs("melee. ");
     A_Melee(obj);
     break;
   case ACTION_SHOOT:
     cputs("shoot. ");
     A_Shoot(obj);
     break;
   case ACTION_FALL:
     cputs("fall. ");
     A_Fall(obj);
     break;
   }
   gotoxy(0,1);
   cprintf("x %d y %d s %d. ", obj->x, obj->y, obj->sector);
   gotoxy(0,2);
   cprintf("dir %d rt %d mc %d. ", obj->movedir, obj->reactiontime, obj->movecount);
}

char getTexture(mobj_t *obj)
{
   return states[obj->stateIndex].texture;
}

char numMobj = 0;
#define MAX_MOBJ 16
#define MAX_OBJ 32

mobj_t mobjs[MAX_MOBJ];
char objForMobj[MAX_MOBJ];
char mobjForObj[MAX_OBJ];

int allocated = 0;
char allocMobj(char o)
{
  char i;
  mobj_t *mobj;
  if (allocated) return -1;
  allocated = 1;
  for (i = 0; i < MAX_MOBJ; ++i)
  {
	mobj = &mobjs[i];
    if (!mobj->allocated)
    {
      objForMobj[i] = o;
      mobjForObj[o] = i;

      mobj->allocated = true;
      mobj->mobjIndex = i;
      mobj->x = getObjectX(o);
      mobj->y = getObjectY(o);
      mobj->momx = 0;
      mobj->momy = 0;
      mobj->sector = getObjectSector(o);
      mobj->movedir = 0;
      mobj->flags = 0;
      mobj->reactiontime = 2;
      mobj->movecount = 0;
      switch (getObjectType(o))
      {
      case 0:
        mobj->health = 20;
        mobj->infoType = 0;
        mobj->stateIndex = STATE_POSCHASE;
        break;
      case 1:
        mobj->health = 50;
        mobj->infoType = 0;
        mobj->stateIndex = STATE_POSCHASE;
        break;
      case 2:
        mobj->health = 50;
        mobj->infoType = 0;
        mobj->stateIndex = STATE_POSCHASE;
        break;
      case 3:
        mobj->health = 100;
        mobj->infoType = 0;
        mobj->stateIndex = STATE_POSCHASE;
        break;
      case 4:
        mobj->health = 300;
        mobj->infoType = 0;
        mobj->stateIndex = STATE_POSCHASE;
        break;
      }
      return i;
    }
  }
  return -1;
}

void p_enemy_think(char o)
{
  if (getObjectType(o) < 5)
  {
    char mobjIndex = mobjForObj[o];
    mobj_t *mobj = &mobjs[mobjIndex];
    callAction(mobj);
  }
}

char P_Random(void);

//
// P_ApproxDistance
// Gives an estimation of distance (not exact)
//

fixed_t
P_ApproxDistance
( fixed_t	dx,
  fixed_t	dy )
{
    dx = abs(dx);
    dy = abs(dy);
    if (dx < dy)
		return dx+dy-(dx>>1);
    return dx+dy-(dy>>1);
}


typedef enum
{
    DI_EAST,
    DI_NORTHEAST,
    DI_NORTH,
    DI_NORTHWEST,
    DI_WEST,
    DI_SOUTHWEST,
    DI_SOUTH,
    DI_SOUTHEAST,
    DI_NODIR,
    NUMDIRS
    
} dirtype_t;


//
// P_NewChaseDir related LUT.
//
dirtype_t opposite[] =
{
  DI_WEST, DI_SOUTHWEST, DI_SOUTH, DI_SOUTHEAST,
  DI_EAST, DI_NORTHEAST, DI_NORTH, DI_NORTHWEST, DI_NODIR
};

dirtype_t diags[] =
{
    DI_NORTHWEST, DI_NORTHEAST, DI_SOUTHWEST, DI_SOUTHEAST
};



//
// ENEMY THINKING
// Enemies are allways spawned
// with targetplayer = -1
// Most monsters are spawned unaware of all players,
// but some can be made preaware
//


//
// P_CheckSight & P_LookForPlayers
//
// hacked version of this for D20M
//

boolean P_CheckSight(mobj_t *actor)
{
  if (actor->sector == playerSector) return true;
  // this table will be cleared at the start of render
  // and filled in during render
  if (actor->flags & MF_WASSEENTHISFRAME) return true;
  return false;
}

boolean P_LookForPlayers(mobj_t *actor)
{
	if (P_CheckSight(actor))
	{
		return true;
	}
	return false;
}

void S_StartSound(mobj_t *actor, char sound)
{
   // just try to play it
   // will succeed or fail based on priorities
   // perhaps set a volume based on actor position?
   playSound(sound);
}

void P_SetMobjState(mobj_t *actor, char state)
{
  actor->stateIndex = state;
}

void P_DamageMobj(mobj_t *actor, int damage)
{
	actor->health -= damage;
	if (actor->health <= 0)
	{
		// kill actor - FIX!
		P_SetMobjState(actor, mobjinfo[actor->infoType].deathstate);
	}
	else
	{
		actor->flags |= MF_JUSTATTACKED;
	}
}

void P_RadiusAttack(mobj_t *actor, int radius)
{
   // attempt to damage the player
    fixed_t	dist;
	
    dist = P_ApproxDistance(playerx-actor->x, playery-actor->y);
    if (dist < radius)
    {
      //damagePlayer(20);
    }
}

//
// P_CheckMeleeRange
//
boolean P_CheckMeleeRange(mobj_t *actor)
{
    fixed_t	dist;
	
    dist = P_ApproxDistance(playerx - actor->x, playery - actor->y);

    if (dist >= MELEERANGE)
	return false;
	
    if (! P_CheckSight (actor) )
	return false;
							
    return true;		
}

//
// P_CheckMissileRange
//
boolean P_CheckMissileRange (mobj_t* actor)
{
    fixed_t	dist;
	
    if (! P_CheckSight (actor) )
	return false;
	
    if ( actor->flags & MF_JUSTHIT )
    {
	// the target just hit the enemy,
	// so fight back!
	actor->flags &= ~MF_JUSTHIT;
	return true;
    }
	
    if (actor->reactiontime)
	return false;	// do not attack yet
		
    // OPTIMIZE: get this from a global checksight
    dist = P_ApproxDistance ( actor->x - playerx,
			     actor->y - playery) - 64*FRACUNIT;

    if (!mobjinfo[actor->infoType].meleestate)
	dist -= 128*FRACUNIT;	// no melee attack, so fire more

    dist >>= 16;
    
    if (dist > 200)
	dist = 200;
		
    if (P_Random () < dist)
	return false;
		
    return true;
}


//
// P_Move
// Move in the current direction,
// returns false if the move is blocked.
//
#define MIN_SPEED 32
#define FU_45 22
fixed_t	xspeed[8] = {MIN_SPEED,FU_45,0,-FU_45,-MIN_SPEED,-FU_45,0,FU_45};
fixed_t yspeed[8] = {0,FU_45,MIN_SPEED,FU_45,0,-FU_45,-MIN_SPEED,-FU_45};

int try_move(mobj_t *actor, int tryx, int tryy)
{
  // probably a good idea to check the edges we can cross first
  // if any of them teleport us, move, then push_out in the new sector

  char thatSector;
  char i, ni;
  int v1x, v1y, v2x, v2y;
  long ex;
  long ey;
  long px, py;
  long height;
  long edgeLen;
  long dist;
  long dx, dy;
  char edgeGlobalIndex;
  char curSector = actor->sector;
  char secNumVerts = getNumVerts(curSector);
  
  // see which edge the new coordinate is behind
  for (i = 0; i < secNumVerts; ++i)
  {
	 ni = getNextEdge(curSector, i);
     v1x = getSectorVertexX(curSector, i);
     v1y = getSectorVertexY(curSector, i);
     v2x = getSectorVertexX(curSector, ni);
     v2y = getSectorVertexY(curSector, ni);
     ex = ((long)v2x) - v1x;
     ey = ((long)v2y) - v1y;
     px = tryx - 256*v1x;
     py = tryy - 256*v1y;
     edgeLen = getEdgeLen(curSector, i);
     height = (px * ey - py * ex) / edgeLen;
     if (height < INNERCOLLISIONRADIUS)
     {
        // check we're within the extents of the edge
        dist = (px * ex + py * ey)/edgeLen;
        if (dist > 0 && dist < 256*edgeLen)
        {
           thatSector = getOtherSector(curSector, i);
           edgeGlobalIndex = getEdgeIndex(curSector, i);
           if (thatSector != -1)// && doorClosedAmount[edgeGlobalIndex] == 0)
           {
              if (height < 0)
              {
                 return thatSector;
              }
           }
           else
           {
              // hit a wall
              return -1;
           }
        }
        else if (dist > -INNERCOLLISIONRADIUS
          && dist < 256*edgeLen + INNERCOLLISIONRADIUS)
        {
          if (dist > 0)
          {
			   dx = tryx - 256*v2x;
			   dy = tryy - 256*v2y;
		  }
		   height = px * px + py * py;
		   if (height < INNERCOLLISIONRADIUS*INNERCOLLISIONRADIUS)
		   {
		      return -1;
		   }
		}
     }
  }
  return curSector;
}



boolean P_TryMove(mobj_t *actor, fixed_t tryx, fixed_t tryy)
{
   // TODO: check the move is valid
   // also, copy the position to the object
   // and, update the sector!
   char nextSector = try_move(actor, tryx, tryy);
   if (nextSector != -1)
   {
     char o = objForMobj[actor->mobjIndex];
     actor->x = tryx;
     actor->y = tryy;
     actor->sector = nextSector;
     
     setObjectX(o, actor->x);
     setObjectY(o, actor->y);
     setObjectSector(o, actor->sector);
   }

   return true;
}

boolean P_Move (mobj_t*	actor)
{
    fixed_t	tryx;
    fixed_t	tryy;
    
    // warning: 'catch', 'throw', and 'try'
    // are all C++ reserved words
		
    if (actor->movedir == DI_NODIR)
	return false;
		
    tryx = actor->x + mobjinfo[actor->infoType].speed*xspeed[actor->movedir];
    tryy = actor->y + mobjinfo[actor->infoType].speed*yspeed[actor->movedir];

	// need to replace this function with one of my own devising
	// should just check the lines of this sector
    return P_TryMove (actor, tryx, tryy);
}


//
// TryWalk
// Attempts to move actor on
// in its current (obj->movedir) direction.
// If blocked by either a wall or an actor
// returns FALSE
// If move is either clear or blocked only by a door,
// returns TRUE and sets...
// If a door is in the way,
// an OpenDoor call is made to start it opening.
//
boolean P_TryWalk (mobj_t* actor)
{	
    if (!P_Move (actor))
    {
	   return false;
    }

    actor->movecount = P_Random()&3; // was 15!
    return true;
}




void P_NewChaseDir (mobj_t*	actor)
{
    fixed_t	deltax;
    fixed_t	deltay;
    
    dirtype_t	d[3];
    
    int		tdir;
    dirtype_t	olddir;
    
    dirtype_t	turnaround;

    olddir = actor->movedir;
    turnaround=opposite[olddir];

    deltax = playerx - actor->x;
    deltay = playery - actor->y;

    if (deltax>FRACUNIT)
	d[1]= DI_EAST;
    else if (deltax<-FRACUNIT)
	d[1]= DI_WEST;
    else
	d[1]=DI_NODIR;

    if (deltay<-FRACUNIT)
	d[2]= DI_SOUTH;
    else if (deltay>FRACUNIT)
	d[2]= DI_NORTH;
    else
	d[2]=DI_NODIR;
	
	gotoxy(0,12);
	cprintf("dx %d dy %d. ", deltax, deltay);
	gotoxy(0,13);
	cprintf("d1 %d d2 %d. ", d[1], d[2]);

    // try direct route
    if (d[1] != DI_NODIR
	&& d[2] != DI_NODIR)
    {
	actor->movedir = diags[((deltay<0)<<1)+(deltax>0)];
	if (actor->movedir != turnaround && P_TryWalk(actor))
	    return;
    }

    // try other directions
    if (P_Random() > 200
	||  abs(deltay)>abs(deltax))
    {
	tdir=d[1];
	d[1]=d[2];
	d[2]=tdir;
    }

    if (d[1]==turnaround)
	d[1]=DI_NODIR;
    if (d[2]==turnaround)
	d[2]=DI_NODIR;
	
    if (d[1]!=DI_NODIR)
    {
	actor->movedir = d[1];
	if (P_TryWalk(actor))
	{
	    // either moved forward or attacked
	    return;
	}
    }

    if (d[2]!=DI_NODIR)
    {
	actor->movedir =d[2];

	if (P_TryWalk(actor))
	    return;
    }

    // there is no direct path to the player,
    // so pick another direction.
    if (olddir!=DI_NODIR)
    {
	actor->movedir =olddir;

	if (P_TryWalk(actor))
	    return;
    }

    // randomly determine direction of search
    if (P_Random()&1) 	
    {
	for ( tdir=DI_EAST;
	      tdir<=DI_SOUTHEAST;
	      tdir++ )
	{
	    if (tdir!=turnaround)
	    {
		actor->movedir =tdir;
		
		if ( P_TryWalk(actor) )
		    return;
	    }
	}
    }
    else
    {
	for ( tdir=DI_SOUTHEAST;
	      tdir != (DI_EAST-1);
	      tdir-- )
	{
	    if (tdir!=turnaround)
	    {
		actor->movedir =tdir;
		
		if ( P_TryWalk(actor) )
		    return;
	    }
	}
    }

    if (turnaround !=  DI_NODIR)
    {
	actor->movedir =turnaround;
	if ( P_TryWalk(actor) )
	    return;
    }

    actor->movedir = DI_NODIR;	// can not move
}


//
// ACTION ROUTINES
//

//
// A_Look
// Stay in state until a player is sighted.
//
void A_Look (mobj_t* actor)
{
    if ((actor->flags & MF_WASSEENTHISFRAME)
		|| actor->sector == playerSector)
	{
	    S_StartSound (actor, mobjinfo[actor->infoType].seesound);
	    // go into chase state
	    P_SetMobjState (actor, mobjinfo[actor->infoType].chasestate);
	}
}


//
// A_Chase
// Actor has a melee attack,
// so it tries to close as fast as possible
//
void A_Chase (mobj_t*	actor)
{
    if (actor->reactiontime)
	actor->reactiontime--;
				
    // do not attack twice in a row
    if (actor->flags & MF_JUSTATTACKED)
    {
		actor->flags &= ~MF_JUSTATTACKED;
	    P_NewChaseDir (actor);
		return;
    }
    
    // check for melee attack
    if (mobjinfo[actor->infoType].meleestate != 0xff
	&& P_CheckMeleeRange (actor))
    {
	if (mobjinfo[actor->infoType].meleesound != 0xff)
	    S_StartSound (actor, mobjinfo[actor->infoType].meleesound);

	P_SetMobjState (actor, mobjinfo[actor->infoType].meleestate);
	return;
    }
    
    // check for missile attack
    if (mobjinfo[actor->infoType].shootstate)
    {
	if (actor->movecount)
	{
	    goto nomissile;
	}
	
	if (!P_CheckMissileRange (actor))
	    goto nomissile;
	
	P_SetMobjState (actor, mobjinfo[actor->infoType].shootstate);
	actor->flags |= MF_JUSTATTACKED;
	return;
    }

    // ?
  nomissile:
    
    // chase towards player
    if (--actor->movecount < 0
		|| !P_Move (actor))
    {
		P_NewChaseDir (actor);
    }
    
    // make active sound
    if (mobjinfo[actor->infoType].activesound != -1
		&& P_Random () < 3)
    {
		S_StartSound (actor, mobjinfo[actor->infoType].activesound);
    }
}


char R_PointToAngle(int x, int y)
{
    if (x>= 0)
    {
		if (y>= 0)
		{
			if (x>2*y)
			{
				return DI_EAST;
			}
			else if (y > 2*x)
			{
				return DI_NORTH;
			}
			else
			{
				return DI_NORTHEAST;
			}
		}
		else
		{
			// y<0
			y = -y;

			if (x>2*y)
			{
				return DI_EAST;
			}
			else if (y > 2*x)
			{
				return DI_SOUTH;
			}
			else
			{
				return DI_SOUTHEAST;
			}
		}
    }
    else
    {
		// x<0
		x = -x;

		if (y>= 0)
		{
			if (x>2*y)
			{
				return DI_WEST;
			}
			else if (y > 2*x)
			{
				return DI_NORTH;
			}
			else
			{
				return DI_NORTHEAST;
			}
		}
		else
		{
			// y<0
			y = -y;

			if (x>2*y)
			{
				return DI_WEST;
			}
			else if (y > 2*x)
			{
				return DI_SOUTH;
			}
			else
			{
				return DI_SOUTHWEST;
			}
		}
    }
    return 0;
}

//
// A_FaceTarget
//
void A_FaceTarget (mobj_t* actor)
{	
    actor->flags &= ~MF_AMBUSH;
	
    actor->movedir = R_PointToAngle (actor->x - playerx,
				    actor->y - playery);
}


//
// A_Shoot
//
void A_Shoot (mobj_t* actor)
{
    int		damage;
    fixed_t dist;
	
    A_FaceTarget (actor);

    S_StartSound (actor, sfx_pistol);
    dist = P_ApproxDistance(actor->x - playerx, actor->y - playery);
    if (dist > 220) dist == 220;
    if (P_Random() > dist)
    {
	    damage = ((P_Random()&3)+2)*3; // this was ((r%5)+1)*3
	    //damagePlayer(damage);
	}
	P_SetMobjState (actor, mobjinfo[actor->infoType].chasestate);
}


//
// A_Melee
//
void A_Melee (mobj_t* actor)
{
    int		damage;
	
    A_FaceTarget (actor);
    if (P_CheckMeleeRange (actor))
    {
		damage = ((P_Random()&7)+1)*3;
		//damagePlayer (damage);
    }
    else
    {    
		// launch a missile
		//P_SpawnMissile (actor, MT_TROOPSHOT);
	}
	P_SetMobjState (actor, mobjinfo[actor->infoType].chasestate);
}


void A_Fall (mobj_t *actor)
{
   if (--actor->reactiontime == 0)
   {
     // make the object into a static corpse
     mobjs[actor->mobjIndex].allocated = false;
   }
}

void A_Flinch(mobj_t *actor)
{
  if (--actor->reactiontime == 0)
  {
	P_SetMobjState (actor, mobjinfo[actor->infoType].chasestate);
  }
}

//
// A_Explode
//
void A_Explode (mobj_t* thingy)
{
    P_RadiusAttack ( thingy, 128 );
}
