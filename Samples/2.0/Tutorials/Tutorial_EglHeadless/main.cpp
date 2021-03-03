#include <iostream>

#include "OgreArchiveManager.h"
#include "OgreCamera.h"
#include "OgreConfigFile.h"
#include "OgreRoot.h"
#include "OgreWindow.h"

#include "OgreHlmsManager.h"
#include "OgreHlmsPbs.h"
#include "OgreHlmsUnlit.h"
#include "OgreHlmsPbsDatablock.h"
#include "OgreImage2.h"
#include "OgreItem.h"
#include "OgreTextureGpuManager.h"
#include "OgreTextureFilters.h"

#include "Compositor/OgreCompositorManager2.h"

#include "OgreWindowEventUtilities.h"

#include <OgreOverlayPrerequisites.h>
#include <OgreOverlayManager.h>
#include <OgreOverlayElement.h>
#include <OgreOverlayContainer.h>
#include <OgreFontManager.h>
#include <OgreOverlaySystem.h>

static void registerHlms( void )
{
    using namespace Ogre;

#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
    // Note:  macBundlePath works for iOS too. It's misnamed.
    const String resourcePath = Ogre::macBundlePath() + "/Contents/Resources/";
#elif OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
    const String resourcePath = Ogre::macBundlePath() + "/";
#else
    String resourcePath = "";
#endif

  {
    // Load resource paths from config file
    Ogre::ConfigFile cf;
    cf.load(resourcePath + "resources2.cfg");

    // Go through all sections & settings in the file
    Ogre::ConfigFile::SectionIterator seci = cf.getSectionIterator();

    Ogre::String secName, typeName, archName;
    while( seci.hasMoreElements() )
    {
        secName = seci.peekNextKey();
        Ogre::ConfigFile::SettingsMultiMap *settings = seci.getNext();

        if( secName != "Hlms" )
        {
            Ogre::ConfigFile::SettingsMultiMap::iterator i;
            for (i = settings->begin(); i != settings->end(); ++i)
            {
                typeName = i->first;
                archName = i->second;
                Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
                  archName, typeName, secName);
            }
        }
    }
  }

    ConfigFile cf;
    cf.load( resourcePath + "resources2.cfg" );

#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE || OGRE_PLATFORM == OGRE_PLATFORM_APPLE_IOS
    String rootHlmsFolder = macBundlePath() + '/' + cf.getSetting( "DoNotUseAsResource", "Hlms", "" );
#else
    String rootHlmsFolder = resourcePath + cf.getSetting( "DoNotUseAsResource", "Hlms", "" );
#endif

    if( rootHlmsFolder.empty() )
        rootHlmsFolder = "./";
    else if( *( rootHlmsFolder.end() - 1 ) != '/' )
        rootHlmsFolder += "/";

    // At this point rootHlmsFolder should be a valid path to the Hlms data folder

    HlmsUnlit *hlmsUnlit = 0;
    HlmsPbs *hlmsPbs = 0;

    // For retrieval of the paths to the different folders needed
    String mainFolderPath;
    StringVector libraryFoldersPaths;
    StringVector::const_iterator libraryFolderPathIt;
    StringVector::const_iterator libraryFolderPathEn;

    ArchiveManager &archiveManager = ArchiveManager::getSingleton();

    {
        // Create & Register HlmsUnlit
        // Get the path to all the subdirectories used by HlmsUnlit
        HlmsUnlit::getDefaultPaths( mainFolderPath, libraryFoldersPaths );
        Archive *archiveUnlit =
            archiveManager.load( rootHlmsFolder + mainFolderPath, "FileSystem", true );
        ArchiveVec archiveUnlitLibraryFolders;
        libraryFolderPathIt = libraryFoldersPaths.begin();
        libraryFolderPathEn = libraryFoldersPaths.end();
        while( libraryFolderPathIt != libraryFolderPathEn )
        {
            Archive *archiveLibrary =
                archiveManager.load( rootHlmsFolder + *libraryFolderPathIt, "FileSystem", true );
            archiveUnlitLibraryFolders.push_back( archiveLibrary );
            ++libraryFolderPathIt;
        }

        // Create and register the unlit Hlms
        hlmsUnlit = OGRE_NEW HlmsUnlit( archiveUnlit, &archiveUnlitLibraryFolders );
        Root::getSingleton().getHlmsManager()->registerHlms( hlmsUnlit );
    }

    {
        // Create & Register HlmsPbs
        // Do the same for HlmsPbs:
        HlmsPbs::getDefaultPaths( mainFolderPath, libraryFoldersPaths );
        Archive *archivePbs = archiveManager.load( rootHlmsFolder + mainFolderPath, "FileSystem", true );

        // Get the library archive(s)
        ArchiveVec archivePbsLibraryFolders;
        libraryFolderPathIt = libraryFoldersPaths.begin();
        libraryFolderPathEn = libraryFoldersPaths.end();
        while( libraryFolderPathIt != libraryFolderPathEn )
        {
            Archive *archiveLibrary =
                archiveManager.load( rootHlmsFolder + *libraryFolderPathIt, "FileSystem", true );
            archivePbsLibraryFolders.push_back( archiveLibrary );
            ++libraryFolderPathIt;
        }

        // Create and register
        hlmsPbs = OGRE_NEW HlmsPbs( archivePbs, &archivePbsLibraryFolders );
        Root::getSingleton().getHlmsManager()->registerHlms( hlmsPbs );
    }

    RenderSystem *renderSystem = Root::getSingletonPtr()->getRenderSystem();
    if( renderSystem->getName() == "Direct3D11 Rendering Subsystem" )
    {
        // Set lower limits 512kb instead of the default 4MB per Hlms in D3D 11.0
        // and below to avoid saturating AMD's discard limit (8MB) or
        // saturate the PCIE bus in some low end machines.
        bool supportsNoOverwriteOnTextureBuffers;
        renderSystem->getCustomAttribute( "MapNoOverwriteOnDynamicBufferSRV",
                                          &supportsNoOverwriteOnTextureBuffers );

        if( !supportsNoOverwriteOnTextureBuffers )
        {
            hlmsPbs->setTextureBufferDefaultSize( 512 * 1024 );
            hlmsUnlit->setTextureBufferDefaultSize( 512 * 1024 );
        }
    }
}

