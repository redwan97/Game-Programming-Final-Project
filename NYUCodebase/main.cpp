#ifdef _WINDOWS
	#include <GL/glew.h>
	#include <SDL_mixer.h>
    #include <Windows.h>
	#define RESOURCE_FOLDER ""
	#define RESOURCE_FOLDER2 ""
#else
    #define RESOURCE_FOLDER2 "/Users/Laila/Desktop/FinalGameProject/Game-Programming-Final-Project/NYUCodebase/"
	#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
	#include </Library/Frameworks/SDL2_mixer.framework/Headers/SDL_mixer.h>
#endif

#define SPRITE_COUNT_X 24
#define SPRITE_COUNT_Y 16
#define TILE_SIZE 0.2f
#define LEVEL_HEIGHT 32
#define LEVEL_WIDTH 128
#define FIXED_TIMESTEP 0.01666666f
#define MAX_TIMESTEPS 6
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "ShaderProgram.h"
#include "Matrix.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>


using namespace std;

//Global Variables
enum GameState { TITLE_SCREEN, GAME_STATE, GAME_OVER };
enum LevelState { LEVEL_1, LEVEL_2, LEVEL_3};
enum EntityType { PLAYER, ENEMY, GOAL, SPIKE };
vector<float> vertexData;
vector<float> texCoordData;
GameState gState = TITLE_SCREEN;
LevelState lState = LEVEL_1;
SDL_Window* displayWindow;
ShaderProgram* program;
Mix_Chunk* jumpSound;
Mix_Music* gameMusic;
GLuint sheet;
GLuint font;
Matrix modelMatrix;
Matrix viewMatrix;
Matrix projectionMatrix;
int mapHeight;
int mapWidth;
short** levelData;

