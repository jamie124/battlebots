#include "BattleBots.h"

// Declare our game instance
BattleBots game;

#define NORTH 1
#define SOUTH 2
#define EAST 4
#define WEST 8
#define RUNNING 16

#define WALK_SPEED 5.0f
#define STRAFE_SPEED 1.5f
#define RUN_SPEED 15.0f
#define CAMERA_FOCUS_DISTANCE 16.0f

struct TerrainHitFilter : public PhysicsController::HitFilter
{
	TerrainHitFilter(Terrain* terrain)
	{
		terrainObject = terrain->getNode()->getCollisionObject();
	}

	bool filter(PhysicsCollisionObject* object)
	{
		// Filter out all objects but the terrain
		return (object != terrainObject);
	}

	PhysicsCollisionObject* terrainObject;
};

BattleBots::BattleBots()
	: _font(NULL), _scene(NULL), _terrain(NULL), _sky(NULL), _characterPhysics(NULL), _characterNode(NULL), _characterScaleNode(NULL), _characterMeshNode(NULL), _cameraPivot(NULL), _rotateX(0), _rotateY(0), 
	_yaw(0), _pitch(0), _keyFlags(0), snapToGround(true), vsync(true), mode(MODE_LOOK)
{
}

void BattleBots::initialize()
{
	setMultiTouch(true);

	// Load the font.
	_font = Font::create("res/arial40.gpb");

	_scene = Scene::load("res/battlebots.scene");

	_terrain = _scene->findNode("terrain")->getTerrain();

	_sky = _scene->findNode("sky");

	// Set the aspect ratio for the scene's camera to match the current resolution
	//_scene->getActiveCamera()->setAspectRatio((float)getWidth() / (float)getHeight());

	//_cameraPivot = _scene->findNode("cameraPivot");


	initializeCharacter();

	_scene->visit(this, &BattleBots::initializeScene);

	// Use script camera for navigation
	//Game::getInstance()->getScriptController()->executeFunction<void>("camera_setActive", "b", true);
	//Game::getInstance()->getScriptController()->executeFunction<void>("camera_setSpeed", "ff", 100, 1000);
}

void BattleBots::initializeCharacter() {
	//Node* node = _scene->findNode("character");
	_characterNode = _scene->findNode("character");
	_characterPhysics = static_cast<PhysicsCharacter*>(_characterNode->getCollisionObject());

	_characterScaleNode = _characterNode->findNode("characterScale");
	_characterMeshNode = _characterNode->findNode("characterMesh");
}

bool BattleBots::initializeScene(Node* node)
{
	// Get model from node
	Model* model = node->getModel();

	if (model && model->getMaterial()) {
		// If model has material pass it to init method
		initializeMaterial(_scene, node, model->getMaterial());
	}

	return true;
}



void BattleBots::initializeMaterial(Scene* scene, Node* node, Material* material)
{
	if (node->hasTag("dynamic")) {
		Node* light = scene->findNode("directionalLight1");

		material->getParameter("u_ambientColor")->bindValue(scene, &Scene::getAmbientColor);
		material->getParameter("u_lightColor")->bindValue(light->getLight(), &Light::getColor);
		material->getParameter("u_lightDirection")->bindValue(light, &Node::getForwardVectorView);
	} 
}

void BattleBots::finalize()
{
	SAFE_RELEASE(_font);
	SAFE_RELEASE(_scene);
}

