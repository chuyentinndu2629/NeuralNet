# NeuralNet
A small neural network. Just a small one, this is just an experiment.

I am trying to create a network of infrastructure inside Vietnam, with the help of a Neural network, I will make it good. Kind of.

### Prequisites
- C++20
- Godot 4.7.x

### Libraries
- [CPR](https://github.com/libcpr/cpr)
- [nlohmann/json](https://github.com/nlohmann/json)
- [ASIO](https://github.com/chriskohlhoff/asio/)
- [libwebsocket](https://libwebsockets.org/)

### Compilation

Compiling this project is straightforward. Just roll up CMake, then run:
```bash
cmake -B build
```

And now there should be ***MAGICALLY***, a new folder called `build/` in the repo's directory, in it is the Makefiles for your specified build system. Build with that build system (for instance, `make` for Unix Makefiles, etc), and you should be in the clear.