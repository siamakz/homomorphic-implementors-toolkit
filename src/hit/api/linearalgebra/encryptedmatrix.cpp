// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "encryptedmatrix.h"

#include <algorithm>
#include <execution>

using namespace std;

namespace hit {
    EncryptedMatrix::EncryptedMatrix(int height, int width, const EncodingUnit &unit,
                                     const vector<vector<CKKSCiphertext>> &cts)
        : height_(height), width_(width), unit(unit), cts(move(cts)) {
        validate_init();
    }

    void EncryptedMatrix::readFromProto(const std::shared_ptr<seal::SEALContext> &context,
                                        const protobuf::EncryptedMatrix &encrypted_matrix) {
        height_ = encrypted_matrix.height();
        width_ = encrypted_matrix.width();
        unit = EncodingUnit(encrypted_matrix.unit());

        cts.reserve(encrypted_matrix.cts_size());
        for (int i = 0; i < encrypted_matrix.cts_size(); i++) {
            const protobuf::CiphertextVector &proto_ciphertext_vector = encrypted_matrix.cts(i);
            vector<CKKSCiphertext> ciphertext_vector;
            ciphertext_vector.reserve(proto_ciphertext_vector.cts_size());
            deserialize_vector(context, proto_ciphertext_vector, ciphertext_vector);
            cts.push_back(ciphertext_vector);
        }
        validate_init();
    }

    EncryptedMatrix::EncryptedMatrix(const std::shared_ptr<seal::SEALContext> &context,
                                     const protobuf::EncryptedMatrix &encrypted_matrix) {
        readFromProto(context, encrypted_matrix);
    }

    protobuf::EncryptedMatrix *EncryptedMatrix::serialize() const {
        auto *encrypted_matrix = new protobuf::EncryptedMatrix();
        encrypted_matrix->set_height(height_);
        encrypted_matrix->set_width(width_);
        encrypted_matrix->set_allocated_unit(unit.serialize());
        for (const auto &ciphertext_vector : cts) {
            encrypted_matrix->mutable_cts()->AddAllocated(serialize_vector(ciphertext_vector));
        }
        return encrypted_matrix;
    }

    void EncryptedMatrix::save(ostream &stream) const {
        protobuf::EncryptedMatrix *proto_mat = serialize();
        proto_mat->SerializeToOstream(&stream);
        delete proto_mat;
    }

    EncodingUnit EncryptedMatrix::encoding_unit() const {
        return unit;
    }

    int EncryptedMatrix::height() const {
        return height_;
    }

    int EncryptedMatrix::width() const {
        return width_;
    }

    int EncryptedMatrix::num_vertical_units() const {
        return cts.size();
    }

    int EncryptedMatrix::num_horizontal_units() const {
        return cts[0].size();
    }

    int EncryptedMatrix::num_slots() const {
        return cts[0][0].num_slots();
    }

    int EncryptedMatrix::he_level() const {
        // assumes that cts is non-empty and that we enforce all cts must have the same level
        return cts[0][0].he_level();
    }

    double EncryptedMatrix::scale() const {
        // assumes that cts is non-empty and that we enforce all cts must have the same scale
        return cts[0][0].scale();
    }

    Matrix EncryptedMatrix::plaintext() const {
        vector<vector<Matrix>> plaintext_pieces(cts.size());

        for (int i = 0; i < cts.size(); i++) {
            vector<Matrix> plaintext_row(cts[0].size());
            for (int j = 0; j < cts[0].size(); j++) {
                // The CKKSCiphertext plaintext is just a list of coefficients.
                // We know that it has additional meaning here: it's really a matrix
                // with the dimensions of the encoding unit.
                // To decode and recover the underlying plaintext matrix, we must first
                // add this additional context.
                Vector raw_plaintext = cts[i][j].plaintext();
                if (raw_plaintext.size() != unit.encoding_height() * unit.encoding_width()) {
                    throw invalid_argument("Plaintext has the wrong number of coefficients.");
                }

                Matrix formatted_plaintext =
                    Matrix(unit.encoding_height(), unit.encoding_width(), raw_plaintext.data());
                plaintext_row[j] = formatted_plaintext;
            }
            plaintext_pieces[i] = plaintext_row;
        }

        return decode_matrix(plaintext_pieces, height_, width_);
    }

    bool EncryptedMatrix::initialized() const {
        if (cts.empty() || cts[0].empty()) {
            return false;
        }

        if (cts.size() != ceil(height_ / static_cast<double>(unit.encoding_height()))) {
            return false;
        }

        if (cts[0].size() != ceil(width_ / static_cast<double>(unit.encoding_width()))) {
            return false;
        }

        int row_size = cts[0].size();
        for (const auto &cts_i : cts) {
            if (cts_i.size() != row_size) {
                return false;
            }
            for (const auto &ct : cts_i) {
                if (ct.scale() != cts[0][0].scale() || ct.he_level() != cts[0][0].he_level()) {
                    return false;
                }
            }
        }
        /* if we got to this point:
         *   - cts is non-empty
         *   - each row has the same number of units
         *   - all cts have the same scale
         *   - all cts have the same he_level
         */
        return unit.initialized() && num_vertical_units() == cts.size() && num_horizontal_units() == cts[0].size();
    }

