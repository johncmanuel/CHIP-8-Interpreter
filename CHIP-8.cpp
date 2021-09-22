// CHIP-8 Interpreter
// Documentations utilized:
// - https://en.wikipedia.org/wiki/CHIP-8#Opcode_table
// - https://github.com/mattmikolay/chip-8/wiki/CHIP%E2%80%908-Technical-Reference

#include <vector>
#include <SDL.h>
#include <algorithm>
#include <iostream>
#include <fstream>
#pragma warning(disable:4996)

// Documentation specifies that memory ranges from 0x000 -> 0x1FF, so negative values are not
// needed. Opcodes are 1 word, or 2 bytes. 1 byte = 4 bits.
// Rather than writing unsigned char or unsigned short int all the time, it's best to refer to
// them as BYTE or WORD.
typedef unsigned char BYTE;
typedef unsigned short int WORD;

// Memory, registers, and all that good stuff.
BYTE m_GameMemory[0xFFF];
BYTE m_Registers[16];
BYTE m_Keyboard[16];
WORD m_AddressI;
WORD m_PC;
std::vector<WORD> m_Stack;

// Timers!
uint8_t delayTimer;
uint8_t soundTimer;

// Screen dimensions and data
const unsigned int SCREEN_WIDTH = 64;
const unsigned int SCREEN_HEIGHT = 32;
BYTE m_ScreenData[SCREEN_WIDTH][SCREEN_HEIGHT];

// Font data
const unsigned int numOfSprites = 16;
const unsigned int numOfPixels = 5;
BYTE m_FontData[numOfSprites][numOfPixels] = {
    { 0xF0, 0x90, 0x90, 0x90, 0xF0 }, // 0
    { 0x20, 0x60, 0x20, 0x20, 0x70 }, // 1
    { 0xF0, 0x10, 0xF0, 0x80, 0xF0 }, // 2
    { 0xF0, 0x10, 0xF0, 0x10, 0xF0 }, // 3
    { 0x90, 0x90, 0xF0, 0x10, 0x10 }, // 4
    { 0xF0, 0x80, 0xF0, 0x10, 0xF0 }, // 5
    { 0xF0, 0x80, 0xF0, 0x90, 0xF0 }, // 6
    { 0xF0, 0x10, 0x20, 0x40, 0x40 }, // 7
    { 0xF0, 0x90, 0xF0, 0x90, 0xF0 }, // 8
    { 0xF0, 0x90, 0xF0, 0x10, 0xF0 }, // 9
    { 0xF0, 0x90, 0xF0, 0x90, 0x90 }, // A
    { 0xE0, 0x90, 0xE0, 0x90, 0xE0 }, // B
    { 0xF0, 0x80, 0x80, 0x80, 0xF0 }, // C
    { 0xE0, 0x90, 0x90, 0x90, 0xE0 }, // D
    { 0xF0, 0x80, 0xF0, 0x80, 0xF0 }, // E
    { 0xF0, 0x80, 0xF0, 0x80, 0x80 }, // F
};


void CPUReset() {

    // Initialize address memory to 0
    // Program Counter starts at address 0x200
    m_AddressI = 0;
    m_PC = 0x200;

    // Set registers, keyboard, and game memory to 0
    std::fill(std::begin(m_Registers), std::end(m_Registers), 0);
    std::fill(std::begin(m_Keyboard), std::end(m_Keyboard), 0);
    std::fill(std::begin(m_GameMemory), std::end(m_GameMemory), 0);

    // Load fonts in the first set of addresses before
    // address 0x200, where the CHIP-8 programs begin
    for (int i = 0; i < numOfSprites; i++) {
        for (int j = 0; j < numOfPixels; j++) {

            // Thanks to this post, I can insert 2D array elements into 1D!
            // https://stackoverflow.com/questions/24333170/put-a-multidimensional-array-into-a-one-dimensional-array
            m_GameMemory[i * numOfSprites + j] = m_FontData[i][j];
        }
        printf("\n");
    }
}

