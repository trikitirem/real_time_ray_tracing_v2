#include "engine/engine.hpp"

int main()
{
    // true = raster preset (minimal extensions); false = ray tracing preset (needs capable GPU).
    engine::Engine app(true);
    return app.run();
}
