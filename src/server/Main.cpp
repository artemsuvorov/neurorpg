#include "llama.h"

#include <iostream>
#include <string>
#include <vector>


int main(int argc, char** argv)
{
	// --- Configuration (adjust as needed) ---
	std::string model_path = "res/models/mythomax-l2-13b.Q4_K_M.gguf";  // path to your model
	int n_gpu_layers = 20;                                              // offload all layers to GPU if available
	int n_predict = 128;                                                // max tokens to generate
	// ----------------------------------------

	// Initialize llama backend
	llama_backend_init();

	// // Set a no-op logging callback to silence all llama.cpp output
	// llama_log_set(
	//     [](ggml_log_level /*level*/, const char* /*text*/, void* /*user_data*/) {
	//	    // Discard all log messages
	//     },
	//     nullptr);

	ggml_backend_load_all();  // load backends (CPU, CUDA, etc.)

	// Load model
	llama_model_params model_params = llama_model_default_params();
	model_params.n_gpu_layers = n_gpu_layers;
	llama_model* model = llama_load_model_from_file(model_path.c_str(), model_params);
	if (!model)
	{
		std::cerr << "Failed to load model from " << model_path << std::endl;
		return 1;
	}

	const llama_vocab* vocab = llama_model_get_vocab(model);

	// Create context
	llama_context_params ctx_params = llama_context_default_params();
	// We'll set a reasonable context size (prompt length + n_predict)
	ctx_params.n_ctx = 2048;  // can be larger, but 2048 is safe for most
	ctx_params.n_batch = 512;
	ctx_params.no_perf = false;
	llama_context* ctx = llama_new_context_with_model(model, ctx_params);
	if (!ctx)
	{
		std::cerr << "Failed to create context" << std::endl;
		llama_free_model(model);
		llama_backend_free();
		return 1;
	}

	// Create sampler (greedy for simplicity)
	auto sparams = llama_sampler_chain_default_params();
	sparams.no_perf = false;
	llama_sampler* smpl = llama_sampler_chain_init(sparams);
	llama_sampler_chain_add(smpl, llama_sampler_init_greedy());

	std::cout << "Model loaded. Enter prompts (Ctrl+C to exit):" << std::endl;

	// Main loop: read prompts from stdin
	std::string prompt;
	while (true)
	{
		std::cout << "\n> ";
		std::getline(std::cin, prompt);
		if (prompt.empty())
			continue;  // skip empty lines

		// Tokenize prompt
		std::vector<llama_token> prompt_tokens(prompt.size() + 1);
		int n_tokens =
		    llama_tokenize(vocab, prompt.c_str(), prompt.size(), prompt_tokens.data(), prompt_tokens.size(), true, true);
		if (n_tokens < 0)
		{
			std::cerr << "Tokenization failed" << std::endl;
			continue;
		}
		prompt_tokens.resize(n_tokens);

		// Prepare batch for the prompt
		llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size());

		// Decode loop
		int n_decode = 0;
		bool truncated = false;
		std::string response;

		// Print prompt (optional, so user sees what's being answered)
		std::cout << prompt << std::flush;

		for (int n_pos = 0; n_pos + batch.n_tokens < n_predict;)
		{
			if (llama_decode(ctx, batch))
			{
				std::cerr << "Decode failed" << std::endl;
				break;
			}
			n_pos += batch.n_tokens;

			// Sample next token
			llama_token token = llama_sampler_sample(smpl, ctx, -1);
			if (llama_vocab_is_eog(vocab, token))
				break;

			// Convert token to string and print/append
			char buf[128];
			int n = llama_token_to_piece(vocab, token, buf, sizeof(buf), 0, true);
			if (n > 0)
			{
				std::string piece(buf, n);
				response += piece;
				std::cout << piece << std::flush;  // stream output token by token
			}

			// Prepare batch for next token
			batch = llama_batch_get_one(&token, 1);
			++n_decode;

			if (n_decode >= n_predict)
			{
				truncated = true;
				break;
			}
		}
		std::cout << std::endl;
		if (truncated)
			std::cout << "[Truncated by n_predict limit]" << std::endl;
	}

	llama_sampler_free(smpl);
	llama_free(ctx);
	llama_free_model(model);
	llama_backend_free();

	return 0;
}