// Read and store the contents of ROM in memory.
bool LoadCH8ROM(const char* fname) {

    std::fstream ROM;
    ROM.open(fname, std::ios::in | std::ios::binary);

    if (!ROM) {
        printf("Error loading the ROM. This occurs if the file does not exist or the name does not exist.\n");
        return false;
    }

    // Get length of ROM file
    ROM.seekg(0, std::ios::end);
    long long bufferSize = ROM.tellg();
    ROM.seekg(0, std::ios::beg);
    //printf("Size of ROM file: %d\n", (int)bufferSize);

    // Allocate memory to the main buffer
    char* buffer = new char[sizeof(char) * bufferSize];

    if (!buffer) {
        printf("Error while allocating memory.\n");
        return false;
    }

    ROM.read(buffer, bufferSize);

    // Load the contents of ROM into game memory
    if (((0xFFF+1) - 0x200) > bufferSize) {
        for (int i = 0; i < bufferSize; i++) {

            // Load memory contents into addresses after 0x200
            m_GameMemory[i + 0x200] = buffer[i];
            //printf("Element %d: %08X\n", i, buffer[i]);
        }
    }
    else {
        printf("ROM is too large. Please load another ROM file.\n");
        return false;
    }

    ROM.close();
    delete[] buffer;

    return true;
}

int GetRegisterX(WORD opcode) {
    int regx = opcode & 0x0F00;
    return regx >> 8;
}

int GetRegisterY(WORD opcode) {
    int regy = opcode & 0x00F0;
    return regy >> 4;
}

// Fetching the next set of opcode instructions
WORD GetNextOpcode() {
    WORD res = 0;
    res = m_GameMemory[m_PC];
    res <<= 8;

    // OR the bits from the next memory address to add them together.
    res |= m_GameMemory[m_PC + 1];

    // Increment twice b/c an opcode is 1 WORD long (2 bytes). Recall in the
    // first few lines that variable res is making use of two memory addresses,
    // which are 1 byte long. By increasing the PC with 2 bytes, this would fetch 
    // the next instruction.
    m_PC += 2;

    return res;
}

// Opcode 0NNN is for specific computers that uses some unique
// machine code. It would pause a CHIP-8 program, then call a
// machine language subroutine at NNN. Thus, it's best if this opcode is left
// unimplemented because it would cause unexpected results at NNN.

// Clear the screen
void Opcode00E0(WORD opcode) {
    
    // Set every pixel to 0.
    std::fill(m_ScreenData[0], m_ScreenData[0] + SCREEN_WIDTH * SCREEN_HEIGHT, 0);
}

// Return from a subroutine
void Opcode00EE(WORD opcode) {
    m_PC = m_Stack.back();
    m_Stack.pop_back();
}

// Jump to address NNN
void Opcode1NNN(WORD opcode) {
    m_PC = opcode & 0x0FFF;
}

// Call subroutine at NNN
void Opcode2NNN(WORD opcode) {
    m_Stack.push_back(m_PC);
    m_PC = opcode & 0x0FFF;
}

// Skips next instruction if VX == NN
void Opcode3XNN(WORD opcode) {
    int regx = GetRegisterX(opcode);
    int nn = opcode & 0x00FF;
    if (m_Registers[regx] == nn) {
        m_PC += 2;
    }
}

// Skips next instruction if VX != NN
void Opcode4XNN(WORD opcode) {
    int regx = GetRegisterX(opcode);
    int nn = opcode & 0x00FF;
    if (m_Registers[regx] != nn) {
        m_PC += 2;
    }
}

// Skips next instruction if VX == VY
void Opcode5XY0(WORD opcode) {
    if (m_Registers[GetRegisterX(opcode)] == m_Registers[GetRegisterY(opcode)]) {

        // Skip to the next line of instruction.
        m_PC += 2;
    }
}

// Store number NN in register VX
void Opcode6XNN(WORD opcode) {
    int regx = GetRegisterX(opcode);
    int nn = opcode & 0x00FF;
    m_Registers[regx] = nn;
}

// Add the value NN to register VX
void Opcode7XNN(WORD opcode) {
    int nn = opcode & 0x00FF;
    m_Registers[GetRegisterX(opcode)] += nn;
}

// Store the value of register VY in register VX
void Opcode8XY0(WORD opcode) {
    m_Registers[GetRegisterX(opcode)] = m_Registers[GetRegisterY(opcode)];
}

// Set VX to VX OR VY
void Opcode8XY1(WORD opcode) {
    m_Registers[GetRegisterX(opcode)] |= m_Registers[GetRegisterY(opcode)];
}

// Set VX to VX AND VY
void Opcode8XY2(WORD opcode) {
    m_Registers[GetRegisterX(opcode)] &= m_Registers[GetRegisterY(opcode)];
}

// Set VX to VX XOR VY
void Opcode8XY3(WORD opcode) {
    m_Registers[GetRegisterX(opcode)] ^= m_Registers[GetRegisterY(opcode)];
}