int main( int argc, const char *argv[] )
{
    using namespace Ogre;

    Root *root = OGRE_NEW Root( "plugins.cfg", "ogre.cfg", "Ogre2_test.log" );

    // auto ogreOverlaySystem = new v1::OverlaySystem();

    Ogre::RenderSystem *renderSys;
    const Ogre::RenderSystemList *rsList;

    rsList = &(root->getAvailableRenderers());

    int c = 0;

    renderSys = nullptr;

    do
    {
      if (c == static_cast<int>(rsList->size()))
        break;

      renderSys = rsList->at(c);
      c++;
    }
    // cpplint has a false positive when extending a while call to multiple lines
    // (it thinks the while loop is empty), so we must put the whole while
    // statement on one line and add NOLINT at the end so that cpplint doesn't
    // complain about the line being too long
    while (renderSys && renderSys->getName().compare("OpenGL 3+ Rendering Subsystem") != 0); // NOLINT

    if (renderSys == nullptr)
    {
      std::cerr << "unable to find OpenGL rendering system. OGRE is probably "
              "installed incorrectly. Double check the OGRE cmake output, "
              "and make sure OpenGL is enabled." << std::endl;
    }

    // We operate in windowed mode
    std::cerr << "setConfigOption" << '\n';
    renderSys->setConfigOption("Full Screen", "No");

    renderSys->setConfigOption( "Interface", "Headless EGL / PBuffer" );

    {
      const Ogre::ConfigOptionMap &configOptions = renderSys->getConfigOptions();
      Ogre::ConfigOptionMap::const_iterator itor = configOptions.find( "Device" );
      if( itor == configOptions.end() )
      {
          fprintf( stderr, "Something must be wrong with EGL init. Cannot find Device" );
          return -1;
      }

      if( isatty( fileno( stdin ) ) )
      {
          printf( "Select device (this sample supports selecting between the first 10):\n" );

          int devNum = 0;
          Ogre::StringVector::const_iterator itDev = itor->second.possibleValues.begin();
          Ogre::StringVector::const_iterator enDev = itor->second.possibleValues.end();

          while( itDev != enDev )
          {
              printf( "[%i] %s\n", devNum, itDev->c_str() );
              ++devNum;
              ++itDev;
          }

          devNum = std::min( devNum, 10 );

          int devIdxChar = '2';//getchar();
          // while( devIdxChar < '0' || devIdxChar - '0' > devNum )
          //     devIdxChar = getchar();

          const uint32_t devIdx = static_cast<uint32_t>( devIdxChar - '0' );

          printf( "Selecting [%i] %s\n", devIdx, itor->second.possibleValues[devIdx].c_str() );
          renderSys->setConfigOption( "Device", itor->second.possibleValues[devIdx] );
      }
      else
      {
          printf( "!!! IMPORTANT !!!\n" );
          printf(
              "App is running from a file or pipe. Selecting a default device. Run from a real "
              "terminal for interactive selection\n" );
          printf( "!!! IMPORTANT !!!\n" );
          fflush( stdout );
      }
    }

    root->setRenderSystem(renderSys);
    root->initialise(false);

    Ogre::StringVector paramsVector;
    Ogre::NameValuePairList params;
    Ogre::Window * window = nullptr;

      // params["parentWindowHandle"] = _handle;

    // params["FSAA"] = std::to_string(_antiAliasing);
    params["stereoMode"] = "Frame Sequential";

    // TODO(anyone): determine api without qt

    // Hide window if dimensions are less than or equal to one.
    params["border"] = "none";

    // std::ostringstream stream;
    // stream << "OgreWindow(0)" << "_" << _handle;

    // Needed for retina displays
    // params["contentScalingFactor"] = std::to_string(_ratio);

    // Ogre 2 PBS expects gamma correction
    params["gamma"] = "Yes";

      params["externalGLControl"] = "true";
      params["currentGLContext"] = "true";

    int attempts = 0;
    while (window == nullptr && (attempts++) < 10)
    {
      try
      {
        window = Ogre::Root::getSingleton().createRenderWindow(
            "OgreWindow", 800, 600, false, &params);
        registerHlms();
        // Initialise, parse scripts etc
        Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups( true );
      }
      catch(...)
      {
        std::cerr << " Unable to create the rendering window\n";
        window = nullptr;
      }
    }

    if (attempts >= 10)
    {
      std::cerr << "Unable to create the rendering window\n" << std::endl;
      return -1;
    }

    if (window)
    {
      // window->setActive(true);
      window->_setVisible(true);

      // Windows needs to reposition the render window to 0,0.
      window->reposition(0, 0);
    }

    // Create SceneManager
    const size_t numThreads = 1u;
    SceneManager *sceneManager = root->createSceneManager( ST_GENERIC, numThreads, "ExampleSMInstance" );

    auto ogreOverlaySystem = new Ogre::v1::OverlaySystem();
    sceneManager->addRenderQueueListener(ogreOverlaySystem);

    sceneManager->getRenderQueue()->setSortRenderQueue(
        Ogre::v1::OverlayManager::getSingleton().mDefaultRenderQueueId,
        Ogre::RenderQueue::StableSort);

    // Set sane defaults for proper shadow mapping
    sceneManager->setShadowDirectionalLightExtrusionDistance(500.0f);
    sceneManager->setShadowFarDistance(500.0f);

    // enable forward plus to support multiple lights
    // this is required for non-shadow-casting point lights and
    // spot lights to work
    sceneManager->setForwardClustered(true, 16, 8, 24, 96, 0, 0, 1, 500);


    // Create & setup camera
    Camera *camera = sceneManager->createCamera( "Main Camera" );

    // Position it at 500 in Z direction
    camera->setPosition( Vector3( 0, 2, 5 ) );
    // Look back along -Z
    camera->lookAt( Vector3( 0, 0, 0 ) );
    camera->setNearClipDistance( 0.2f );
    camera->setFarClipDistance( 1000.0f );
    camera->setAutoAspectRatio( true );

    // Setup a basic compositor with a blue clear colour
    CompositorManager2 *compositorManager = root->getCompositorManager2();
    const String workspaceName( "Demo Workspace" );
    const ColourValue backgroundColour( 0.f, 0.0f, 0.0f );
    compositorManager->createBasicWorkspaceDef( workspaceName, backgroundColour, IdString() );
    compositorManager->addWorkspace( sceneManager, window->getTexture(), camera, workspaceName, true );

    Ogre::String meshName = "Sphere1000.mesh";

    Ogre::Item *item = sceneManager->createItem( meshName,
                                                 Ogre::ResourceGroupManager::
                                                 AUTODETECT_RESOURCE_GROUP_NAME,
                                                 Ogre::SCENE_DYNAMIC );

    Ogre::HlmsManager *hlmsManager = root->getHlmsManager();

    Ogre::HlmsPbs *hlmsPbs = static_cast<Ogre::HlmsPbs*>( hlmsManager->getHlms(Ogre::HLMS_PBS) );
    Ogre::TextureGpuManager *textureMgr = root->getRenderSystem()->getTextureGpuManager();

    Ogre::String datablockName = "Test" + Ogre::StringConverter::toString( 0 );
    Ogre::HlmsPbsDatablock *datablock = static_cast<Ogre::HlmsPbsDatablock*>(
                hlmsPbs->createDatablock( "myRealName", "myHumanReadableName",
                                          Ogre::HlmsMacroblock(),
                                          Ogre::HlmsBlendblock(),
                                          Ogre::HlmsParamVec(), true) );

    // Ogre::TextureGpu *texture = textureMgr->createOrRetrieveTexture(
    //                                 "SaintPetersBasilica.dds",
    //                                 Ogre::GpuPageOutStrategy::Discard,
    //                                 Ogre::TextureFlags::PrefersLoadingFromFileAsSRGB,
    //                                 Ogre::TextureTypes::TypeCube,
    //                                 Ogre::ResourceGroupManager::
    //                                 AUTODETECT_RESOURCE_GROUP_NAME,
    //                                 Ogre::TextureFilter::TypeGenerateDefaultMipmaps );
    //
    // datablock->setTexture( Ogre::PBSM_REFLECTION, texture );
    datablock->setDiffuse( Ogre::Vector3( 0, 0, 0.8 ) );
    datablock->setSpecular( Ogre::Vector3( 0.5f, 0.5f, 0.5f ) );
    // datablock->setEmissive( Ogre::Vector3( 1.0f, 1.0f, 1.0f ) );
    datablock->setEmissive( Ogre::Vector3( .0f, .0f, .0f ) );
    // datablock->setWorkflow(Ogre::HlmsPbsDatablock::MetallicWorkflow);
    datablock->setMetalness(0);
    datablock->setRoughness( 0.5f );
    datablock->setReceiveShadows(true);
    datablock->setTransparency(1, Ogre::HlmsPbsDatablock::None);
    datablock->setTexture(Ogre::PBSM_DIFFUSE, "");
    datablock->setTexture(Ogre::PBSM_NORMAL, "");
    datablock->setTexture(Ogre::PBSM_ROUGHNESS, "");
    datablock->setTexture(Ogre::PBSM_METALLIC, "");
    datablock->setTexture(Ogre::PBSM_REFLECTION, "");
    datablock->setTexture(Ogre::PBSM_EMISSIVE, "");
    datablock->setTexture(Ogre::PBSM_DETAIL0, "");

    Ogre::HlmsMacroblock macroblock(
        *datablock->getMacroblock());
    macroblock.mDepthCheck = false;
    macroblock.mDepthWrite = false;
    macroblock.mDepthBiasConstant = 1;
    datablock->setMacroblock(macroblock);

    std::cerr << "datablock->getNameStr() " << *datablock->getNameStr() << '\n';

    // datablock->setFresnel( Ogre::Vector3( 5 / Ogre::max( 1, (float)(5-1) ) ), false );
    item->setCastShadows(true);

    // item->setDatablock( "myRealName" );
    item->setDatablock( datablock );


    // item->setDatablock( "Rocks" );
    item->setVisibilityFlags( 0x000000001 );

    auto node = sceneManager->getRootSceneNode( Ogre::SCENE_DYNAMIC )->
            createChildSceneNode( Ogre::SCENE_DYNAMIC );

    node->setPosition( 0, 0, 0 );
    node->setScale( 0.65f, 0.65f, 0.65f );

    node->attachObject( item );

    Ogre::SceneNode *rootNode = sceneManager->getRootSceneNode();

    Ogre::Light *light = sceneManager->createLight();
    Ogre::SceneNode *lightNode = rootNode->createChildSceneNode();
    lightNode->attachObject( light );
    light->setPowerScale( 1.0f );
    light->setType( Ogre::Light::LT_DIRECTIONAL );
    light->setDirection( Ogre::Vector3( -1, -1, -1 ).normalisedCopy() );

    sceneManager->setAmbientLight( Ogre::ColourValue( 0.3f, 0.5f, 0.7f ) * 0.1f * 0.75f,
                                   Ogre::ColourValue( 0.6f, 0.45f, 0.3f ) * 0.065f * 0.75f,
                                   -light->getDirection() + Ogre::Vector3::UNIT_Y * 0.2f );

    bool bQuit = false;

    // Run for 120 frames and exit
    int frames = 120;

    while( !bQuit && frames > 0 )
    {
        WindowEventUtilities::messagePump();
        bQuit |= !root->renderOneFrame();

        if( frames == 110 )
        {
            std::cerr << "W: " << window->getTexture()->getWidth() << " H: " <<  window->getTexture()->getHeight()
              << " D: " << window->getTexture()->getDepth() << " S: " << window->getTexture()->getNumSlices() << '\n';
            printf( "Saving one of the rendered frames to EglHeadlessOutput.png\n" );
            Image2 image;
            image.convertFromTexture( window->getTexture(), 0u, 0u );
            image.save( "./EglHeadlessOutput.png", 0u, 1u );
            printf( "Saving done!\n" );
        }

        --frames;
    }

    OGRE_DELETE root;
    root = 0;

    return 0;
}
