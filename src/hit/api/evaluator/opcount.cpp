// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "opcount.h"

#include <glog/logging.h>

#include <iomanip>

using namespace std;
using namespace seal;
namespace hit {

    OpCount::OpCount(const shared_ptr<SEALContext> &context) : CKKSEvaluator(context) {
        depth_finder = new DepthFinder(context);
    }

    OpCount::~OpCount() {
        delete depth_finder;
    }

    void OpCount::reset_internal() {
        {
            scoped_lock lock(mutex_);
            multiplies_ = 0;
            additions_ = 0;
            negations_ = 0;
            rotations_ = 0;
            mod_downs_ = 0;
            mod_down_multi_levels_ = 0;
        }
        depth_finder->reset_internal();
    }

    void OpCount::print_op_count() const {
        shared_lock lock(mutex_);
        LOG(INFO) << "Multiplications: " << multiplies_;
        LOG(INFO) << "ModDownMuls: " << mod_down_multi_levels_;
        LOG(INFO) << "Additions: " << additions_;
        LOG(INFO) << "Negations: " << negations_;
        LOG(INFO) << "Rotations: " << rotations_;
        LOG(INFO) << "ModDownTos: " << mod_downs_;
    }

    int OpCount::get_multiplicative_depth() const {
        return depth_finder->get_multiplicative_depth();
    }

    void OpCount::rotate_right_inplace_internal(CKKSCiphertext &ct, int steps) {
        count_rotation_ops();
        depth_finder->rotate_right_inplace_internal(ct, steps);
    }

    void OpCount::rotate_left_inplace_internal(CKKSCiphertext &ct, int steps) {
        count_rotation_ops();
        depth_finder->rotate_left_inplace_internal(ct, steps);
    }

    void OpCount::negate_inplace_internal(CKKSCiphertext &ct) {
        {
            scoped_lock lock(mutex_);
            negations_++;
        }
        depth_finder->negate_inplace_internal(ct);
    }

    void OpCount::add_inplace_internal(CKKSCiphertext &ct1, const CKKSCiphertext &ct2) {
        count_addition_ops();
        depth_finder->add_inplace_internal(ct1, ct2);
    }

    void OpCount::add_plain_inplace_internal(CKKSCiphertext &ct, double scalar) {
        count_addition_ops();
        depth_finder->add_plain_inplace_internal(ct, scalar);
    }

    void OpCount::add_plain_inplace_internal(CKKSCiphertext &ct, const vector<double> &plain) {
        count_addition_ops();
        depth_finder->add_plain_inplace_internal(ct, plain);
    }

    void OpCount::sub_inplace_internal(CKKSCiphertext &ct1, const CKKSCiphertext &ct2) {
        count_addition_ops();
        depth_finder->sub_inplace_internal(ct1, ct2);
    }

    void OpCount::sub_plain_inplace_internal(CKKSCiphertext &ct, double scalar) {
        count_addition_ops();
        depth_finder->sub_plain_inplace_internal(ct, scalar);
    }

    void OpCount::sub_plain_inplace_internal(CKKSCiphertext &ct, const vector<double> &plain) {
        count_addition_ops();
        depth_finder->sub_plain_inplace_internal(ct, plain);
    }

    void OpCount::multiply_inplace_internal(CKKSCiphertext &ct1, const CKKSCiphertext &ct2) {
        count_multiple_ops();
        depth_finder->multiply_inplace_internal(ct1, ct2);
    }

    void OpCount::multiply_plain_inplace_internal(CKKSCiphertext &ct, double scalar) {
        count_multiple_ops();
        depth_finder->multiply_plain_inplace_internal(ct, scalar);
    }

    void OpCount::multiply_plain_inplace_internal(CKKSCiphertext &ct, const vector<double> &plain) {
        count_multiple_ops();
        depth_finder->multiply_plain_inplace_internal(ct, plain);
    }

    void OpCount::square_inplace_internal(CKKSCiphertext &ct) {
        count_multiple_ops();
        depth_finder->square_inplace_internal(ct);
    }

    void OpCount::mod_down_to_level_inplace_internal(CKKSCiphertext &ct, int level) {
        {
            scoped_lock lock(mutex_);
            if (ct.he_level() - level > 0) {
                mod_downs_++;
            }
            mod_down_multi_levels_ += (ct.he_level() - level);
        }
        depth_finder->mod_down_to_level_inplace_internal(ct, level);
    }

    void OpCount::rescale_to_next_inplace_internal(CKKSCiphertext &ct) {
        depth_finder->rescale_to_next_inplace_internal(ct);
    }

    void OpCount::relinearize_inplace_internal(CKKSCiphertext &ct) {
        depth_finder->relinearize_inplace_internal(ct);
    }
}  // namespace hit