// Add the value of register VY to register VX
// If register VY > register VX, set register VF to 1
void Opcode8XY4(WORD opcode) {
    m_Registers[0xF] = 0;

    int xval = m_Registers[GetRegisterX(opcode)];
    int yval = m_Registers[GetRegisterY(opcode)];

    if (yval > xval) {
        m_Registers[0xF] = 1;
    }

    m_Registers[GetRegisterX(opcode)] += m_Registers[GetRegisterY(opcode)];
}

// Subtract contents of Register Y from Register X
// Set VF to 00 if a borrow occurs
// Set VF to 01 if a borrow does not occur
void Opcode8XY5(WORD opcode) {
    m_Registers[0xF] = 1;
    
    int xval = m_Registers[GetRegisterX(opcode)];
    int yval = m_Registers[GetRegisterY(opcode)];

    if (yval > xval) {
        m_Registers[0xF] = 0;
    }

    m_Registers[GetRegisterX(opcode)] = xval - yval;
}

// Store the value of register VY shifted right one bit in register VX
// Set register VF to the least significant bit prior to the shift
void Opcode8XY6(WORD opcode) {
    int yVal = m_Registers[GetRegisterY(opcode)];

    // Get least significant bit
    // Thanks https://stackoverflow.com/questions/6647783/check-value-of-least-significant-bit-lsb-and-most-significant-bit-msb-in-c-c
    int LSB = yVal & 1;

    m_Registers[0xF] = LSB;
    m_Registers[GetRegisterX(opcode)] = m_Registers[GetRegisterY(opcode)] >>= 1; 
}

// Set register VX to the value of VY minus VX
// Set VF to 00 if a borrow occurs
// Set VF to 01 if a borrow does not occur
void Opcode8XY7(WORD opcode) {
    m_Registers[0xF] = 1;

    int xval = m_Registers[GetRegisterX(opcode)];
    int yval = m_Registers[GetRegisterY(opcode)];

    if (yval > xval) {
        m_Registers[0xF] = 0;
    }

    m_Registers[GetRegisterX(opcode)] -= m_Registers[GetRegisterY(opcode)];
}

// Store the value of register VY shifted left one bit in register VX
// Set register VF to the most significant bit prior to the shift
// VY is unchanged!
void Opcode8XYE(WORD opcode) {

    // Assign MSB to register VF, and store value of register VY shifted one bit to
    // register VX.
    m_Registers[0xF] = m_Registers[GetRegisterX(opcode)] >> 7;
    m_Registers[GetRegisterX(opcode)] <<= 1;
}

// Skip the following instruction if the value of register VX is not equal to the value of register VY
void Opcode9XY0(WORD opcode) {
    if (m_Registers[GetRegisterX(opcode)] != m_Registers[GetRegisterY(opcode)]) {
        m_PC += 2;
    }
}

// Store memory address NNN in register I
void OpcodeANNN(WORD opcode) {
    int nnn = opcode & 0x0FFF;
    m_AddressI = nnn;
}

// Jump to address NNN + V0
void OpcodeBNNN(WORD opcode) {
    int nnn = opcode & 0x0FFF;
    m_PC = nnn + m_Registers[0x0];
}

// Set VX to a random number with a mask of NN
void OpcodeCXNN(WORD opcode) {
    int nn = opcode & 0x00FF;
    m_Registers[GetRegisterX(opcode)] = rand() & nn;
}

// Draw sprite at coord (VX, VY) with width of 8 pixels and N pixels.
// Set VF to 01 if any set pixels are changed to unset, and 00 otherwise
void OpcodeDXYN(WORD opcode) {

    // Get the height of an arbitrary sprite
    // No need to set a width because all sprites
    // are 8 pixels wide.
    int height = opcode & 0x000F;

    // Coords start at top left of the screen
    int coordx = m_Registers[GetRegisterX(opcode)];
    int coordy = m_Registers[GetRegisterY(opcode)];

    m_Registers[0xF] = 0;

    // Loop for each vertical line
    for (int yline = 0; yline < height;  yline++) {

        // Game memory stores sprite data here
        BYTE data = m_GameMemory[m_AddressI + yline];

        // Each byte is one line of the sprite. Each pixel determines
        // whether it is on or off.
        int xpixelinv = 7;
        int xpixel = 0;

        // Iterate through the pixels.
        for (xpixel = 0; xpixel < 8; xpixel++, xpixelinv--) {
            int mask = 1 << xpixelinv;

            // Determine if xpixelinv is 0 or 1 by checking the data and mask.
            if (data & mask) {
                int x = coordx + xpixel;
                int y = coordy + yline;
                if (m_ScreenData[x][y] == 1) {
                    m_Registers[0xF] = 1;
                }

                // Toggle pixel state
                m_ScreenData[x][y] ^= 1;
            }
        }
    }
}

