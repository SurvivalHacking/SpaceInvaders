/*
 * Space Invaders, by Davide Gatti www.survivalhacking.it e MatixVision
 * 17/09/2025
 */

#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "sprites.h" // Includiamo il file degli sprite

// --- DEFINIZIONI DISPLAY ---
#define TFT_BL    9
#define TFT_DC   18
#define TFT_CS   -1
#define TFT_RES   5
#define TFT_SCK   7
#define TFT_MOSI 11
#define TFT_MISO -1
#define TFT_ROTATION 2
#define TFT_WIDTH 240
#define TFT_HEIGHT 320
#define TFT_Y_OFFSET 80 // L'offset specifico per il tuo display ST7789
#define STATUS_BAR_HEIGHT 20 // Altezza della barra superiore per punteggio e vite
#define GAME_AREA_START_Y (TFT_Y_OFFSET + STATUS_BAR_HEIGHT) // Inizio effettivo dell'area di gioco

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, TFT_RES, TFT_ROTATION, true, TFT_WIDTH, TFT_HEIGHT);

// --- DEFINIZIONI PCF8574 ---
#define PCF8574_ADDRESS 0x20
#define I2C_SDA_PIN 33
#define I2C_SCL_PIN 35

// --- DEFINIZIONI BUZZER ---
#define BUZZER_PIN 12

// --- Mappatura pulsanti ---
#define BUTTON_LEFT_PCF_PIN   1
#define BUTTON_RIGHT_PCF_PIN  0
#define BUTTON_FIRE_PCF_PIN   6

// --- VARIABILI GLOBALI PCF ---
uint8_t currentButtonState = 0;
uint8_t previousButtonState = 0;

// --- DEFINIZIONI COLORI (RGB565) ---
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define ORANGE  0xFD20 // Arancione, utile per esplosioni

#define INVADER_COLOR GREEN
#define PLAYER_COLOR  CYAN
#define BARRIER_COLOR GREEN // Colore per le barriere

// --- DEFINIZIONI GIOCO E VARIE ---
#define PLAYER_STEP_SIZE 2 // Velocità di movimento della navicella

// --- DEFINIZIONI NAVICELLA GIOCATORE ---
int playerX;
int playerY;
int playerMoveDelay = 10;
unsigned long lastPlayerMoveTime = 0;

// NUOVE VARIABILI PER LE VITE DEL GIOCATORE E L'ESPLOSIONE
int playerLives = 3; // Inizia con 3 vite
bool playerExploding = false;
unsigned long playerExplosionStartTime = 0;

// Durata di ogni frame in ms per il player
#define PLAYER_EXPLOSION_FRAME_DURATION 1000 // Ogni frame dell'esplosione giocatore dura 1000ms (1 secondo)
#define PLAYER_EXPLOSION_DURATION (EXPLOSION_FRAME_COUNT * PLAYER_EXPLOSION_FRAME_DURATION) // Durata totale animazione (2 secondi totali)

#define PLAYER_RESPAWN_DELAY 500 // 500ms di pausa dopo l'esplosione prima del respawn
unsigned long playerRespawnStartTime = 0;
bool playerRespawning = false; // Flag per indicare che il giocatore è in fase di respawn


// --- DEFINIZIONI PROIETTILE GIOCATORE ---
int bulletX;
int bulletY;
bool bulletActive = false;
int bulletMoveDelay = 10;
unsigned long lastBulletMoveTime = 0;
#define BULLET_STEP_SIZE 4

// Definizione Mystery Ship
bool mysteryActive = false;
int mysteryX;
int mysteryY;
unsigned long lastMysteryTime = 0;
const unsigned long MYSTERY_INTERVAL = 15000; // 15 secondi
#define MYSTERY_SPEED 2 // Velocità UFO in pixel per frame
#define MYSTERY_HEIGHT 16
#define MYSTERY_WIDTH 24
int mysteryScore = 0;           // punteggio corrente dell'UFO
unsigned long mysteryScoreTime; // timestamp per durata visualizzazione punteggio
int ufoToneFreq = 200;           // Frequenza iniziale
bool ufoToneAscending = true;    // Direzione della variazione del tono

// --- DEFINIZIONI INVASORI ---
#define INVADER_ROWS    5
#define INVADER_COLS    8
#define INVADER_SPACING_X 8
#define INVADER_SPACING_Y 8
#define INVADER_START_X 20
#define INVADER_START_Y (GAME_AREA_START_Y + 10) // Ora inizia dopo la status bar

// Variabili per il movimento del blocco di invasori
int invadersOffsetX = 0;
int invadersOffsetY = 0;
int oldInvadersOffsetX = 0;
int oldInvadersOffsetY = 0;
int invaderDirection = 1;
#define INVADER_MOVE_DOWN_AMOUNT 10
#define INVADER_STEP_SIZE 1

int invadersAliveCount = 0;
int totalInvaders = INVADER_ROWS * INVADER_COLS;

// Animazione degli invasori
int currentInvaderFrame = 0; // 0 o 1
unsigned long lastInvaderFrameChangeTime = 0;
#define INVADER_ANIMATION_DELAY 200 // Millisecondi tra un frame e l'altro dell'animazione

// Velocità dinamica degli invasori (inizia lento, accelera)
int invaderMoveDelay = 650; // Ancora più lento
unsigned long lastInvaderMoveTime = 0;

// Variabile per il suono del movimento degli alieni
const int invaderMoveSounds[] = {440, 392, 349, 330}; // Frequenze delle 4 note (La, Sol, Fa, Mi)
int currentInvaderSoundIndex = 0; // Indice per ciclare tra le note
unsigned long lastInvaderSoundTime = 0; // Tempo dell'ultima riproduzione del suono alieno

// Matrice per tenere traccia degli invasori vivi/morti
bool invadersAlive[INVADER_ROWS][INVADER_COLS];
// Matrice per memorizzare le posizioni (X,Y) degli invasori, utile per il calcolo delle collisioni
int invaderCurrentPositions[INVADER_ROWS][INVADER_COLS][2]; // [row][col][0=X, 1=Y]

// --- DEFINIZIONI PROIETTILE ALIENI ---
// Struttura per un singolo proiettile alieno
struct AlienBullet {
  int x;
  int y;
  bool active;
};

#define MAX_ALIEN_BULLETS 3 // Numero massimo di proiettili alieni simultanei
AlienBullet alienBullets[MAX_ALIEN_BULLETS];

int alienBulletMoveDelay = 20; // Velocità dei proiettili alieni
unsigned long lastAlienBulletMoveTime = 0;
unsigned long alienFireInterval_min = 1000; // Minimo ms tra gli spari alieni
unsigned long alienFireInterval_max = 3000; // Massimo ms tra gli spari alieni
unsigned long lastAlienFireTime = 0;
#define ALIEN_BULLET_STEP_SIZE 2 // Quanti pixel si muovono per step


// --- DEFINIZIONI ESPLOSIONI ---
// Durata di ogni frame in ms per gli invasori
#define INVADER_EXPLOSION_FRAME_DURATION 250 // Ogni frame dell'esplosione invasore dura 250ms (0.25 secondi)

#define EXPLOSION_FRAME_COUNT 2 // Numero di frame nell'animazione (0 e 1)

// Struttura per un'esplosione attiva (con l'aggiunta di isPlayerExplosion)
struct Explosion {
  int x;
  int y;
  int frame; // Frame corrente dell'animazione
  unsigned long startTime; // Tempo di inizio dell'esplosione
  bool active;
  bool isPlayerExplosion; // NUOVO: true se è l'esplosione del player, false se di un alieno
};

