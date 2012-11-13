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


BattleBots::BattleBots()
	: _font(NULL), _scene(NULL), _character(NULL), _characterNode(NULL), _characterMeshNode(NULL), _cameraPivot(NULL), _rotateX(0), _rotateY(0), 
	_yaw(0), _pitch(0), _keyFlags(0)
{
}

void BattleBots::initialize()
{
	setMultiTouch(true);

	    // Load the font.
    _font = Font::create("res/arial40.gpb");

	_scene = Scene::load("res/battlebots.scene");

		// Set the aspect ratio for the scene's camera to match the current resolution
	_scene->getActiveCamera()->setAspectRatio((float)getWidth() / (float)getHeight());

	_cameraPivot = _scene->findNode("cameraPivot");

	initializeCharacter();

	_scene->visit(this, &BattleBots::initializeScene);
}

void BattleBots::initializeCharacter() {
	Node* node = _scene->findNode("character");

	_character = static_cast<PhysicsCharacter*>(node->getCollisionObject());

	_characterNode = node->findNode("characterScale");
	_characterMeshNode = node->findNode("characterMesh");
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

	// Construct direction vector from keyboard input
	if (_keyFlags & NORTH)
		_currentDirection.y = 1;
	else if (_keyFlags & SOUTH)
		_currentDirection.y = -1;
	else
		_currentDirection.y = 0;

	if (_keyFlags & EAST)
		_currentDirection.x = 1;
	else if (_keyFlags & WEST)
		_currentDirection.x = -1;
	else 
		_currentDirection.x = 0;

	_currentDirection.normalize();
	if ((_keyFlags & RUNNING) == 0)
		_currentDirection *= 0.5f;

		// Update character animation and velocity
	if (_currentDirection.isZero()){
		_character->setVelocity(Vector3::zero());
	} else {
		bool running = (_currentDirection.lengthSquared() > 0.75f);
		float speed = running ? RUN_SPEED : WALK_SPEED;

		// Orient the character relative to the camera so he faces the direction we want to move.
		const Matrix& cameraMatrix = _scene->getActiveCamera()->getNode()->getWorldMatrix();
		Vector3 cameraRight, cameraForward;
		cameraMatrix.getRightVector(&cameraRight);
		cameraMatrix.getForwardVector(&cameraForward);

		// Get the current forward vector for the mesh node (negate it since the character was modelled facing +z)
		Vector3 currentHeading(-_characterNode->getForwardVectorWorld());

		// Construct a new forward vector for the mesh node
		Vector3 newHeading(cameraForward * _currentDirection.y + cameraRight * _currentDirection.x);

		// Compute the rotation amount based on the difference between the current and new vectors
		float angle = atan2f(newHeading.x, newHeading.z) - atan2f(currentHeading.x, currentHeading.z);
		if (angle > MATH_PI)
			angle -= MATH_PIX2;
		else if (angle < -MATH_PI)
			angle += MATH_PIX2;
		angle *= (float)elapsedTime * 0.001f * MATH_PIX2;
		_characterNode->rotate(Vector3::unitY(), angle);

		// Update the character's velocity
		Vector3 velocity = -_characterNode->getForwardVectorWorld();
		velocity.normalize();
		velocity *= speed;
		_character->setVelocity(velocity);
	}

	// Adjust camera to avoid it from being obstructed by walls and objects in the scene.
	adjustCamera(elapsedTime);

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
	do
	{
		// Perform a ray test to check for camera collisions
		if (!physics->sweepTest(cameraNode->getCollisionObject(), focalPoint, &result) || result.object == _character)
			break;

		occlusion = result.object;

		// Step the camera closer to the focal point to resolve the occlusion
		float d = cameraNode->getTranslationWorld().distance(result.point);
		cameraNode->translateForward(d);
		cameraOffset += d;
		while (physics->sweepTest(cameraNode->getCollisionObject(), focalPoint, &result) && result.object == occlusion)
		{
			// Prevent the camera from getting too close to the character.
			// Without this check, it's possible for the camera to fly past the character
			// and essentially end up in an infinite loop here.
			if (cameraNode->getTranslationWorld().distanceSquared(focalPoint) <= 2.0f)
				return;

			cameraNode->translateForward(0.1f);
			cameraOffset += 0.1f;
		}
	} while (true);

	// If the character is closer than 10 world units to the camera, apply transparency to the character so he does not obstruct the view.
	if (occlusion)
	{
		float d = _scene->getActiveCamera()->getNode()->getTranslationWorld().distance(_characterNode->getTranslationWorld());
		float alpha = d < 10 ? (d * 0.1f) : 1.0f;
		_characterMeshNode->setTag("transparent", alpha < 1.0f ? "true" : NULL);
		//_materialParameterAlpha->setValue(alpha);
	}
	else
	{
		_characterMeshNode->setTag("transparent", NULL);
	   // _materialParameterAlpha->setValue(1.0f);
	}
}

void BattleBots::render(float elapsedTime)
{
	// Clear the color and depth buffers
	clear(CLEAR_COLOR_DEPTH, Vector4::zero(), 1.0f, 0);

	// Visit all the nodes in the scene for drawing
	_scene->visit(this, &BattleBots::drawScene);

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
	Model* model = node->getModel(); 
	if (model)
	{
		model->draw();
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

			/*
			char buffer[100];

			sprintf(buffer, "deltaY:%d deltaX:%d\nPitch:%f Yaw:%f\n", deltaY, deltaX, _pitch, _yaw);

			Logger::log(Logger::Level::LEVEL_WARN, buffer);
			*/

			// Keep camera level
			_cameraPivot->setRotation(0,0,0,1);

			// Apply rotation
			_cameraPivot->rotateY(-_yaw);
			_cameraPivot->rotateX(_pitch);

		}
		break;
	default:
		break;
	}
}