#include "engine/engine.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

int main()
{
    // true = raster preset (minimal extensions); false = ray tracing preset (needs capable GPU).
    engine::Engine app(true);
    return app.run();
}
