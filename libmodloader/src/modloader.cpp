#include <libmain.hpp>
#include "log.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include <sys/mman.h>

// there is *not* a good way to get the name lmao
std::string get_jni_class_name(JNIEnv* env, jclass klass) {
    

    return {};
}

extern "C" JNINativeInterface modloader_main(JavaVM* vm, JNIEnv* env, std::string_view loadSrc) noexcept {
    logf(ANDROID_LOG_VERBOSE, "modloader_main called with vm: 0x%p, env: 0x%p, loadSrc: %s", vm, env, loadSrc.data());

    auto iface = jni::interface::make_passthrough_interface<JNINativeInterface>(&env->functions);

    iface.RegisterNatives = [](JNIEnv* env, jclass klass, JNINativeMethod const* methods_ptr, jint count) {
        using namespace jni::interface;

        std::span methods {methods_ptr, count};
        auto clsname = get_jni_class_name(env, klass);

        for (auto method : methods) {
            logf(ANDROID_LOG_VERBOSE, "Unity registering native on %s: %s %s @ 0x%p", 
                    clsname.data(), method.name, method.signature, method.fnPtr);
        }

        return invoke_original(env, &JNINativeInterface::RegisterNatives, klass, methods_ptr, count);
    };

    return iface;
}

extern "C" void modloader_accept_unity_handle(void* uhandle) noexcept {
    logf(ANDROID_LOG_VERBOSE, "modloader_accept_unity_handle called with uhandle: 0x%p", uhandle);
}

CHECK_MODLOADER_MAIN;
CHECK_MODLOADER_ACCEPT_UNITY_HANDLE;