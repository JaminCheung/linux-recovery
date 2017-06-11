#include <unistd.h>

#include <utils/log.h>
#include <utils/common.h>
#include <input/input_manager.h>

#define LOG_TAG "test_input_manage"

static struct input_manager* input_manager;

static void input_event_listener(struct input_manager* this,
        struct input_event* event) {
    this->dump_event(this, event);
}

int main(int argc, char* argv[]) {
    int error = 0;

    input_manager = _new(struct input_manager, input_manager);

    error = input_manager->init(input_manager);
    if (error < 0) {
        LOGE("Failed to init input manager\n");
        return -1;
    }

    error = input_manager->start(input_manager);
    if (error < 0) {
        LOGE("Failed to start input manager\n");
        return -1;
    }

    input_manager->register_event_listener(input_manager, input_event_listener);

    while (1)
        sleep(1000);

    error = input_manager->stop(input_manager);
    if (error < 0) {
        LOGE("Failed to stop input manager\n");
        return -1;
    }

    error = input_manager->deinit(input_manager);
    if (error < 0) {
        LOGE("Failed to deinit input manager\n");
        return -1;
    }

    return 0;
}
