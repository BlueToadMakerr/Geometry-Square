#include <Geode/Geode.hpp>
#include <Geode/modify/CCTextureCache.hpp>
#include <cocos2d.h>

using namespace geode::prelude;
using namespace cocos2d;

class $modify(TextureColorMod, CCTextureCache) {
public:
    CCTexture2D* addImage(const char* path, bool async) {
        auto original = CCTextureCache::addImage(path, async);
        if (!original) return nullptr;

        auto width = original->getPixelsWide();
        auto height = original->getPixelsHigh();

        auto rt = CCRenderTexture::create(width, height);
        if (!rt) return original;

        rt->beginWithClear(0, 0, 0, 0);
        auto spr = CCSprite::createWithTexture(original);
        spr->setAnchorPoint({0, 0});
        spr->setPosition({0, 0});
        spr->visit();
        rt->end();
        rt->retain();

        auto image = rt->newCCImage();
        auto data = image->getData();
        int dataLen = image->getDataLen();
        if (!data || dataLen <= 0) {
            image->release();
            rt->release();
            return original;
        }

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

        // Instead of initWithRawData (not available), we use initWithImageData
        unsigned char pixel[4] = {avgR, avgG, avgB, 255};

        auto solid = new CCImage();
        solid->initWithImageData(pixel, sizeof(pixel), CCImage::kFmtRawData, 1, 1, 8);

        auto solidTex = new CCTexture2D();
        solidTex->initWithImage(solid);
        solid->release();

        log::info("Replaced {} with average color ({}, {}, {})", path, avgR, avgG, avgB);

        return solidTex;
    }
};