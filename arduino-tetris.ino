#include <SPI.h>
#include <TFT_ILI9163C.h>

// All wiring required, only 3 defines for hardware SPI on 328P
#define __DC 9
#define __CS 10
// MOSI --> (SDA) --> D11
#define __RST 12
// SCLK --> (SCK) --> D13

// Color definitions
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0  
#define WHITE   0xFFFF

// Joystick
#define xpin A1
#define ypin A0
#define joystickbtnpin 2

#define NONE 0
#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4









TFT_ILI9163C tft = TFT_ILI9163C(__CS, __DC, __RST);

enum STATES { STANDBY, IN_PROGRESS, END };
enum TetriminoTypes { I, J, L, O, S, T, Z };

struct Point { int x; int y; };
struct Tetrimino { int x; int y; TetriminoTypes type; int points[4][2]; };

const int WIDTH = 128;
const int HEIGHT = 128;
const int W = 10;
const int H = 20;
const int BASE_TICK = 750;
const int UNIT_SIZE = min(WIDTH / W, HEIGHT / H);




int** createBlockGrid() {
  int** grid = 0;
  grid = new int*[H];
  for (int r = 0; r < H; r++)
  {
    grid[r] = new int[W];
    for (int c = 0; c < W; c++)
    {
      grid[r][c] = 0;
    }
  }
  return grid;
}

static const int TSHAPES[7][4][2] =
  { {{-1,0},{0,0},{1,0},{2,0}}
  , {{-1,0},{0,0},{1,0},{-1,1}}
  , {{-1,0},{0,0},{1,0},{1,1}}
  , {{0,0},{1,0},{0,1},{1,1}}
  , {{-1,0},{0,0},{0,1},{1,1}}
  , {{-1,0},{0,0},{0,1},{1,0}}
  , {{0,0},{1,0},{0,1},{-1,1}}
  };

static const int TCOLORS[] = { RED, BLUE, GREEN, MAGENTA, WHITE, YELLOW, CYAN };

Tetrimino randomBlock() {
    int type = random() % 7;
    const int shape[4][2] = {{0,0},{1,0},{0,1},{-1,1}};
    Tetrimino t =
        { round(W / 2.0)
        , -2
        , type
        , {{0,0},{1,0},{0,1},{-1,1}}
        };
        
    // t.points = copy(TSHAPES[type])
    for (int i = 0; i < 4; i++)
    {
      for (int j = 0; j < 2; j++)
      {
        t.points[i][j] = TSHAPES[type][i][j];
      }
    }
    return t;
}

class Controller {
  public:
    int direction;
    bool isPressing;
    bool waitDebounce = false;
    void read() {
        direction = pickDirection(lastX, lastY);
        isPressing = !digitalRead(joystickbtnpin);
        lastX = analogRead(xpin);
        lastY = analogRead(ypin);
    }
  private:
    int lastX = 512;
    int lastY = 512;
    bool lastPress = 0;
};

class Game {
  public:
    int state;
    Controller controller;
    Tetrimino currentBlock;
    Tetrimino upcomingBlock;
    int** blockGrid;
    int lastTick;
    int score;
    int totalClearedLines;
    Game() {
      state = STANDBY;
      currentBlock = randomBlock();
      upcomingBlock = randomBlock();
      blockGrid = createBlockGrid();
    }   
};

Game game;

void (*States[3])() { standby, tetris, gameOver };

void setup() {
//  Serial.begin(9600);
  pinMode(joystickbtnpin, INPUT_PULLUP);
  initStandby();
}

void loop() {
  States[game.state]();
}

void initStandby() {
  game.state = STANDBY;
  tft.begin();
  tft.clearScreen();
  tft.setTextColor(WHITE);
  tft.setCursor(10, 64);
  tft.println("Press the button to start!");
}

void standby() {
  game.controller.read();
  if (game.controller.isPressing)
  {
    initGame();
  }
  delay(50);
}

void initGame() {
  game.state = IN_PROGRESS;
  tft.clearScreen();
  drawBorder();
  redrawScore(WHITE);
  drawUpcomingBlock(TCOLORS[game.upcomingBlock.type]);
}

void drawBorder() {
  tft.fillRect(W * UNIT_SIZE, 0, 3, H * UNIT_SIZE, 0x3333);
}

