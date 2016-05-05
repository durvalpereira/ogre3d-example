#include <Ogre.h>
#include <OIS/OIS.h>
#include <CEGUI/CEGUI.h>
#include <OgreCEGUIRenderer.h>

#include "btBulletDynamicsCommon.h"

using namespace Ogre;


//function defs
ManualObject* createCubeMesh(Ogre::String name, Ogre::String matName);


// this pattern updates the scenenode position when it changes within the bullet simulation
// taken from BulletMotionState docs page24
class MyMotionState : public btMotionState {
public:
    MyMotionState(const btTransform &initialpos, Ogre::SceneNode *node) {
        mVisibleobj = node;
        mPos1 = initialpos;
    }
    virtual ~MyMotionState() {    }
    void setNode(Ogre::SceneNode *node) {
        mVisibleobj = node;
    }
    virtual void getWorldTransform(btTransform &worldTrans) const {
        worldTrans = mPos1;
    }
    virtual void setWorldTransform(const btTransform &worldTrans) {
        if(NULL == mVisibleobj) return; // silently return before we set a node
        btQuaternion rot = worldTrans.getRotation();
        mVisibleobj->setOrientation(rot.w(), rot.x(), rot.y(), rot.z());
        btVector3 pos = worldTrans.getOrigin();
        // TODO **** XXX need to fix this up such that it renders properly since this doesnt know the scale of the node
        // also the getCube function returns a cube that isnt centered on Z
        mVisibleobj->setPosition(pos.x(), pos.y()+5, pos.z()-5);
    }
protected:
    Ogre::SceneNode *mVisibleobj;
    btTransform mPos1;
};


class Application : public FrameListener, public OIS::KeyListener
{
public:
    void go()
    {
       mContinue = true;
        createRoot();
        defineResources();
        setupRenderSystem();
        createRenderWindow();
        initializeResourceGroups();
        setupScene();
        createBulletSim();
        setupInputSystem();
        setupCEGUI();
        createFrameListener();
        startRenderLoop();
    }

    ~Application()
    {
        mInputManager->destroyInputObject(mKeyboard);
        OIS::InputManager::destroyInputSystem(mInputManager);

        delete mSystem;
        delete mRenderer;
        delete mRoot;

        // cleanup bulletdyanmics

       //cleanup in the reverse order of creation/initialization
       //remove the rigidbodies from the dynamics world and delete them
       for (i=dynamicsWorld->getNumCollisionObjects()-1; i>=0 ;i--)
       {
          btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
          btRigidBody* body = btRigidBody::upcast(obj);
          if (body && body->getMotionState())
          {
             delete body->getMotionState();
          }
          dynamicsWorld->removeCollisionObject( obj );
          delete obj;
       }

       //delete collision shapes
       for (int j=0;j<collisionShapes.size();j++)
       {
          btCollisionShape* shape = collisionShapes[j];
          collisionShapes[j] = 0;
          delete shape;
       }

       delete dynamicsWorld;
       delete solver;
       delete overlappingPairCache;
       delete dispatcher;
       delete collisionConfiguration;
    }

private:
    Root *mRoot;
    SceneManager *mSceneMgr;
    OIS::Keyboard *mKeyboard;
    OIS::InputManager *mInputManager;
    CEGUI::OgreCEGUIRenderer *mRenderer;
    CEGUI::System *mSystem;
    bool mContinue;

      // scene objects
      SceneNode *boxNode;

    // bullet dynamics
     int i;
     btDefaultCollisionConfiguration* collisionConfiguration;
     btCollisionDispatcher* dispatcher;
     btBroadphaseInterface* overlappingPairCache;
     btSequentialImpulseConstraintSolver* solver;
     btDiscreteDynamicsWorld* dynamicsWorld;
     btCollisionShape* groundShape;
      btAlignedObjectArray<btCollisionShape*> collisionShapes;


      // frame listener
   bool frameStarted(const FrameEvent &evt)
    {
      mKeyboard->capture();

        // update physics simulation
      dynamicsWorld->stepSimulation(evt.timeSinceLastFrame,50);
        return mContinue;
    }

   // KeyListener
   bool keyPressed(const OIS::KeyEvent &e) {
      switch (e.key) {
             case OIS::KC_ESCAPE:
                mContinue = false;
                 break;
             default:
                break;
      }
      return true;
   }
   bool keyReleased(const OIS::KeyEvent &e) { return true; }