#define MAX_EXPLOSIONS 5 // Numero massimo di esplosioni simultanee
Explosion explosions[MAX_EXPLOSIONS];


// --- DEFINIZIONI BARRIERE ---
#define NUM_BARRIERS 4 // Numero di barriere
#define BARRIER_BLOCK_SIZE 4 // Dimensione di ogni "blocco" della barriera (4x4 pixel)
#define BARRIER_BLOCK_ROWS 6 // Blocchi in altezza per ogni barriera (RIDOTTO)
#define BARRIER_BLOCK_COLS 8 // Blocchi in larghezza per ogni barriera (RIDOTTO)

// Dimensioni totali di una singola barriera in pixel
#define SINGLE_BARRIER_WIDTH (BARRIER_BLOCK_COLS * BARRIER_BLOCK_SIZE)
#define SINGLE_BARRIER_HEIGHT (BARRIER_BLOCK_ROWS * BARRIER_BLOCK_SIZE)

int barrierYPos; // Posizione Y per tutte le barriere

// Matrice per tenere traccia dello stato di ogni blocco di ogni barriera
bool barriers[NUM_BARRIERS][BARRIER_BLOCK_ROWS][BARRIER_BLOCK_COLS];


// --- VARIABILI GIOCO ---
int playerScore = 0; // Punteggio attuale


// --- STATO DEL GIOCO ---
typedef enum {
  GAME_RUNNING,
  GAME_OVER,
  GAME_WIN
} GameState;

GameState currentGameState = GAME_RUNNING;


// --- START SCREEN ---

void drawStartScreen() {
  gfx->fillScreen(BLACK);
  gfx->drawBitmap(25, 80, SPACEINVADERS_200x86, 200, 86, GREEN, BLACK);

  gfx->setTextSize(1);
  gfx->setTextColor(WHITE);
  gfx->setCursor(60, 165 + 30);
  gfx->println("*SCORE ADVANCE TABLE*");

  // Tabella punteggi
  gfx->setCursor(115, 190 + 30);
  gfx->println("= ");
  gfx->setCursor(130, 190 + 30);
  gfx->println("? MYSTERY");

  gfx->setCursor(115, 205 + 30);
  gfx->println("= ");
  gfx->setCursor(130, 205 + 30);
  gfx->println("30 POINTS");

  gfx->setCursor(115, 220 + 30);
  gfx->println("= ");
  gfx->setCursor(130, 220 + 30);
  gfx->println("20 POINTS");
  
  gfx->setCursor(115, 235 + 30);
  gfx->setTextColor(GREEN);
  gfx->println("= ");
  gfx->setCursor(130, 235 + 30);
  gfx->println("10 POINTS");

  gfx->setCursor(65, 260 + 30);
  gfx->setTextColor(GREEN);
  gfx->println("Press Button to Start");

  // Disegno icone
  gfx->drawBitmap(75, 190 + 30, Sprite_Mystery_24x8, 24, 8, WHITE, BLACK);
  gfx->drawBitmap(80, 205 + 30, Sprite_AlineA1_16x8, 16, 8, WHITE, BLACK);
  gfx->drawBitmap(80, 220 + 30, Sprite_AlineB1_16x8, 16, 8, WHITE, BLACK);
  gfx->drawBitmap(80, 235 + 30, Sprite_AlineC1_16x8, 16, 8, GREEN, BLACK);

  gfx->setTextColor(WHITE);
  gfx->setCursor(150, 310);
  gfx->println("CREDIT ");
  gfx->setCursor(195, 310);
  gfx->println("00");
}


// --- DEFINIZIONI GAME OVER ---
#define GAME_OVER_Y_THRESHOLD (playerY - 10)

// --- FUNZIONI DI GIOCO ---


// Funzione per visualizzare il punteggio nella status bar
void drawScore(int score) {
  gfx->fillRect(0, TFT_Y_OFFSET, gfx->width() / 2, STATUS_BAR_HEIGHT, BLACK); // Cancella vecchia parte del punteggio
  gfx->setTextSize(1);
  gfx->setTextColor(WHITE);
  gfx->setCursor(5, TFT_Y_OFFSET + 5); // Posizione
  gfx->print("SCORE:");
  gfx->print(score);
}


// Funzione per visualizzare le vite nella status bar
void drawLives(int lives) {
  gfx->fillRect(gfx->width() / 2, TFT_Y_OFFSET, gfx->width() / 2, STATUS_BAR_HEIGHT, BLACK); // Cancella vecchia parte delle vite
  gfx->setTextSize(1);
  gfx->setTextColor(WHITE);
  gfx->setCursor(gfx->width() / 2 + 5, TFT_Y_OFFSET + 5); // Posizione testo "LIVES:"
  gfx->print("LIVES:");
  // Disegna una piccola icona per ogni vita (ad es. un quadrato o un player_bits ridotto)
  for (int i = 0; i < lives; i++) {
    // Calcola la posizione per ogni icona di vita
    int iconX = gfx->width() / 2 + 5 + (7 * 6) + (i * (PLAYER_WIDTH / 2 + 2)); // 7 caratteri * 6 pixel/carattere + spaziatura
    gfx->fillRect(iconX, TFT_Y_OFFSET + 5, PLAYER_WIDTH / 2, PLAYER_HEIGHT / 2, PLAYER_COLOR); // Disegna un piccolo rettangolo
  }
  // Cancella le icone delle vite che sono diminuite (fino a un massimo di 3)
  for (int i = lives; i < 3; i++) {
      int iconX = gfx->width() / 2 + 5 + (7 * 6) + (i * (PLAYER_WIDTH / 2 + 2));
      gfx->fillRect(iconX, TFT_Y_OFFSET + 5, PLAYER_WIDTH / 2, PLAYER_HEIGHT / 2, BLACK);
  }
}

void drawPlayerSprite(int x, int y, uint16_t color) {
  if (color == BLACK) {
    gfx->fillRect(x, y, PLAYER_WIDTH, PLAYER_HEIGHT, BLACK);
  } else {
    gfx->draw16bitRGBBitmap(x, y, player_bits, PLAYER_WIDTH, PLAYER_HEIGHT);
  }
}

void drawBulletSprite(int x, int y, uint16_t color) {
  if (color == BLACK) {
    gfx->fillRect(x, y, BULLET_WIDTH, BULLET_HEIGHT, BLACK);
  } else {
    gfx->draw16bitRGBBitmap(x, y, player_bullet_bits, BULLET_WIDTH, BULLET_HEIGHT);
  }
}

// Funzione per disegnare un singolo proiettile alieno (come sprite)
void drawAlienBulletSprite(int x, int y, uint16_t color) {
  if (color == BLACK) {
    gfx->fillRect(x, y, BULLET_WIDTH, BULLET_HEIGHT, BLACK); // Usiamo le stesse dimensioni
  } else {
    gfx->draw16bitRGBBitmap(x, y, alien_bullet_bits, BULLET_WIDTH, BULLET_HEIGHT);
  }
}


