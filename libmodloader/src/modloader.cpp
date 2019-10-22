#include <libmain.hpp>
#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <linux/limits.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <errno.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "jit/jit.hpp"
#include "log.hpp"

#include <sys/mman.h>

using namespace modloader;


#define MOD_PATH_FMT "/sdcard/Android/data/%s/files/mods/"
#define MOD_TEMP_PATH_FMT "/data/data/%s/cache/curmod.so"

char *modPath;
char *modTempPath;

char *trimWhitespace(char *str)
{
  char *end;
  while(isspace((unsigned char)*str)) str++;
  if(*str == 0)
    return str;

  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  end[1] = '\0';

  return str;
}

const int setDataDirs()
{
    FILE *cmdline = fopen("/proc/self/cmdline", "r");
    if (cmdline) {
        //not sure what the actual max is, but path_max should cover it
        char application_id[PATH_MAX] = {0};
        fread(application_id, sizeof(application_id), 1, cmdline);
        fclose(cmdline);
        trimWhitespace(application_id);
        modTempPath = (char*)malloc(PATH_MAX);
        modPath = (char*)malloc(PATH_MAX);
        sprintf(modPath, MOD_PATH_FMT, application_id);
        sprintf(modTempPath, MOD_TEMP_PATH_FMT, application_id);
        return 0;
    } else
    {
        return -1;
    }    
}

int mkpath(char* file_path, mode_t mode) {
    for (char* p = strchr(file_path + 1, '/'); p; p = strchr(p + 1, '/')) {
        *p = '\0';
        if (mkdir(file_path, mode) == -1) {
            if (errno != EEXIST) {
                *p = '/';
                return -1;
            }
        }
        *p = '/';
    }
    return 0;
}

extern "C" void modloader_preload() noexcept {
    log(ANDROID_LOG_VERBOSE, "modloader_preload called (should be really early)");
}

int load_mods()
{
    __android_log_write(ANDROID_LOG_INFO, "QuestHook", "Loading mods!");
    if (setDataDirs() != 0)
    {
         __android_log_write(ANDROID_LOG_ERROR, "QuestHook", "Unable to determine data directories.");
        return -1;
    }
    if (mkpath(modPath, 0) != 0)
    {
        __android_log_print(ANDROID_LOG_ERROR, "QuestHook", "Unable to access or create mod path at '%s'", modPath);
        return -1;
    }
    if (mkpath(modTempPath, 0) != 0)
    {
        __android_log_print(ANDROID_LOG_ERROR, "QuestHook", "Unable to access or create mod temporary path at '%s'", modTempPath);
        return -1;
    }

    struct dirent *dp;
    DIR *dir = opendir(modPath);

    while ((dp = readdir(dir)) != NULL)
    {
        if (strlen(dp->d_name) > 3 && !strcmp(dp->d_name + strlen(dp->d_name) - 3, ".so"))
        {
            char full_path[PATH_MAX];
            strcpy(full_path, modPath);
            strcat(full_path, dp->d_name);
            __android_log_print(ANDROID_LOG_INFO, "QuestHook", "Loading mod: %s", full_path);
            int infile = open(full_path, O_RDONLY);
            off_t filesize = lseek(infile, 0, SEEK_END);
            lseek(infile, 0, SEEK_SET);
            unlink(modTempPath);
            int outfile = open(modTempPath, O_CREAT | O_WRONLY);
            sendfile(outfile, infile, 0, filesize);
            close(infile);
            close(outfile);
            chmod(modTempPath, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP);
            dlopen(modTempPath, RTLD_NOW);
        }
    }
    closedir(dir);
    return 0;
}

extern "C" JNINativeInterface modloader_main(JavaVM* vm, JNIEnv* env, std::string_view loadSrc) noexcept {
    logf(ANDROID_LOG_VERBOSE, "modloader_main called with vm: 0x%p, env: 0x%p, loadSrc: %s", vm, env, loadSrc.data());

    auto iface = jni::interface::make_passthrough_interface<JNINativeInterface>(&env->functions);

    __android_log_write(ANDROID_LOG_INFO, "QuestHook", "Welcome!");
    if (load_mods() != 0)
    {
        __android_log_write(ANDROID_LOG_ERROR, "QuestHook", "QuestHook failed to initialize, mods will not load.");
    }
    else
    {
        __android_log_write(ANDROID_LOG_INFO, "QuestHook", "Done loading mods!");
    }

    return iface;
}

extern "C" void modloader_accept_unity_handle(void* uhandle) noexcept {
    logf(ANDROID_LOG_VERBOSE, "modloader_accept_unity_handle called with uhandle: 0x%p", uhandle);
}

CHECK_MODLOADER_PRELOAD;
CHECK_MODLOADER_MAIN;
CHECK_MODLOADER_ACCEPT_UNITY_HANDLE;