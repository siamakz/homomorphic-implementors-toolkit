// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "encryptor.h"
#include "../common.h"

using namespace std;
using namespace seal;

CKKSEncryptor::CKKSEncryptor(const shared_ptr<SEALContext> context, int numSlots, bool includePlaintext):
    encoder(nullptr), encryptor(nullptr), context(context), numSlots(numSlots) {
  mode = includePlaintext ? ENC_PLAIN : ENC_META;
}

CKKSEncryptor::CKKSEncryptor(const shared_ptr<SEALContext> context, CKKSEncoder *enc, Encryptor *encryptor, bool debug):
    encoder(enc), encryptor(encryptor), context(context), numSlots(encoder->slot_count()) {
  mode = debug ? ENC_DEBUG : ENC_NORMAL;
}

void CKKSEncryptor::encryptMatrix(const Matrix mat, double scale, CKKSCiphertext &dest, int lvl) {
  // in ENC_META, CKKSInstance sets numSlots to 4096 and doesn't actually attempt to calcuate the correct value.
  // We have to ignore that case here. Otherwise, matrix size should exactly equal the number of slots.
  if(mode != ENC_META && mat.size1()*mat.size2() != numSlots) {
    // bad things can happen if you don't plan for your matrix to be smaller than the ciphertext
    // This forces the caller to ensure that the matrix has the correct size or is at least appropriately padded
    throw invalid_argument("You can only encode matrices which exactly fit in the ciphertext: Expected " + to_string(numSlots) + ", got " + to_string(mat.size1()*mat.size2()));
  }

  dest.height = mat.size1();
  dest.width = mat.size2();
  dest.encoded_height = mat.size1();
  dest.encoded_width = mat.size2();
  dest.encoding = MATRIX;

  if(lvl == -1) {
    lvl = context->first_context_data()->chain_index();
  }

  auto context_data = context->first_context_data();
  while (context_data->chain_index() > lvl) {
    // order of operations is very important: floating point arithmetic is not associative
    scale = (scale*scale) / (double)context_data->parms().coeff_modulus().back().value();
    context_data = context_data->next_context_data();
  }

  // only set heLevel and scale if we aren't in Homomorphic mode
  if(mode != ENC_NORMAL) {
    // only for the DepthFinder evaluator
    dest.heLevel = lvl;
    dest.scale = scale;
  }
  // Only set the plaintext in Plaintext or Debug modes
  if(mode == ENC_PLAIN || mode == ENC_DEBUG) {
    dest.encoded_pt = Vector(mat.data().size());
    dest.encoded_pt.data() = mat.data();
  }
  // Only set the ciphertext in Normal or Debug modes
  if(mode == ENC_NORMAL || mode == ENC_DEBUG) {
    Plaintext temp;
    encoder->encode(mat.data(), context_data->parms_id(), scale, temp);
    encryptor->encrypt(temp, dest.sealct);
  }
}

void CKKSEncryptor::encryptColVec(const vector<double> &plain, int matHeight, double scale, CKKSCiphertext &destination, int lvl) {
  Matrix encodedVec = colVecToMatrix(plain, matHeight);
  encryptMatrix(encodedVec, scale, destination, lvl);
  destination.encoding = COL_VEC;
  destination.height = plain.size();
  destination.width = 1;
}

void CKKSEncryptor::encryptRowVec(const vector<double> &plain, int matWidth, double scale, CKKSCiphertext &destination, int lvl) {
  Matrix encodedVec = rowVecToMatrix(plain, matWidth);
  encryptMatrix(encodedVec, scale, destination, lvl);
  destination.encoding = ROW_VEC;
  destination.height = 1;
  destination.width = plain.size();
}