#include <memory>

#include "app.h"
#include "platform.h"

int main() {
    App app(createPlatform());
    return app.run();
}
