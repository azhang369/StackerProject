#include "timerISR.h"
#include "helper.h"
#include "periph.h"
#include "LCD.h"
#include "spiAVR.h"
#include "irAVR.h"
//#include "serialATmega.h"
#include <stdlib.h>
#include <time.h>
#include <avr/eeprom.h>


volatile char songCount = 0;
volatile char fallCount = 0;
volatile char random_piece;
volatile bool gamestate[15][15] = {};//all false, all squares empty at start
//piece dimension mapping all 3 boxes of piece for gamestate
volatile int gridXA;
volatile int gridXB;
volatile int gridXC;
volatile int gridYA;
volatile int gridYB;
volatile int gridYC;
//piece dimension mapping movement
volatile char currXstart;
volatile char currXend;
volatile char currYstart;
volatile char currYend;
//extra piece dimension mapping movement for arrows
volatile char currXstartt;
volatile char currXendd;
volatile char currYstartt;
volatile char currYendd;

volatile char game = 0;
volatile unsigned char score = 0;//increases every piece placed
volatile unsigned char highScore = 0;//if score>highscore 

#define NUM_TASKS 4

typedef struct _task{
	signed 	 char state; 		//Task's current state
	unsigned long period; 		//Task period
	unsigned long elapsedTime; 	//Time elapsed since last task tick
	int (*TickFct)(int); 		//Task tick function
} task;

const unsigned long BUTTON_PERIOD = 200;
const unsigned long SONG_PERIOD = 200;
const unsigned long LCD_PERIOD = 200;
const unsigned long PIECE_PERIOD = 100;
const unsigned long GCD_PERIOD = 100;

task tasks[NUM_TASKS];

enum ButtonStates {button};
enum SongStates {songOff, songOn};
enum LCDStates {LCD};
enum PieceStates {pieceOff, createPiece, flatLine, vertLine, bottom, topLeft, topRight};

void HardwareReset(){
  PORTC = SetBit(PORTC, 5, 0);
  _delay_ms(200);
  PORTC = SetBit(PORTC, 5, 1);
  _delay_ms(200);
}
void Send_Command(char data) {
  PORTB = SetBit(PORTB, 0, 0);
  SPI_SEND(data);
}
void Send_Data(char data) {
  PORTB = SetBit(PORTB, 0, 1);
  SPI_SEND(data);
}
void ST7735_init(){
  HardwareReset();
  Send_Command(0x01);
  _delay_ms(150);
  Send_Command(0x11);
  _delay_ms(200);
  Send_Command(0x3A);
  Send_Data(0x06);
  _delay_ms(10);
  Send_Command(0x29);
  _delay_ms(200);
}
//display an area 
void drawBox(int xStart, int xEnd, int yStart, int yEnd, int red, int green, int blue) {
  Send_Command(0x2A);//CASET column 
  Send_Data(0);
  Send_Data(xStart);
  Send_Data(0);
  Send_Data(xEnd);
  Send_Command(0x2B);//RASET row
  Send_Data(0);
  Send_Data(yStart);
  Send_Data(0);
  Send_Data(yEnd);
  Send_Command(0x2C); //RAMWR memory write
  for (int i = 0; i <= xEnd*yEnd; i++) {
    Send_Data(blue);
    Send_Data(green);
    Send_Data(red);
  }
}