    void createRoot()
    {
        mRoot = new Root();
    }

    void defineResources()
    {
        String secName, typeName, archName;
        ConfigFile cf;
        cf.load("resources.cfg");

        ConfigFile::SectionIterator seci = cf.getSectionIterator();
        while (seci.hasMoreElements())
        {
            secName = seci.peekNextKey();
            ConfigFile::SettingsMultiMap *settings = seci.getNext();
            ConfigFile::SettingsMultiMap::iterator i;
            for (i = settings->begin(); i != settings->end(); ++i)
            {
                typeName = i->first;
                archName = i->second;
                ResourceGroupManager::getSingleton().addResourceLocation(archName, typeName, secName);
            }
        }
    }

    void setupRenderSystem()
    {
        if (!mRoot->restoreConfig() && !mRoot->showConfigDialog())
            throw Exception(52, "User canceled the config dialog!", "Application::setupRenderSystem()");

        //// Do not add this to the application
        //RenderSystem *rs = mRoot->getRenderSystemByName("Direct3D9 Rendering Subsystem");
        //                                      // or use "OpenGL Rendering Subsystem"
        //mRoot->setRenderSystem(rs);
        //rs->setConfigOption("Full Screen", "No");
        //rs->setConfigOption("Video Mode", "800 x 600 @ 32-bit colour");
    }

    void createRenderWindow()
    {
        mRoot->initialise(true, "Tutorial Render Window");

        //// Do not add this to the application
        //mRoot->initialise(false);
        //HWND hWnd = 0;  // Get the hWnd of the application!
        //NameValuePairList misc;
        //misc["externalWindowHandle"] = StringConverter::toString((int)hWnd);
        //RenderWindow *win = mRoot->createRenderWindow("Main RenderWindow", 800, 600, false, &misc);
    }

    void initializeResourceGroups()
    {
        TextureManager::getSingleton().setDefaultNumMipmaps(5);
        ResourceGroupManager::getSingleton().initialiseAllResourceGroups();
    }

    void setupScene()
    {
        mSceneMgr = mRoot->createSceneManager(ST_GENERIC, "Default SceneManager");
        Camera *cam = mSceneMgr->createCamera("Camera");
        Viewport *vp = mRoot->getAutoCreatedWindow()->addViewport(cam);

          Entity *ent;

        mSceneMgr->setAmbientLight(ColourValue(0.25, 0.25, 0.25));
        mSceneMgr->setShadowTechnique( SHADOWTYPE_STENCIL_ADDITIVE );

        // make a cube to bounce around
      ManualObject *cmo = createCubeMesh("manual", "");
      cmo->convertToMesh("cube");
      ent = mSceneMgr->createEntity("Cube", "cube.mesh");
      ent->setCastShadows(true);
      boxNode = mSceneMgr->getRootSceneNode()->createChildSceneNode();
      boxNode->attachObject(ent);
      boxNode->setScale(Vector3(0.1,0.1,0.1)); // for some reason converttomesh multiplied dimensions by 10


      // make a rock wall on the floor
        Plane plane(Vector3::UNIT_Y, 0);
        MeshManager::getSingleton().createPlane("ground",
               ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, plane,
               1500,1500,20,20,true,1,5,5,Vector3::UNIT_Z);
        ent = mSceneMgr->createEntity("GroundEntity", "ground");
        mSceneMgr->getRootSceneNode()->createChildSceneNode()->attachObject(ent);
      ent->setMaterialName("Examples/Rockwall");
      ent->setCastShadows(false);


      // make a light to see stuff with
        Light *light = mSceneMgr->createLight("Light1");
        light->setType(Light::LT_POINT);
        light->setPosition(Vector3(250, 150, 250));
        light->setDiffuseColour(ColourValue::White);
        light->setSpecularColour(ColourValue::White);

        // Create the scene node
        SceneNode *node = mSceneMgr->getRootSceneNode()->createChildSceneNode("CamNode1", Vector3(-200, 100, 200));
        node->yaw(Degree(-45));
        node->attachObject(cam);
    }

