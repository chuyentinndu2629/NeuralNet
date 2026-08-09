# NeuralNet
A small neural network. Just a small one, this is just an experiment.

I am trying to create a network of infrastructure inside Vietnam, with the help of a Neural network, I will make it good. Kind of.

### Prequisites
- C++20
- OpenGL 4.6

### Compilation
Because GLAD is retarded on Arch Linux and needs to "tailor" to your current OpenGL version, the glad files are gonna be saved into `glad`.

Therefore, if you compile this on another distro/OS, remember what I said above.

The glad compilation command on Arch Linux is somewhere along the line of:
```bash
glad --api=gl:core=4.6 --out-path=./glad
```

Compiling this project is straightforward. Just roll up CMake, then run:
```bash
cmake -B build
```

And now there should be ***MAGICALLY***, a new folder called `build/` in the repo's directory, in it is the Makefiles for your specified build system. Build with that build system (for instance, `make` for Unix Makefiles, etc), and you should be in the clear.