int ButtonTick(int state) {
  switch (state) {
    case button: // button only turns game on when pressed, pressing when game is on does nothing
      if (!GetBit(PINC, 4)) {
        game = 1;
      }
      break;
  }
  return state;
}
// my song notes, tetris theme recreation
//1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
//E B D C A C E D B C  D  E  C  A  A! A 
//17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32
//D  F  A  F  E  C  E  D  B  C  D  E  C  A  A! A  
int SongTick(int state) {
  switch (state) {
    case songOff:
      ICR1=0;
      OCR1A = ICR1 / 2;
      if (game==1) {
        state = songOn;
      }
      break;
    case songOn:
      if (game==0) {
        state = songOff;
      }
      songCount = (songCount + 1) % 32;
      if (songCount == 1 || songCount == 7 || songCount == 12 || songCount == 21 || songCount == 23 || songCount == 28) {
        ICR1 = 16000000/(8*330) - 1;//E
      }
      if (songCount == 2 || songCount == 9 || songCount == 25) {
        ICR1 = 16000000/(8*247) - 1;//B
      }
      if (songCount == 3 || songCount == 8 || songCount == 11 || songCount == 17 || songCount == 24 || songCount == 27) {
        ICR1 = 16000000/(8*294) - 1;//D
      }
      if (songCount == 4 || songCount == 6 || songCount == 10 || songCount == 13 || songCount == 22 || songCount == 26 || songCount == 29) {
        ICR1 = 16000000/(8*262) - 1;//C
      }
      if (songCount == 5 || songCount == 14 || songCount == 16 || songCount == 19 || songCount == 30 || songCount == 32) {
        ICR1 = 16000000/(8*220) - 1;//low A
      }
      if (songCount == 18) {
        ICR1 = 16000000/(8*349) - 1;//F
      }
      if (songCount == 15 || songCount == 19 || songCount == 31) {
        ICR1 = 16000000/(8*440) - 1;//high A
      }
      if (songCount == 20) {
        ICR1 = 16000000/(8*392) - 1;//G
      }
      OCR1A = ICR1 / 2;
      break;
  }
  return state;
}
int LCDTick(int state) {
  switch (state) {
    case LCD:
      lcd_clear();
      lcd_goto_xy(0, 0);
      lcd_write_str("High score: ");
      unsigned char temp = highScore;
      lcd_goto_xy(0, 14);
      char displayONES = temp%10;
      temp = temp/10;
      lcd_write_character(displayONES + '0');
      lcd_goto_xy(0, 13);
      char displayTENS = temp%10;
      temp = temp/10;
      lcd_write_character(displayTENS + '0');
      lcd_goto_xy(0, 12);
      char displayHUNDREDS = temp%10;
      temp = temp/10;
      lcd_write_character(displayHUNDREDS + '0');

      lcd_goto_xy(1, 0);
      if (game==0) {
        lcd_write_str("Game over: ");
        lcd_goto_xy(1, 11);
        if (score>=highScore) {
          lcd_write_str("won");
          highScore = score;
        }
        else {
          lcd_write_str("lost");
        }
      }
      if (game==1) {
        lcd_write_str("Score: ");
        unsigned char temppp = score;
        lcd_goto_xy(1, 9);
        char displayONESSS = temppp%10;
        temppp = temppp/10;
        lcd_write_character(displayONESSS + '0');
        lcd_goto_xy(1, 8);
        char displayTENSSS = temppp%10;
        temppp = temppp/10;
        lcd_write_character(displayTENSSS + '0');
        lcd_goto_xy(1, 7);
        char displayHUNDREDSSS = temppp%10;
        temppp = temppp/10;
        lcd_write_character(displayHUNDREDSSS + '0');
        lcd_goto_xy(1, 10);
      }
      break;
    default:
      state = LCD;
      break;
  }
  return state;
}
int PieceTick(int state) {
  switch (state) {
    case pieceOff:
      srand(time(NULL));
      if (game==1) {
        score = 0;
        state = createPiece;
      }
      break;
    case createPiece:
      score++;
      random_piece = rand()%6+1;
      //this is every piece's starting position at top center of board
      // 5 13 21 29 37 45 53 61middle69 77 85  93  101  109  117  125  y axis
      // 6 14 22 30 38 46 54 62middle70 78 86  94  102  110  118  126  x axis
      //  0  1  2  3  4  5  6   7mid   8  9  10  11   12   13   14   bool grid gaps
      if (random_piece==1) {//flat line
        drawBox(54, 78-1, 5, 13-1, 255, 0, 0);
        currXstart=54; currXend=77, currYstart=5, currYend=12;
        gridXA=6; gridXB=7; gridXC=8; gridYA=0; gridYB=0; gridYC=0;
        state = flatLine;
      }
      else if (random_piece==2) {//vertical line
        drawBox(62, 70-1, 5, 29-1, 255, 0, 0);
        currXstart=62; currXend=69, currYstart=5, currYend=28;
        gridXA=7; gridXB=7; gridXC=7; gridYA=0; gridYB=1; gridYC=2;
        state = vertLine;
      }
      else if (random_piece==3) {//bottom left arrow
        drawBox(54, 62-1, 5, 13-1, 0, 255, 0);
        drawBox(54, 70-1, 13, 21-1, 0, 255, 0);
        currXstart=54; currXend=61, currYstart=5, currYend=12;
        currXstartt=54; currXendd=69, currYstartt=13, currYendd=20;
        gridXA=6; gridXB=6; gridXC=7; gridYA=0; gridYB=1; gridYC=1;
        state = bottom;
      }
      else if (random_piece==4) {//bottom right arrow
        drawBox(62, 70-1, 5, 13-1, 0, 255, 0);
        drawBox(54, 70-1, 13, 21-1, 0, 255, 0);
        currXstart=62; currXend=69, currYstart=5, currYend=12;
        currXstartt=54; currXendd=69, currYstartt=13, currYendd=20;
        gridXA=7; gridXB=6; gridXC=7; gridYA=0; gridYB=1; gridYC=1;
        state = bottom;
      }
      else if (random_piece==5) {//top left arrow
        drawBox(54, 70-1, 5, 13-1, 0, 0, 255);
        drawBox(54, 62-1, 5, 21-1, 0, 0, 255);
        currXstart=54; currXend=69, currYstart=5, currYend=12;
        currXstartt=54; currXendd=61, currYstartt=5, currYendd=20;
        gridXA=6; gridXB=7; gridXC=6; gridYA=0; gridYB=0; gridYC=1;
        state = topLeft;
      }
      else if (random_piece==6) {//top right arrow
        drawBox(54, 70-1, 5, 13-1, 0, 0, 255);
        drawBox(62, 70-1, 5, 21-1, 0, 0, 255);
        currXstart=54; currXend=69, currYstart=5, currYend=12;
        currXstartt=62; currXendd=69, currYstartt=5, currYendd=20;
        gridXA=6; gridXB=7; gridXC=7; gridYA=0; gridYB=0; gridYC=1;
        state = topRight;
      }
      break;
    case flatLine:
      fallCount++;
      drawBox(currXstart, currXend, currYstart, currYend, 0, 0, 0);//reset above space
      if (fallCount==2) {
        currYstart += 8; currYend += 8;
        gridYA++; gridYB++; gridYC++;
        fallCount=0;
      }
      if (ADC_read(3)<100 && gridXA!=0 && !gamestate[gridXA-1][gridYA]) {
        currXstart -= 8; currXend -= 8;
        gridXA--; gridXB--; gridXC--;
      }
      else if (ADC_read(3)>800 && gridXC!=14 && !gamestate[gridXC+1][gridYC]) {
        currXstart += 8; currXend += 8;
        gridXA++; gridXB++; gridXC++;
      }
      drawBox(currXstart, currXend, currYstart, currYend, 255, 0, 0);//redraw shape after lowering it
      if (gridYC>=14 || gamestate[gridXA][gridYA+1] || gamestate[gridXB][gridYB+1] || gamestate[gridXC][gridYC+1]) {
        gamestate[gridXA][gridYA] = true; gamestate[gridXB][gridYB] = true; gamestate[gridXC][gridYC] = true;
        if (gridYA==0) {//if at top game over
          game=0;
          drawBox(6, 125, 5, 124, 0, 0, 0);//reset grid pieces
          for (int i=0; i<15; i++) {
            for (int j=0; j<15; j++) {
              gamestate[i][j] = false;//reset detection bool
            }
          }
          state = pieceOff;
        }
        else {
          score++;
          state = createPiece;
        }
      }
      break;
    case vertLine:
      fallCount++;
      drawBox(currXstart, currXend, currYstart, currYend, 0, 0, 0);//reset above space
      if (fallCount==2) {
        currYstart += 8; currYend += 8;
        gridYA++; gridYB++; gridYC++;
        fallCount=0;
      }
      if (ADC_read(3)<100 && gridXA!=0 && !gamestate[gridXA-1][gridYA] && !gamestate[gridXB-1][gridYB] && !gamestate[gridXC-1][gridYC]) {
        currXstart -= 8; currXend -= 8;
        gridXA--; gridXB--; gridXC--;
      }
      else if (ADC_read(3)>800 && gridXC!=14 && !gamestate[gridXA+1][gridYA] && !gamestate[gridXB+1][gridYB] && !gamestate[gridXC+1][gridYC]) {
        currXstart += 8; currXend += 8;
        gridXA++; gridXB++; gridXC++;
      }
      drawBox(currXstart, currXend, currYstart, currYend, 255, 0, 0);//redraw shape after lowering it
      if (gridYC>=14 || gamestate[gridXC][gridYC+1]) {
        gamestate[gridXA][gridYA] = true; gamestate[gridXB][gridYB] = true; gamestate[gridXC][gridYC] = true;
        if (gridYA==0) {//if at top game over
          game=0;
          drawBox(6, 125, 5, 124, 0, 0, 0);//reset grid pieces
          for (int i=0; i<15; i++) {
            for (int j=0; j<15; j++) {
              gamestate[i][j] = false;//reset detection bool
            }
          }
          state = pieceOff;
        }
        else {
          score++;
          state = createPiece;
        }
      }
      break;
    case bottom:
      fallCount++;
      drawBox(currXstart, currXend, currYstart, currYend, 0, 0, 0);//reset above space
      drawBox(currXstartt, currXendd, currYstartt, currYendd, 0, 0, 0);
      if (fallCount==2) {
        currYstart += 8; currYend += 8; currYstartt += 8; currYendd += 8;
        gridYA++; gridYB++; gridYC++;
        fallCount=0;
      }
      if (ADC_read(3)<100 && gridXB!=0 && !gamestate[gridXA-1][gridYA] && !gamestate[gridXB-1][gridYB]) {
        currXstart -= 8; currXend -= 8; currXstartt -= 8; currXendd -= 8;
        gridXA--; gridXB--; gridXC--;
      }
      else if (ADC_read(3)>800 && gridXC!=14 && !gamestate[gridXA+1][gridYA] && !gamestate[gridXC+1][gridYC]) {
        currXstart += 8; currXend += 8; currXstartt += 8; currXendd += 8;
        gridXA++; gridXB++; gridXC++;
      }
      drawBox(currXstart, currXend, currYstart, currYend, 0, 255, 0);//redraw shape after lowering it
      drawBox(currXstartt, currXendd, currYstartt, currYendd, 0, 255, 0);
      if (gridYC>=14 || gamestate[gridXB][gridYB+1] || gamestate[gridXC][gridYC+1]) {
        gamestate[gridXA][gridYA] = true; gamestate[gridXB][gridYB] = true; gamestate[gridXC][gridYC] = true;
        if (gridYA==0) {//if at top game over
          game=0;
          drawBox(6, 125, 5, 124, 0, 0, 0);//reset grid pieces
          for (int i=0; i<15; i++) {
            for (int j=0; j<15; j++) {
              gamestate[i][j] = false;//reset detection bool
            }
          }
          state = pieceOff;
        }
        else {
          score++;
          state = createPiece;
        }
      }
      break;
    case topLeft:
      fallCount++;
      drawBox(currXstart, currXend, currYstart, currYend, 0, 0, 0);//reset above space
      drawBox(currXstartt, currXendd, currYstartt, currYendd, 0, 0, 0);
      if (fallCount==2) {
        currYstart += 8; currYend += 8; currYstartt += 8; currYendd += 8;
        gridYA++; gridYB++; gridYC++;
        fallCount=0;
      }
      if (ADC_read(3)<100 && gridXA!=0 && !gamestate[gridXA-1][gridYA] && !gamestate[gridXC-1][gridYC]) {
        currXstart -= 8; currXend -= 8; currXstartt -= 8; currXendd -= 8;
        gridXA--; gridXB--; gridXC--;
      }
      else if (ADC_read(3)>800 && gridXB!=0 && !gamestate[gridXB+1][gridYB] && !gamestate[gridXC+1][gridYC]) {
        currXstart += 8; currXend += 8; currXstartt += 8; currXendd += 8;
        gridXA++; gridXB++; gridXC++;
      }
      drawBox(currXstart, currXend, currYstart, currYend, 0, 0, 255);//redraw shape after lowering it
      drawBox(currXstartt, currXendd, currYstartt, currYendd, 0, 0, 255);
      if (gridYC>=14 || gamestate[gridXB][gridYB+1] || gamestate[gridXC][gridYC+1]) {
        gamestate[gridXA][gridYA] = true; gamestate[gridXB][gridYB] = true; gamestate[gridXC][gridYC] = true;
        if (gridYA==0) {//if at top game over
          game=0;
          drawBox(6, 125, 5, 124, 0, 0, 0);//reset grid pieces
          for (int i=0; i<15; i++) {
            for (int j=0; j<15; j++) {
              gamestate[i][j] = false;//reset detection bool
            }
          }
          state = pieceOff;
        }
        else {
          score++;
          state = createPiece;
        }
      }
      break;
    case topRight:
      fallCount++;
      drawBox(currXstart, currXend, currYstart, currYend, 0, 0, 0);//reset above space
      drawBox(currXstartt, currXendd, currYstartt, currYendd, 0, 0, 0);
      if (fallCount==2) {
        currYstart += 8; currYend += 8; currYstartt += 8; currYendd += 8;
        gridYA++; gridYB++; gridYC++;
        fallCount=0;
      }
      if (ADC_read(3)<100 && gridXA!=0 && !gamestate[gridXA-1][gridYA] && !gamestate[gridXC-1][gridYC]) {
        currXstart -= 8; currXend -= 8; currXstartt -= 8; currXendd -= 8;
        gridXA--; gridXB--; gridXC--;
      }
      else if (ADC_read(3)>800 && gridXB!=0 && !gamestate[gridXB+1][gridYB] && !gamestate[gridXC+1][gridYC]) {
        currXstart += 8; currXend += 8; currXstartt += 8; currXendd += 8;
        gridXA++; gridXB++; gridXC++;
      }
      drawBox(currXstart, currXend, currYstart, currYend, 0, 0, 255);//redraw shape after lowering it
      drawBox(currXstartt, currXendd, currYstartt, currYendd, 0, 0, 255);
      if (gridYC>=14 || gamestate[gridXA][gridYA+1] || gamestate[gridXC][gridYC+1]) {
        gamestate[gridXA][gridYA] = true; gamestate[gridXB][gridYB] = true; gamestate[gridXC][gridYC] = true;
        if (gridYA==0) {//if at top game over
          game=0;
          drawBox(6, 125, 5, 124, 0, 0, 0);//reset grid pieces
          for (int i=0; i<15; i++) {
            for (int j=0; j<15; j++) {
              gamestate[i][j] = false;//reset detection bool
            }
          }
          state = pieceOff;
        }
        else {
          score++;
          state = createPiece;
        }
      }
      break;
    default:
      state = pieceOff;
      break;
  }
  return state;
}  