// Skips next instruction if key in VX is pressed
void OpcodeEX9E(WORD opcode) {
    if (m_Registers[GetRegisterX(opcode)] == m_Keyboard[GetRegisterX(opcode)]) {
        m_PC += 2;
    }
}

// Skips next instruction if key in VX is not pressed
void OpcodeEXA1(WORD opcode) {
    if (m_Registers[GetRegisterX(opcode)] != m_Keyboard[GetRegisterX(opcode)]) {
        m_PC += 2;
    }
}

// Store the current value of the delay timer in register VX
void OpcodeFX07(WORD opcode) {
    m_Registers[GetRegisterX(opcode)] = delayTimer;
}

// Set the delay timer to the value of register VX
void OpcodeFX15(WORD opcode) {
    delayTimer = m_Registers[GetRegisterX(opcode)];
}

// Set the sound timer to the value of register VX
void OpcodeFX18(WORD opcode) {
    soundTimer = m_Registers[GetRegisterX(opcode)];
}

// Add the value stored in register VX to register I
void OpcodeFX1E(WORD opcode) {
    m_AddressI += m_Registers[GetRegisterX(opcode)];
}

// Wait for a keypress and store the result in register VX
void OpcodeFX0A(WORD opcode) {
    bool keyPressed = false;

    for (int i = 0; i < sizeof(m_Keyboard); i++) {
        if (m_Keyboard[i] != 0) {
            m_Registers[GetRegisterX(opcode)] = i;
            keyPressed = true;
        }
    }
}

// Set register I to the memory address of the sprite data corresponding to 
// the hexadecimal digit stored in register VX
void OpcodeFX29(WORD opcode) {
    int regx = m_Registers[GetRegisterX(opcode)];
    m_AddressI = *m_ScreenData[regx];
}

// Store Binary-coded decimal in register VX
void OpcodeFX33(WORD opcode) {
    int value = m_Registers[GetRegisterX(opcode)];

    int hundreds = value / 100;
    int tens = (value / 10) % 10;
    int units = value % 10;

    m_GameMemory[m_AddressI] = hundreds;
    m_GameMemory[m_AddressI + 1] = tens;
    m_GameMemory[m_AddressI + 2] = units;
}

// Stores V0 to VX in memory starting at address I
void OpcodeFX55(WORD opcode) {
    int regx = GetRegisterX(opcode);
    for (int i = 0; i <= regx; i++) {
        m_GameMemory[m_AddressI + i] = m_Registers[i];
    }
    m_AddressI = m_AddressI + regx + 1;
}

// Fills V0 to VX with values from memory starting at address I
void OpcodeFX65(WORD opcode) {
    int xval = GetRegisterX(opcode);
    for (int i = 0; i <= xval; i++) {
        m_Registers[i] = m_GameMemory[m_AddressI + i];
    }
    m_AddressI = m_AddressI + xval + 1;
}