void tetris() {
  const int t = millis();    

  game.lastTick || (game.lastTick = t);
  game.controller.read(); 

  const bool shouldTick = shouldTickCurrentBlock(t);
  const int controllerInputNumber = getControlInputNumber();
  const bool shouldControl = controllerInputNumber > 0;

  if (shouldTick || shouldControl)
  {
    // Erase the block
    drawBlock(game.currentBlock, BLACK);

    if (shouldTick)    tickCurrentBlock(t);
    if (shouldControl) controlCurrentBlock(controllerInputNumber);
    
    // Draw the block
    drawBlock(game.currentBlock, TCOLORS[game.currentBlock.type]);
  }
  
  delay(10);
}

void gameOver() {
  tft.clearScreen();
  tft.setCursor(40, 10);
  tft.setTextColor(RED);
  tft.println("GAME OVER!");
  // Give em a count-down.
  for (int count = 5; count > 0; count--)
  {
    tft.setTextColor(WHITE);
    tft.setCursor(WIDTH / 2, HEIGHT / 2);
    tft.print(count);
    delay(1000);
    tft.setTextColor(BLACK);
    tft.setCursor(WIDTH / 2, HEIGHT / 2);
    tft.print(count);
  }
  initStandby();
}

int getControlInputNumber() {
  // Control Input Number
  // if n >= 5 then controller.pressing = true, n -= 5;
  // controller.direction = n
  if (game.controller.direction == NONE && !game.controller.isPressing)
  {
    game.controller.waitDebounce = false;
    return 0;
  }
  if (game.controller.waitDebounce)
  {
    return 0;
  }
  game.controller.waitDebounce = true;
  int n = game.controller.direction;
  int pressCertficate = 5;
  if (game.controller.isPressing)
  {
    n += pressCertficate;
  }
  return collisionExists()
    ? 0
    : n;
}

void controlCurrentBlock(int n) {
  
  const int oldX = game.currentBlock.x;
  const int oldY = game.currentBlock.y;
  const int pressCertificate = 5;
  const bool isPressing = n >= pressCertificate;
  if (isPressing)
  {
    n -= pressCertificate;
  }

  if (n == RIGHT)
  {
    game.currentBlock.x++;
  }
  else if (n == LEFT)
  {
    game.currentBlock.x--;
  }
  else if (n == DOWN)
  {
    game.currentBlock.y++;
    if (collisionExists())
    {
      game.currentBlock.y--;
      nextBlock();
      return;
    }
  }
  else if (n == UP)
  {
    // Hard Drop:
    while (!collisionExists())
    {
      game.currentBlock.y++;
    }
    game.currentBlock.y--;
    nextBlock();
    return;
  }

  bool didRotateBlock = false;
  if (isPressing)
  {
    rotateBlock(1);
    didRotateBlock = true;
  }
  if (collisionExists())
  {
    game.currentBlock.x = oldX;
    game.currentBlock.y = oldY;
    if (didRotateBlock)
    {
      rotateBlock(-1);
    }
  }
}

void rotateBlock(int forward) {
  // const x = game.currentBlock.x;
  // const y = game.currentBlock.y;
  // const centerX = x + 0.5;
  // const centerY = y + 0.5;
  for (int i = 0; i < 4; i++)
  {
    const int newX = forward * game.currentBlock.points[i][1];
    const int newY = forward * -game.currentBlock.points[i][0];
    game.currentBlock.points[i][0] = newX;
    game.currentBlock.points[i][1] = newY;
  }
}

boolean shouldTickCurrentBlock(int t) {
  const int dt = t - game.lastTick;
  const int level = floor(game.totalClearedLines / 10);
  const int tick = max(100, BASE_TICK - level * 50);
  return dt >= tick;
}

void tickCurrentBlock(int t) {
  game.lastTick = t;
  game.currentBlock.y++;
  if (collisionExists())
  {
    game.currentBlock.y--;
    nextBlock();
  }
}

boolean collisionExists() {
  const int x = game.currentBlock.x;
  const int y = game.currentBlock.y;
  int minX = 999;
  int maxX = -999;
  int maxY = -999;
  for (int i = 0; i < 4; i++)
  {
    minX = min(minX, game.currentBlock.points[i][0] + x);
    maxX = max(maxX, game.currentBlock.points[i][0] + x);
    maxY = max(maxY, game.currentBlock.points[i][1] + y);

    if (minX < 0 || maxX >= W || maxY >= H)
    {
      return true;
    }
  }
  for (int i = 0; i < 4; i++)
  {
    const int Y = y + game.currentBlock.points[i][1];
    const int X = x + game.currentBlock.points[i][0];
    if (Y >= 0 && game.blockGrid[Y][X] != 0)
    {
      return true;
    }
  }
  return false;
}

