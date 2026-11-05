/*
 * Flashlight HAL implementation for Motorola Edge 50 Pro (eqe)
 * Controls the camera flash LED as a flashlight/torch
 */

#define LOG_TAG "FlashlightHAL"

#include <android/hardware/light/2.0/ILight.h>
#include <android/hardware/light/2.0/types.h>
#include <hidl/HidlSupport.h>
#include <hidl/HidlTransportSupport.h>
#include <utils/StrongPointer.h>
#include <log/log.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

using android::hardware::light::V2_0::ILight;
using android::hardware::light::V2_0::LightState;
using android::hardware::light::V2_0::Flash;
using android::hardware::light::V2_0::Status;
using android::hardware::light::V2_0::Type;
using android::hardware::Return;
using android::hardware::Void;
using android::hardware::hidl_vec;
using android::sp;

#define FLASHLIGHT_SYSFS_PATH "/sys/class/leds/led:torch_0/brightness"
#define FLASHLIGHT_MAX_BRIGHTNESS_PATH "/sys/class/leds/led:torch_0/max_brightness"

class Flashlight : public ILight {
public:
    Flashlight() {
        mMaxBrightness = readMaxBrightness();
        ALOGI("Flashlight HAL initialized, max brightness: %d", mMaxBrightness);
    }

    Return<Status> setLight(Type type, const LightState& state) override {
        if (type != Type::FLASHLIGHT) {
            ALOGW("Unsupported light type: %d", static_cast<int>(type));
            return Status::LIGHT_NOT_SUPPORTED;
        }

        uint32_t brightness = state.color & 0xFF;
        uint32_t scaledBrightness = (brightness * mMaxBrightness) / 255;

        ALOGD("Setting flashlight brightness: %d (scaled: %d)", brightness, scaledBrightness);

        int fd = open(FLASHLIGHT_SYSFS_PATH, O_WRONLY);
        if (fd < 0) {
            ALOGE("Failed to open flashlight sysfs: %s", strerror(errno));
            return Status::INTERNAL_ERROR;
        }

        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%d", scaledBrightness);
        if (write(fd, buf, len) < 0) {
            ALOGE("Failed to write flashlight brightness: %s", strerror(errno));
            close(fd);
            return Status::INTERNAL_ERROR;
        }

        close(fd);
        return Status::SUCCESS;
    }

    Return<void> getSupportedTypes(getSupportedTypes_cb _hidl_cb) override {
        hidl_vec<Type> types;
        types.setToExternal(const_cast<Type*>(&kSupportedTypes[0]), 1);
        _hidl_cb(types);
        return Void();
    }

private:
    uint32_t mMaxBrightness;
    static constexpr Type kSupportedTypes[] = {Type::FLASHLIGHT};

    uint32_t readMaxBrightness() {
        int fd = open(FLASHLIGHT_MAX_BRIGHTNESS_PATH, O_RDONLY);
        if (fd < 0) {
            ALOGW("Failed to read max brightness, defaulting to 255: %s", strerror(errno));
            return 255;
        }

        char buf[16];
        ssize_t len = read(fd, buf, sizeof(buf) - 1);
        close(fd);

        if (len > 0) {
            buf[len] = '\0';
            return static_cast<uint32_t>(atoi(buf));
        }
        return 255;
    }
};

extern "C" ILight* HIDL_FETCH_ILight(const char* /* name */) {
    return new Flashlight();
}