    void setupInputSystem()
    {
        size_t windowHnd = 0;
        std::ostringstream windowHndStr;
        OIS::ParamList pl;
        RenderWindow *win = mRoot->getAutoCreatedWindow();

        win->getCustomAttribute("WINDOW", &windowHnd);
        windowHndStr << windowHnd;
        pl.insert(std::make_pair(std::string("WINDOW"), windowHndStr.str()));
        mInputManager = OIS::InputManager::createInputSystem(pl);

        try
        {
            mKeyboard = static_cast<OIS::Keyboard*>(mInputManager->createInputObject(OIS::OISKeyboard, true));
            mKeyboard->setEventCallback(this);
            //mMouse = static_cast<OIS::Mouse*>(mInputManager->createInputObject(OIS::OISMouse, false));
            //mJoy = static_cast<OIS::JoyStick*>(mInputManager->createInputObject(OIS::OISJoyStick, false));
        }
        catch (const OIS::Exception &e)
        {
            throw new Exception(42, e.eText, "Application::setupInputSystem");
        }
    }

    void setupCEGUI()
    {
        SceneManager *mgr = mRoot->getSceneManager("Default SceneManager");
        RenderWindow *win = mRoot->getAutoCreatedWindow();

        // CEGUI setup
        mRenderer = new CEGUI::OgreCEGUIRenderer(win, Ogre::RENDER_QUEUE_OVERLAY, false, 3000, mgr);
        mSystem = new CEGUI::System(mRenderer);

        // Other CEGUI setup here.
    }

    void createFrameListener()
    {
        mRoot->addFrameListener(this);
    }

    void startRenderLoop()
    {
        mRoot->startRendering();
    }

    void createBulletSim(void) {
         ///collision configuration contains default setup for memory, collision setup. Advanced users can create their own configuration.
         collisionConfiguration = new btDefaultCollisionConfiguration();

         ///use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
         dispatcher = new   btCollisionDispatcher(collisionConfiguration);

         ///btDbvtBroadphase is a good general purpose broadphase. You can also try out btAxis3Sweep.
         overlappingPairCache = new btDbvtBroadphase();

         ///the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
         solver = new btSequentialImpulseConstraintSolver;

         dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher,overlappingPairCache,solver,collisionConfiguration);
         dynamicsWorld->setGravity(btVector3(0,-10,0));

         ///create a few basic rigid bodies
         // start with ground plane, 1500, 1500
         btCollisionShape* groundShape = new btBoxShape(btVector3(btScalar(1500.),btScalar(1.),btScalar(1500.)));

         collisionShapes.push_back(groundShape);

         btTransform groundTransform;
         groundTransform.setIdentity();
         groundTransform.setOrigin(btVector3(0,-2,0));

         {
            btScalar mass(0.);

            //rigidbody is dynamic if and only if mass is non zero, otherwise static
            bool isDynamic = (mass != 0.f);

            btVector3 localInertia(0,0,0);
            if (isDynamic)
               groundShape->calculateLocalInertia(mass,localInertia);

            // lathe - this plane isnt going to be moving so i dont care about setting the motion state
            //using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
            btDefaultMotionState* myMotionState = new btDefaultMotionState(groundTransform);
            btRigidBody::btRigidBodyConstructionInfo rbInfo(mass,myMotionState,groundShape,localInertia);
            btRigidBody* body = new btRigidBody(rbInfo);

            //add the body to the dynamics world
            dynamicsWorld->addRigidBody(body);
         }


         {
            //create a dynamic rigidbody

            btCollisionShape* colShape = new btBoxShape(btVector3(1,1,1));
  //          btCollisionShape* colShape = new btSphereShape(btScalar(1.));
            collisionShapes.push_back(colShape);

            /// Create Dynamic Objects
            btTransform startTransform;
            startTransform.setIdentity();

            btScalar   mass(1.f);

            //rigidbody is dynamic if and only if mass is non zero, otherwise static
            bool isDynamic = (mass != 0.f);

            btVector3 localInertia(0,0,-1.0);
            if (isDynamic)
               colShape->calculateLocalInertia(mass,localInertia);

               startTransform.setOrigin(btVector3(0,250,0));
               // *** give it a slight twist so it bouncees more interesting
               startTransform.setRotation(btQuaternion(btVector3(1.0, 1.0, 0.0), 0.6));

               //using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
               //btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
               MyMotionState* motionState = new MyMotionState(startTransform, boxNode);
               btRigidBody::btRigidBodyConstructionInfo rbInfo(mass,motionState,colShape,localInertia);
               btRigidBody* body = new btRigidBody(rbInfo);

               dynamicsWorld->addRigidBody(body);
         }
    }
};