    void EncryptedMatrix::validate_init() const {
        if (!initialized()) {
            throw invalid_argument("Invalid cts to EncryptedMatrix.");
        }
    }

    size_t EncryptedMatrix::num_cts() const {
        return cts.size() * cts[0].size();
    }

    CKKSCiphertext &EncryptedMatrix::operator[](size_t idx) {
        size_t num_cols = cts[0].size();
        size_t row = idx / num_cols;
        size_t col = idx % num_cols;
        return cts[row][col];
    }

    const CKKSCiphertext &EncryptedMatrix::operator[](size_t idx) const {
        size_t num_cols = cts[0].size();
        size_t row = idx / num_cols;
        size_t col = idx % num_cols;
        return cts[row][col];
    }

    bool EncryptedMatrix::same_size(const EncryptedMatrix &enc_mat) const {
        return height_ == enc_mat.height() && width_ == enc_mat.width() && unit == enc_mat.encoding_unit();
    }

    /*********   CKKS Basics   *********
     * The basic form of a CKKS plaintext is an 'array' of real or complex values
     * (distinguished from a 'vector', which will refer to linear algebra vectors
     * below). All plaintext arrays must first be *encoded* into a CKKS Plaintext
     * type. This encoding is done implicitly in the high-level API.
     * Plaintexts can then be encrypted to obtain a Ciphertext.
     *
     *********   Matrix Encoding   *********
     * A matrix is encoded as a single array (which is then encoded as a CKKS
     * plaintext, and then encrypted into a CKKS ciphertext) by concatenating the
     * rows of the matrix (i.e., row-major encoding). Any operations that refer to
     * matrices really is talking about inducing something on the underlying array
     * representation. (Note: It *really* induces an operation on the *padded* array.
     * CKKS encoding takes a plaintext array like < 1,2,3,4 > and first pads it with
     * 0s until it has length poly_modulus_degree/2.)
     * A good example is a rotation. Rotations don't operate
     * directly on rows of the matrix, they operate on the array as a whole, which
     * does not correspond to rotating the rows of the matrix. We have to do extra
     * work to build "matrix row rotation" out of "array rotation".
     */

    vector<vector<Matrix>> encode_matrix(const Matrix &mat, const EncodingUnit &unit) {
        int height = mat.size1();
        int width = mat.size2();

        int num_vertical_units = ceil(height / static_cast<double>(unit.encoding_height()));
        int num_horizontal_units = ceil(width / static_cast<double>(unit.encoding_width()));

        vector<vector<Matrix>> cts(num_vertical_units);
        for (int i = 0; i < num_vertical_units; i++) {
            vector<Matrix> row_units(num_horizontal_units);
            for (int j = 0; j < num_horizontal_units; j++) {
                vector<double> unit_ij;
                unit_ij.reserve(unit.encoding_height() * unit.encoding_width());
                for (int k = 0; k < unit.encoding_height(); k++) {
                    for (int l = 0; l < unit.encoding_width(); l++) {
                        int row = unit.encoding_height() * i + k;
                        int col = unit.encoding_width() * j + l;
                        if (row < height && col < width) {
                            unit_ij.emplace_back(mat.data()[row * width + col]);
                        } else {
                            unit_ij.emplace_back(0);
                        }
                    }
                }
                row_units[j] = Matrix(unit.encoding_height(), unit.encoding_width(), unit_ij);
            }
            cts[i] = row_units;
        }
        return cts;
    }

    Matrix decode_matrix(const vector<vector<Matrix>> &mats, int trim_height, int trim_width) {
        if (mats.empty() || mats[0].empty()) {
            throw invalid_argument("decode_matrix: input cannot be empty");
        }

        int height = mats[0][0].size1();
        int width = mats[0][0].size2();

        if (trim_height < 0) {
            trim_height = static_cast<int>(mats.size() * height);
        }
        if (trim_width < 0) {
            trim_width = static_cast<int>(mats[0].size() * width);
        }

        vector<double> linear_matrix;
        linear_matrix.reserve(trim_height * trim_width);
        for (int i = 0; i < mats.size(); i++) {
            if (mats[i].size() != mats[0].size()) {
                throw invalid_argument("decode_matrix: all rows must have the same length");
            }
            // for each Matrix row
            for (int j = 0; j < height && i * height + j < trim_height; j++) {
                for (int k = 0; k < mats[0].size(); k++) {
                    if (mats[i][k].size1() != height || mats[i][k].size2() != width) {
                        throw invalid_argument("decode_matrix: all matrices must have the same dimension");
                    }
                    for (int l = 0; l < width && k * width + l < trim_width; l++) {
                        linear_matrix.emplace_back(mats[i][k].data()[j * width + l]);
                    }
                }
            }
        }
        return Matrix(trim_height, trim_width, linear_matrix);
    }
}  // namespace hit