//Function to load texture
GLuint LoadTexture(const char* filePath) {
	int w, h, comp;
	unsigned char* image = stbi_load(filePath, &w, &h, &comp, STBI_rgb_alpha);
	if (image == NULL) {
		std::cout << "Unable to load image. Make sure the path is correct\n";
		assert(false);
	}
	GLuint retTexture;
	glGenTextures(1, &retTexture);
	glBindTexture(GL_TEXTURE_2D, retTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	stbi_image_free(image);
	return retTexture;
}

//Function to draw text
void DrawText(ShaderProgram *program, int fontTexture, std::string text, float size, float spacing) {
	float texture_size = 1.0 / 16.0f;
	std::vector<float> vertexData;
	std::vector<float> texCoordData;
	for (size_t i = 0; i < text.size(); i++) {
		float texture_x = (float)(((int)text[i]) % 16) / 16.0f;
		float texture_y = (float)(((int)text[i]) / 16) / 16.0f;
		vertexData.insert(vertexData.end(), {
			((size + spacing) * i) + (-0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
		});
		texCoordData.insert(texCoordData.end(), {
			texture_x, texture_y,
			texture_x, texture_y + texture_size,
			texture_x + texture_size, texture_y,
			texture_x + texture_size, texture_y + texture_size,
			texture_x + texture_size, texture_y,
			texture_x, texture_y + texture_size,
		});
	}
	glUseProgram(program->programID);
	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
	glEnableVertexAttribArray(program->positionAttribute);
	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
	glEnableVertexAttribArray(program->texCoordAttribute);
	glBindTexture(GL_TEXTURE_2D, fontTexture);
	glDrawArrays(GL_TRIANGLES, 0, text.size() * 6);
	glDisableVertexAttribArray(program->positionAttribute);
	glDisableVertexAttribArray(program->texCoordAttribute);
}

//Draw Text driver
void write(std::string text, float x, float y, float z = 0.0f, float size = 0.25f, float spacing = 0.0f) {
	modelMatrix.identity();
	modelMatrix.Translate(x, y, z);
	program->setModelMatrix(modelMatrix);
	DrawText(program, font, text, size, spacing);
}

//Linear Interpolation
float lerp(float v0, float v1, float t) {
	return (1.0f - t)*v0 + t*v1;
}

//Mapping Value Ranges
float mapValue(float value, float srcMin, float srcMax, float dstMin, float dstMax) {
    float retVal = dstMin + ((value - srcMin)/(srcMax-srcMin) * (dstMax-dstMin));
    if(retVal < dstMin) {
        retVal = dstMin;
    }
    if(retVal > dstMax) {
        retVal = dstMax;
    }
    return retVal;
}

//SpriteSheet class, Not Needed
class SheetSprite {
public:
	SheetSprite(unsigned int xtextureID, float tu, float tv, float twidth, float theight, float tsize) {
		u = tu;
		v = tv;
		width = twidth;
		height = theight;
		size = tsize;
		textureID = xtextureID;
	}
	SheetSprite() {}
	//void Draw(ShaderProgram *program);
	float size;
	unsigned int textureID;
	float u;
	float v;
	float width;
	float height;
};

//Entity Class
class Entity {
public:
	Entity() {}
	Entity(float posX, float posY, float entityIndex, string type);
	void Update(float elapsed);
	void Render(ShaderProgram *program);
	void collisionX();
	void collisionY();
	bool collidesWith(Entity *entity);	//Driver for Private func collision

	SheetSprite sprite;
	Matrix ModelMatrix;
	EntityType entityType;

	float x;
	float y;
	float width = TILE_SIZE;
	float height = TILE_SIZE;
	float velocity_x = 0.0f;
	float velocity_y = 0.0f;
	float acceleration_x = 0.0f;
	float acceleration_y = 0.0f;
	float friction_x = 0.0f;
	float friction_y = 0.0f;
	float pen_x = 0.0f;
	float pen_y = 0.0f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;

	float index;
	bool alive = true;
	bool won = false;

	bool isStatic = false;
	bool collidedTop = false;
	bool collidedBottom = false;
	bool collidedLeft = false;
	bool collidedRight = false;

private:
	bool collision(Entity* otherEntity);
	bool isSolidTile(int index);
	void worldToTileCoordinates(float worldX, float worldY, int *gridX, int *gridY);
};

//Instances of Entities 
Entity player;
Entity goal;
vector<Entity*> enemies;
vector<Entity*> spikes;

//Function to place entities in respected positions
void placeEntity(const std::string& type, float x, float y) {
	if (type == "Player" || type == "player") { player = Entity(x, y, 90, type); }
	else if (type == "Goal" || type == "goal") { goal = Entity(x, y, 233, type); }
	else if (type == "Enemy" || type == "enemy") { enemies.push_back(new Entity(x, y, 136, type)); }
	else if (type == "Spike" || type == "spike") { spikes.push_back(new Entity(x, y, 148, type)); }
}

//Function for scrolling. Mostly centers the viewMatrix on player
void centerPlayer(ShaderProgram* program) {
	viewMatrix.identity();
	viewMatrix.Scale(2.0f, 2.0f, 1.0f);
	if (player.y > -5.5f) { viewMatrix.Translate(-player.x, -player.y, 0.0f); }
	else { viewMatrix.Translate(-player.x, 5.5f, 0.0f); }
	program->setViewMatrix(viewMatrix);
}

//Function to draw map
void drawMap(vector<float>& vertexData, vector<float>& texCoordData) {
	for (int y = 0; y < LEVEL_HEIGHT; y++) {
		for (int x = 0; x < LEVEL_WIDTH; x++) {
			if (levelData[y][x] != 0) {
				float u = (float)(((int)levelData[y][x]) % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X;
				float v = (float)(((int)levelData[y][x]) / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y;
				float spriteWidth = 1.0f / (float)SPRITE_COUNT_X;
				float spriteHeight = 1.0f / (float)SPRITE_COUNT_Y;

				vertexData.insert(vertexData.end(), {
					TILE_SIZE * x, -TILE_SIZE * y,
					TILE_SIZE * x, (-TILE_SIZE * y) - TILE_SIZE,
					(TILE_SIZE * x) + TILE_SIZE, (-TILE_SIZE * y) - TILE_SIZE,
					TILE_SIZE * x, -TILE_SIZE * y,
					(TILE_SIZE * x) + TILE_SIZE, (-TILE_SIZE * y) - TILE_SIZE,
					(TILE_SIZE * x) + TILE_SIZE, -TILE_SIZE * y
				});

				texCoordData.insert(texCoordData.end(), {
					u, v,
					u, v + (spriteHeight),
					u + spriteWidth, v + (spriteHeight),
					u, v,
					u + spriteWidth, v + (spriteHeight),
					u + spriteWidth, v
				});
			}
		}
	}
}


//Function to render map
void renderMap(ShaderProgram* program, vector<float>& vertexData, vector<float>& texCoordData, float elapsed) {
    glUseProgram(program->programID);
	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
	glEnableVertexAttribArray(program->positionAttribute);
	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
	glEnableVertexAttribArray(program->texCoordAttribute);
	modelMatrix.identity();
	program->setModelMatrix(modelMatrix);
	glBindTexture(GL_TEXTURE_2D, sheet);
	glDrawArrays(GL_TRIANGLES, 0, vertexData.size() / 2);
	glDisableVertexAttribArray(program->positionAttribute);
	glDisableVertexAttribArray(program->texCoordAttribute);
}

//Function to read HeaderData from map file
bool readHeader(std::ifstream &stream) {
	string line;
	mapWidth = -1;
	mapHeight = -1;
	while (getline(stream, line)) {
		if (line == "") { break; }
		istringstream sStream(line);
		string key, value;
		getline(sStream, key, '=');
		getline(sStream, value);
		if (key == "width") { mapWidth = atoi(value.c_str()); }
		else if (key == "height") { mapHeight = atoi(value.c_str()); }
	}
	if (mapWidth == -1 || mapHeight == -1) { return false; }
	else { // allocate our map data
		levelData = new short*[mapHeight];
		for (int i = 0; i < mapHeight; ++i) { levelData[i] = new short[mapWidth]; }
		return true;
	}
}

//Function to read LayerData from map file
bool readLayerData(std::ifstream &stream) {
	string line;
	while (getline(stream, line)) {
		if (line == "") { break; }
		istringstream sStream(line);
		string key, value;
		getline(sStream, key, '=');
		getline(sStream, value);
		if (key == "data") {
			for (int y = 0; y < mapHeight; y++) {
				getline(stream, line);
				istringstream lineStream(line);
				string tile;
				for (int x = 0; x < mapWidth; x++) {
					getline(lineStream, tile, ',');
					int val = (int)atoi(tile.c_str());
					if (val > 0) {
						// be careful, the tiles in this format are indexed from 1 not 0
						levelData[y][x] = val - 1;
						if (y == 14 && x == 10) {
							ostringstream ss;
							ss << val << " " << y << " " << x << " " << tile.c_str() << " " << (int)levelData[y][x] << "\n";
							//OutputDebugString(ss.str().c_str());
						}
					} 
					else {
						levelData[y][x] = 0;
					}
				}
			}
		}
	}
	return true;
}

//Function to read EntityData from map file
bool readEntityData(std::ifstream &stream) {
	string line;
	string type;
	while (getline(stream, line)) {
		if (line == "") { break; }
		istringstream sStream(line);
		string key, value;
		getline(sStream, key, '=');
		getline(sStream, value);
		if (key == "type") { 
			type = value; 
		} 
		else if (key == "location") {
			istringstream lineStream(value);
			string xPosition, yPosition;
			getline(lineStream, xPosition, ',');
			getline(lineStream, yPosition, ',');
			float placeX = atoi(xPosition.c_str())*TILE_SIZE;
			float placeY = atoi(yPosition.c_str())*-TILE_SIZE;
			placeEntity(type, placeX, placeY);
		}
	}
	return true;
}

//Function to intialize level: inputFile determines level
void levelInit(std::string inputFile, vector<float>& vertexData, vector<float>& texCoordData) {
	enemies.clear();
    spikes.clear();
	vertexData.clear();
	texCoordData.clear();
	ifstream infile(inputFile);
	string line;
	while (getline(infile, line)) {
		if (line == "[header]") { if (!readHeader(infile)) { break; } }
		else if (line == "[layer]") { readLayerData(infile); }
		else if (line == "[Object Layer 1]") { readEntityData(infile); }
	}
	drawMap(vertexData, texCoordData);
	gState = GAME_STATE;
}

//Function to update entities based on elapsed time
void update(float elapsed) {
	player.Update(elapsed);
	goal.Update(elapsed);
	player.collidesWith(&goal);
	for (size_t i = 0; i < enemies.size(); i++) {
		enemies[i]->Update(elapsed);
		player.collidesWith(enemies[i]);
	}
	for (size_t i = 0; i < spikes.size(); i++) {
		spikes[i]->Update(elapsed);
		player.collidesWith(spikes[i]);
	}
}

//Function to render entities
void render(ShaderProgram* program) {
	player.Render(program);
	centerPlayer(program);
	goal.Render(program);
	for (size_t i = 0; i < enemies.size(); i++) { enemies[i]->Render(program); }
	for (size_t i = 0; i < spikes.size(); i++) { spikes[i]->Render(program); }
}

int main(int argc, char *argv[])
{	
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("First Platformer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
    Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 4096 );
#ifdef _WINDOWS
	glewInit();
#endif
	glViewport(0, 0, 640, 360);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	program = new ShaderProgram(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
    sheet = LoadTexture(RESOURCE_FOLDER2"dirt-tiles.png");
    font = LoadTexture(RESOURCE_FOLDER2"font.png");
	jumpSound = Mix_LoadWAV("jump2.wav");
    gameMusic = Mix_LoadMUS("gameMusic4.mp3");
	projectionMatrix.setOrthoProjection(-3.55f, 3.55f, -2.0f, 2.0f, -1.0f, 1.0f);
	glUseProgram(program->programID);
	SDL_Event event;
	float lastTick = 0.0f;
	bool done = false;
	while (!done) {
		float ticks = (float)SDL_GetTicks() / 1000.0f; //Fixed Game Ticks
		float elapsed = ticks - lastTick;
		lastTick = ticks;
		float fixedElapsed = elapsed;
        
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) { done = true; }
			else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.scancode == SDL_SCANCODE_P && gState == GAME_OVER) { gState = TITLE_SCREEN;}
				if (event.key.keysym.scancode == SDL_SCANCODE_Q && gState == GAME_OVER) { SDL_Quit();; }
				else if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                    if (gState == TITLE_SCREEN) {
                        Mix_PlayMusic(gameMusic, -1);
                        lState = LEVEL_1;
                        levelInit(RESOURCE_FOLDER2"map2.txt", vertexData, texCoordData); } //Level 1 Initilization
                    else if (gState == GAME_STATE){
                        if(lState == LEVEL_1){
                            levelInit(RESOURCE_FOLDER2"map1.txt", vertexData, texCoordData);
                            lState = LEVEL_2;
                        }
                        else if (lState == LEVEL_2){
                            levelInit(RESOURCE_FOLDER2"map3.txt", vertexData, texCoordData);
                            lState = LEVEL_3;
                        }else{
                            gState = TITLE_SCREEN;
                            lState = LEVEL_1;
                        }
                    }
				}
			}
			else if (event.type == SDL_KEYUP) {
				//Allows friction to stop the player by settin acceleration=0 when keys are let go
				if (
					event.key.keysym.scancode == SDL_SCANCODE_RIGHT ||
					event.key.keysym.scancode == SDL_SCANCODE_D ||
					event.key.keysym.scancode == SDL_SCANCODE_LEFT ||
					event.key.keysym.scancode == SDL_SCANCODE_A 
				) { player.acceleration_x = 0.0f; }
			}
		}

		glClearColor(0.80f, 0.80f, 0.80f, 0.5f);
		glClear(GL_COLOR_BUFFER_BIT);
		program->setProjectionMatrix(projectionMatrix);
		program->setViewMatrix(viewMatrix);
		glEnable(GL_BLEND);

		switch (gState) 
		{
		case TITLE_SCREEN:
			write("Platformers!", -1.75f, 1.5f, 0.0f);
			write("Instructions: ", -3.3f, 0.9f, 0.0f, 0.23f);
			write("Use Arrow keys OR", -2.70f, 0.65f, 0.0f, 0.23f);
			write("WASD to move", -2.70f, 0.40f, 0.0f, 0.23f);
			write("Find the goal ", -2.70f, 0.15f, 0.0f, 0.23f);
			write("Press Space to Play!", -2.70f, -0.35f, 0.0f);
			viewMatrix.identity();
			program->setViewMatrix(viewMatrix);
			break;

		case GAME_STATE:
			//Map rendering, Fixed Elapsed time updating, Entity Updating and rendering
			renderMap(program, vertexData, texCoordData, fixedElapsed);
			if (fixedElapsed > FIXED_TIMESTEP * MAX_TIMESTEPS) { fixedElapsed = FIXED_TIMESTEP * MAX_TIMESTEPS; }
			while (fixedElapsed >= FIXED_TIMESTEP) { fixedElapsed -= FIXED_TIMESTEP; update(FIXED_TIMESTEP); }
			update(fixedElapsed);
			render(program);
			break;

		case GAME_OVER:
			string output = "GG GAME DIDNT WORK";
			if (player.won) { output = "YOU WON!"; }
			else { output = "YOU LOST!"; }
			write(output, -1.75f, 1.5f, 0.0f, 0.5f);
			write("Press P to play again!", -3.29f, 0.4f, 0.0f, 0.28f, 0.0f);
			if (player.won == false) { write("Press Q to quit, QUITTER!", -3.29f, 0.0f, 0.0f, 0.28f); }
			viewMatrix.identity();
			program->setViewMatrix(viewMatrix);
			break;
		}
		SDL_GL_SwapWindow(displayWindow);
	}
	//Cleanup
	Mix_FreeChunk(jumpSound);
    Mix_FreeMusic(gameMusic);
	SDL_Quit();
	return 0;
}

