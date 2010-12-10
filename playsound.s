.setcpu		"6502"

.export _playSoundInitialize
.export _playSound
.import soundTable

irqVector = $314
irqContinue = $eabf

effectsVolume = $a
musicVolume = $1

.segment "CODE"

; format of a song is
; statusByte [note] [statusByte [note]]* $ff
; status byte is [on/off:1][voice:2][timeToNextEvent:5]

.include "e1m1.s"

timeToNextEvent:
.byte 0
noteInVoice:
.byte $ff, $ff, $ff, $ff
songIndex = $32
noteTable:
.byte 131, 140, 145, 151, 158, 161, 166, 173, 178, 181, 185, 189
.byte 192, 197, 200, 203, 206, 208, 211, 214, 216, 218, 220, 222
.byte 224, 226, 227, 229, 231, 232, 233, 234, 235, 237, 237, 239
noteTable2:
.byte 131, 140, 145, 151, 158, 162, 167, 174, 178, 182, 186, 190
.byte 195, 197, 200, 203, 207, 209, 212, 214, 216, 219, 221, 223
.byte 224, 226, 228, 229, 231, 232, 233, 235, 236, 237, 238, 239

soundPointer = $30
soundIndex:
.byte $ff
soundCount:
.byte 0
soundMax:
.byte 0

.proc playSoundIrq : near

; check this came from timer 1
bit $912d
bpl end

; check we're playing a sound
lda soundIndex
cmp #$ff
beq playMusic

; play next sample
inc soundCount
ldy soundCount
cpy soundMax
beq stopPlaying
lda (soundPointer),y
; scale the sample back from 0..15
asl
asl
asl
adc #127
sta $900d

jmp end
;end:
;jmp irqContinue

stopPlaying:
lda #127
sta $900d

lda #$ff
sta soundIndex

; set to music volume
lda $900e
and #$f0
ora #musicVolume
sta $900e

jmp end

playMusic:

dec timeToNextEvent
beq nextEvent

; switch between notes

lda timeToNextEvent
and #1
bne odd

; even
ldx #3
loop:
lda noteInVoice,x
bmi @next
tay
lda noteTable,y
sta $900a,x
@next:
dex
bpl loop

bmi end

odd:
ldx #3
loop2:
lda noteInVoice,x
bmi @next
tay
lda noteTable2,y
sta $900a,x
@next:
dex
bpl loop2

end:

jmp irqContinue

nextEvent:

ldy #0
lda (songIndex),y
cmp #$ff
beq restart

; status byte [on/off:1][voice:2][timeToNextEvent:5]
pha
and #$1f
clc
adc #1
sta timeToNextEvent
pla
pha
rol
rol
rol
rol
and #$03
tax
pla
cmp #0
bpl turnOn

; turn off
lda #0
sta $900a,x
lda #$ff
sta noteInVoice,x

; increment songIndex
inc songIndex
bne end
inc songIndex+1
jmp end

restart:

lda #<startOfSong
sta songIndex
lda #>startOfSong
sta songIndex+1
lda #1
sta timeToNextEvent

jmp end

turnOn:

ldy #1
lda (songIndex),y
sta noteInVoice,x
tay
lda noteTable,y
sta $900a,x

lda songIndex
clc
adc #2
sta songIndex
lda songIndex+1
adc #0
sta songIndex+1

jmp end

.endproc

.proc _playSound : near

tax
asl
tay
; first stop the old sound
lda #$ff
sta soundIndex
; then set up the pointer to the data
lda soundTable,y
sta soundPointer
lda soundTable+1,y
sta soundPointer+1
; then the counters
ldy #0
lda (soundPointer),y
tay
iny
sty soundMax
lda #0
sta soundCount

; turn off music
sta $900a
sta $900b
sta $900c

; turn up volume a bit
lda $900e
and #$f0
ora #effectsVolume
sta $900e

; start the new sound playing
stx soundIndex

rts

.endproc

.proc _playSoundInitialize : near

lda #$ff
sta soundIndex

lda #<startOfSong
sta songIndex
lda #>startOfSong
sta songIndex+1
lda #1
sta timeToNextEvent

; insert into chain
lda #<playSoundIrq
ldx #>playSoundIrq
sta irqVector
stx irqVector+1

; turn off all channels
lda #0
sta $900a
sta $900b
sta $900c
sta $900d

; set music volume
lda $900e
and #$f0
ora #musicVolume
sta $900e

rts

.endproc