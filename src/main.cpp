#include <Geode/Geode.hpp>
#include <Geode/modify/CCTextureCache.hpp>
#include <Geode/modify/CCTexture2D.hpp>
#include <cocos2d.h>

using namespace geode::prelude;
using namespace cocos2d;

class $modify(TextureColorMod, CCTextureCache) {
public:
    CCTexture2D* addImage(const char* path, bool async) {
        auto original = CCTextureCache::addImage(path, async);
        if (!original) return nullptr;

        // Get size
        auto width = original->getPixelsWide();
        auto height = original->getPixelsHigh();

        // Create a render texture to read pixel data
        auto rt = CCRenderTexture::create(width, height);
        if (!rt) return original;

        rt->beginWithClear(0, 0, 0, 0);
        auto spr = CCSprite::createWithTexture(original);
        spr->setAnchorPoint({0, 0});
        spr->setPosition({0, 0});
        spr->visit();
        rt->end();
        rt->retain();

        // Get pixel data
        auto image = rt->newCCImage();
        auto data = image->getData();
        int dataLen = image->getDataLen();
        if (!data || dataLen <= 0) {
            image->release();
            rt->release();
            return original;
        }

        // Compute average color
        long long rSum = 0, gSum = 0, bSum = 0, count = 0;
        for (int i = 0; i < dataLen; i += 4) {
            rSum += data[i];
            gSum += data[i + 1];
            bSum += data[i + 2];
            count++;
        }
        unsigned char avgR = rSum / count;
        unsigned char avgG = gSum / count;
        unsigned char avgB = bSum / count;

        image->release();
        rt->release();

        // Create 1x1 texture of that color
        unsigned char pixel[4] = {avgR, avgG, avgB, 255};
        auto solid = new CCImage();
        solid->initWithRawData(pixel, sizeof(pixel), kFmtRawData, 1, 1, 8);

        auto solidTex = new CCTexture2D();
        solidTex->initWithImage(solid);
        solid->release();

        log::info("Replaced {} with average color ({}, {}, {})", path, avgR, avgG, avgB);

        return solidTex;
    }
};
