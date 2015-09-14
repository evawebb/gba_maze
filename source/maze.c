#include "toolbox.h"

// Constants.
#define STACK_SIZE 5000
#define SEED 2

// Big functions.
void draw();
void gen_maze();

// Maze generation utilities.
u32 neighbors(u32 x, u32 y);
u32 check_px(u32 x, u32 y);
void write_px(u32 x, u32 y, u32 val);
void clear_maze();

// Stack utilities.
void init_stack();
void push(u32 value);
u32 pop();

// Other utilities.
u32 rand();

// Global variables.
u32 maze[SCREEN_HEIGHT][8]; // 240 / 32 = 7.5, so each row has 16 extra
                            // bits at the end that should be left alone.
u32 stack[STACK_SIZE];
u32 stack_i;
u32 stack_num_values;
u32 seed = SEED;

int main() {
  // Set the graphics to background mode 3 and activate background 2.
  REG_DISPCNT = DCNT_MODE3 | DCNT_BG2;
  
  while (1) {
    // Wait until the user presses start.
    u32 start_pressed = 0;
    u32 key_status = 0;
    while (start_pressed == 0) {
      key_status = (u32)(*((vu16*)REG_KEYINPUT));
      start_pressed = ~key_status & 0x00000008;
      seed += 1;
    }
    
    clear_maze();
    draw();
    
    gen_maze();
    draw();
  }
  
  return 0;
}

// Draw the maze to the screen.
void draw() {
  for (u32 row = 0; row < SCREEN_HEIGHT; row += 1) {
    for (u32 col = 0; col < 8; col += 1) {
      // Each u32 in the maze array is a cluster of 32 horizontal pixels
      // on the screen. Compare is used to mask the cluster value, and the
      // result is dumped into check.
      u32 cluster = maze[row][col];
      u32 check = 0x00000000;
      u32 compare = 0x80000000;
      
      // If this is the last cluster in the row, only scan half of it.
      u32 limit = (col < 7) ? 32 : 16;
      
      // Check every bit of cluster. If it is a 1, draw white to the screen.
      for (u32 index = 0; index < limit; index += 1) {
        // Mask the cluster value, and notch up the compare value.
        check = cluster & compare;
        compare = compare >> 1;
        
        if (check > 0) {
          m3_plot(col * 32 + index, row, RGB15(31, 31, 31));
        } else {
          m3_plot(col * 32 + index, row, RGB15(0, 0, 31));
        }
      }
    }
  }
}

// Create the maze.
void gen_maze() {  
  // Start in the center of the screen.
  u32 currx = SCREEN_WIDTH / 2 - 1;
  u32 curry = SCREEN_HEIGHT / 2 - 1;
  write_px(currx, curry, 1);
  
  // Initialize the point stack.
  init_stack();
  push(currx | (curry << 16));
  
  // When the stack is empty, the maze should be done.
  while (stack_num_values > 0) {
    if (stack_num_values > STACK_SIZE) {
      maze[SCREEN_HEIGHT - 1][7] = 0xffffffff;
      break;
    }
    
    u32 dir_vals[4];
    
    // Do the first retrieval of this point's neighbors.
    u32 n = neighbors(currx, curry);
    u32 num_n = (n & 0xffff0000) >> 16;
        
    // Go as far as possible with this path.
    while (num_n > 0) {     
      push(currx | (curry << 16));
      
      // Set the values of the dir_vals array.
      // 0 = n, 1 = e, 2 = s, 3 = w
      u32 di = 0;
      if ((n & 0x01) > 0) {
        dir_vals[di] = 0;
        di += 1;
      }
      if ((n & 0x02) > 0) {
        dir_vals[di] = 1;
        di += 1;
      }
      if ((n & 0x04) > 0) {
        dir_vals[di] = 2;
        di += 1;
      }
      if ((n & 0x08) > 0) {
        dir_vals[di] = 3;
        di += 1;
      }
      
      // Get a random direction.
      u32 divisor = 0xffffffff / num_n;
      u32 dir = dir_vals[rand() / divisor];
      if (dir == 0) { // n
        write_px(currx, curry - 1, 1);
        write_px(currx, curry - 2, 1);
        curry -= 2;
      } else if (dir == 1) { // e
        write_px(currx + 1, curry, 1);
        write_px(currx + 2, curry, 1);
        currx += 2;
      } else if (dir == 2) { // s
        write_px(currx, curry + 1, 1);
        write_px(currx, curry + 2, 1);
        curry += 2;
      } else { // w
        write_px(currx - 1, curry, 1);
        write_px(currx - 2, curry, 1);
        currx -= 2;
      }
      
      // Get the point's neighbors.
      n = neighbors(currx, curry);
      num_n = (n & 0xffff0000) >> 16;
    }
    
    // Once that path is exhausted, work our way back down the stack.
    u32 next_point = pop();
    currx = next_point & 0x0000ffff;
    next_point = next_point >> 16;
    curry = next_point & 0x0000ffff;
  }
}

