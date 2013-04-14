#include <stdbool.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <inttypes.h>
#include <stdio.h>
#include "usb.h"
#include "timer.h"
#include "stepper.h"
#include "string.h"
#include "gcode.h"



#define MAX_CMD_SIZE 96
#define BUFSIZE 4

char cmdbuffer[BUFSIZE][MAX_CMD_SIZE];
int bufindr = 0;
int bufindw = 0;
int buflen = 0;
char serial_char;
int serial_count = 0;
bool comment_mode = false;
char *strchr_pointer; // just a pointer to find chars in the cmd string like X, Y, Z, E, etc
long gcode_N, gcode_LastN;


bool hpgl_mode = false;


bool relative_mode = true;
int xpos=0;
int ypos=0;
int xoff=0;
int yoff=0;
bool penup = true;


void gcode_loop(void) {
    if (buflen < (BUFSIZE - 1))
        get_command();

    if (buflen) {
        process_commands();
        buflen = (buflen - 1);
        bufindr = (bufindr + 1) % BUFSIZE;
    }
}


void enquecommand(const char *cmd) {
    if (buflen < BUFSIZE) {
        //this is dangerous if a mixing of serial and this happsens
        strcpy(&(cmdbuffer[bufindw][0]), cmd);
        bufindw = (bufindw + 1) % BUFSIZE;
        buflen += 1;
    }
}


void get_command() {
    while (1) {
        int foo = usb_peek();
        if (foo < 0 || buflen >= BUFSIZE) {
            break;
        }
        serial_char = foo;
        
        if (serial_char == '\n' ||
                serial_char == '\r' ||
                (serial_char == ':' && comment_mode == false) ||
                serial_count >= (MAX_CMD_SIZE - 1)) {
            if (!serial_count) { //if empty line
                comment_mode = false; //for new command
                return;
            }
            cmdbuffer[bufindw][serial_count] = 0; //terminate string
            if (!comment_mode) {
                comment_mode = false; //for new command
                
                
                
                
                
                if (strstr(cmdbuffer[bufindw], "N") != NULL) {
                    strchr_pointer = strchr(cmdbuffer[bufindw], 'N');
                    gcode_N = (strtol(&cmdbuffer[bufindw][strchr_pointer - cmdbuffer[bufindw] + 1], NULL, 10));
                    if (gcode_N != gcode_LastN + 1 && (strstr(cmdbuffer[bufindw], "M110") == NULL)) {
                        printf("Error: Line Number is not Last Line Number+1, Last Line:%lu\n",gcode_LastN);
                        //Serial.println(gcode_N);
                        FlushSerialRequestResend();
                        serial_count = 0;
                        return;
                    }

                    if (strstr(cmdbuffer[bufindw], "*") != NULL) {
                        unsigned char checksum = 0;
                        unsigned char count = 0;
                        while (cmdbuffer[bufindw][count] != '*') checksum = checksum^cmdbuffer[bufindw][count++];
                        strchr_pointer = strchr(cmdbuffer[bufindw], '*');

                        if ((int) (strtod(&cmdbuffer[bufindw][strchr_pointer - cmdbuffer[bufindw] + 1], NULL)) != checksum) {
                            printf("Error:checksum mismatch, Last Line:%lu\n",gcode_LastN);
                            FlushSerialRequestResend();
                            serial_count = 0;
                            return;
                            //if no errors, continue parsing
                        }
                    } else {
                        printf("Error:No Checksum with line number, Last Line:%lu\n",gcode_LastN);
                        FlushSerialRequestResend();
                        serial_count = 0;
                        return;
                    }

                    gcode_LastN = gcode_N;
                    //if no errors, continue parsing
                } else // if we don't receive 'N' but still see '*'
                {
                    if ((strstr(cmdbuffer[bufindw], "*") != NULL)) {
                        printf("Error:No Line Number with checksum, Last Line:%lu\n",gcode_LastN);
                        serial_count = 0;
                        return;
                    }
                }
                if ((strstr(cmdbuffer[bufindw], "G") != NULL)) {
                    strchr_pointer = strchr(cmdbuffer[bufindw], 'G');
                    switch ((int) ((strtod(&cmdbuffer[bufindw][strchr_pointer - cmdbuffer[bufindw] + 1], NULL)))) {
                        case 0:
                        case 1:
                        case 2:
                        case 3:
                            printf("ok\n");
                            break;
                        default:
                            break;
                    }

                }
                bufindw = (bufindw + 1) % BUFSIZE;
                buflen += 1;
            }
            serial_count = 0; //clear buffer
        } else {
            if (serial_char == ';') {
            if (!hpgl_mode) {
                if (cmdbuffer[bufindw][0] == 'P' &&
                    cmdbuffer[bufindw][1] == 'U') {
                        
                        serial_count = 0;
                        return;
                } 
                if (cmdbuffer[bufindw][0] == 'P' &&
                    cmdbuffer[bufindw][1] == 'D') {
                    
                        serial_count = 0;
                        return;
                } 
            }else{
                if (serial_count==2 && 
                    cmdbuffer[bufindw][0] == 'I' &&
                    cmdbuffer[bufindw][1] == 'N') {
                        hpgl_mode = true;
                        serial_count = 0;
                        return;
                } else {
                        comment_mode = true;
                }
              }
            }
            if (!comment_mode) {
                cmdbuffer[bufindw][serial_count++] = serial_char;
            }
        }
    }

}

