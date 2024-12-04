#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rs232.h"
#include "serial.h"

#define bdrate 115200               /* 115200 baud */
#define MAX_MOVEMENTS 1000
#define MAX_CHARACTERS 256
#define MAX_TEXT_LENGTH 1024

// Structure to hold a single movement data (X, Y, Pen)
typedef struct {
    int x;    // X-coordinate
    int y;    // Y-coordinate
    int pen;  // Pen state: 0 = pen up, 1 = pen down
} Movement;

// Structure to hold character drawing data
typedef struct {
    int num_movements;              // Number of movements for this character
    Movement movements[MAX_MOVEMENTS];  // Array of movements
} Character;

//Functions
void loadFontData(const char *filename);
void scaleFontData(float height);
void generateGCode(const char *text, float height);

//Function to send commands to the robot
void SendCommands (char *buffer );

int main()
{

    //char mode[]= {'8','N','1',0};
    char buffer[100];

    // If we cannot open the port then give up immediately
    if ( CanRS232PortBeOpened() == -1 )
    {
        printf ("\nUnable to open the COM port (specified in serial.h) ");
        exit (0);
    }

    // Time to wake up the robot
    printf ("\nAbout to wake up the robot\n");

    // We do this by sending a new-line
    sprintf (buffer, "\n");
     // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    Sleep(100);

    // This is a special case - we wait  until we see a dollar ($)
    WaitForDollar();

    printf ("\nThe robot is now ready to draw\n");

        //These commands get the robot into 'ready to draw mode' and need to be sent before any writing commands
    sprintf (buffer, "G1 X0 Y0 F1000\n");
    SendCommands(buffer);
    sprintf (buffer, "M3\n");
    SendCommands(buffer);
    sprintf (buffer, "S0\n");
    SendCommands(buffer);


   

    // Before we exit the program we need to close the COM port
    CloseRS232Port();
    printf("Com port now closed\n");

    return (0);
}

// Send the data to the robot - note in 'PC' mode you need to hit space twice
// as the dummy 'WaitForReply' has a getch() within the function.
void SendCommands (char *buffer )
{
    // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    WaitForReply();
    Sleep(100); // Can omit this when using the writing robot but has minimal effect
    // getch(); // Omit this once basic testing with emulator has taken place
}