// Return a list of unvisited neighbors of a point.
// bit 0: north
// bit 1: east
// bit 2: south
// bit 3: west
// bits 16-31: the number of neighbors
u32 neighbors(u32 x, u32 y) {
  u32 result = 0x00000000;
  u32 num = 0;
  
  if (y > 2 && check_px(x, y - 2) == 0) { // n
    result |= 0x00000001;
    num += 1;
  }
  if (x < SCREEN_WIDTH - 3 && check_px(x + 2, y) == 0) { // e
    result |= 0x00000002;
    num += 1;
  }
  if (y < SCREEN_HEIGHT - 3 && check_px(x, y + 2) == 0) { // s
    result |= 0x00000004;
    num += 1;
  }
  if (x > 2 && check_px(x - 2, y) == 0) { // w
    result |= 0x00000008;
    num += 1;
  }
  
  return result | (num << 16);
}

// Query a pixel of the maze.
u32 check_px(u32 x, u32 y) {
  u32 cluster = x / 32;
  u32 mask = 0x80000000 >> (x % 32);
  return maze[y][cluster] & mask;
}

// Write to a pixel. For val == 0, a 0 will be written, anything else 
// will write a 1.
void write_px(u32 x, u32 y, u32 val) {
  u32 bit = 0x80000000 >> (x % 32);
  u32 col = x / 32;
  if (val == 0) {
    maze[y][col] &= ~bit;
    m3_plot(x, y, RGB15(0, 0, 31));
  } else {
    maze[y][col] |= bit;
    m3_plot(x, y, RGB15(31, 31, 31));
  }
}

// Clear the entire maze so a new one can be generated.
void clear_maze() {
  for (u32 row = 0; row < SCREEN_HEIGHT; row += 1) {
    for (u32 col = 0; col < 8; col += 1) {
      maze[row][col] = 0x00000000;
    }
  }
}

// im making a stack in c because i hate myself haha lol
void init_stack() {
  for (u32 i = 0; i < STACK_SIZE; i += 1) {
    stack[i] = 0;
  }
  
  stack_i = 0;
  stack_num_values = 0;
}
void push(u32 value) {
  stack[stack_i] = value;
  
  stack_num_values += 1;
  stack_i += 1;
  if (stack_i >= STACK_SIZE) {
    stack_i = 0;
  }
}
u32 pop() {
  stack_num_values -= 1;
  stack_i -= 1;
  if (stack_i < 0 && stack_num_values > 0) {
    stack_i = STACK_SIZE - 1;
  }
  
  return stack[stack_i];
}

// Simple PRNG.
u32 rand() {
  seed = (1103515245 * seed + 12345);
  if (seed > 0xfffffffb) {
    return 0xfffffffb; // Results higher than this give wonky results
                       // in the direction choosing part of the code.
  } else {
    return seed;
  }
}
