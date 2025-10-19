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

        auto size = obj->getContentSize();

        // World position of the object
        CCPoint center = obj->convertToWorldSpace(CCPointZero);

        // Get batch layer (2 levels above)
        auto batchLayer = obj->getParent()->getParent();
        if (!batchLayer) batchLayer = obj->getParent(); // fallback

        // Rotation and scale
        float objRot = -CC_DEGREES_TO_RADIANS(obj->getRotation());
        float batchRot = -CC_DEGREES_TO_RADIANS(batchLayer->getRotation());

        float sx = obj->getScaleX() + (batchLayer->getScaleX() - 1.0f);
        float sy = obj->getScaleY() + (batchLayer->getScaleY() - 1.0f);

        // Vertices relative to center
        CCPoint verts[4] = {
            { -size.width * sx / 2, -size.height * sy / 2 },
            {  size.width * sx / 2, -size.height * sy / 2 },
            {  size.width * sx / 2,  size.height * sy / 2 },
            { -size.width * sx / 2,  size.height * sy / 2 }
        };

        // Apply object rotation
        for (int i = 0; i < 4; ++i) {
            float dx = verts[i].x;
            float dy = verts[i].y;
            verts[i].x = dx * cos(objRot) - dy * sin(objRot);
            verts[i].y = dx * sin(objRot) + dy * cos(objRot);
        }

        // Apply batch layer rotation and translate to world center
        for (int i = 0; i < 4; ++i) {
            float dx = verts[i].x;
            float dy = verts[i].y;
            verts[i].x = center.x + dx * cos(batchRot) - dy * sin(batchRot);
            verts[i].y = center.y + dx * sin(batchRot) + dy * cos(batchRot);
        }

        // Color seed based on object position
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
        layer->addChild(drawNode, obj->getZOrder(), 9999);
        drawNode->drawPolygon(verts, 4, color, 0, color);
    }
}

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