// Funzione per disegnare un singolo invasore (come sprite animato)
void drawSingleInvaderSprite(int row, int col, int offsetX, int offsetY, uint16_t color, int frame) {
  int invaderX = INVADER_START_X + col * (INVADER_WIDTH + INVADER_SPACING_X) + offsetX;
  int invaderY = INVADER_START_Y + row * (INVADER_HEIGHT + INVADER_SPACING_Y) + offsetY;

  if (color == BLACK) {
    gfx->fillRect(invaderX, invaderY, INVADER_WIDTH, INVADER_HEIGHT, BLACK);
  } else {
    const uint16_t* currentInvaderSprite;

    // Seleziona lo sprite in base alla riga (ogni 2 righe cambia)
    // E in base al frame di animazione
    if (row < 2) { // Prime due righe usano Tipo 0
      currentInvaderSprite = (frame == 0) ? invader0_bits : invader1_bits;
    } else { // Tutte le altre righe usano Tipo 1
      currentInvaderSprite = (frame == 0) ? invader2_bits : invader3_bits; // Usa i nuovi sprite
    }

    gfx->draw16bitRGBBitmap(invaderX, invaderY, currentInvaderSprite, INVADER_WIDTH, INVADER_HEIGHT);
  }
}

// Funzione per avviare una nuova esplosione
// Ora accetta un parametro per indicare se è un'esplosione del player
void startExplosion(int x, int y, bool isPlayer) {
  for (int i = 0; i < MAX_EXPLOSIONS; i++) {
    if (!explosions[i].active) {
      explosions[i].active = true;
      explosions[i].x = x;
      explosions[i].y = y;
      explosions[i].frame = 0;
      explosions[i].startTime = millis();
      explosions[i].isPlayerExplosion = isPlayer; // Salva il tipo di esplosione
      break;
    }
  }
}

// Funzione per disegnare le esplosioni attive
void drawExplosions() {
  for (int i = 0; i < MAX_EXPLOSIONS; i++) {
    if (explosions[i].active) {
      unsigned long elapsed = millis() - explosions[i].startTime;
      
      // Determina la durata del frame in base al tipo di esplosione
      unsigned long currentExplosionFrameDuration = explosions[i].isPlayerExplosion ? PLAYER_EXPLOSION_FRAME_DURATION : INVADER_EXPLOSION_FRAME_DURATION;
      
      int currentFrame = elapsed / currentExplosionFrameDuration;

      if (currentFrame >= EXPLOSION_FRAME_COUNT) {
        // Animazione finita, cancella L'ULTIMO FRAME DAL DISPLAY e disattiva
        // Usa le dimensioni appropriate per l'esplosione (Player o Invader)
        if (explosions[i].isPlayerExplosion) {
            gfx->fillRect(explosions[i].x, explosions[i].y, PLAYER_WIDTH, PLAYER_HEIGHT, BLACK);
        } else {
            gfx->fillRect(explosions[i].x, explosions[i].y, INVADER_WIDTH, INVADER_HEIGHT, BLACK);
        }
        explosions[i].active = false;
      } else {
        const uint16_t* currentExplosionSprite = (currentFrame == 0) ? explosion0_bits : explosion1_bits;
        // La dimensione dello sprite di esplosione è unica, ma il rettangolo da cancellare dipende
        // da chi è esploso. Per disegnare l'esplosione usiamo sempre la dimensione dello sprite.
        gfx->draw16bitRGBBitmap(explosions[i].x, explosions[i].y, currentExplosionSprite, INVADER_WIDTH, INVADER_HEIGHT);
      }
    }
  }
}

// Nuova funzione per aggiornare l'intero blocco di invasori (cancellazione e disegno)
void updateInvadersDisplay(int oldOffsetX, int oldOffsetY, int newOffsetX, int newOffsetY, int currentInvaderAnimFrame) {
  for (int row = 0; row < INVADER_ROWS; row++) {
    for (int col = 0; col < INVADER_COLS; col++) {
      int oldInvaderX = INVADER_START_X + col * (INVADER_WIDTH + INVADER_SPACING_X) + oldOffsetX;
      int oldInvaderY = INVADER_START_Y + row * (INVADER_HEIGHT + INVADER_SPACING_Y) + oldOffsetY;
      gfx->fillRect(oldInvaderX, oldInvaderY, INVADER_WIDTH, INVADER_HEIGHT, BLACK);

      if (invadersAlive[row][col]) {
        drawSingleInvaderSprite(row, col, newOffsetX, newOffsetY, INVADER_COLOR, currentInvaderAnimFrame);
        invaderCurrentPositions[row][col][0] = INVADER_START_X + col * (INVADER_WIDTH + INVADER_SPACING_X) + newOffsetX;
        invaderCurrentPositions[row][col][1] = INVADER_START_Y + row * (INVADER_HEIGHT + INVADER_SPACING_Y) + newOffsetY;
      }
    }
  }
}

// Funzione per il rilevamento collisioni AABB (Axis-Aligned Bounding Box)
bool checkCollision(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) {
  return (x1 < x2 + w2 &&
          x1 + w1 > x2 &&
          y1 < y2 + h2 &&
          y1 + h1 > y2);
}

// Funzione per disegnare tutte le barriere
void drawBarriers() {
  // Calcola la spaziatura tra le barriere per centrarle
  int totalBarriersWidth = NUM_BARRIERS * SINGLE_BARRIER_WIDTH;
  // Calcola lo spazio totale disponibile per le barriere (escludendo i margini laterali)
  int availableWidth = gfx->width();
  int totalSpacing = availableWidth - totalBarriersWidth;
  int barrierSpacing = totalSpacing / (NUM_BARRIERS + 1); // Spazio prima della prima, tra le barriere, e dopo l'ultima

  for (int i = 0; i < NUM_BARRIERS; i++) {
    int barrierStartX = barrierSpacing + i * (SINGLE_BARRIER_WIDTH + barrierSpacing);
    for (int r = 0; r < BARRIER_BLOCK_ROWS; r++) {
      for (int c = 0; c < BARRIER_BLOCK_COLS; c++) {
        int blockX = barrierStartX + c * BARRIER_BLOCK_SIZE;
        int blockY = barrierYPos + r * BARRIER_BLOCK_SIZE;

        if (barriers[i][r][c]) {
          gfx->fillRect(blockX, blockY, BARRIER_BLOCK_SIZE, BARRIER_BLOCK_SIZE, BARRIER_COLOR);
        } else {
          // Cancella il blocco distrutto (assicurati che sia nero)
          gfx->fillRect(blockX, blockY, BARRIER_BLOCK_SIZE, BARRIER_BLOCK_SIZE, BLACK);
        }
      }
    }
  }
}


// Funzione per visualizzare la schermata di Game Over
void showGameOverScreen() {
  gfx->fillScreen(BLACK);
  gfx->setTextSize(3);
  gfx->setTextColor(WHITE);
  gfx->setCursor((gfx->width() - (7 * 18)) / 2, (gfx->height() - 24) / 2); // Centra il testo "GAME OVER"
  gfx->println("GAME");
  gfx->setCursor((gfx->width() - (7 * 18)) / 2, (gfx->height() - 24) / 2 + 30);
  gfx->println("OVER");
  gfx->setTextSize(1);
  gfx->setCursor((gfx->width() - (15 * 6)) / 2, (gfx->height() - 8) / 2 + 80); // Piccola riga sotto
  gfx->setTextColor(RED);
  gfx->println("Invaders Win!");
}