void nextBlock() {
  int minY = 9999;
  const int y = game.currentBlock.y;
  for (int i = 0; i < 4; i++)
  {
    minY = min(minY, game.currentBlock.points[i][1] + y);
  }
  if (minY < 0)
  {
    game.state = END;
    // tft.setCursor(40,20);
    // tft.println("GAME OVER");
    // delay(10000);
    return;
  }
  for (int i = 0; i < 4; i++)
  {
    int p[2];
    for (int j = 0; j < 2; j++) p[j] = game.currentBlock.points[i][j];
    if (0 <= y + p[1] && y + p[1] < H)
    {
      const int X = game.currentBlock.x + p[0];
      const int Y = y + p[1];
      game.blockGrid[Y][X] = TCOLORS[game.currentBlock.type];
      pixel(X, Y, TCOLORS[game.currentBlock.type]);
    }
  }
  drawUpcomingBlock(BLACK);
  game.currentBlock = game.upcomingBlock;
  game.upcomingBlock = randomBlock();
  drawUpcomingBlock(TCOLORS[game.upcomingBlock.type]);
  clearCompletedLines();
}

void clearCompletedLines() {
  int nLines = 0;
  int lines[4] = { 0, 0, 0, 0 };
  for (int r = H-1; r >= 0; r--)
  {
    bool isComplete = true;
    for (int c = 0; c < W; c++)
    {
      if (game.blockGrid[r][c] == 0)
      {
        isComplete = false;
        break;
      }
    }
    if (isComplete)
    {
      lines[nLines++] = r;
    }
  }
  if (nLines == 0) return;
  redrawScore(BLACK);
  game.totalClearedLines += nLines;
  const int level = game.totalClearedLines / 10.0;
  static const int scores[5] = { 0, 40, 100, 300, 1200 };
  const int points = scores[nLines] * (level + 1);
  game.score += points;
  
  int n = 0;
  for (int r = H-1; r >= 0; r--)
  {
    if (n < nLines && lines[n] == r)
    {
      n++;
    }
    else
    {
      for (int c = 0; c < W; c++)
      {
        game.blockGrid[r+n][c] = game.blockGrid[r][c];
      }
    }
  }
  redrawBlocks();
  redrawScore(WHITE);
}

void redrawBlocks() {
  tft.fillRect(0, 0, W * UNIT_SIZE, H * UNIT_SIZE, BLACK);
  drawBlock(game.currentBlock, TCOLORS[game.currentBlock.type]);
  drawBlockGrid();
}

void redrawScore(int color) {
  const int level = floor(game.totalClearedLines / 10);
  const int x = (W + 2) * UNIT_SIZE;
  tft.setTextColor(color);
  tft.setCursor(x, UNIT_SIZE * 2);
  tft.println("Score:"); 
  tft.setCursor(x, UNIT_SIZE * 4);
  tft.println(game.score);
  tft.setCursor(x, UNIT_SIZE * 6);
  tft.print("Level:"); tft.println(level);
}

void drawBlock(Tetrimino b, int color) {
  for (int i = 0; i < 4; i++)
  {
    pixel(b.x + b.points[i][0], b.y + b.points[i][1], color);
  }
}

void drawBlockGrid() {
  for (int r = 0; r < H; r++)
  {
    for (int c = 0; c < W; c++)
    {
      const int color = game.blockGrid[r][c];
      if (color)
      {
        pixel(c, r, color);
      }
    }
  }
 }

void drawUpcomingBlock(int color) {
  const int x = W + W / 2;
  const int y = H / 2;
  for (int i = 0; i < 4; i++)
  {
    pixel(
      x + game.upcomingBlock.points[i][0]
    , y + game.upcomingBlock.points[i][1]
    , color);
  }
}

byte pickDirection(int x, int y) {
  const int threshold = 300;
  const int middle = 512;
  const int dx = middle - x;
  const int dy = middle - y;
  const bool wentX = abs(dx) > threshold;
  const bool wentY = abs(dy) > threshold;
 
  if (wentX || wentY)
  {
    const bool pickedX = abs(dx) > abs(dy);
    const int z = pickedX ? x : y;
    const bool wentLeft = z > middle;
    return pickedX
      ? wentLeft
        ? LEFT
        : RIGHT
      : wentLeft
        ? DOWN
        : UP
      ;
  }
  return NONE;
}

void pixel(int x, int y, int color) {
  tft.fillRect(x * UNIT_SIZE, y * UNIT_SIZE, UNIT_SIZE, UNIT_SIZE, color);
}

void printJoystickInfo2() {
  tft.setCursor(0, 0);
  tft.print(game.controller.isPressing);
  tft.print(", ");
  tft.println(game.controller.direction);
}
