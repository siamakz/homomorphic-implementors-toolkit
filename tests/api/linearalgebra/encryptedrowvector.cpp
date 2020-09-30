// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "hit/api/linearalgebra/linearalgebra.h"

#include <iostream>

#include "../../testutil.h"
#include "gtest/gtest.h"
#include "hit/api/evaluator/homomorphic.h"
#include "hit/api/ciphertext.h"
#include "hit/common.h"
#include "hit/sealutils.h"

using namespace std;
using namespace hit;

const int NUM_OF_SLOTS = 4096;
const int ZERO_MULTI_DEPTH = 0;
const int LOG_SCALE = 45;

TEST(EncryptedRowVectorTest, Serialization) {
    HomomorphicEval ckks_instance = HomomorphicEval(NUM_OF_SLOTS, ZERO_MULTI_DEPTH, LOG_SCALE);
    auto laInst = LinearAlgebra(ckks_instance);
    EncodingUnit unit1 = laInst.make_unit(64);
    Vector plaintext = random_vec(64);
    EncryptedRowVector ct1 = laInst.encrypt_row_vector(plaintext, unit1);
    EncryptedRowVector ct2 = EncryptedRowVector(ckks_instance.context, *ct1.serialize());
    ASSERT_EQ(ct1.width(), ct2.width());
    ASSERT_EQ(ct1.encoding_unit(), ct2.encoding_unit());
    Vector output = laInst.decrypt(ct2);
    ASSERT_LT(relative_error(plaintext.data(), output.data()), MAX_NORM);
}