void setup() {
  Serial.begin(115200);
  Serial.println("--- INIZIO SETUP (Con Sprite e Animazione Invasori) ---");

  delay(1000);
  Serial.println("Setup: Delay iniziale di 1 secondo completato.");

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  Serial.println("Setup: Accensione Backlight.");

  gfx->begin();
  delay(200);
  Serial.println("Setup: Inizializzazione GFX begin().");

  // --- Inizializzazione I2C (Wire.begin) ---
  Serial.println("Setup: Inizializzazione Wire.begin().");
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  delay(200);

  Serial.println("Setup: Tentativo di impostazione pin PCF8574 a HIGH.");
  byte status;
  int retries = 0;
  const int maxRetries = 5;
  do {
    Wire.beginTransmission(PCF8574_ADDRESS);
    Wire.write(0xFF);
    status = Wire.endTransmission();
    if (status != 0) {
      Serial.print("PCF8574: Errore iniziale (status "); Serial.print(status); Serial.print("). Riprovo "); Serial.print(retries + 1); Serial.print("/"); Serial.println(maxRetries);
      delay(50);
    }
    retries++;
  } while (status != 0 && retries < maxRetries);

  if (status == 0) {
    Serial.println("PCF8574: Invio iniziale di 0xFF OK.");
  } else {
    Serial.print("PCF8574: FALLITO ANCORA dopo "); Serial.print(maxRetries); Serial.println(" tentativi. Blocco fatale.");
    Serial.println("SDA: " + String(I2C_SDA_PIN) + ", SCL: " + String(I2C_SCL_PIN));
    while(1);
  }

  // --- Inizializzazione Buzzer ---
  Serial.println("Setup: Inizializzazione Buzzer.");
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("Setup: Buzzer inizializzato.");

  drawStartScreen();
  waitkey();
  gfx->fillScreen(BLACK);

  // --- POSIZIONAMENTO INIZIALE NAVICELLA ---
  playerX = (gfx->width() - PLAYER_WIDTH) / 2;
  playerY = gfx->height() - PLAYER_HEIGHT - 5; // Questo resta relativo al fondo dello schermo
  playerLives = 3; // Imposta le vite all'inizio del gioco
  playerScore = 0; // Inizializza il punteggio a zero

  Serial.println("Setup: Posizione iniziale navicella calcolata.");

  // --- INIZIALIZZAZIONE INVASORI ---
  Serial.println("Setup: Inizializzazione invasori.");
  invadersAliveCount = 0;
  for (int row = 0; row < INVADER_ROWS; row++) {
    for (int col = 0; col < INVADER_COLS; col++) {
      invadersAlive[row][col] = true; // Tutti gli invasori sono vivi all'inizio
      invadersAliveCount++;
    }
  }
  // Inizializza i proiettili alieni come inattivi
  for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
    alienBullets[i].active = false;
  }
  // Inizializza le esplosioni come inattive
  for (int i = 0; i < MAX_EXPLOSIONS; i++) {
    explosions[i].active = false;
  }

  // --- INIZIALIZZAZIONE BARRIERE ---
  Serial.println("Setup: Inizializzazione barriere.");
  // Posiziona le barriere tra gli alieni e il giocatore
  // Ad esempio, a 2/3 dell'altezza totale dal fondo dello schermo
  barrierYPos = playerY - SINGLE_BARRIER_HEIGHT - 20; // 20 pixel sopra il giocatore (modificato)
  if (barrierYPos < INVADER_START_Y + INVADER_ROWS * (INVADER_HEIGHT + INVADER_SPACING_Y)) {
      // Assicurati che non si sovrappongano agli invasori
      barrierYPos = INVADER_START_Y + INVADER_ROWS * (INVADER_HEIGHT + INVADER_SPACING_Y) + 10; // 10 pixel sotto gli invasori
  }


  for (int i = 0; i < NUM_BARRIERS; i++) {
    for (int r = 0; r < BARRIER_BLOCK_ROWS; r++) {
      for (int c = 0; c < BARRIER_BLOCK_COLS; c++) {
        barriers[i][r][c] = true; // Inizialmente tutti i blocchi sono intatti

        // NUOVA LOGICA PER LA FORMA DELLA BARRIERA (più fedele all'originale)
        // Angoli superiori smussati
        if (r < 2 && (c < 2 || c >= BARRIER_BLOCK_COLS - 2)) {
            barriers[i][r][c] = false;
        }
        // Piccola apertura al centro superiore
        if (r == 0 && (c == BARRIER_BLOCK_COLS / 2 - 1 || c == BARRIER_BLOCK_COLS / 2)) {
            barriers[i][r][c] = false;
        }
        // Parte inferiore centrale (la "bocca" della barriera)
        if (r >= BARRIER_BLOCK_ROWS - 2 && (c >= BARRIER_BLOCK_COLS / 2 - 2 && c < BARRIER_BLOCK_COLS / 2 + 2)) {
            barriers[i][r][c] = false;
        }
        // I due blocchi ai lati della "bocca" centrale (per completare la forma ad U rovesciata)
        if (r == BARRIER_BLOCK_ROWS - 3 && (c == BARRIER_BLOCK_COLS / 2 - 2 || c == BARRIER_BLOCK_COLS / 2 + 1)) {
             barriers[i][r][c] = false;
        }
      }
    }
  }


  // Disegna la status bar iniziale
  gfx->fillRect(0, TFT_Y_OFFSET, gfx->width(), STATUS_BAR_HEIGHT, BLACK); // Sfondo nero per la status bar
  drawScore(playerScore);
  drawLives(playerLives);

  // Disegna gli invasori nella posizione iniziale
  updateInvadersDisplay(invadersOffsetX, invadersOffsetY, invadersOffsetX, invadersOffsetY, currentInvaderFrame);
  // Disegna le barriere
  drawBarriers();
  Serial.println("Setup: Invasori e barriere inizializzati e disegnati la prima volta.");
  Serial.println("--- FINE SETUP ---");
  lastMysteryTime = millis();  // inizializza il timer per far partire il primo UFO dopo MYSTERY_INTERVAL
}