// Starts the opcode decoding cycle
void DecodeOpcodeCycle(WORD opcode) {
    switch (opcode & 0xF000) {
        case 0x0000: {
            switch (opcode & 0x000F) {
                case 0x0000: Opcode00E0(opcode); break;
                case 0x000E: Opcode00EE(opcode); break;
            }
        } break;
        case 0x1000: Opcode1NNN(opcode); break;
        case 0x2000: Opcode2NNN(opcode); break;
        case 0x3000: Opcode3XNN(opcode); break;
        case 0x4000: Opcode4XNN(opcode); break;
        case 0x5000: Opcode5XY0(opcode); break;
        case 0x6000: Opcode6XNN(opcode); break;
        case 0x7000: Opcode7XNN(opcode); break;
        case 0x8000: {
            switch (opcode & 0x000F) {
                case 0x0000: Opcode8XY0(opcode); break;
                case 0x0001: Opcode8XY1(opcode); break;
                case 0x0002: Opcode8XY2(opcode); break;
                case 0x0003: Opcode8XY3(opcode); break;
                case 0x0004: Opcode8XY4(opcode); break;
                case 0x0005: Opcode8XY5(opcode); break;
                case 0x0006: Opcode8XY6(opcode); break;
                case 0x0007: Opcode8XY7(opcode); break;
                case 0x000E: Opcode8XYE(opcode); break;
            }
        } break;
        case 0x9000: Opcode9XY0(opcode); break;
        case 0xA000: OpcodeANNN(opcode); break;
        case 0xB000: OpcodeBNNN(opcode); break;
        case 0xC000: OpcodeCXNN(opcode); break;
        case 0xD000: OpcodeDXYN(opcode); break;
        case 0xE000: {
            switch (opcode & 0x00FF) {
                case 0x009E: OpcodeEX9E(opcode); break;
                case 0x00A1: OpcodeEXA1(opcode); break;
            }
        } break;
        case 0xF000: {
            switch (opcode & 0x00FF) {
                case 0x0007: OpcodeFX07(opcode); break;
                case 0x000A: OpcodeFX0A(opcode); break;
                case 0x0015: OpcodeFX15(opcode); break;
                case 0x0018: OpcodeFX18(opcode); break;
                case 0x001E: OpcodeFX1E(opcode); break;
                case 0x0029: OpcodeFX29(opcode); break;
                case 0x0033: OpcodeFX33(opcode); break;
                case 0x0055: OpcodeFX55(opcode); break;
                case 0x0065: OpcodeFX65(opcode); break;
            }
        } break;
        default: 
            printf("No additional opcodes from this ROM.");
            break;
    };

    // If delay timer is bigger than 0, decrement
    if (delayTimer > 0) {
        delayTimer--;
    }

    // Same as delay timer, but if it is above 0, play a beeping sound
    if (soundTimer > 0) {
        printf("Beeping sound");
        soundTimer--;
    }
}

// Graphics

// Initialize SDL!
bool initSDL(SDL_Window* &window, SDL_Renderer* &renderer, SDL_Surface* &mainSurface) {

    // Initialize video subsystem
    // Additional flags can be found here: https://wiki.libsdl.org/SDL_Init
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("Unable to initialize SDL: %s", SDL_GetError());
        return false;
    }
    // Load window, renderer, and main surface
    else {

        // More flags here: https://wiki.libsdl.org/SDL_WindowFlags
        SDL_CreateWindowAndRenderer(SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN, &window, &renderer);

        mainSurface = SDL_GetWindowSurface(window);
        if (!mainSurface) {
            printf("Cannot make surface. See more: %s\n", SDL_GetError());
        }

    }
    
    return true;
}

// Draw and render the pixels in SDL
void DrawPixels(SDL_Renderer* &renderer, SDL_Texture* &gameScreen) {
    
    // Set game screen to be a texture with RGBA8888 format.
    // Because the CHIP-8 is developed in a big endian fashion,
    // this format is the most suited candidate.
    // Read more about this: https://en.wikipedia.org/wiki/RGBA_color_model
    gameScreen = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, SCREEN_WIDTH, SCREEN_HEIGHT);

    if (!gameScreen) {
        printf("gameScreen cannot be loaded. See more: %s\n", SDL_GetError());
    }

    if (SDL_SetRenderTarget(renderer, gameScreen) != 0) {
        printf("Error: Render target is not equal to 0. Here's the complete message: %s\n", SDL_GetError());
    }

    // Assign each pixel their color
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            if (m_ScreenData[x][y] == 0) {

                // If the pixel is 0, color it black.
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            }
            else if (m_ScreenData[x][y] == 1) {

                // If the pixel is 1, color it white.
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                m_Registers[0xF] = 1;
            }
            
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }

    // Create a rect with the dimensions of the application window,
    // then fill this rect with the gameScreen texture. This is
    // another way to render the pixels onto the screen.
    SDL_SetRenderTarget(renderer, NULL);
    SDL_Rect tempRect;
    tempRect.x = 0;
    tempRect.y = 0;
    tempRect.w = SCREEN_WIDTH;
    tempRect.h = SCREEN_HEIGHT;
    SDL_RenderCopy(renderer, gameScreen, NULL, &tempRect);
}

