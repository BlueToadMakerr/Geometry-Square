#include <Geode/Geode.hpp>
#include <Geode/modify/CCSprite.hpp>
#include <Geode/modify/PlayLayer.hpp>
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

static void drawGameObjectOverlays(PlayLayer* layer) {
    if (!layer) return;

    // Remove any previous overlay node if it exists
    if (auto oldNode = layer->getChildByTag(9999)) {
        oldNode->removeFromParent();
    }

    // Create new draw node
    auto drawNode = CCDrawNode::create();
    layer->addChild(drawNode, 9999, 9999); // Draw on top of everything

    // Take a snapshot of m_objects to avoid iterator invalidation
    std::vector<GameObject*> objectsCopy;
    objectsCopy.reserve(layer->m_objects->count());
    for (auto obj : CCArrayExt<GameObject*>(layer->m_objects)) {
        if (obj) objectsCopy.push_back(obj);
    }

    for (auto obj : objectsCopy) {
        if (!obj) continue;
        if (!obj->getParent()) continue; // skip if removed

        auto pos = obj->getPosition();

        // Get the camera's position and offset the object by that
        auto parent = obj->getParent();
        while (parent && parent != layer) {
            pos.x += parent->getPositionX();
            pos.y += parent->getPositionY();
            parent = parent->getParent();
        }

        auto size = obj->getContentSize();
        float sx = obj->getScaleX();
        float sy = obj->getScaleY();

        CCPoint origin(pos.x - (size.width * sx) / 2, pos.y - (size.height * sy) / 2);
        CCPoint dest(pos.x + (size.width * sx) / 2, pos.y + (size.height * sy) / 2);

        auto cpos = obj->getPosition();

        // Red channel
        unsigned char r = ((int)(cpos.x + pos.y) * 37) % 256;

        // Green channel
        unsigned char g = ((int)(cpos.x * 17 + pos.y * 29)) % 256;

        // Blue channel
        unsigned char b = ((int)(cpos.x * 23 + pos.y * 41)) % 256;

        ccColor4F color = {
            r / 255.0f,
            g / 255.0f,
            b / 255.0f,
            0.4f // semi-transparent
        };

        CCPoint verts[4] = {
            origin,
            {dest.x, origin.y},
            dest,
            {origin.x, dest.y}
        };
        drawNode->drawPolygon(verts, 4, color, 0, color);
    }
}

class $modify(PlayLayerOverlayHook, PlayLayer) {
public:
    void updateOverlays(float dt) {
        log::info("updateOverlays tick");
        drawGameObjectOverlays(this);
    }

    void onEnterTransitionDidFinish() {
        PlayLayer::onEnterTransitionDidFinish();
        log::info("Scheduling overlay updates...");
        this->schedule(schedule_selector(PlayLayerOverlayHook::updateOverlays), 1.0f / 30.0f);
    }
};

$execute {
    log::info("RandomColorSprite mod loaded!");
}