void loop() {
  // Queste condizioni di fine gioco devono essere le prime per bloccare tutto immediatamente
  if (currentGameState == GAME_OVER) {
    noTone(BUZZER_PIN); // Ferma qualsiasi suono
    showGameOverScreen();
    delay(5000); // Rimani sulla schermata per 5 secondi
    ESP.restart(); // Riavvia l'ESP32
    return; // Esci dal loop()
  }

  if (currentGameState == GAME_WIN) {
    noTone(BUZZER_PIN); // Ferma qualsiasi suono
    delay(2000);        // Pausa prima di ricominciare
    resetWave();        // Reimposta alieni, proiettili e barriere
    currentGameState = GAME_RUNNING; // Torna a giocare
    return;
  }

  // Blocco della logica di gioco quando il giocatore sta esplodendo o respawnando
  if (playerExploding || playerRespawning) {
      // Gestione esplosione navicella giocatore
      if (playerExploding) {
          if (millis() - playerExplosionStartTime > PLAYER_EXPLOSION_DURATION) {
              playerExploding = false; // Fine animazione esplosione
              noTone(BUZZER_PIN); // Ferma il suono dell'esplosione
              // Cancella l'ultima frame dell'esplosione (già fatto in drawExplosions)

              if (playerLives > 0) {
                  playerRespawning = true; // Inizia la fase di respawn
                  playerRespawnStartTime = millis();
                  // Non resetta playerX/Y qui, lo faremo dopo la pausa di respawn
              }
          }
      }

      // Gestione fase di respawn
      if (playerRespawning) {
          if (millis() - playerRespawnStartTime > PLAYER_RESPAWN_DELAY) {
              playerRespawning = false; // Fine pausa respawn
              // Ora resetta la posizione del giocatore
              playerX = (gfx->width() - PLAYER_WIDTH) / 2; // Riposiziona al centro
              playerY = gfx->height() - PLAYER_HEIGHT - 5;
              // La navicella verrà ridisegnata dal codice sotto nel loop
          }
      }
      drawExplosions(); // Continua a disegnare l'esplosione se attiva
      delay(1); // Piccolo ritardo per non saturare la CPU
      return; // Salta il resto del loop per bloccare il gioco
  }


  // Logica di gioco normale quando GAME_RUNNING e non in esplosione/respawn
  previousButtonState = currentButtonState;
  uint8_t newButtonState = 0;
  uint8_t pcf_read_value = 0xFF;

  Wire.requestFrom(PCF8574_ADDRESS, 1);
  if (Wire.available()) {
    pcf_read_value = Wire.read();
  }

  uint8_t inverted_pcf_value = ~pcf_read_value;

  if (inverted_pcf_value & (1 << BUTTON_LEFT_PCF_PIN)) {
    newButtonState |= (1 << 0);
  }
  if (inverted_pcf_value & (1 << BUTTON_RIGHT_PCF_PIN)) {
    newButtonState |= (1 << 1);
  }
  if (inverted_pcf_value & (1 << BUTTON_FIRE_PCF_PIN)) {
    newButtonState |= (1 << 2);
  }
  currentButtonState = newButtonState;

  // --- Movimento Navicella Temporizzato ---
  int oldPlayerX = playerX;

  // Movimento giocatore solo se non sta esplodendo E non è in respawn (già controllato sopra)
  if (millis() - lastPlayerMoveTime > playerMoveDelay) {
    lastPlayerMoveTime = millis();

    // Cancella la vecchia posizione della navicella prima di muoverla
    drawPlayerSprite(oldPlayerX, playerY, BLACK);

    if (currentButtonState & (1 << 0)) {
      playerX -= PLAYER_STEP_SIZE;
    }
    if (currentButtonState & (1 << 1)) {
      playerX += PLAYER_STEP_SIZE;
    }

    if (playerX < 0) {
      playerX = 0;
    }
    if (playerX > gfx->width() - PLAYER_WIDTH) {
      playerX = gfx->width() - PLAYER_WIDTH;
    }
  }


  // --- Gestione Proiettili Giocatore ---
  int oldBulletY = bulletY;

  // Il giocatore spara solo se non sta esplodendo E non è in respawn (già controllato sopra)
  if (currentButtonState & (1 << 2) && !(previousButtonState & (1 << 2))) {
    if (!bulletActive) {
      bulletActive = true;
      bulletX = playerX + (PLAYER_WIDTH / 2) - (BULLET_WIDTH / 2);
      bulletY = playerY - BULLET_HEIGHT;
      lastBulletMoveTime = millis();
    }
  }

  if (bulletActive) {
    if (millis() - lastBulletMoveTime > bulletMoveDelay) {
      lastBulletMoveTime = millis();

      // Cancella il proiettile nella sua vecchia posizione prima di muoverlo
      drawBulletSprite(bulletX, oldBulletY, BLACK);

      bulletY -= BULLET_STEP_SIZE;

      if (bulletY + BULLET_HEIGHT < GAME_AREA_START_Y) { // Il proiettile esce dall'area di gioco
        bulletActive = false;
      } else {
        // --- Rilevamento Collisioni Proiettile Giocatore-Barriera ---
        bool hitBarrier = false;
        // Calcola la spaziatura tra le barriere per centrarle (ripetuto per coerenza)
        int totalBarriersWidth = NUM_BARRIERS * SINGLE_BARRIER_WIDTH;
        int availableWidth = gfx->width();
        int totalSpacing = availableWidth - totalBarriersWidth;
        int barrierSpacing = totalSpacing / (NUM_BARRIERS + 1);

        for (int i = 0; i < NUM_BARRIERS; i++) {
          int barrierStartX = barrierSpacing + i * (SINGLE_BARRIER_WIDTH + barrierSpacing);
          for (int r = 0; r < BARRIER_BLOCK_ROWS; r++) {
            for (int c = 0; c < BARRIER_BLOCK_COLS; c++) {
              if (barriers[i][r][c]) { // Solo se il blocco è intatto
                int blockX = barrierStartX + c * BARRIER_BLOCK_SIZE;
                int blockY = barrierYPos + r * BARRIER_BLOCK_SIZE;

                if (checkCollision(bulletX, bulletY, BULLET_WIDTH, BULLET_HEIGHT,
                                   blockX, blockY, BARRIER_BLOCK_SIZE, BARRIER_BLOCK_SIZE)) {
                  barriers[i][r][c] = false; // Distruggi il blocco
                  gfx->fillRect(blockX, blockY, BARRIER_BLOCK_SIZE, BARRIER_BLOCK_SIZE, BLACK); // Cancella dal display
                  bulletActive = false; // Disattiva il proiettile
                  tone(BUZZER_PIN, 1000, 50); // Piccolo suono per colpo alla barriera
                  hitBarrier = true;
                  break; // Esci dal loop dei blocchi
                }
              }
            }
            if (hitBarrier) break; // Esci dal loop delle righe
          }
          if (hitBarrier) break; // Esci dal loop delle barriere
        }


        // --- Rilevamento Collisioni Proiettile Giocatore-Invasore (solo se non ha colpito una barriera) ---
        if (!hitBarrier) {
            for (int row = 0; row < INVADER_ROWS; row++) {
                for (int col = 0; col < INVADER_COLS; col++) {
                    if (invadersAlive[row][col]) {
                        if (checkCollision(bulletX, bulletY, BULLET_WIDTH, BULLET_HEIGHT,
                                           invaderCurrentPositions[row][col][0], invaderCurrentPositions[row][col][1],
                                           INVADER_WIDTH, INVADER_HEIGHT)) {

                            invadersAlive[row][col] = false;
                            invadersAliveCount--;
                            bulletActive = false;

                            playerScore += 10; // Incrementa il punteggio
                            drawScore(playerScore); // Aggiorna il punteggio sul display

                            tone(BUZZER_PIN, 220, 100); // Suono di distruzione invasore

                            // Avvia l'animazione di esplosione nella posizione dell'invasore colpito
                            startExplosion(invaderCurrentPositions[row][col][0], invaderCurrentPositions[row][col][1], false); // FALSE: è un'esplosione aliena

                            // Cancella subito il proiettile
                            drawBulletSprite(bulletX, bulletY, BLACK);
                            break; // Esci dal loop interno per non controllare altre collisioni con questo proiettile
                        }
                    }
                }
                if (!bulletActive) break; // Esci anche dal loop esterno se il proiettile è disattivato
            }
        }
      }
    }
  }

  // --- Movimento Invasori e Accelerazione ---
  if (invadersAliveCount > 0) {
      // invaderMoveDelay inizia a 650 (più lento) e accelera fino a 5ms (velocissimo)
      invaderMoveDelay = map(invadersAliveCount, 1, totalInvaders, 5, 650); // Modificato il valore minimo
      if (invaderMoveDelay < 5) invaderMoveDelay = 5; // Limite minimo
  } else {
      currentGameState = GAME_WIN; // Nessuno invasore, vittoria
      return;
  }

  // Animazione Invasori (cambio frame)
  if (millis() - lastInvaderFrameChangeTime > INVADER_ANIMATION_DELAY) {
      lastInvaderFrameChangeTime = millis();
      currentInvaderFrame = 1 - currentInvaderFrame; // Alterna 0 e 1
  }

  // Movimento Invasori e SUONO OTTIMIZZATO (la velocità del suono può essere fissa per gli ultimi alieni)
  if (millis() - lastInvaderMoveTime > invaderMoveDelay) {
    lastInvaderMoveTime = millis();

    // --- Suono Invasori ---
    int effectiveSoundDelay;
    if (invadersAliveCount <= 4) {
        // Per gli ultimi 4 alieni (compresi), usa la cadenza fissa di quando ce ne sono 4
        // (Circa 55ms con le attuali definizioni)
        effectiveSoundDelay = map(4, 1, totalInvaders, 5, 650);
    } else {
        // Altrimenti, usa il delay dinamico degli invasori per il suono
        effectiveSoundDelay = invaderMoveDelay;
    }

    // Riproduci il suono solo se il delay effettivo per il suono non è troppo basso
    // (minimo 50ms per la stabilità della funzione tone())
    // E se è passato abbastanza tempo dall'ultima riproduzione del suono con questa cadenza.
    if (effectiveSoundDelay >= 50 && millis() - lastInvaderSoundTime > effectiveSoundDelay) {
        tone(BUZZER_PIN, invaderMoveSounds[currentInvaderSoundIndex], 50); // Durata nota 50ms
        currentInvaderSoundIndex = (currentInvaderSoundIndex + 1) % 4; // Passa alla prossima nota
        lastInvaderSoundTime = millis(); // Aggiorna il timestamp del suono
    } else if (effectiveSoundDelay < 50) { // Se il delay di movimento è troppo veloce, spegni il suono
        noTone(BUZZER_PIN);
    }


    oldInvadersOffsetX = invadersOffsetX;
    oldInvadersOffsetY = invadersOffsetY;

    int leftmostInvaderX = gfx->width();
    int rightmostInvaderX = 0;
    int lowestInvaderY = 0;

    bool foundAnyInvader = false;
    for (int col = 0; col < INVADER_COLS; col++) {
      for (int row = 0; row < INVADER_ROWS; row++) {
        if (invadersAlive[row][col]) {
          foundAnyInvader = true;
          int currentInvaderX_calc = INVADER_START_X + col * (INVADER_WIDTH + INVADER_SPACING_X) + invadersOffsetX;
          int currentInvaderY_calc = INVADER_START_Y + row * (INVADER_HEIGHT + INVADER_SPACING_Y) + invadersOffsetY;

          if (currentInvaderX_calc < leftmostInvaderX) leftmostInvaderX = currentInvaderX_calc;
          if (currentInvaderX_calc + INVADER_WIDTH > rightmostInvaderX) rightmostInvaderX = currentInvaderX_calc + INVADER_WIDTH;
          if (currentInvaderY_calc + INVADER_HEIGHT > lowestInvaderY) lowestInvaderY = currentInvaderY_calc + INVADER_HEIGHT;
        }
      }
    }

    if (!foundAnyInvader) {
        return; // Non ci sono più invasori, gestito da GAME_WIN sopra
    }

    // --- CONTROLLO GAME OVER (invasori arrivano in fondo) ---
    if (lowestInvaderY >= GAME_OVER_Y_THRESHOLD) {
      currentGameState = GAME_OVER;
      return;
    }

    // Spostamento orizzontale
    invadersOffsetX += invaderDirection * INVADER_STEP_SIZE;

    // Controllo dei bordi del display per il blocco di invasori VIVI
    if (invaderDirection == 1 && rightmostInvaderX >= gfx->width()) {
      invaderDirection = -1;
      invadersOffsetY += INVADER_MOVE_DOWN_AMOUNT;
      // Corregge l'offset per far sì che il blocco non superi il bordo destro
      invadersOffsetX = invadersOffsetX - (rightmostInvaderX - gfx->width());

    } else if (invaderDirection == -1 && leftmostInvaderX <= 0) {
      invaderDirection = 1;
      invadersOffsetY += INVADER_MOVE_DOWN_AMOUNT;
      // Corregge l'offset per far sì che il blocco non superi il bordo sinistro
      invadersOffsetX = invadersOffsetX - leftmostInvaderX;
    }

    // Passiamo il frame corrente per l'animazione
    updateInvadersDisplay(oldInvadersOffsetX, oldInvadersOffsetY, invadersOffsetX, invadersOffsetY, currentInvaderFrame);

  }

  // --- Gestione Proiettili Alieni ---
  // Ogni tanto, un alieno spara
  if (millis() - lastAlienFireTime > random(alienFireInterval_min, alienFireInterval_max)) {
    lastAlienFireTime = millis();
    // Trova un alieno vivo a caso dalla riga più bassa per sparare
    int alienColToFire = -1;
    // Crea una lista di colonne che contengono alieni vivi nella riga più bassa
    int lowestAliensInCols[INVADER_COLS];
    int lowestAlienCount = 0;

    for (int col = 0; col < INVADER_COLS; col++) {
      for (int row = INVADER_ROWS - 1; row >= 0; row--) { // Partiamo dal basso
        if (invadersAlive[row][col]) {
          lowestAliensInCols[lowestAlienCount++] = col;
          break; // Trovato l'alieno più basso in questa colonna, passa alla prossima colonna
        }
      }
    }

    if (lowestAlienCount > 0) {
      // Scegli una colonna a caso tra quelle che hanno alieni vivi nella riga più bassa
      int randomColIndex = random(0, lowestAlienCount);
      alienColToFire = lowestAliensInCols[randomColIndex];

      // Trova uno slot libero per il proiettile alieno
      for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
        if (!alienBullets[i].active) {
          alienBullets[i].active = true;
          // Posiziona il proiettile alla base dell'alieno più basso della colonna selezionata
          int lowestAlienRow = -1;
          for(int r = INVADER_ROWS - 1; r >= 0; r--) {
              if(invadersAlive[r][alienColToFire]) {
                  lowestAlienRow = r;
                  break;
              }
          }
          if (lowestAlienRow != -1) { // Assicurati di aver trovato un alieno
              alienBullets[i].x = invaderCurrentPositions[lowestAlienRow][alienColToFire][0] + (INVADER_WIDTH / 2) - (BULLET_WIDTH / 2);
              alienBullets[i].y = invaderCurrentPositions[lowestAlienRow][alienColToFire][1] + INVADER_HEIGHT;
              break; // Trovato uno slot libero per il proiettile, esci dal loop
          }
        }
      }
    }
  }

  // Aggiornamento e disegno proiettili alieni
  if (millis() - lastAlienBulletMoveTime > alienBulletMoveDelay) {
    lastAlienBulletMoveTime = millis();
    for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
      if (alienBullets[i].active) {
        drawAlienBulletSprite(alienBullets[i].x, alienBullets[i].y, BLACK); // Cancella vecchia posizione
        alienBullets[i].y += ALIEN_BULLET_STEP_SIZE; // Muovi verso il basso

        if (alienBullets[i].y > gfx->height()) { // Fuori dallo schermo
          alienBullets[i].active = false;
        } else {
          // --- Rilevamento Collisione Proiettile Alieno-Barriera ---
          bool hitBarrier = false;
          // Calcola la spaziatura tra le barriere per centrarle (ripetuto per coerenza)
          int totalBarriersWidth = NUM_BARRIERS * SINGLE_BARRIER_WIDTH;
          int availableWidth = gfx->width();
          int totalSpacing = availableWidth - totalBarriersWidth;
          int barrierSpacing = totalSpacing / (NUM_BARRIERS + 1);

          for (int b = 0; b < NUM_BARRIERS; b++) {
            int barrierStartX = barrierSpacing + b * (SINGLE_BARRIER_WIDTH + barrierSpacing);
            for (int r = 0; r < BARRIER_BLOCK_ROWS; r++) {
              for (int c = 0; c < BARRIER_BLOCK_COLS; c++) {
                if (barriers[b][r][c]) { // Solo se il blocco è intatto
                  int blockX = barrierStartX + c * BARRIER_BLOCK_SIZE;
                  int blockY = barrierYPos + r * BARRIER_BLOCK_SIZE;

                  if (checkCollision(alienBullets[i].x, alienBullets[i].y, BULLET_WIDTH, BULLET_HEIGHT,
                                     blockX, blockY, BARRIER_BLOCK_SIZE, BARRIER_BLOCK_SIZE)) {
                    barriers[b][r][c] = false; // Distruggi il blocco
                    gfx->fillRect(blockX, blockY, BARRIER_BLOCK_SIZE, BARRIER_BLOCK_SIZE, BLACK); // Cancella dal display
                    alienBullets[i].active = false; // Disattiva il proiettile
                    // tone(BUZZER_PIN, 1000, 50); // Piccolo suono per colpo alla barriera
                    hitBarrier = true;
                    break; // Esci dal loop dei blocchi
                  }
                }
              }
              if (hitBarrier) break; // Esci dal loop delle righe
            }
            if (hitBarrier) break; // Esci dal loop delle barriere
          }

          // --- Rilevamento Collisione Proiettile Alieno-Navicella Giocatore (solo se non ha colpito una barriera) ---
          if (!hitBarrier) {
            if (checkCollision(alienBullets[i].x, alienBullets[i].y, BULLET_WIDTH, BULLET_HEIGHT,
                               playerX, playerY, PLAYER_WIDTH, PLAYER_HEIGHT)) {
              // Player hit! Gestione delle vite
              // Verifica di nuovo lo stato di esplosione/respawn qui, se il proiettile è già in volo
              if (!playerExploding && !playerRespawning) {
                  playerLives--;
                  drawLives(playerLives); // Aggiorna le vite sul display
                  tone(BUZZER_PIN, 50, 200); // Suono esplosione giocatore
                  startExplosion(playerX, playerY, true); // TRUE: è l'esplosione del player
                  playerExploding = true;
                  playerExplosionStartTime = millis();

                  // --- RESETTA TUTTI I PROIETTILI QUANDO IL GIOCATORE VIENE COLPITO ---
                  bulletActive = false; // Proiettile del giocatore
                  drawBulletSprite(bulletX, bulletY, BLACK); // Cancella dal display

                  for (int j = 0; j < MAX_ALIEN_BULLETS; j++) { // Tutti i proiettili alieni
                      if (alienBullets[j].active) {
                          drawAlienBulletSprite(alienBullets[j].x, alienBullets[j].y, BLACK); // Cancella dal display
                          alienBullets[j].active = false;
                      }
                  }
                  // Fine reset proiettili

                  alienBullets[i].active = false; // Disattiva il proiettile che ha colpito

                  if (playerLives <= 0) {
                      currentGameState = GAME_OVER;
                  }
              } else {
                  // Se il giocatore sta già esplodendo o respawnando, il proiettile alieno viene comunque disattivato
                  alienBullets[i].active = false;
              }
            }
          }
        }
      }
    }
  }

  // --- Aggiornamento Display: Navicella, Proiettile Giocatore, Proiettili Alieni, Esplosioni ---
  // Disegna la navicella solo se non sta esplodendo E non è in fase di respawn
  if (!playerExploding && !playerRespawning) {
      drawPlayerSprite(playerX, playerY, PLAYER_COLOR);
  }

  if (bulletActive) {
      drawBulletSprite(bulletX, bulletY, WHITE);
  }

  for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
    if (alienBullets[i].active) {
      drawAlienBulletSprite(alienBullets[i].x, alienBullets[i].y, RED); // Disegna i proiettili alieni
    }
  }

  updateMysteryShip(); // gestisci l'ufo
  drawExplosions(); // Disegna le esplosioni attive
  drawBarriers(); // Disegna le barriere (aggiornando quelle distrutte)

  delay(1);
}


