// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "decryptor.h"
#include "../common.h"

using namespace std;
using namespace seal;

CKKSDecryptor::CKKSDecryptor(const shared_ptr<SEALContext> context, CKKSEncoder *enc, const SecretKey &secret_key):
  encoder(enc), context(context) {
  decryptor = new Decryptor(context, secret_key);
}

CKKSDecryptor::~CKKSDecryptor() {
  delete(decryptor);
}

vector<double> CKKSDecryptor::decrypt(const CKKSCiphertext &encrypted, bool verbose) {
  Plaintext temp;

  int lvl = encrypted.getLevel(context);
  if(lvl != 0 && verbose) {
    cout << "WARNING: Decrypting a ciphertext that is not at level 0! Consider starting with a smaller modulus to improve performance!" << endl;
  }

  decryptor->decrypt(encrypted.sealct, temp);

  vector<double> temp_vec;
  encoder->decode(temp, temp_vec);

  return decodePlaintext(temp_vec, encrypted.encoding, encrypted.height, encrypted.width, encrypted.encoded_height, encrypted.encoded_width);
}