void BattleBots::update(float elapsedTime)
{
	//_cameraPivot->rotateY(MATH_DEG_TO_RAD((float)elapsedTime / 10000.0f * 180.0f));
	//_cameraPivot->rotateX(MATH_DEG_TO_RAD((float)elapsedTime / 10000.0f * _rotateX));

	Node* camera = _scene->getActiveCamera()->getNode();

	// Construct direction vector from keyboard input

	// Keep the sky centered around the viewer
	_sky->setTranslationX(-_characterNode->getTranslationX());
    _sky->setTranslationZ(-_characterNode->getTranslationZ());

	 _currentDirection.set(Vector2::zero());

	if (_currentDirection.isZero()) {
		if (_keyFlags & NORTH) {
			_currentDirection.y = 1;
		} else if (_keyFlags & SOUTH) {
			_currentDirection.y = -1;
		} else {
			_currentDirection.y = 0;
		}

		if (_keyFlags & EAST) {
			_currentDirection.x = 1;
		} else if (_keyFlags & WEST) {
			_currentDirection.x = -1;
		} else {
			_currentDirection.x = 0;
		}

		_currentDirection.normalize();

		if ((_keyFlags & RUNNING) == 0) {
			_currentDirection *= 0.5f;
		}
	}

	// Update character animation and velocity
	if (_currentDirection.isZero()) {
		_characterPhysics->setVelocity(Vector3::zero());
	} else {
		bool running = (_currentDirection.lengthSquared() > 0.75f);
		float speed = running ? RUN_SPEED : WALK_SPEED;

		// Orient the character relative to the camera so he faces the direction we want to move.
		const Matrix& cameraMatrix = _scene->getActiveCamera()->getNode()->getWorldMatrix();
		Vector3 cameraRight, cameraForward;
		cameraMatrix.getRightVector(&cameraRight);
		cameraMatrix.getForwardVector(&cameraForward);

		// Get the current forward vector for the mesh node (negate it since the character was modelled facing +z)
		Vector3 currentHeading(-_characterScaleNode->getForwardVectorWorld());

		// Construct a new forward vector for the mesh node
		Vector3 newHeading(cameraForward * _currentDirection.y + cameraRight * _currentDirection.x);

		// Compute the rotation amount based on the difference between the current and new vectors
		float angle = atan2f(newHeading.x, newHeading.z) - atan2f(currentHeading.x, currentHeading.z);
		if (angle > MATH_PI)
			angle -= MATH_PIX2;
		else if (angle < -MATH_PI)
			angle += MATH_PIX2;
		angle *= (float)elapsedTime * 0.001f * MATH_PIX2;
		_characterScaleNode->rotate(Vector3::unitY(), angle);

		// Update the character's velocity
		Vector3 velocity = -_characterScaleNode->getForwardVectorWorld();
		velocity.normalize();
		velocity *= speed;

		_characterPhysics->setVelocity(velocity);
	}


	//_cameraPivot->setTranslation(_characterScaleNode->getTranslationWorld());

	// Adjust camera to avoid it from being obstructed by walls and objects in the scene.
	adjustCamera(elapsedTime);


	if (snapToGround)
	{
		// Get current camera location in world coords
		Vector3 pos = _characterScaleNode->getTranslationWorld();

		// Query the height of our terrain at this location
		float height = _terrain->getHeight(pos.x, pos.z);

		// Snap our camera to the ground
		Node* node = _scene->findNode("character");
		node->setTranslationY(height + 10);
	}

	// Keep the sky centered around the viewer
	//  _sky->setTranslationX(camera->getTranslationX());
	//  _sky->setTranslationZ(camera->getTranslationZ());

	// Prune dropped physics shapes that fall off the terrain
	for (std::list<Node*>::iterator itr = shapes.begin(); itr != shapes.end(); ) {
		Node* shape = *itr;

		if (shape->getTranslation().y < 0) {
			_scene->removeNode(shape);
			std::list<Node*>::iterator oldItr = itr;
			++itr;
			shapes.erase(oldItr);
		} else {
			++itr;
		}
	}
}