Entity::Entity(float posX, float posY, float entityIndex, string type) {
	x = posX;
	y = posY;
	index = entityIndex;

	if (type == "player" || type == "Player") {
		entityType = PLAYER;
		acceleration_x = 0.0f;
		friction_x = 0.20f;
	} 
	else if (type == "enemy" || type == "Enemy") {
		entityType = ENEMY;
		acceleration_x = 1.0f; 
		friction_x = 0.50f;
	} 
	else if (type == "goal" || type == "Goal") {
		entityType = GOAL;
		friction_x = 0.0f;
		friction_y = 0.0f;
	}
	else if (type == "spike" || type == "Spike") {
		entityType = SPIKE;
		friction_x = 0.0f;
		friction_y = 0.0f;
	}
}

void Entity::Update(float elapsed) {
	float maxMovingSpeed = 12.0f;
	velocity_x = lerp(velocity_x, 0.0f, elapsed*friction_x);
	velocity_y = lerp(velocity_y, 0.0f, elapsed*friction_y);
    scale_x = mapValue(fabs(velocity_y), 10.0, 0.0, 0.8, 1.0);
    scale_y = mapValue(fabs(velocity_y), 0.0, 10.0, 1.0, 1.6);
	if (entityType == PLAYER) {
		const Uint8 *keys = SDL_GetKeyboardState(NULL);
		if ((keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) && collidedBottom == true) { 
			velocity_y = 3.35f; //3.5 for testing, 3.2 original value 
			Mix_PlayChannel(-1, jumpSound, 0);
		}
		if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) { acceleration_x = 0.80f; }
		else if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) { acceleration_x = -0.80f; }
	}
    if(entityType == ENEMY){
        scale_x = mapValue(fabs(velocity_x), 0.0, 3.0, 1.0, 1.6);
    }
	if (velocity_x < maxMovingSpeed) { velocity_x += acceleration_x * elapsed; }
	velocity_y += acceleration_y * elapsed;

	y += velocity_y * elapsed;
	collisionY();
	x += velocity_x * elapsed;
	collisionX();
}

