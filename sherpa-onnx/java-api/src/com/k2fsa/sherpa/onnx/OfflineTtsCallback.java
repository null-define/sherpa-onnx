// Copyright 2024 Xiaomi Corporation

package com.k2fsa.sherpa.onnx;

@FunctionalInterface
public interface OfflineTtsCallback {
    Integer invoke(float[] samples);
}