// Chiama questa funzione SOLO dopo Wire.begin(...) e l'inizializzazione del PCF8574
void waitkey() {
  const uint8_t mask = (1 << BUTTON_LEFT_PCF_PIN) | (1 << BUTTON_RIGHT_PCF_PIN) | (1 << BUTTON_FIRE_PCF_PIN);
  uint8_t pcf_read_value = 0xFF;
  uint8_t inverted_pcf_value = 0x00;

  // 1) Attendi che *tutti* i tasti siano rilasciati (evita uscita immediata se un tasto è tenuto premuto)
  while (true) {
    Wire.requestFrom(PCF8574_ADDRESS, 1);
    if (Wire.available()) {
      pcf_read_value = Wire.read();
    } else {
      pcf_read_value = 0xFF; // Se non risponde, consideriamo tutti rilasciati
    }
    inverted_pcf_value = ~pcf_read_value;
    if ((inverted_pcf_value & mask) == 0) break; // tutti rilasciati
    delay(10);
  }

  // 2) Ora aspetta la pressione di *qualunque* tasto (con debounce)
  while (true) {
    Wire.requestFrom(PCF8574_ADDRESS, 1);
    if (Wire.available()) {
      pcf_read_value = Wire.read();
    } else {
      pcf_read_value = 0xFF;
    }
    inverted_pcf_value = ~pcf_read_value;

    if (inverted_pcf_value & mask) {
      // Debounce: piccolo ritardo e verifica ancora
      delay(20);
      Wire.requestFrom(PCF8574_ADDRESS, 1);
      if (Wire.available()) {
        pcf_read_value = Wire.read();
      } else {
        pcf_read_value = 0xFF;
      }
      inverted_pcf_value = ~pcf_read_value;
      if (inverted_pcf_value & mask) {
        // Traduci i bit del PCF nella stessa mappatura usata nel loop:
        uint8_t newButtonState = 0;
        if (inverted_pcf_value & (1 << BUTTON_LEFT_PCF_PIN))  newButtonState |= (1 << 0); // left -> bit0
        if (inverted_pcf_value & (1 << BUTTON_RIGHT_PCF_PIN)) newButtonState |= (1 << 1); // right -> bit1
        if (inverted_pcf_value & (1 << BUTTON_FIRE_PCF_PIN))  newButtonState |= (1 << 2); // fire -> bit2

        previousButtonState = 0;        // evita falsi edge
        currentButtonState = newButtonState;

        delay(30); // piccolo ritardo finale per evitare rimbalzi multipli all'uscita
        break;     // esci: tasto premuto e debounced
      }
    }
    delay(10);
  }
}

