#include "console.h"
#include "renderer.h"
#include <strings.h>

Console console(false);

int main(int argc, char *argv[]) {
    // Parse the arguments at runtime
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);

        if (arg == "--help" || arg == "-h") {
            console.print("There are a multitude of different arguments you can use with this little tool.\nThat includes:\n-h / --help : Display this help screen\n-v / --verbose : Display verbose logging\n");

            return 0; // Just displaying the help screen
        } 
        
        else if (arg == "--verbose" || arg == "-v") console.setVerbosity(true);

        else {
            // This is not recognized.
            console.log(arg + " is not recognized as a valid argument. Use the '--help' argument to list all possible arguments.", DEBUG_FAIL, true);
        }
    }

    console.print("Welcome to [bi]NeuralNet[/]!\n");

    Renderer renderer(960, console);

    if (!renderer.init()) return -1;

    while (!renderer.shouldClose()) {
        renderer.update();
    }

    // ~Renderer destructor will automatically run here.
    return 0;
}