#if OGRE_PLATFORM == PLATFORM_WIN32 || OGRE_PLATFORM == OGRE_PLATFORM_WIN32
#define WIN32_LEAN_AND_MEAN
#include "windows.h"

INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR strCmdLine, INT)
#else
int main(int argc, char **argv)
#endif
{
    try
    {
        Application app;
        app.go();
    }
    catch(Exception& e)
    {
#if OGRE_PLATFORM == PLATFORM_WIN32 || OGRE_PLATFORM == OGRE_PLATFORM_WIN32
        MessageBoxA(NULL, e.getFullDescription().c_str(), "An exception has occurred!", MB_OK | MB_ICONERROR | MB_TASKMODAL);
#else
        fprintf(stderr, "An exception has occurred: %s\n",
            e.getFullDescription().c_str());
#endif
    }

    return 0;
}



// make a cube, no cube primiatives in ogre
// yanked from betajaen
//http://www.ogre3d.org/forums/viewtopic.php?p=301318&sid=ce193664e1d3d7c4af509e6f4e2718c6
ManualObject* createCubeMesh(Ogre::String name, Ogre::String matName) {

   ManualObject* cube = new ManualObject(name);

   cube->begin(matName);

   cube->position(0.5,-0.5,1.0);cube->normal(0.408248,-0.816497,0.408248);cube->textureCoord(1,0);
   cube->position(-0.5,-0.5,0.0);cube->normal(-0.408248,-0.816497,-0.408248);cube->textureCoord(0,1);
   cube->position(0.5,-0.5,0.0);cube->normal(0.666667,-0.333333,-0.666667);cube->textureCoord(1,1);
   cube->position(-0.5,-0.5,1.0);cube->normal(-0.666667,-0.333333,0.666667);cube->textureCoord(0,0);
   cube->position(0.5,0.5,1.0);cube->normal(0.666667,0.333333,0.666667);cube->textureCoord(1,0);
   cube->position(-0.5,-0.5,1.0);cube->normal(-0.666667,-0.333333,0.666667);cube->textureCoord(0,1);
   cube->position(0.5,-0.5,1.0);cube->normal(0.408248,-0.816497,0.408248);cube->textureCoord(1,1);
   cube->position(-0.5,0.5,1.0);cube->normal(-0.408248,0.816497,0.408248);cube->textureCoord(0,0);
   cube->position(-0.5,0.5,0.0);cube->normal(-0.666667,0.333333,-0.666667);cube->textureCoord(0,1);
   cube->position(-0.5,-0.5,0.0);cube->normal(-0.408248,-0.816497,-0.408248);cube->textureCoord(1,1);
   cube->position(-0.5,-0.5,1.0);cube->normal(-0.666667,-0.333333,0.666667);cube->textureCoord(1,0);
   cube->position(0.5,-0.5,0.0);cube->normal(0.666667,-0.333333,-0.666667);cube->textureCoord(0,1);
   cube->position(0.5,0.5,0.0);cube->normal(0.408248,0.816497,-0.408248);cube->textureCoord(1,1);
   cube->position(0.5,-0.5,1.0);cube->normal(0.408248,-0.816497,0.408248);cube->textureCoord(0,0);
   cube->position(0.5,-0.5,0.0);cube->normal(0.666667,-0.333333,-0.666667);cube->textureCoord(1,0);
   cube->position(-0.5,-0.5,0.0);cube->normal(-0.408248,-0.816497,-0.408248);cube->textureCoord(0,0);
   cube->position(-0.5,0.5,1.0);cube->normal(-0.408248,0.816497,0.408248);cube->textureCoord(1,0);
   cube->position(0.5,0.5,0.0);cube->normal(0.408248,0.816497,-0.408248);cube->textureCoord(0,1);
   cube->position(-0.5,0.5,0.0);cube->normal(-0.666667,0.333333,-0.666667);cube->textureCoord(1,1);
   cube->position(0.5,0.5,1.0);cube->normal(0.666667,0.333333,0.666667);cube->textureCoord(0,0);

   cube->triangle(0,1,2);      cube->triangle(3,1,0);
   cube->triangle(4,5,6);      cube->triangle(4,7,5);
   cube->triangle(8,9,10);      cube->triangle(10,7,8);
   cube->triangle(4,11,12);   cube->triangle(4,13,11);
   cube->triangle(14,8,12);   cube->triangle(14,15,8);
   cube->triangle(16,17,18);   cube->triangle(16,19,17);
   cube->end();

   return cube;
}