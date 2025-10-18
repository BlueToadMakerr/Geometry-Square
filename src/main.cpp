#include <Geode/Geode.hpp>
#include <Geode/modify/CCSprite.hpp>
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

    // NEW: Hook setTexture to catch dynamic sprites
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

$execute {
    log::info("RandomColorSprite mod loaded!");
}