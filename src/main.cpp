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

static void drawGameObjectOverlaysForLayer(CCLayer* layer, CCArray* objects) {
    if (!layer || !objects) return;

    // Remove all previous overlay nodes
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

    for (auto obj : objectsCopy) {
        if (!obj || !obj->getParent() || !obj->isVisible()) continue;

        auto opacity = obj->getOpacity();
        if (opacity == 0) continue;

        auto center = obj->convertToWorldSpace(CCPointZero);

        auto size = obj->getContentSize();
        float sx = obj->getScaleX();
        float sy = obj->getScaleY();

        CCPoint verts[4] = {
            {center.x - size.width*sx/2, center.y - size.height*sy/2},
            {center.x + size.width*sx/2, center.y - size.height*sy/2},
            {center.x + size.width*sx/2, center.y + size.height*sy/2},
            {center.x - size.width*sx/2, center.y + size.height*sy/2}
        };

        // Apply rotation
        float rot = obj->getRotation();
        float rad = -CC_DEGREES_TO_RADIANS(rot);
        for (int i = 0; i < 4; ++i) {
            float dx = verts[i].x - center.x;
            float dy = verts[i].y - center.y;
            verts[i].x = center.x + dx * cos(rad) - dy * sin(rad);
            verts[i].y = center.y + dx * sin(rad) + dy * cos(rad);
        }

        // Color seed based on position
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

        // Create a drawNode per object to respect z-order
        auto drawNode = CCDrawNode::create();
        layer->addChild(drawNode, obj->getZOrder(), 9999); // tag = 9999 to identify overlays
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