void Entity::Render(ShaderProgram * program) {
	if (alive) {
		float u = (float)(((int)(index)) % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X;
		float v = (float)(((int)(index)) / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y;
		float spriteWidth = 1.0f / (float)SPRITE_COUNT_X;
		float spriteHeight = 1.0f / (float)SPRITE_COUNT_Y;

		GLfloat textCoords[] = {
			u, v + spriteHeight,
			u + spriteWidth, v,
			u, v,
			u + spriteWidth, v,
			u, v + spriteHeight,
			u + spriteWidth, v + spriteHeight
		};
		float vertices[] = {
			-0.5f*TILE_SIZE, -0.5f*TILE_SIZE,
			0.5f*TILE_SIZE, 0.5f*TILE_SIZE,
			-0.5f*TILE_SIZE, 0.5f*TILE_SIZE,
			0.5f*TILE_SIZE, 0.5f*TILE_SIZE,
			-0.5f*TILE_SIZE, -0.5f*TILE_SIZE,
			0.5f*TILE_SIZE, -0.5f*TILE_SIZE
		};

		ModelMatrix.identity();
		ModelMatrix.Translate(x, y, 0);
        ModelMatrix.Scale(scale_x, scale_y, 1.0f);
		program->setModelMatrix(ModelMatrix);

		glUseProgram(program->programID);
		glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program->positionAttribute);
		glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, textCoords);
		glEnableVertexAttribArray(program->texCoordAttribute);
		glBindTexture(GL_TEXTURE_2D, sheet);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glDisableVertexAttribArray(program->positionAttribute);
		glDisableVertexAttribArray(program->texCoordAttribute);
	}
}