// (Ri)inizializza le barriere con la stessa forma usata in setup()
void initBarriers() {
  // (ri)calcola la Y delle barriere basandola sulla posizione corrente del player
  barrierYPos = playerY - SINGLE_BARRIER_HEIGHT - 20; // 20 pixel sopra il giocatore
  if (barrierYPos < INVADER_START_Y + INVADER_ROWS * (INVADER_HEIGHT + INVADER_SPACING_Y)) {
      // Assicurati che non si sovrappongano agli invasori
      barrierYPos = INVADER_START_Y + INVADER_ROWS * (INVADER_HEIGHT + INVADER_SPACING_Y) + 10; // 10 pixel sotto gli invasori
  }

  for (int i = 0; i < NUM_BARRIERS; i++) {
    for (int r = 0; r < BARRIER_BLOCK_ROWS; r++) {
      for (int c = 0; c < BARRIER_BLOCK_COLS; c++) {
        barriers[i][r][c] = true; // Inizialmente tutti i blocchi sono intatti

        // Stessa forma "a U rovesciata" e smussature usate in setup()
        if (r < 2 && (c < 2 || c >= BARRIER_BLOCK_COLS - 2)) {
            barriers[i][r][c] = false;
        }
        if (r == 0 && (c == BARRIER_BLOCK_COLS / 2 - 1 || c == BARRIER_BLOCK_COLS / 2)) {
            barriers[i][r][c] = false;
        }
        if (r >= BARRIER_BLOCK_ROWS - 2 && (c >= BARRIER_BLOCK_COLS / 2 - 2 && c < BARRIER_BLOCK_COLS / 2 + 2)) {
            barriers[i][r][c] = false;
        }
        if (r == BARRIER_BLOCK_ROWS - 3 && (c == BARRIER_BLOCK_COLS / 2 - 2 || c == BARRIER_BLOCK_COLS / 2 + 1)) {
             barriers[i][r][c] = false;
        }
      }
    }
  }
}

