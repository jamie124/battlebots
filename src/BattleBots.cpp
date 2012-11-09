#include "BattleBots.h"

// Declare our game instance
BattleBots game;

BattleBots::BattleBots()
    : _scene(NULL)
{
}

void BattleBots::initialize()
{
    setMultiTouch(true);

    _scene = Scene::load("res/battlebots.scene");

    Node* node = _scene->findNode("box");

    // Get the physics object for box

   // _character = static_cast<PhysicsCharacter*>(node->getCollisionObject());
    //_characterNode = _scene->findNode("box");

    // Set the aspect ratio for the scene's camera to match the current resolution
    _scene->getActiveCamera()->setAspectRatio((float)getWidth() / (float)getHeight());

    _scene->visit(this, &BattleBots::initializeScene);
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
    SAFE_RELEASE(_scene);
}

void BattleBots::update(float elapsedTime)
{
    // Rotate model
}

void BattleBots::render(float elapsedTime)
{
    // Clear the color and depth buffers
    clear(CLEAR_COLOR_DEPTH, Vector4::zero(), 1.0f, 0);

    // Visit all the nodes in the scene for drawing
    _scene->visit(this, &BattleBots::drawScene);
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
        }
    }
}

void BattleBots::touchEvent(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex)
{
    switch (evt)
    {
    case Touch::TOUCH_PRESS:
        break;
    case Touch::TOUCH_RELEASE:
        break;
    case Touch::TOUCH_MOVE:
        break;
    };
}
