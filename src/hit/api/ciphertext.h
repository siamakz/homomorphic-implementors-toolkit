// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "hit/protobuf/ciphertext.pb.h"  // NOLINT
#include "metadata.h"
#include "seal/context.h"
#include "seal/seal.h"

namespace hit {
    /* This is a wrapper around the SEAL `Ciphertext` type.
     */
    struct CKKSCiphertext : public CiphertextMetadata<std::vector<double>> {
        // A default constructor is useful since we often write, e.g, `Ciphertext a;`
        CKKSCiphertext() = default;

        // Deserialize a ciphertext
        CKKSCiphertext(const std::shared_ptr<seal::SEALContext> &context, const hit::protobuf::Ciphertext &proto_ct);

        // Serialize a ciphertext
        hit::protobuf::Ciphertext *serialize() const;

        // Ciphertext metadata
        int num_slots() const override;
        int he_level() const override;
        double scale() const override;
        std::vector<double> plaintext() const override;

        // all evaluators need access for encryption and decryption
        friend class DebugEval;
        friend class DepthFinder;
        friend class HomomorphicEval;
        friend class PlaintextEval;
        friend class OpCount;
        friend class ScaleEstimator;

       private:

        // The raw plaintxt. This is used with some of the evaluators tha track ciphertext
        // metadata (e.g., DebugEval and PlaintextEval), but not by the Homomorphic evaluator.
        // This plaintext is not CKKS-encoded; in particular it is not scaled by the scale factor.
        std::vector<double> raw_pt;

        // SEAL ciphertext
        seal::Ciphertext seal_ct;

        // `scale` is used by the ScaleEstimator evaluator
        double scale_ = 0;

        // flag indicating whether this CT has been initialized or not
        // CKKSCiphertexts are initialized upon encryption
        bool initialized = false;

        // heLevel is used by the depthFinder
        int he_level_ = 0;

        // number of plaintext slots
        size_t num_slots_ = 0;
    };
}  // namespace hit