// Reset completo dell'ondata (invasori, proiettili, esplosioni, barriere). NON tocca score né vite.
void resetWave() {
  // Ferma suoni e sincronizza timer
  noTone(BUZZER_PIN);
  lastInvaderMoveTime = millis();
  lastInvaderFrameChangeTime = millis();
  lastInvaderSoundTime = millis();
  currentInvaderSoundIndex = 0;
  currentInvaderFrame = 0;

  // Pulisci l'area di gioco (lascia la status bar intatta)
  gfx->fillRect(0, GAME_AREA_START_Y, gfx->width(), gfx->height() - GAME_AREA_START_Y, BLACK);

  // Reimposta invasori vivi
  invadersAliveCount = totalInvaders;
  for (int row = 0; row < INVADER_ROWS; row++) {
    for (int col = 0; col < INVADER_COLS; col++) {
      invadersAlive[row][col] = true;
    }
  }

  // Reset offset/direzione/velocità invasori
  invadersOffsetX = 0;
  invadersOffsetY = 0;
  oldInvadersOffsetX = 0;
  oldInvadersOffsetY = 0;
  invaderDirection = 1;
  invaderMoveDelay = 650; // valore iniziale (come nel tuo setup originale)
  lastInvaderMoveTime = millis();

  // Resetta proiettili giocatore (cancella eventuali sprite residui)
  if (bulletActive) {
    drawBulletSprite(bulletX, bulletY, BLACK);
    bulletActive = false;
  }

  // Resetta proiettili alieni (cancella eventuali sprite residui)
  for (int i = 0; i < MAX_ALIEN_BULLETS; i++) {
    if (alienBullets[i].active) {
      drawAlienBulletSprite(alienBullets[i].x, alienBullets[i].y, BLACK);
      alienBullets[i].active = false;
    }
  }

  // Resetta esplosioni attive (cancella i pixel ancora visibili)
  for (int i = 0; i < MAX_EXPLOSIONS; i++) {
    if (explosions[i].active) {
      // cancella la zona dell'esplosione (usa la più grande possibile)
      gfx->fillRect(explosions[i].x, explosions[i].y, max(PLAYER_WIDTH, INVADER_WIDTH), max(PLAYER_HEIGHT, INVADER_HEIGHT), BLACK);
      explosions[i].active = false;
    }
  }

  // Rigenera le barriere
  initBarriers();

  // Ridisegna status bar (puncti e vite restano invariati)
  drawScore(playerScore);
  drawLives(playerLives);

  // Ridisegna invasori (updateInvadersDisplay gestisce la memorizzazione invaderCurrentPositions)
  // Passiamo oldOffset==newOffset per un disegno pulito iniziale
  updateInvadersDisplay(invadersOffsetX, invadersOffsetY, invadersOffsetX, invadersOffsetY, currentInvaderFrame);

  // Ridisegna barriere e player
  drawBarriers();
  if (!playerExploding && !playerRespawning) {
    drawPlayerSprite(playerX, playerY, PLAYER_COLOR);
  }
}




void updateMysteryShip() {
    static int moveCounter = 0;       // contatore locale statico
    const int MOVE_DIVISOR = 10;      // più grande = più lento

    // Controlla se è il momento di far apparire l'UFO
    if (!mysteryActive && millis() - lastMysteryTime > MYSTERY_INTERVAL) {
        mysteryActive = true;
        mysteryX = -MYSTERY_WIDTH;            // Partenza fuori schermo a sinistra
        mysteryY = GAME_AREA_START_Y + 5 - 7; // Spostato 7 pixel verso l'alto
        lastMysteryTime = millis();
        moveCounter = 0;                       // reset contatore
    }

    if (mysteryActive) {
        // Cancella vecchia posizione dell'UFO
        gfx->fillRect(mysteryX, mysteryY, MYSTERY_WIDTH, MYSTERY_HEIGHT, BLACK);

        // Aggiorna posizione solo ogni MOVE_DIVISOR chiamate
        moveCounter++;
        if (moveCounter >= MOVE_DIVISOR) {
            mysteryX += MYSTERY_SPEED;
            moveCounter = 0;
        }

        // Controllo collisione con il proiettile singolo
        if (bulletActive &&
            bulletX >= mysteryX && bulletX < mysteryX + MYSTERY_WIDTH &&
            bulletY >= mysteryY && bulletY < mysteryY + MYSTERY_HEIGHT) {
            
            // Colpito! Calcola punteggio casuale tra 100 e 500
            mysteryScore = (random(1, 6)) * 100;
            mysteryScoreTime = millis();

            // Disattiva UFO e proiettile
            mysteryActive = false;

            // Cancella proiettile dal display
            gfx->fillRect(bulletX, bulletY, BULLET_WIDTH, BULLET_HEIGHT, BLACK);
            bulletActive = false;

            // Somma punteggio al giocatore
            playerScore += mysteryScore;

            // Suono colpo
            tone(BUZZER_PIN, 1200, 100);
            
            // Aggiorna timer per riapparizione
            lastMysteryTime = millis();
        }

        // Disegna UFO solo se attivo
        if (mysteryActive) {
            gfx->drawBitmap(mysteryX, mysteryY, Sprite_Mystery_24x8, MYSTERY_WIDTH, MYSTERY_HEIGHT, WHITE, BLACK);
        }

        // Se esce dal display, disattivalo e aggiorna timer
        if (mysteryX > gfx->width()) {
            mysteryActive = false;
            lastMysteryTime = millis();
        }
    }

    // Mostra punteggio casuale per 2 secondi
    if (mysteryScore > 0) {
        if (millis() - mysteryScoreTime < 2000) {
            char buf[8];
            sprintf(buf, "<%d>", mysteryScore);
            int scoreX = mysteryX + (MYSTERY_WIDTH / 2) - 10; // centrato sull'UFO
            int scoreY = mysteryY + 2;                        // 10 pixel più in basso
            gfx->setCursor(scoreX, scoreY);
            gfx->setTextColor(WHITE, BLACK);
            gfx->print(buf);
        } else {
            // cancella punteggio dopo 2 secondi
            gfx->fillRect(mysteryX, mysteryY + 2, 32, 10, BLACK); 
            mysteryScore = 0; 
        }
    }

    // Aggiorna suono UFO solo se attivo
    if (mysteryActive) {
        // Modifica frequenza per effetto "UFO"
        if (ufoToneAscending) {
            ufoToneFreq += 20;           // aumenta frequenza
            if (ufoToneFreq > 800) ufoToneAscending = false;
        } else {
            ufoToneFreq -= 20;           // diminuisce frequenza
            if (ufoToneFreq < 200) ufoToneAscending = true;
        }

        // Emetti il suono
        tone(BUZZER_PIN, ufoToneFreq);
//    } else {
//        noTone(BUZZER_PIN);             // spegne il buzzer se l'UFO non è attivo
    }

}



// Funzione vuota per test, può essere rimossa se non usata
void testDisplay() { }