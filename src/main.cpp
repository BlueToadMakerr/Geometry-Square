#include <Geode/Geode.hpp>
#include <Geode/modify/CCSprite.hpp>
#include <cocos2d.h>
#include <queue>
#include <random>

using namespace geode::prelude;
using namespace cocos2d;

// ---------------------
// Sprite task queue
// ---------------------
struct SpriteTask {
    CCSprite* sprite;
    CCTexture2D* texture;
    CCRect rect;
};

static std::queue<SpriteTask> g_queue;
static std::mt19937 rng(std::random_device{}());

// ---------------------
// Helper to make a 1x1 solid color texture
// ---------------------
static CCTexture2D* makeSolidColor(unsigned char r, unsigned char g, unsigned char b) {
    unsigned char pixel[4] = {r, g, b, 255};
    auto img = new CCImage();
    img->initWithImageData(pixel, sizeof(pixel), CCImage::kFmtRawData, 1, 1, 8);
    auto tex = new CCTexture2D();
    tex->initWithImage(img);
    img->release();
    return tex;
}

// ---------------------
// Process queue worker
// ---------------------
void processQueue(float) {
    int count = 0;
    while (!g_queue.empty() && count < 5) { // limit per frame
        auto task = g_queue.front();
        g_queue.pop();
        count++;

        if (!task.sprite || !task.texture) {
            log::warn("Invalid sprite or texture, skipping");
            continue;
        }

        log::info("Processing sprite %p with texture %p, rect = (%f, %f, %f, %f)",
                  task.sprite, task.texture,
                  task.rect.origin.x, task.rect.origin.y,
                  task.rect.size.width, task.rect.size.height);

        auto rt = CCRenderTexture::create(task.rect.size.width, task.rect.size.height);
        if (!rt) {
            log::warn("Failed to create render texture");
            continue;
        }

        rt->beginWithClear(0, 0, 0, 0);
        auto spr = CCSprite::createWithTexture(task.texture, task.rect);
        spr->setAnchorPoint({0, 0});
        spr->setPosition({0, 0});
        spr->visit();
        rt->end();

        auto img = rt->newCCImage();
        if (!img) {
            log::warn("Failed to get CCImage from render texture");
            continue;
        }

        unsigned char* data = img->getData();
        int width = img->getWidth();
        int height = img->getHeight();
        int len = img->getDataLen();
        if (!data || width <= 0 || height <= 0 || len <= 0) {
            log::warn("No data in CCImage: width=%d, height=%d, len=%d", width, height, len);
            img->release();
            continue;
        }

        // Sample 10 random pixels
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

        log::info("Applied random average color (%d, %d, %d) to sprite %p",
                  (int)r, (int)g, (int)b, task.sprite);
    }
}

// ---------------------
// Hook all CCSprites to queue them
// ---------------------
class $modify(AverageColorSprite, CCSprite) {
    bool initWithSpriteFrame(CCSpriteFrame* frame) {
        if (!CCSprite::initWithSpriteFrame(frame))
            return false;

        if (frame && frame->getTexture()) {
            g_queue.push({ this, frame->getTexture(), frame->getRect() });
            log::info("Queued sprite %p from initWithSpriteFrame", this);
        }

        return true;
    }

    bool initWithTexture(CCTexture2D* texture, const CCRect& rect) {
        if (!CCSprite::initWithTexture(texture, rect))
            return false;

        if (texture) {
            g_queue.push({ this, texture, rect });
            log::info("Queued sprite %p from initWithTexture", this);
        }

        return true;
    }
};

// ---------------------
// Helper node to tick processQueue
// ---------------------
class ProcessNode : public CCNode {
public:
    void updateProcess(float dt) {
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
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

// ---------------------
// Execute on mod load
// ---------------------
$execute {
    auto node = ProcessNode::create();
    CCDirector::sharedDirector()->getRunningScene()->addChild(node);
    log::info("ProcessNode added to scene, sprite queue will start processing.");
}