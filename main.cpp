#include "engine/engine.hpp"

int main()
{
    // true = raster preset (minimal extensions); false = ray tracing preset (needs capable GPU).
    engine::Engine app(false);
    return app.run();
}
