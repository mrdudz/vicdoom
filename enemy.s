.setcpu	"6502"
.autoimport	on
.importzp sp

.export _P_ApproxDistance
.export _setMobjIndex
.export _mobjAllocated
.export _setMobjAllocated
.export _mobjMovedir
.export _setMobjMovedir
.export _mobjFlags
.export _setMobjFlags
.export _removeMobjFlags
.export _addMobjFlags
.export _testMobjFlags
.export _mobjReactiontime
.export _setMobjReactiontime
.export _decMobjReactiontime
.export _mobjMovecount
.export _setMobjMovecount
.export _decMobjMovecount
.export _incMobjMovecount
.export _mobjHealth
.export _setMobjHealth
.export _mobjInfoType
.export _setMobjInfoType
.export _mobjStateIndex
.export _setMobjStateIndex

.export _setMobjCurrentType
.export _getMobjSpeed
.export _getMobjPainChance
.export _getMobjSpawnHealth
.export _getMobjChaseState
.export _getMobjPainState
.export _getMobjMeleeState
.export _getMobjShootState
.export _getMobjDeathState

.export _texFrameTexture
.export _texFrameSolid
.export _texFrameWidthScale
.export _texFrameStartY
.export _texFrameHeight
.export _texFrameStartX
.export _texFrameWidth


.segment "LOWCODE"

;
; P_ApproxDistance
; Gives an estimation of distance (not exact)
;

dy:
.byte 0
dx:
.byte 0

_P_ApproxDistance:

; TOS - dx
; AX - dy

; just bin the lower byte

txa
bpl :+
eor #$ff
:
sta dy


ldy #1
lda (sp),y
bpl :+
eor #$ff
:
sta dx

; dx > dy, dx+dy/2
; dx < dy, dx/2+dy

cmp dy
bcc shiftY
lsr
jmp :+
shiftY:
lsr dy
:
clc
adc dy
jmp incsp2



.segment "CODE"


; states are specific to enemy types
STATE_POSCHASE = 0
STATE_POSPAIN = 1
STATE_POSSHOOT = 2
STATE_POSFALL = 3
STATE_IMPCHASE = 4
STATE_IMPPAIN = 5
STATE_IMPCLAW = 6
STATE_IMPMISSILE = 7
STATE_IMPFALL = 8
STATE_DMNCHASE = 9
STATE_DMNPAIN = 10
STATE_DMNBITE = 11
STATE_DMNFALL = 12
STATE_CACCHASE = 13
STATE_CACPAIN = 14
STATE_CACBITE = 15
STATE_CACMISSILE = 16
STATE_CACFALL = 17
STATE_IMPSHOTFLY = 18

mobjSpeed:
.byte 3,4,6,5
mobjPainChance:
.byte 2,3,4,5
mobjSpawnHealth:
.byte 10,20,20,99

mobjChaseState:
.byte STATE_POSCHASE, STATE_IMPCHASE, STATE_DMNCHASE, STATE_CACCHASE
mobjPainState:
.byte STATE_POSPAIN, STATE_IMPPAIN, STATE_DMNPAIN, STATE_CACPAIN
mobjMeleeState:
.byte $ff, STATE_IMPCLAW, STATE_DMNBITE, STATE_CACBITE
mobjShootState:
.byte STATE_POSSHOOT, STATE_IMPMISSILE, $ff, STATE_CACMISSILE
mobjDeathState:
.byte STATE_POSFALL, STATE_IMPFALL, STATE_DMNFALL, STATE_CACFALL

mobjType:
.byte 0

_setMobjCurrentType:
  sta mobjType
  rts

_getMobjSpeed:
  ldx mobjType
  lda mobjSpeed,x
  ldx #0
  rts

_getMobjPainChance:
  ldx mobjType
  lda mobjPainChance,x
  ldx #0
  rts

_getMobjSpawnHealth:
  ldx mobjType
  lda mobjSpawnHealth,x
  ldx #0
  rts

_getMobjChaseState:
  ldx mobjType
  lda mobjChaseState,x
  ldx #0
  rts

_getMobjPainState:
  ldx mobjType
  lda mobjPainState,x
  ldx #0
  rts

_getMobjMeleeState:
  ldx mobjType
  lda mobjMeleeState,x
  ldx #0
  rts

_getMobjShootState:
  ldx mobjType
  lda mobjShootState,x
  ldx #0
  rts

_getMobjDeathState:
  ldx mobjType
  lda mobjDeathState,x
  ldx #0
  rts


