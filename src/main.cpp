#include <Geode/Geode.hpp>
#include <Geode/modify/CCSprite.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <cocos2d.h>
#include <unordered_map>
#include <random>

using namespace geode::prelude;
using namespace cocos2d;

// Random number generator
static std::mt19937 rng(std::random_device{}());
static std::uniform_int_distribution<int> dist(0, 255);

// Stores already generated colors for textures
static std::unordered_map<CCTexture2D*, cocos2d::ccColor4B> g_textureColors;

// Creates a solid color texture
static CCTexture2D* makeSolidColor(unsigned char r, unsigned char g, unsigned char b) {
    unsigned char pixel[4] = { r, g, b, 255 };
    auto img = new CCImage();
    img->initWithImageData(pixel, sizeof(pixel), CCImage::kFmtRawData, 1, 1, 8);
    auto tex = new CCTexture2D();
    tex->initWithImage(img);
    img->release();
    return tex;
}

// Hook all sprites
class $modify(RandomColorSprite, CCSprite) {
    bool initWithSpriteFrame(CCSpriteFrame* frame) {
        if (!CCSprite::initWithSpriteFrame(frame))
            return false;

        if (frame && frame->getTexture()) {
            auto tex = frame->getTexture();
            if (g_textureColors.find(tex) == g_textureColors.end()) {
                g_textureColors[tex] = { (GLubyte)dist(rng), (GLubyte)dist(rng), (GLubyte)dist(rng), 255 };
            }
            auto color = g_textureColors[tex];
            this->setTexture(makeSolidColor(color.r, color.g, color.b));
        }

        return true;
    }

    bool initWithTexture(CCTexture2D* texture, const CCRect& rect) {
        if (!CCSprite::initWithTexture(texture, rect))
            return false;

        if (texture) {
            if (g_textureColors.find(texture) == g_textureColors.end()) {
                g_textureColors[texture] = { (GLubyte)dist(rng), (GLubyte)dist(rng), (GLubyte)dist(rng), 255 };
            }
            auto color = g_textureColors[texture];
            this->setTexture(makeSolidColor(color.r, color.g, color.b));
        }

        return true;
    }

    void setTexture(CCTexture2D* texture) {
        if (texture) {
            if (g_textureColors.find(texture) == g_textureColors.end()) {
                g_textureColors[texture] = { (GLubyte)dist(rng), (GLubyte)dist(rng), (GLubyte)dist(rng), 255 };
            }
            auto color = g_textureColors[texture];
            CCSprite::setTexture(makeSolidColor(color.r, color.g, color.b));
        } else {
            CCSprite::setTexture(nullptr);
        }
    }
};

// Track when the PlayLayer is ready
static bool g_playLayerReady = false;
static bool g_overlayDrawn = false;

// Hook PlayLayer::init to mark ready
class $modify(PlayLayerInitHook, PlayLayer) {
    bool init(GJGameLevel* level) {
        g_playLayerReady = false;
        auto result = PlayLayer::init(level, false, false);
        g_playLayerReady = true;
        return result;
    }
};