void Entity::collisionX() {
	int gridX = 0;
	int gridY = 0;

	//Collision with tiles to the right of entity
	worldToTileCoordinates(x + width / 2, y, &gridX, &gridY);
	if (isSolidTile(levelData[gridY][gridX])) {
		velocity_x = 0;
		if (entityType == ENEMY) { acceleration_x *= -1.0f; }
		pen_x = (TILE_SIZE*gridX) - (x + width / 2);
		x += (pen_x - 0.005f);
		collidedRight = true;
	}
	else {
		pen_x = 0;
		collidedRight = false;
	}
	//Collision with tiles to the left of entity
	worldToTileCoordinates(x - width / 2.0f, y, &gridX, &gridY);
	if (isSolidTile(levelData[gridY][gridX])) {
		velocity_x = 0.0f;
		if (entityType == ENEMY) { acceleration_x *= -1.0f; }
		pen_x = (x - width / 2.0f) - (TILE_SIZE*gridX + TILE_SIZE);
		x -= (pen_x - 0.005f);
		collidedLeft = true;
	}
	else {
		pen_x = 0;
		collidedLeft = false;
	}
}

void Entity::collisionY() {
	int tileX = 0;
	int tileY = 0;

	//Collision with tiles to the bottom of entity
	worldToTileCoordinates(x, y - height / 2, &tileX, &tileY);
	if (isSolidTile(levelData[tileY][tileX])) {
		velocity_y = 0;
		acceleration_y = 0;
		pen_y = (-TILE_SIZE*tileY) - (y - height / 2);
		y += pen_y + 0.002f;
		collidedBottom = true;
	}
	else {
		acceleration_y = -9.8f;
		pen_y = 0;
		collidedBottom = false;
	}
	//Collision with tiles to the top of entity
	worldToTileCoordinates(x, y + height / 2, &tileX, &tileY);
	if (isSolidTile(levelData[tileY][tileX])) {
		velocity_y = 0;
		pen_y = fabs((y + height / 2) - ((-TILE_SIZE*tileY) - TILE_SIZE));
		y -= (pen_y + 0.002f);
		collidedTop = true;
	}
	else {
		pen_y = 0;
		collidedTop = false;
	}
}

