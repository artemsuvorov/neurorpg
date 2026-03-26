# Neuro RPG

> This is a work in progress. Contributions and suggestions are welcome!

A role-playing game where NPCs are powered by AI models, enabling immersive, dynamic dialogues.


## Use

Build and compile

```cmd
# Windows
cmake -B build -S . -G "Visual Studio 17 2022"
cmake --build build --config Release -j8
```

It creates two targets executables: for a client and a server.


#### Model Download

This project requires one or more GGUF model files.
The recommended model is **MythoMist-7B** (Q4_K_M).

To download it, run:

```bash
py scripts/download_model.py
```

For other models, use:

```bash
py scripts/download_model.py <URL-or-repo> --filename <filename>
```


## Notes

### CUDA

To enable GPU acceleration, install the CUDA Toolkit and ensure your GPU supports compute capability 6.0 or higher. For dev's machine version `12.x` was used for compatibility reasons.

The build system will automatically detect CUDA if available. For the best performance with limited VRAM, choose a model that fits your GPU's memory (e.g., 7B Q4 models for 6 GB cards).