mobjAllocated:
.res 21,0
mobjMovedir:
.res 21,0
mobjFlags:
.res 21,0
mobjReactiontime:
.res 21,0
mobjMovecount:
.res 21,0
mobjHealth:
.res 21,0
mobjInfoType:
.res 21,0
mobjStateIndex:
.res 21,0

mobjIndex:
.byte 0

_setMobjIndex:
  sta mobjIndex
  rts

_mobjAllocated:
  tax
  lda mobjAllocated,x
  ldx #0
  rts

_setMobjAllocated:
  ldx mobjIndex
  sta mobjAllocated,x
  rts

_mobjMovedir:
  ldx mobjIndex
  lda mobjMovedir,x
  ldx #0
  rts

_setMobjMovedir:
  ldx mobjIndex
  sta mobjMovedir,x
  rts

_mobjFlags:
  ldx mobjIndex
  lda mobjFlags,x
  ldx #0
  rts

_setMobjFlags:
  ldx mobjIndex
  sta mobjFlags,x
  rts

_removeMobjFlags:
  ldx mobjIndex
  eor #$ff
  and mobjFlags,x
  sta mobjFlags,x
  rts

_addMobjFlags:
  ldx mobjIndex
  ora mobjFlags,x
  sta mobjFlags,x
  rts

_testMobjFlags:
  ldx mobjIndex
  and mobjFlags,x
  rts

_mobjReactiontime:
  ldx mobjIndex
  lda mobjReactiontime,x
  ldx #0
  rts

_setMobjReactiontime:
  ldx mobjIndex
  sta mobjReactiontime,x
  rts

_decMobjReactiontime:
  ldx mobjIndex
  dec mobjReactiontime,x
  rts

_mobjMovecount:
  ldx mobjIndex
  lda mobjMovecount,x
  ldx #0
  rts

_setMobjMovecount:
  ldx mobjIndex
  sta mobjMovecount,x
  rts

_decMobjMovecount:
  ldx mobjIndex
  dec mobjMovecount,x
  lda mobjMovecount,x
  ldx #0
  rts

_incMobjMovecount:
  ldx mobjIndex
  inc mobjMovecount,x
  rts

_mobjHealth:
  ldx mobjIndex
  lda mobjHealth,x
  ldx #0
  rts

_setMobjHealth:
  ldx mobjIndex
  sta mobjHealth,x
  rts

_mobjInfoType:
  ldx mobjIndex
  lda mobjInfoType,x
  ldx #0
  rts

_setMobjInfoType:
  ldx mobjIndex
  sta mobjInfoType,x
  rts

_mobjStateIndex:
  ldx mobjIndex
  lda mobjStateIndex,x
  ldx #0
  rts

_setMobjStateIndex:
  ldx mobjIndex
  sta mobjStateIndex,x
  rts


texFrameTexture:
.byte 8,11,14,17,11,22,22,23,23,23,23,23,24,21,24,22,20,20,20,20,19,20,19,25

texFrameWidthScale:
.byte 5,5,3,3,5,5,5,5,8,8,8,8,8,5,8,2,4,4,4,3,3,3,4,2

; from the bottom of the texture
texFrameStartY:
.byte 0,0,0,0,0, 8,0,8,24,24,16,16,16,0,0,16,0,0,8,16,0,24,16,0

texFrameHeight:
.byte 0,0,0,0,0, 8,8,8,8,8,8,8,16,0,16,4,8,8,8,8,16,8,16,32

; the next three tables could be codified quite easily
; but it would only save a handful of bytes

texFrameSolid:
.byte 1,1,1,1,1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0

texFrameStartX:
.byte 0,0,0,0,0, 0,0,0,0,8,0,8,0,0,0,0,0,0,0,0,0,0,0,0

; either 8 or 16
texFrameWidth:
.byte 0,0,0,0,0, 16,16,16,8,8,8,8,16,0,16,16,16,16,16,16,16,16,16,16

_texFrameTexture:
  tay
  lda texFrameTexture,y
  ldx #0
  rts

_texFrameSolid:
  tay
  lda texFrameSolid,y
  ldx #0
  rts

_texFrameWidthScale:
  tay
  lda texFrameWidthScale,y
  ldx #0
  rts

_texFrameStartY:
  tay
  lda texFrameStartY,y
  ldx #0
  rts

_texFrameHeight:
  tay
  lda texFrameHeight,y
  ldx #0
  rts

_texFrameStartX:
  tay
  lda texFrameStartX,y
  ldx #0
  rts

_texFrameWidth:
  tay
  lda texFrameWidth,y
  ldx #0
  rts
