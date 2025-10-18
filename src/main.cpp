#include <Geode/Geode.hpp>
#include <Geode/modify/CCSprite.hpp>
#include <Geode/modify/CCSpriteFrameCache.hpp>
#include <Geode/modify/CCTextureCache.hpp>
#include <cocos2d.h>
#include <unordered_map>
#include <random>

using namespace geode::prelude;
using namespace cocos2d;

// RNG for random colors
static std::mt19937 rng(std::random_device{}());
static std::uniform_int_distribution<int> dist(0, 255);

// Store already generated random textures
static std::unordered_map<CCTexture2D*, CCTexture2D*> g_randomTextures;

// Create a solid color texture
static CCTexture2D* makeSolidColor(unsigned char r, unsigned char g, unsigned char b) {
    unsigned char pixel[4] = {r, g, b, 255};
    auto img = new CCImage();
    img->initWithImageData(pixel, sizeof(pixel), CCImage::kFmtRawData, 1, 1, 8);
    auto tex = new CCTexture2D();
    tex->initWithImage(img);
    img->release();
    return tex;
}

// Ensure a texture has a random color replacement
static CCTexture2D* getRandomTexture(CCTexture2D* original) {
    if (!original) return nullptr;

    auto it = g_randomTextures.find(original);
    if (it != g_randomTextures.end()) return it->second;

    auto tex = makeSolidColor(dist(rng), dist(rng), dist(rng));
    g_randomTextures[original] = tex;
    return tex;
}

// Hook CCSpriteFrameCache to replace all loaded frames
class $modify(RandomColorSpriteFrameCache, CCSpriteFrameCache) {
public:
    using CCSpriteFrameCache::addSpriteFramesWithFile;

    void addSpriteFramesWithFile(const char* plist, const char* texture_filename) {
        CCSpriteFrameCache::addSpriteFramesWithFile(plist, texture_filename);

        auto dict = CCDictionary::createWithContentsOfFile(plist);
        if (!dict) return;

        auto frames_dict = static_cast<CCDictionary*>(dict->objectForKey("frames"));
        if (!frames_dict) return;

        CCDictElement* element = nullptr;
        CCDICT_FOREACH(frames_dict, element) {
            const char* frame_name = element->getStrKey();
            auto frame = this->spriteFrameByName(frame_name);
            if (!frame) continue;

            auto tex = frame->getTexture();
            auto randomTex = getRandomTexture(tex);
            if (!randomTex) continue;

            CCRect rect = frame->getRect();
            auto new_frame = CCSpriteFrame::createWithTexture(randomTex, rect);
            new_frame->setOffset(frame->getOffset());
            new_frame->setOriginalSize(frame->getOriginalSize());
            new_frame->setRect(rect);

            this->removeSpriteFrameByName(frame_name);
            this->addSpriteFrame(new_frame, frame_name);
        }
    }

    void addSpriteFramesWithFile(const char* plist) {
        CCSpriteFrameCache::addSpriteFramesWithFile(plist);
        addSpriteFramesWithFile(plist, nullptr);
    }
};

// Also hook normal CCSprite init to cover runtime-created sprites
class $modify(RandomColorSprite, CCSprite) {
    bool initWithSpriteFrame(CCSpriteFrame* frame) {
        if (!CCSprite::initWithSpriteFrame(frame)) return false;
        this->setTexture(getRandomTexture(frame->getTexture()));
        return true;
    }

    bool initWithTexture(CCTexture2D* texture, const CCRect& rect) {
        if (!CCSprite::initWithTexture(texture, rect)) return false;
        this->setTexture(getRandomTexture(texture));
        return true;
    }
};

$execute {
    log::info("RandomColorSprite (full coverage) mod loaded!");
}