void BattleBots::adjustCamera(float elapsedTime)
{
	static float cameraOffset = 0.0f;

	PhysicsController* physics = getPhysicsController();
	Node* cameraNode = _scene->getActiveCamera()->getNode();

	// Reset camera
	if (cameraOffset != 0.0f)
	{
		cameraNode->translateForward(-cameraOffset);
		cameraOffset = 0.0f;
	}

	Vector3 cameraPosition = cameraNode->getTranslationWorld();
	Vector3 cameraDirection = cameraNode->getForwardVectorWorld();
	cameraDirection.normalize();

	// Get focal point of camera (use the resolved world location of the head joint as a focal point)
	Vector3 focalPoint(cameraPosition + (cameraDirection * CAMERA_FOCUS_DISTANCE));

	Vector3 oldPosition = cameraNode->getTranslationWorld();

	PhysicsController::HitResult result;
	PhysicsCollisionObject* occlusion = NULL;

	do {
		// Perform a ray test to check for camera collisions
		if (!physics->sweepTest(cameraNode->getCollisionObject(), focalPoint, &result) || result.object == _characterPhysics)
			break;

		occlusion = result.object;

		// Step the camera closer to the focal point to resolve the occlusion
		float d = cameraNode->getTranslationWorld().distance(result.point);
		cameraNode->translateForward(d);
		cameraOffset += d;
		while (physics->sweepTest(cameraNode->getCollisionObject(), focalPoint, &result) && result.object == occlusion)
		{
			Node* camera = _scene->getActiveCamera()->getNode();
			// Prevent the camera from getting too close to the character.
			// Without this check, it's possible for the camera to fly past the character
			// and essentially end up in an infinite loop here.
			if (cameraNode->getTranslationWorld().distanceSquared(focalPoint) <= 2.0f) {
				return;
			}

			cameraNode->translateForward(0.1f);
			cameraOffset += 0.1f;
		}
	} while (true);

	// If the character is closer than 10 world units to the camera, apply transparency to the character so he does not obstruct the view.
	if (occlusion)
	{
		float d = _scene->getActiveCamera()->getNode()->getTranslationWorld().distance(_characterScaleNode->getTranslationWorld());
		float alpha = d < 10 ? (d * 0.1f) : 1.0f;
		//_characterMeshNode->setTag("transparent", alpha < 1.0f ? "true" : NULL);
		//_materialParameterAlpha->setValue(alpha);
	}
	else
	{
		//_characterMeshNode->setTag("transparent", NULL);
		// _materialParameterAlpha->setValue(1.0f);
	}
}

void BattleBots::render(float elapsedTime)
{
	// Clear the color and depth buffers
	//clear(CLEAR_COLOR_DEPTH, Vector4::zero(), 1.0f, 0);
	clear(Game::CLEAR_COLOR_DEPTH, 0, 0, 0, 1, 1, 0);

	// Visit all the nodes in the scene for drawing
	_scene->visit(this, &BattleBots::drawScene);

	//getPhysicsController()->drawDebug(_scene->getActiveCamera()->getViewProjectionMatrix());

	_font->start();

	sprintf(fps, "%d", getFrameRate());
	_font->drawText(fps, 5, 5, Vector4(1,1,0,1), 20);

	sprintf(cameraPos, "yaw:%d\npitch:%d", _yaw, _pitch
		);
	_font->drawText(cameraPos, 5, 25, Vector4(1,1,0,1), 20);

	_font->finish();
}

bool BattleBots::drawScene(Node* node)
{
	// If the node visited contains a model, draw it
	if (node->getModel()) {
		node->getModel()->draw();
	} else if (node->getTerrain()) {
		node->getTerrain()->draw(false);
	}

	return true;
}

void BattleBots::keyEvent(Keyboard::KeyEvent evt, int key)
{
	if (evt == Keyboard::KEY_PRESS)
	{
		switch (key)
		{
		case Keyboard::KEY_ESCAPE:
			exit();
			break;
		case Keyboard::KEY_W:
		case Keyboard::KEY_CAPITAL_W:
		case Keyboard::KEY_UP_ARROW:
			_keyFlags |= NORTH;
			_keyFlags &= ~SOUTH;
			break;
		case Keyboard::KEY_S:
		case Keyboard::KEY_CAPITAL_S:
		case Keyboard::KEY_DOWN_ARROW:
			_keyFlags |= SOUTH;
			_keyFlags &= ~NORTH;
			break;
		case Keyboard::KEY_A:
		case Keyboard::KEY_CAPITAL_A:
		case Keyboard::KEY_LEFT_ARROW:
			_keyFlags |= WEST;
			_keyFlags &= ~EAST;
			break;
		case Keyboard::KEY_D:
		case Keyboard::KEY_CAPITAL_D:
		case Keyboard::KEY_RIGHT_ARROW:
			_keyFlags |= EAST;
			_keyFlags &= ~WEST;
			break;
		case Keyboard::KEY_SPACE:
			break;
		case Keyboard::KEY_SHIFT:
			_keyFlags |= RUNNING;
			break;
		case Keyboard::KEY_M:
		case Keyboard::KEY_CAPITAL_M:
			break;
		}
	}
	else if (evt == Keyboard::KEY_RELEASE)
	{
		switch (key)
		{
		case Keyboard::KEY_W:
		case Keyboard::KEY_CAPITAL_W:
			_keyFlags &= ~NORTH;
			break;
		case Keyboard::KEY_S:
		case Keyboard::KEY_CAPITAL_S:
			_keyFlags &= ~SOUTH;
			break;
		case Keyboard::KEY_A:
		case Keyboard::KEY_CAPITAL_A:
			_keyFlags &= ~WEST;
			break;
		case Keyboard::KEY_D:
		case Keyboard::KEY_CAPITAL_D:
			_keyFlags &= ~EAST;
			break;
		case Keyboard::KEY_SHIFT:
			_keyFlags &= ~RUNNING;
			break;
		}
	}
}

