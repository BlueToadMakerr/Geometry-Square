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

// Draw overlays on all GameObjects
static void drawGameObjectOverlays(PlayLayer* layer) {
    if (!layer || g_overlayDrawn) return;
    g_overlayDrawn = true;
    log::info("We are in!");

    auto drawNode = CCDrawNode::create();
    layer->addChild(drawNode, 9999); // Draw on top of everything

    for (auto obj : CCArrayExt<GameObject*>(layer->m_objects)) {
        log::info("Object found");
        if (!obj) continue;
        log::info("Object real!");
        auto pos = obj->getPosition();
        auto size = obj->getContentSize();
        float sx = obj->getScaleX();
        float sy = obj->getScaleY();

        CCPoint origin(pos.x - (size.width * sx) / 2, pos.y - (size.height * sy) / 2);
        CCPoint dest(pos.x + (size.width * sx) / 2, pos.y + (size.height * sy) / 2);

        ccColor4F color = {
            dist(rng) / 255.0f,
            dist(rng) / 255.0f,
            dist(rng) / 255.0f,
            0.4f // semi-transparent
        };

        CCPoint verts[4] = {origin, {dest.x, origin.y}, dest, {origin.x, dest.y}};
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