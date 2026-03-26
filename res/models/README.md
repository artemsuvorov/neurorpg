# Models

This directory stores the **GGUF** model files used by the RPG server to generate immersive role-play dialogues with in-game NPCs.

Because these files are often several gigabytes in size, they are **not** included in the repository. You must obtain them separately.


## Getting a Model

### Recommended Model (Automatic Download)

To automatically download a recommended model, run the appropriate script (follow the instructions in the root `README.md`).

### Manual download

1. Visit [Hugging Face](https://huggingface.co/models?library=gguf&sort=downloads) and search for a GGUF model that suits your needs.
2. Download the `.gguf` file (e.g., `mythomist-7b.Q4_K_M.gguf`).
3. Copy the file into this folder.


## Model Configuration

```
TODO: Add info about model configuration when it is settled (via API, cmd args or hardcoded).
```


## Notes

* Only `.gguf` format models are supported.
* Ensure the model file is large enough to fit in your GPU/CPU memory.
* If you use a different model, you may need to adjust prompt formatting - see the model's documentation.