static void drawGameObjectOverlaysForLayer(CCLayer* layer, CCArray* objects, float cameraRotationDegrees = 0.0f, float camScale = 1.0f) {
    if (!layer || !objects) return;

    // Remove previous overlay nodes
    std::vector<CCNode*> oldNodes;
    for (auto child : CCArrayExt<CCNode*>(layer->getChildren())) {
        if (child->getTag() == 9999) oldNodes.push_back(child);
    }
    for (auto node : oldNodes) node->removeFromParent();

    // Copy objects safely
    std::vector<GameObject*> objectsCopy;
    objectsCopy.reserve(objects->count());
    for (auto obj : CCArrayExt<GameObject*>(objects)) {
        if (obj) objectsCopy.push_back(obj);
    }

    // Get screen center for camera rotation
    CCPoint screenCenter = {
        CCDirector::sharedDirector()->getWinSize().width / 2,
        CCDirector::sharedDirector()->getWinSize().height / 2
    };
    float camRot = -CC_DEGREES_TO_RADIANS(cameraRotationDegrees);

    for (auto obj : objectsCopy) {
        if (!obj || !obj->getParent() || !obj->isVisible()) continue;

        auto opacity = obj->getOpacity();
        if (opacity == 0) continue;

        auto size = obj->getContentSize();

        // Object anchor in world space
        auto anchor = obj->getAnchorPointInPoints();
        CCPoint center = obj->convertToWorldSpace(anchor);

        // Vertices relative to anchor
        CCPoint verts[4] = {
            { 0 - anchor.x, 0 - anchor.y },
            { size.width - anchor.x, 0 - anchor.y },
            { size.width - anchor.x, size.height - anchor.y },
            { 0 - anchor.x, size.height - anchor.y }
        };

        // Apply object rotation around object center
        float objRot = -CC_DEGREES_TO_RADIANS(obj->getRotation());
        for (int i = 0; i < 4; ++i) {
            float dx = verts[i].x;
            float dy = verts[i].y;
            verts[i].x = center.x + dx * cos(objRot) - dy * sin(objRot);
            verts[i].y = center.y + dx * sin(objRot) + dy * cos(objRot);
        }

        // Apply camera rotation around screen center
        for (int i = 0; i < 4; ++i) {
            float dx = verts[i].x - screenCenter.x;
            float dy = verts[i].y - screenCenter.y;
            verts[i].x = screenCenter.x + dx * cos(camRot) - dy * sin(camRot);
            verts[i].y = screenCenter.y + dx * sin(camRot) + dy * cos(camRot);
        }

        // Apply camera scale
        if (camScale != 1.0f) {
            for (int i = 0; i < 4; ++i) {
                verts[i].x = screenCenter.x + (verts[i].x - screenCenter.x) * camScale;
                verts[i].y = screenCenter.y + (verts[i].y - screenCenter.y) * camScale;
            }
        }

        // Color based on object position
        auto cpos = obj->getPosition();
        unsigned char r = ((int)(cpos.x + cpos.y) * 37) % 256;
        unsigned char g = ((int)(cpos.x * 17 + cpos.y * 29)) % 256;
        unsigned char b = ((int)(cpos.x * 23 + cpos.y * 41)) % 256;

        ccColor4F color = {
            r / 255.0f,
            g / 255.0f,
            b / 255.0f,
            opacity / 255.0f
        };

        // Draw node
        auto drawNode = CCDrawNode::create();
        layer->addChild(drawNode, obj->getZOrder(), 9999);
        drawNode->drawPolygon(verts, 4, color, 0, color);
    }
}

static void drawGameObjectOverlays(PlayLayer* layer) {
    if (!layer) return;
    drawGameObjectOverlaysForLayer(layer, layer->m_objects);
}

static void drawGameObjectOverlays(LevelEditorLayer* layer) {
    if (!layer) return;
    drawGameObjectOverlaysForLayer(layer, layer->m_objects);
}

class $modify(LevelEditorOverlayHook, LevelEditorLayer) {
public:
    bool init(GJGameLevel* level, bool dontCreateObjects) {
        if (!LevelEditorLayer::init(level, dontCreateObjects))
            return false;

        log::info("Scheduling overlay updates in LevelEditorLayer...");
        this->schedule(schedule_selector(LevelEditorOverlayHook::updateOverlays), 0.0f);
        return true;
    }

    void updateOverlays(float dt) {
        drawGameObjectOverlays(this);
    }
};

class $modify(PlayLayerOverlayHook, PlayLayer) {
public:
    void updateOverlays(float dt) {
        drawGameObjectOverlays(this);
    }

    void onEnterTransitionDidFinish() {
        PlayLayer::onEnterTransitionDidFinish();
        log::info("Scheduling overlay updates in PlayLayer...");
        this->schedule(schedule_selector(PlayLayerOverlayHook::updateOverlays), 0.0f);
    }
};

$execute {
    log::info("RandomColorSprite mod loaded!");
}