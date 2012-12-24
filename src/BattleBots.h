#ifndef TEMPLATEGAME_H_
#define TEMPLATEGAME_H_

#include "gameplay.h"

using namespace gameplay;

/**
 * Main game class.
 */
class BattleBots: public Game
{
public:

	/**
	 * Constructor.
	 */
	BattleBots();

	/**
	 * @see Game::keyEvent
	 */
	void keyEvent(Keyboard::KeyEvent evt, int key);
	
	/**
	 * @see Game::touchEvent
	 */
	void touchEvent(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex);

	void gestureSwipeEvent(int x, int y, int direction);
    
    void gesturePinchEvent(int x, int y, float scale);
    
    void gestureTapEvent(int x, int y);

protected:

	/**
	 * @see Game::initialize
	 */
	void initialize();

	/**
	 * @see Game::finalize
	 */
	void finalize();

	/**
	 * @see Game::update
	 */
	void update(float elapsedTime);

	/**
	 * @see Game::render
	 */
	void render(float elapsedTime);

private:

	/**
	 * Draws the scene each frame.
	 */
	bool drawScene(Node* node);
	bool initializeScene(Node* node);
	void initializeMaterial(Scene* scene, Node* node, Material* material);
	void initializeCharacter();

	void adjustCamera(float elapsedTime);
	bool isOnFloor() const;

	Font* _font;

	Scene* _scene;

	PhysicsCharacter* _character;
	Node* _characterNode;
	Node* _characterMeshNode;
	
	Node* _cameraPivot;

	float _floorLevel;

	int _rotateX;
	int _rotateY;
	float _yaw, _pitch;

	unsigned int _keyFlags;

	Vector2 _currentDirection;

	char fps[32];
	char cameraPos[32];
};

#endif
