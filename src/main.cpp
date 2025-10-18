#include <Geode/Geode.hpp>
#include <Geode/modify/CCSprite.hpp>
#include <cocos2d.h>
#include <queue>
#include <random>

using namespace geode::prelude;
using namespace cocos2d;

struct SpriteTask {
    CCSprite* sprite;
    CCTexture2D* texture;
    CCRect rect;
};

static std::queue<SpriteTask> g_queue;

static std::mt19937 rng(std::random_device{}());

static CCTexture2D* makeSolidColor(unsigned char r, unsigned char g, unsigned char b) {
    unsigned char pixel[4] = {r, g, b, 255};
    auto img = new CCImage();
    img->initWithImageData(pixel, sizeof(pixel), CCImage::kFmtRawData, 1, 1, 8);
    auto tex = new CCTexture2D();
    tex->initWithImage(img);
    img->release();
    return tex;
}

// Worker that processes a few sprites per frame
void processQueue(float) {
    int count = 0;
    while (!g_queue.empty() && count < 5) {
        auto task = g_queue.front();
        g_queue.pop();
        count++;

        auto tex = task.texture;
        auto rect = task.rect;

        auto rt = CCRenderTexture::create(rect.size.width, rect.size.height);
        if (!rt) continue;
        rt->beginWithClear(0, 0, 0, 0);
        auto spr = CCSprite::createWithTexture(tex, rect);
        spr->setAnchorPoint({0, 0});
        spr->setPosition({0, 0});
        spr->visit();
        rt->end();

        auto img = rt->newCCImage();
        unsigned char* data = img->getData();
        int width = img->getWidth();
        int height = img->getHeight();
        if (!data || width <= 0 || height <= 0) {
            img->release();
            continue;
        }

        std::uniform_int_distribution<int> randX(0, width - 1);
        std::uniform_int_distribution<int> randY(0, height - 1);

        long long rSum = 0, gSum = 0, bSum = 0;
        const int sampleCount = 10;

        for (int i = 0; i < sampleCount; i++) {
            int x = randX(rng);
            int y = randY(rng);
            int idx = (y * width + x) * 4;
            rSum += data[idx];
            gSum += data[idx + 1];
            bSum += data[idx + 2];
        }

        unsigned char r = rSum / sampleCount;
        unsigned char g = gSum / sampleCount;
        unsigned char b = bSum / sampleCount;

        img->release();

        auto solid = makeSolidColor(r, g, b);
        task.sprite->setTexture(solid);
    }
}

// Hook for all sprites
class $modify(AverageColorSprite, CCSprite) {
    bool initWithSpriteFrame(CCSpriteFrame* frame) {
        if (!CCSprite::initWithSpriteFrame(frame))
            return false;

        if (frame && frame->getTexture())
            g_queue.push({ this, frame->getTexture(), frame->getRect() });

        return true;
    }

    bool initWithTexture(CCTexture2D* texture, const CCRect& rect) {
        if (!CCSprite::initWithTexture(texture, rect))
            return false;

        if (texture)
            g_queue.push({ this, texture, rect });

        return true;
    }
};

// Startup logic
$execute {
    CCDirector::sharedDirector()->getScheduler()->scheduleSelector(
        schedule_selector(processQueue), nullptr, 0.05f, false);
}