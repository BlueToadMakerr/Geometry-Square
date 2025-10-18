#include <Geode/Geode.hpp>
#include <Geode/modify/CCSprite.hpp>
#include <Geode/modify/CCSpriteFrameCache.hpp>
#include <Geode/modify/CCTextureCache.hpp>
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
    unsigned char pixel[4] = {r, g, b, 255};
    auto img = new CCImage();
    img->initWithImageData(pixel, sizeof(pixel), CCImage::kFmtRawData, 1, 1, 8);
    auto tex = new CCTexture2D();
    tex->initWithImage(img);
    img->release();
    return tex;
}

// Hook all CCSprites
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
};

// Hook CCSpriteFrameCache for all sprite sheets
class $modify(RandomColorSpriteFrameCache, CCSpriteFrameCache) {
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

            auto orig_tex = frame->getTexture();
            if (g_textureColors.find(orig_tex) == g_textureColors.end()) {
                g_textureColors[orig_tex] = { (GLubyte)dist(rng), (GLubyte)dist(rng), (GLubyte)dist(rng), 255 };
            }
            auto color = g_textureColors[orig_tex];
            auto new_tex = makeSolidColor(color.r, color.g, color.b);

            auto rect = frame->getRect();
            auto offset = frame->getOffset();
            auto orig_size = frame->getOriginalSize();

            auto new_frame = CCSpriteFrame::createWithTexture(new_tex, rect);
            new_frame->setOffset(offset);
            new_frame->setOriginalSize(orig_size);
            new_frame->setRect(rect);

            this->removeSpriteFrameByName(frame_name);
            this->addSpriteFrame(new_frame, frame_name);
        }
    }

    void addSpriteFramesWithFile(const char* plist) {
        addSpriteFramesWithFile(plist, nullptr);
    }
};

// Hook CCTextureCache to replace any dynamically loaded texture
class $modify(RandomColorTextureCache, CCTextureCache) {
    using CCTextureCache::addImage;

    CCTexture2D* addImage(const char* path, bool p1) {
        auto orig = CCTextureCache::addImage(path, p1);
        if (!orig) return orig;

        if (g_textureColors.find(orig) == g_textureColors.end()) {
            g_textureColors[orig] = { (GLubyte)dist(rng), (GLubyte)dist(rng), (GLubyte)dist(rng), 255 };
        }
        auto color = g_textureColors[orig];
        return makeSolidColor(color.r, color.g, color.b);
    }
};

$execute {
    log::info("RandomColor mod loaded: every sprite will now be a cached colored square!");
}