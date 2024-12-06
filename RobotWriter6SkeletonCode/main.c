#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rs232.h"
#include "serial.h"

#define bdrate 115200               /* 115200 baud */
#define MAX_MOVEMENTS 1000          // Maximum number of movements a character can have
#define MAX_CHARACTERS 256          // Maximum number of characters in the font
#define MAX_TEXT_LENGTH 1024        // Maximum length of the input text

// Structure to represent a movement (X, Y coordinates and pen state)
typedef struct {
    int x;    // X-coordinate of the movement
    int y;    // Y-coordinate of the movement
    int pen;  // Pen state: 0 = pen up, 1 = pen down
} Movement;

// Structure to represent a character, containing a number of movements
typedef struct {
    int num_movements;               // Number of movements for a character
    Movement movements[MAX_MOVEMENTS];  // Array of movements for a character
} Character;

// Array to store font data for each character
Character fontData[MAX_CHARACTERS];

// Functions used in the code
void SendCommands(char *buffer);
FILE *openFile(const char *filename, const char *mode);
void loadFontData(const char *filename);
void scaleFontData(float height);
void processWord(const char *word, int *x_pos, int *y_pos, int *penState, int charWidth, int maxLineWidth, int *lowestY, int lineGap, int minY);
void generateGCode(const char *text, float height);

// Function to open a file and return its pointer
FILE *openFile(const char *filename, const char *mode) {
    FILE *file = fopen(filename, mode);
    if (!file) {
        printf("Error opening file: %s\n", filename);
        exit(1);  // Exit the program if file cannot be opened
    }
    return file;
}

// Function to load font data from a file
void loadFontData(const char *filename) {
    FILE *file = openFile(filename, "r");
    char line[256];  // Temporary buffer to read each line from the file
    int currentChar = -1;  // Variable to track the current character being loaded
    int numMovements = 0;  // Variable to count the movements for the current character

    // Read each line from the file
    while (fgets(line, sizeof(line), file)) {
        int x, y, p;  

        // Check if the line indicates a new character
        if (strncmp(line, "999", 3) == 0) {
            if (currentChar != -1) {
                fontData[currentChar].num_movements = numMovements;  // Update movement count for the previous character
            }
            sscanf(line, "999 %d %d", &currentChar, &numMovements);  // Extract character ID and number of movements
        } else {
            sscanf(line, "%d %d %d", &x, &y, &p);  // Extract movement data (x, y, pen state)
            fontData[currentChar].movements[numMovements++] = (Movement){x, y, p};  // Store the movement for the current character
        }
    }

    // Update movement count for the last character
    if (currentChar != -1) {
        fontData[currentChar].num_movements = numMovements;
    }

    fclose(file);  
}

// Function to scale the font data based on the desired height
void scaleFontData(float height) {
    float scaleFactor = height / 18.0f;  // Calculate the scale factor based on the desired height 
    // Apply scaling factor to all characters and scale their movements
    for (int i = 0; i < MAX_CHARACTERS; i++) {
        if (fontData[i].num_movements > 0) {
            for (int j = 0; j < fontData[i].num_movements; j++) {
                fontData[i].movements[j].x = (int)((float)fontData[i].movements[j].x * scaleFactor);  // Scale the X coordinate
                fontData[i].movements[j].y = (int)((float)fontData[i].movements[j].y * scaleFactor);  // Scale the Y coordinate
            }
        }
    }
}

// Function to process a word and convert it into G-code for the robot to draw
void processWord(const char *word, int *x_pos, int *y_pos, int *penState, int charWidth, int maxLineWidth, int *lowestY, int lineGap, int minY) {
    char buffer[256];  // Temporary buffer to store G-code commands
    // Calculate the width of the word based on character width
    int wordWidth = (int)((size_t)strlen(word) * (size_t)charWidth);

    // If the word exceeds the maximum line width, move to the next line
    if (*x_pos + wordWidth > maxLineWidth) {
        *y_pos = *lowestY - lineGap;
        if (*y_pos < minY) {
            printf("Error: Text exceeds Y-axis limit.\n");
            exit(1);  // Exit if the text goes beyond the Y-axis limit
        }
        *x_pos = 0;
        *lowestY = *y_pos;
        sprintf(buffer, "G0 X0 Y%d\n", *y_pos);  // Move to the next line
        SendCommands(buffer);
    }

    // Process each character in the word
    for (int i = 0; word[i] != '\0'; i++) {
        unsigned char currentChar = (unsigned char)word[i];  // Get the current character
        if (currentChar >= 32 && currentChar <= 126) {
            Character charData = fontData[currentChar];  // Get the font data for the current character
            for (int j = 0; j < charData.num_movements; j++) {
                Movement m = charData.movements[j];  // Get the movement data for the current character
                int newX = m.x + *x_pos;  // Calculate the new X coordinate
                int newY = m.y + *y_pos;  // Calculate the new Y coordinate

                // Update the lowest Y position if necessary
                if (newY < *lowestY) {
                    *lowestY = newY;
                }

                // If the pen state has changed, update the pen
                if (m.pen != *penState) {
                    *penState = m.pen;
                    sprintf(buffer, *penState == 1 ? "S1000\n" : "S0\n");
                    SendCommands(buffer);
                }

                // Send the movement command (G1 for pen down, G0 for pen up)
                sprintf(buffer, *penState == 1 ? "G1 X%d Y%d\n" : "G0 X%d Y%d\n", newX, newY);
                SendCommands(buffer);
            }
            *x_pos += charWidth;  // Move the X position by the character width
        }
    }
    *x_pos += charWidth;  // Add extra space after the word
}