void TimerISR() {
	for ( unsigned int i = 0; i < NUM_TASKS; i++ ) {                   // Iterate through each task in the task array
		if ( tasks[i].elapsedTime == tasks[i].period ) {           // Check if the task is ready to tick
			tasks[i].state = tasks[i].TickFct(tasks[i].state); // Tick and set the next state for this task
			tasks[i].elapsedTime = 0;                          // Reset the elapsed time for the next tick
		}
		tasks[i].elapsedTime += GCD_PERIOD;                        // Increment the elapsed time by GCD_PERIOD
	}
}


int main (void) {
  //serial_init(9600);
  uint8_t ByteOfData;
  if (highScore > ByteOfData) {
    ByteOfData = eeprom_read_byte((uint8_t*)highScore);//store highscore in EEPROM
  }
  else {
    highScore = ByteOfData;//restores high score from EEPROM if game powered off
  }
  ADC_init();
  DDRB = 0xEF; PORTB = 0x10;
  DDRC = 0x20; PORTC = 0x1F;
  DDRD = 0x00; PORTD = 0x00;
  SPI_INIT();
  ST7735_init();
  TCCR1A |= (1 << WGM11) | (1 << COM1A1);
  TCCR1B |= (1 << WGM12) | (1 << WGM13) | (1 << CS11);
  //note: (0,0) at top left, x: L to R, y: top to bottom 
  drawBox(6, 125, 5, 124, 0, 0, 0); //set 120x120 field to black
  drawBox(1, 5, 1, 129, 255, 255, 255);//left
  drawBox(126, 129, 1, 129, 255, 255, 255);//right
  drawBox(1, 129, 1, 4, 255, 255, 255);//top
  drawBox(1, 129, 125, 129, 255, 255, 255);//bottom
  //5 13 21 29 37 45 53 61middle69 77 85 93 101 109 117 125 y axis
  //6 14 22 30 38 46 54 62middle70 78 86 94 102 110 118 126 x axis
  // 0  1  2  3  4  5  6     7    8  9  10 11  12  13  14   grid gaps
  
  tasks[0].period = BUTTON_PERIOD;
  tasks[0].state = button;
  tasks[0].elapsedTime = 0;
  tasks[0].TickFct = &ButtonTick;
  
  tasks[1].period = SONG_PERIOD;
  tasks[1].state = songOff;
  tasks[1].elapsedTime = 0;
  tasks[1].TickFct = &SongTick;

  tasks[2].period = LCD_PERIOD;
  tasks[2].state = LCD;
  tasks[2].elapsedTime = 0;
  tasks[2].TickFct = &LCDTick;

  tasks[3].period = PIECE_PERIOD;
  tasks[3].state = pieceOff;
  tasks[3].elapsedTime = 0;
  tasks[3].TickFct = &PieceTick;

  TimerSet(GCD_PERIOD);
  TimerOn();
  lcd_init();
  
  while (1) {}
  return 0;
}