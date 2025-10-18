#include <Geode/Geode.hpp>
#include <Geode/modify/CCSpriteFrameCache.hpp>
#include <Geode/modify/CCTextureCache.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <cocos2d.h>
#include <unordered_map>
#include <random>

using namespace geode::prelude;
using namespace cocos2d;

// ------------------------------
// Random color generator & cache
// ------------------------------
static std::mt19937 rng(std::random_device{}());
static std::unordered_map<CCTexture2D*, CCTexture2D*> g_randomTextureCache;

static CCTexture2D* makeRandomColorTexture(CCTexture2D* original) {
    // Check if we already have a random texture for this one
    auto it = g_randomTextureCache.find(original);
    if (it != g_randomTextureCache.end()) {
        return it->second;
    }

    std::uniform_int_distribution<int> dist(0, 255);
    unsigned char r = dist(rng);
    unsigned char g = dist(rng);
    unsigned char b = dist(rng);

    unsigned char pixel[4] = {r, g, b, 255};
    auto img = new CCImage();
    img->initWithImageData(pixel, sizeof(pixel), CCImage::kFmtRawData, 1, 1, 8);
    auto tex = new CCTexture2D();
    tex->initWithImage(img);
    img->release();

    g_randomTextureCache[original] = tex;
    return tex;
}

// ------------------------------
// Helper function to replace frames
// ------------------------------
static void replaceFrames(CCSpriteFrameCache* cache, const char* plist) {
    auto dict = CCDictionary::createWithContentsOfFile(plist);
    if (!dict) return;

    auto frames_dict = static_cast<CCDictionary*>(dict->objectForKey("frames"));
    if (!frames_dict) return;

    CCDictElement* element = nullptr;
    CCDICT_FOREACH(frames_dict, element) {
        const char* frame_name = element->getStrKey();
        auto frame = cache->spriteFrameByName(frame_name);
        if (!frame) continue;

        auto origTex = frame->getTexture();
        auto randTex = makeRandomColorTexture(origTex);
        if (!randTex) continue;

        CCRect rect = frame->getRect();
        auto new_frame = CCSpriteFrame::createWithTexture(randTex, rect);
        new_frame->setOffset(frame->getOffset());
        new_frame->setOriginalSize(frame->getOriginalSize());
        new_frame->setRect(rect);

        cache->removeSpriteFrameByName(frame_name);
        cache->addSpriteFrame(new_frame, frame_name);
    }
}

// ------------------------------
// Modifications
// ------------------------------
struct RandomSpriteCache : Modify<RandomSpriteCache, CCSpriteFrameCache> {
    using CCSpriteFrameCache::addSpriteFramesWithFile;

    // Override single-argument version
    void addSpriteFramesWithFile(const char* plist) {
        CCSpriteFrameCache::addSpriteFramesWithFile(plist);
        replaceFrames(this, plist);
    }

    // Override two-argument version
    void addSpriteFramesWithFile(const char* plist, const char* texture_filename) {
        CCSpriteFrameCache::addSpriteFramesWithFile(plist, texture_filename);
        replaceFrames(this, plist);
    }
};

// Optional: play a sound in the menu (like the Lemon mod)
struct RandomMenuLayer : Modify<RandomMenuLayer, MenuLayer> {
    bool init() {
        if (!MenuLayer::init())
            return false;

        return true;
    }
};