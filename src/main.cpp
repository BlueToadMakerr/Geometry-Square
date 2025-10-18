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
    log::info("makeSolidColor called with r=%d g=%d b=%d", (int)r, (int)g, (int)b);
    unsigned char pixel[4] = {r, g, b, 255};
    auto img = new CCImage();
    img->initWithImageData(pixel, sizeof(pixel), CCImage::kFmtRawData, 1, 1, 8);
    auto tex = new CCTexture2D();
    tex->initWithImage(img);
    img->release();
    log::info("Solid color texture created");
    return tex;
}

void processQueue(float) {
    log::info("processQueue called, queue size=%zu", g_queue.size());
    int count = 0;
    while (!g_queue.empty() && count < 5) {
        auto task = g_queue.front();
        g_queue.pop();
        count++;
        log::info("Processing sprite %llu, texture %llu", (uintptr_t)task.sprite, (uintptr_t)task.texture);

        auto tex = task.texture;
        auto rect = task.rect;

        auto rt = CCRenderTexture::create(rect.size.width, rect.size.height);
        if (!rt) {
            log::warn("Failed to create render texture");
            continue;
        }
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
            log::warn("Invalid image data, skipping");
            img->release();
            continue;
        }
        log::info("Image data ready, width=%d height=%d", width, height);

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
            log::info("Sample %d: x=%d y=%d r=%d g=%d b=%d", i, x, y, data[idx], data[idx + 1], data[idx + 2]);
        }

        unsigned char r = rSum / sampleCount;
        unsigned char g = gSum / sampleCount;
        unsigned char b = bSum / sampleCount;
        log::info("Average color: r=%d g=%d b=%d", (int)r, (int)g, (int)b);

        img->release();

        auto solid = makeSolidColor(r, g, b);
        task.sprite->setTexture(solid);
        log::info("Applied solid color to sprite %llu", (uintptr_t)task.sprite);
    }
}

class $modify(AverageColorSprite, CCSprite) {
    bool initWithSpriteFrame(CCSpriteFrame* frame) {
        bool result = CCSprite::initWithSpriteFrame(frame);
        log::info("initWithSpriteFrame called on %llu, result=%d", (uintptr_t)this, result);
        if (frame && frame->getTexture()) {
            g_queue.push({ this, frame->getTexture(), frame->getRect() });
            log::info("Queued sprite %llu from initWithSpriteFrame", (uintptr_t)this);
        }
        return result;
    }

    bool initWithTexture(CCTexture2D* texture, const CCRect& rect) {
        bool result = CCSprite::initWithTexture(texture, rect);
        log::info("initWithTexture called on %llu, result=%d", (uintptr_t)this, result);
        if (texture) {
            g_queue.push({ this, texture, rect });
            log::info("Queued sprite %llu from initWithTexture", (uintptr_t)this);
        }
        return result;
    }
};

class ProcessNode : public CCNode {
public:
    void updateProcess(float dt) {
        log::info("updateProcess called");
        processQueue(dt);
    }

    static ProcessNode* create() {
        auto ret = new ProcessNode();
        if (ret && ret->init()) {
            ret->autorelease();
            CCDirector::sharedDirector()->getScheduler()->scheduleSelector(
                schedule_selector(ProcessNode::updateProcess),
                ret, 0.05f, false
            );
            log::info("ProcessNode scheduled");
            return ret;
        }
        CC_SAFE_DELETE(ret);
        log::warn("Failed to create ProcessNode");
        return nullptr;
    }
};

$execute {
    auto node = ProcessNode::create();
    CCDirector::sharedDirector()->getRunningScene()->addChild(node);
    log::info("ProcessNode added to scene");
}