void BattleBots::touchEvent(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex)
{
	/*
	if (evt == Touch::TOUCH_PRESS)
	{
	// If the FPS region is touched, toggle vsync (if platform supports it)
	if (x >= 65 && x < 300 && y >= 18 && y <= 48)
	{
	vsync = !vsync;
	setVsync(vsync);
	} else if (mode != MODE_LOOK) {
	// Ray test
	Ray pickRay;
	_scene->getActiveCamera()->pickRay(Rectangle (0, 0, getWidth(), getHeight()), x, y, &pickRay);

	PhysicsController::HitResult hitResult;
	TerrainHitFilter hitFilter(_terrain);
	if (Game::getInstance()->getPhysicsController()->rayTest(pickRay, 1000000, &hitResult, &hitFilter) && hitResult.object == _terrain->getNode()->getCollisionObject())
	{
	Node* clone = NULL;
	PhysicsCollisionShape::Definition rbShape;
	const char* material = NULL;

	switch (mode)
	{
	case MODE_DROP_SPHERE:
	{
	//clone = _sphere->clone();
	//rbShape = PhysicsCollisionShape::sphere();
	//material = "res/common/terrain/shapes.material#sphere";
	}
	break;

	case MODE_DROP_BOX:
	{
	//clone = _box->clone();
	//rbShape = PhysicsCollisionShape::box();
	//material = "res/common/terrain/shapes.material#box";
	}
	break;
	}

	if (clone)
	{
	clone->setScale(10,10,10);
	clone->setTranslation(hitResult.point.x, hitResult.point.y + 50, hitResult.point.z);
	PhysicsRigidBody::Parameters rbParams(1);
	clone->setCollisionObject(PhysicsCollisionObject::RIGID_BODY, rbShape, &rbParams);
	_scene->addNode(clone);
	clone->getModel()->setMaterial(material);
	clone->release();

	shapes.push_back(clone);

	mode = MODE_LOOK;
	//setMessage(NULL);
	}
	}
	}
	}
	*/

	// This should only be called if the gamepad did not handle the touch event.
	switch (evt)
	{
	case Touch::TOUCH_PRESS:
		{
			_rotateX = x;
			_rotateY = y;
		}
		break;
	case Touch::TOUCH_RELEASE:
		{
			_rotateX = 0;
			_rotateY = 0;
		}
		break;
	case Touch::TOUCH_MOVE:
		{
			int deltaX = x - _rotateX;
			int deltaY = y - _rotateY;
			_rotateX = x;
			_rotateY = y;

			// Calculate the angles to move the camera
			_pitch = _pitch + -MATH_DEG_TO_RAD(deltaY * 0.5f);
			_yaw = _yaw + MATH_DEG_TO_RAD(deltaX * 0.5f);


			Node* camera = _scene->getActiveCamera()->getNode();
			// Keep camera level
			camera->setRotation(0,0,0,1);
			// Apply rotation
			camera->rotateY(-_yaw);
			camera->rotateX(_pitch);
		}
		break;
	default:
		break;
	}

}

void BattleBots::gestureSwipeEvent(int x, int y, int direction) {

}

void BattleBots::gesturePinchEvent(int x, int y, float scale) {

}

void BattleBots::gestureTapEvent(int x, int y) {

}