// Function to generate G-code from text input
void generateGCode(const char *text, float height) {
    scaleFontData(height);  // Scale the font data to match the desired height

    // Initialize variables for G-code generation
    int x_pos = 0;
    int y_pos = -(int)height;
    int penState = 0;  // Pen state: 0 = pen up, 1 = pen down
    int charWidth = (int)(height * 1.0f);  // Character width based on height
    int lineGap = (int)(height + 5.0f);  // Line gap between text lines
    int maxLineWidth = 100;  // Maximum width of a line in the drawing
    int lowestY = y_pos;  // Variable to track the lowest Y position reached
    int minY = -90 - (int)height;  // Minimum allowed Y position

    char buffer[256];
    char word[128];  // Temporary buffer to store a word
    int wordIndex = 0;

    // Iterate through each character in the input text
    for (const char *ptr = text; *ptr != '\0'; ptr++) {
        char c = *ptr;

        // If the current character is a space, newline, or end of the string, process the word
        if (c == ' ' || c == '\n' || *(ptr + 1) == '\0') {
            if (*(ptr + 1) == '\0' && c != ' ' && c != '\n') {
                word[wordIndex++] = c;  // Add last character to the word
            }

            word[wordIndex] = '\0';  // Null-terminate the word
            processWord(word, &x_pos, &y_pos, &penState, charWidth, maxLineWidth, &lowestY, lineGap, minY);  // Process the word

            // If a newline is encountered, move to the next line
            if (c == '\n') {
                y_pos = lowestY - lineGap;
                if (y_pos < minY) {
                    printf("Error: Text exceeds Y-axis limit.\n");
                    exit(1);  // Exit if the text goes beyond the Y-axis limit
                }
                x_pos = 0;
                lowestY = y_pos;
                sprintf(buffer, "G0 X0 Y%d\n", y_pos);  // Move to the next line
                SendCommands(buffer);
            }

            wordIndex = 0;  // Reset word index for the next word
        } else {
            word[wordIndex++] = c;  // Add character to the current word
        }
    }

    // If the pen is still down, lift it
    if (penState != 0) {
        sprintf(buffer, "S0\n");
        SendCommands(buffer);
    }

    // Return the pen to the origin (0, 0)
    sprintf(buffer, "G0 X0 Y0\n");
    SendCommands(buffer);
}

// Main function
int main() {
    char buffer[256];

    // Check if the COM port can be opened
    if (CanRS232PortBeOpened() == -1) {
        printf("\nUnable to open the COM port (specified in serial.h) ");
        exit(0);  // Exit if COM port cannot be opened
    }

    printf("\nAbout to wake up the robot\n");

    sprintf(buffer, "\n");  // Send wake-up signal
    PrintBuffer(&buffer[0]);
    Sleep(100);  
    WaitForDollar();  // Wait for robot to be ready

    printf("\nThe robot is now ready to draw\n");

    // Initialize the robot for drawing
    sprintf(buffer, "G1 X0 Y0 F1000\n");
    SendCommands(buffer);
    sprintf(buffer, "M3\n");
    SendCommands(buffer);
    sprintf(buffer, "S0\n");
    SendCommands(buffer);

    loadFontData("SingleStrokeFont.txt");  // Load font data from file

    printf("Enter the desired text height (between 4 and 10mm): ");
    float height;
    scanf("%f", &height);
    if (height < 4.0f || height > 10.0f) {
        printf("Error: Height must be between 4 and 10mm.\n");
        CloseRS232Port();
        return 1;  // Exit if height is out of range
    }

    // Ask for the text file containing the content to be drawn
    char textFileName[256];
    printf("Enter the name of the text file: ");
    scanf("%s", textFileName);

    // Open the text file
    FILE *textFile = openFile(textFileName, "r");
    char text[MAX_TEXT_LENGTH] = {0};  // Buffer to store the text from the file
    size_t index = 0;

    // Read the text from the file
    while (fgets(&text[index], (size_t)(sizeof(text) - index), textFile)) {
        index += strlen(&text[index]);
        if (index >= sizeof(text) - 1) {
            break;  // Stop reading if the buffer is full
        }
    }

    fclose(textFile);  // Close the text file
    generateGCode(text, height);  // Generate G-code for the text

    CloseRS232Port();  // Close the COM port
    printf("COM port now closed\n");

    return 0;
}

// Function to send commands to the robot and wait for a reply
void SendCommands(char *buffer) {
    PrintBuffer(buffer);  // Print the buffer to the robot
    WaitForReply();  
    Sleep(100);  
}