bool Entity::collidesWith(Entity * entity) {
	bool isColliding = collision(entity);
	if (entity->entityType == GOAL) {
		won = isColliding ? true : false;	
		if (lState == LEVEL_1) {
			if (won) {
				lState = LEVEL_2;
				won = false; 
                levelInit(RESOURCE_FOLDER2"map1.txt", vertexData, texCoordData); //Level 2 Initilization
 			}
		}
		if (lState == LEVEL_2) {
			if (won) {
				lState = LEVEL_3;
				won = false;
                levelInit(RESOURCE_FOLDER2"map3.txt", vertexData, texCoordData); //Level 3 Initilization
			}
		}
		if (lState == LEVEL_3) {
			gState = won ? GAME_OVER : gState;
		}
	}
	if (entity->entityType == ENEMY || entity->entityType == SPIKE) {
		alive = isColliding ? false : true;
        gState = alive? gState : GAME_OVER;
	}
	return isColliding;
}

bool Entity::collision(Entity* otherEntity) {
	return 
	   (y - (height / 2) > otherEntity->y + (otherEntity->height) / 2 ||
		y + (height / 2) < otherEntity->y - (otherEntity->height) / 2 ||
		x - (width / 2) > otherEntity->x + (otherEntity->width) / 2 ||
		x + (width / 2) < otherEntity->x - (otherEntity->width) / 2)
		? false : true;
}

bool Entity::isSolidTile(int index) {
	if (lState == LEVEL_1 || lState == LEVEL_3) {
		return (index == 0 || index == 8 || index == 78 || index == 187 || index == 209
                || index == 233 || index == 232 || index == 208) ? false : true;
	}
	return (index == 32 || index == 51 || index == 274 || index == 275 || index == 299 ||
		    index == 321 || index == 347 || index == 345 || index == 301 || index == 171)
		? true : false;
}

void Entity::worldToTileCoordinates(float worldX, float worldY, int * gridX, int * gridY) {
	*gridX = (int)(worldX / TILE_SIZE);
	*gridY = (int)(-worldY / TILE_SIZE);
}
