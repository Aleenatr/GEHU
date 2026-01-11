package com.example.demolition.ai

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

object GGUFChat {

    private const val TAG = "GGUFChat"

    suspend fun ask(
        modelPath: String,
        prompt: String,
        context: String? = null
    ): String = withContext(Dispatchers.IO) {

        if (!LlamaNative.isLibraryLoaded()) {
            val error = LlamaNative.getLoadError() ?: "Native libraries not loaded"
            Log.e(TAG, "Cannot generate response: $error")
            return@withContext "Error: $error"
        }

        var modelPtr = 0L
        var ctxPtr = 0L

        try {
            if (modelPath.isBlank()) {
                throw IllegalArgumentException("Model path is empty")
            }

            val gemmaPrompt =
                if (!context.isNullOrBlank()) {
                    """<start_of_turn>user
You are a helpful educational assistant for students. Answer based ONLY on the following context.

CONTEXT:
$context

QUESTION: $prompt

Provide a clear, friendly answer using only the information from the context above. Be conversational and encouraging. Do not use ** or * symbols.<end_of_turn>
<start_of_turn>model
"""
                } else {
                    """<start_of_turn>user
$prompt<end_of_turn>
<start_of_turn>model
"""
                }

            Log.d(TAG, "Loading model from: $modelPath")
            modelPtr = LlamaNative.loadModel(modelPath)
            if (modelPtr == 0L) {
                throw IllegalStateException("Failed to load model")
            }

            Log.d(TAG, "Creating context...")
            ctxPtr = LlamaNative.createContext(modelPtr)
            if (ctxPtr == 0L) {
                throw IllegalStateException("Failed to create context")
            }

            Log.d(TAG, "Generating response...")
            val output = LlamaNative.generateText(ctxPtr, gemmaPrompt)

            return@withContext output.trim()

        } catch (e: Exception) {
            Log.e(TAG, "GGUFChat failed", e)
            return@withContext "Error: ${e.message ?: "Unknown error"}"

        } finally {
            if (ctxPtr != 0L) {
                LlamaNative.freeContext(ctxPtr)
                Log.d(TAG, "Context freed")
            }
            if (modelPtr != 0L) {
                LlamaNative.freeModel(modelPtr)
                Log.d(TAG, "Model freed")
            }
        }
    }
}