// The humble beginnings of a C++ program
int main(int argc, char* argv[])
{
    std::string fname, path;
    std::cout << "Enter filename inside your ROMS folder without file extension: " << '\n';
    std::cin >> fname;
    std::string fileExtension = ".ch8";
    std::string folderPath = "ROMS/";
    path = folderPath + fname + fileExtension;

    // Important stuff
    SDL_Window* window;
    SDL_Surface* mainSurface;
    SDL_Renderer* renderer;
    SDL_Texture* gameScreen = NULL;    

    // Event stuff
    SDL_Event event;
    bool exit = false;

    // Ensure that SDL works
    if (initSDL(window, renderer, mainSurface)) {

        // Reset the registers, keys, and memory
        CPUReset();

        // Start application loop
        while (!exit) {

            // Get opcode and load ROM file
            WORD opcode = GetNextOpcode();
            bool isLoaded = LoadCH8ROM(path.c_str());

            // Once loaded, commence the opcode cycle!
            if (isLoaded) {
                DecodeOpcodeCycle(opcode);
            }
            else {
                exit = true;
            }

            // SDL Input loop
            while (SDL_PollEvent(&event) != 0) {
                if (event.type == SDL_QUIT) {
                    exit = true;
                }
                else if (event.type == SDL_KEYDOWN) {
                    switch (event.key.keysym.sym) {
                        case SDLK_0:
                            m_Keyboard[0x0] = 1;
                            break;
                        case SDLK_1:
                            m_Keyboard[0x1] = 1;
                            break;
                        case SDLK_2:
                            m_Keyboard[0x2] = 1;
                            break;
                        case SDLK_3:
                            m_Keyboard[0x3] = 1;
                            break;
                        case SDLK_4:
                            m_Keyboard[0x4] = 1;
                            break;
                        case SDLK_5:
                            m_Keyboard[0x5] = 1;
                            break;
                        case SDLK_6:
                            m_Keyboard[0x6] = 1;
                            break;
                        case SDLK_7:
                            m_Keyboard[0x7] = 1;
                            break;
                        case SDLK_8:
                            m_Keyboard[0x8] = 1;
                            break;
                        case SDLK_9:
                            m_Keyboard[0x9] = 1;
                            break;
                        case SDLK_a:
                            m_Keyboard[0xA] = 1;
                            break;
                        case SDLK_b:
                            m_Keyboard[0xB] = 1;
                            break;
                        case SDLK_c:
                            m_Keyboard[0xC] = 1;
                            break;
				        case SDLK_d:
                            m_Keyboard[0xD] = 1;
                            break;
                        case SDLK_e:
                            m_Keyboard[0xE] = 1;
                            break;
                        case SDLK_f:
                            m_Keyboard[0xF] = 1;
                            break;
                        default: 
                            printf("no other actions after pressing any key\n");
                            break;
                    }
                }
                else if (event.type == SDL_KEYUP) {
                    switch (event.key.keysym.sym) {
                        case SDLK_0:
                            m_Keyboard[0x0] = 0;
                            break;
                        case SDLK_1:
                            m_Keyboard[0x1] = 0;
                            break;
                        case SDLK_2:
                            m_Keyboard[0x2] = 0;
                            break;
                        case SDLK_3:
                            m_Keyboard[0x3] = 0;
                            break;
                        case SDLK_4:
                            m_Keyboard[0x4] = 0;
                            break;
                        case SDLK_5:
                            m_Keyboard[0x5] = 0;
                            break;
                        case SDLK_6:
                            m_Keyboard[0x6] = 0;
                            break;
                        case SDLK_7:
                            m_Keyboard[0x7] = 0;
                            break;
                        case SDLK_8:
                            m_Keyboard[0x8] = 0;
                            break;
                        case SDLK_9:
                            m_Keyboard[0x9] = 0;
                            break;
                        case SDLK_a:
                            m_Keyboard[0xA] = 0;
                            break;
                        case SDLK_b:
                            m_Keyboard[0xB] = 0;
                            break; 
                        case SDLK_c:
                            m_Keyboard[0xC] = 0;
                            break;
                        case SDLK_d:
                            m_Keyboard[0xD] = 0;
                            break;
                        case SDLK_e:
                            m_Keyboard[0xE] = 0;
                            break;
                        case SDLK_f:
                            m_Keyboard[0xF] = 0;
                            break;
                        default: 
                            printf("no other actions after lifting from any key\n");
                            break;
                    }
                }
            }

            DrawPixels(renderer, gameScreen);

            // Update window
            SDL_RenderPresent(renderer);
    }
    }

    // Free surface and text texture 
    SDL_FreeSurface(mainSurface);
    SDL_DestroyTexture(gameScreen);

    // Close window and renderer
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