void ClearToSend(void) {
    printf("ok\n");
}

float code_value(void) {
    return (strtod(&cmdbuffer[bufindr][strchr_pointer - cmdbuffer[bufindr] + 1], NULL));
}

long code_value_long(void) {
    return (strtol(&cmdbuffer[bufindr][strchr_pointer - cmdbuffer[bufindr] + 1], NULL, 10));
}
/*
bool code_seen(char code_string[]) //Return True if the string was found
{
    return (strstr(cmdbuffer[bufindr], code_string) != NULL);
}
*/
bool code_seen(char code) {
    strchr_pointer = strchr(cmdbuffer[bufindr], code);
    return (strchr_pointer != NULL); //Return True if a character was found
}

void movePen(int x, int y) {
    x+=xoff;
    y+=yoff;

    if (x<0) x=0;
    if (y<0) y=0;
    
    
    x=x*401;
    y=y*401;
    if (penup) {
        stepper_move( x, y );
    } else {
        stepper_draw( x, y );
    }
}

void FlushSerialRequestResend() {
    printf("Resend:%lu\n",gcode_LastN + 1);
    ClearToSend();
}

void process_commands() {
    unsigned long codenum; //throw away variable
    //char *starpos = NULL;

    if (code_seen('G')) {
        switch ((int) code_value()) {
            case 0: // G0 -> G1
            case 1: // G1
                if (code_seen('Z')) {
                    codenum = code_value();
                    penup = codenum > 0;
                }
                
                if (relative_mode) {
                    int newx = xpos;
                    int newy = ypos;
                    if (code_seen('X')) {
                        newx = code_value();
                    }
                    if (code_seen('Y')) {
                        newy = code_value();
                    }
                    movePen(xpos+newx,ypos+newy);
                } else {
                    int newx = xpos;
                    int newy = ypos;
                    if (code_seen('X')) {
                        newx = code_value();
                    }
                    if (code_seen('Y')) {
                        newy = code_value();
                    }
                    movePen(newx,newy);
                }
                break;
                
            case 4: // G4 dwell
                codenum = 0;
                if (code_seen('P')) codenum = code_value(); // milliseconds to wait
                if (code_seen('S')) codenum = code_value() * 1000; // seconds to wait
                codenum = codenum / 10;
                while (codenum-- > 0) {
                    wdt_reset();
                    msleep(10);
                }
                break;

            case 28: //G28 Home all Axis one at a time
                stepper_move(0,0);
                xoff=0;
                yoff=0;
                break;
                
            case 90: // G90
                relative_mode = false;
                break;

            case 91: // G91
                relative_mode = true;
                break;

            case 92: // G92 home
                if (code_seen('X')) {
                    xoff = code_value();
                }
                if (code_seen('Y')) {
                    yoff = code_value();
                }                
                break;
        }
    }
    else
    if (code_seen('M')) {
        switch ((int) code_value()) {
            case 106://load paper
                stepper_load_paper();
                break;

            case 107://unload paper
                stepper_unload_paper();
                break;

            case 300://S0=down S255=up
                if (code_seen('S')) {
                    penup = code_value() > 0;
                }
                break;
        }
    }//M
    else {
        printf("Error:unknown commando\n");
    }

}