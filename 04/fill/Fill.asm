// This file is part of www.nand2tetris.org
// and the book "The Elements of Computing Systems"
// by Nisan and Schocken, MIT Press.
// File name: projects/04/Fill.asm

// Runs an infinite loop that listens to the keyboard input.
// When a key is pressed (any key), the program blackens the screen,
// i.e. writes "black" in every pixel;
// the screen should remain fully black as long as the key is pressed. 
// When no key is pressed, the program clears the screen, i.e. writes
// "white" in every pixel;
// the screen should remain fully clear as long as no key is pressed.

// Put your code here.

(LOOP)
@KBD
D=M
@BLACK
D;JNE

(WHITE)
@SCREEN
D=A
@R0
M=D

(WHITE_LOOP)
// drawing
@R0
D=M
A=D
M=0 // white
@R0
M=M+1
// bounds checking
@R0
D=M
@KBD
D=A-D
@LOOP
D;JEQ

@WHITE_LOOP
0;JMP

(BLACK)
@SCREEN
D=A
@R0
M=D
(BLACK_LOOP)
// drawing
@R0
D=M
A=D
M=-1 // black
@R0
M=M+1
// bounds checking
@R0
D=M
@KBD
D=A-D
@LOOP
D;JEQ

@BLACK_LOOP
0;JMP


@LOOP
0;JMP
