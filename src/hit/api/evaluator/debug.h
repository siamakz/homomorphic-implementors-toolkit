// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../ciphertext.h"
#include "../decryptor.h"
#include "../evaluator.h"
#include "homomorphic.h"
#include "scaleestimator.h"
#include "seal/seal.h"

namespace hit {

    /* This is the full debug evaluator. It combines all of the
     * other evaluators, thereby tracking all information
     * from DepthFinder, PlaintextEval, and ScaleEstimator,
     * as well as performing the ciphertext operations.
     */

    class DebugEval : public CKKSEvaluator {
       public:
        DebugEval(const std::shared_ptr<seal::SEALContext> &context, seal::CKKSEncoder &encoder,
                  seal::Encryptor &encryptor, const seal::GaloisKeys &galois_keys, const seal::RelinKeys &relin_keys,
                  double scale, CKKSDecryptor &decryptor);

        /* For documentation on the API, see ../evaluator.h */
        ~DebugEval() override;

        DebugEval(const DebugEval &) = delete;
        DebugEval &operator=(const DebugEval &) = delete;
        DebugEval(DebugEval &&) = delete;
        DebugEval &operator=(DebugEval &&) = delete;

        // primarily used to indicate the maximum value for each *input* to the function.
        // For functions which are a no-op, this function is the only way the evaluator
        // can learn the maximum plaintext values, and thereby appropriately restrict the scale.
        void update_plaintext_max_val(double x);

        // return the base-2 log of the maximum plaintext value in the computation
        // this is useful for putting an upper bound on the scale parameter
        double get_exact_max_log_plain_val() const;

        // return the base-2 log of the maximum scale that can be used for this
        // computation. Using a scale larger than this will result in the plaintext
        // exceeding SEAL's maximum size, and using a scale smaller than this value
        // will unnecessarily reduce precision of the computation.
        double get_estimated_max_log_scale() const;

       protected:
        void rotate_right_inplace_internal(CKKSCiphertext &ct, int steps) override;

        void rotate_left_inplace_internal(CKKSCiphertext &ct, int steps) override;

        void negate_inplace_internal(CKKSCiphertext &ct) override;

        void add_inplace_internal(CKKSCiphertext &ct1, const CKKSCiphertext &ct2) override;

        void add_plain_inplace_internal(CKKSCiphertext &ct, double scalar) override;

        void add_plain_inplace_internal(CKKSCiphertext &ct, const std::vector<double> &plain) override;

        void sub_inplace_internal(CKKSCiphertext &ct1, const CKKSCiphertext &ct2) override;

        void sub_plain_inplace_internal(CKKSCiphertext &ct, double scalar) override;

        void sub_plain_inplace_internal(CKKSCiphertext &ct, const std::vector<double> &plain) override;

        void multiply_inplace_internal(CKKSCiphertext &ct1, const CKKSCiphertext &ct2) override;

        void multiply_plain_inplace_internal(CKKSCiphertext &ct, double scalar) override;

        void multiply_plain_inplace_internal(CKKSCiphertext &ct, const std::vector<double> &plain) override;

        void square_inplace_internal(CKKSCiphertext &ct) override;

        void mod_down_to_level_inplace_internal(CKKSCiphertext &ct, int level) override;

        void rescale_to_next_inplace_internal(CKKSCiphertext &ct) override;

        void relinearize_inplace_internal(CKKSCiphertext &ct) override;

        // reuse this evaluator for another computation
        void reset_internal() override;

       private:
        HomomorphicEval *homomorphic_eval;
        ScaleEstimator *scale_estimator;
        CKKSDecryptor &decryptor;
        const double init_state_;

        void print_stats(const CKKSCiphertext &ct) const;
        void check_scale(const CKKSCiphertext &ct) const;

    };
}  // namespace hit
