// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "common.h"
#include <iomanip> // std::setprecision

uint64_t elapsedTimeMs(timepoint start, timepoint end) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
}

std::string elapsedTimeToStr(timepoint start, timepoint end, TimeScale ts) {
  auto elapsedMs = static_cast<double>(elapsedTimeMs(start, end));
  std::stringstream buffer;
  double msPerSec = 1000;
  double msPerMin = 60 * msPerSec;
  double msPerHour = 60 * msPerMin;
  if(ts == TS_MS || (ts == TS_DYNAMIC && elapsedMs < msPerSec)) {
    buffer << std::setprecision(3) << elapsedMs << " ms";
  }
  else if(ts == TS_SEC || (ts == TS_DYNAMIC && elapsedMs < msPerMin)) {
    buffer << std::setprecision(3) << elapsedMs/msPerSec << " seconds";
  }
  else if(ts == TS_MIN || (ts == TS_DYNAMIC && elapsedMs < msPerHour)) {
    buffer << std::setprecision(3) << elapsedMs/msPerMin << " minutes";
  }
  else {
    buffer << std::setprecision(3) << elapsedMs/msPerHour << " hours";
  }
  return buffer.str();
}

std::string bytesToStr(uintmax_t sizeBytes) {
  double unitMultiplier = 1000;
  double bytesPerKB = unitMultiplier;
  double bytesPerMB = bytesPerKB * unitMultiplier;
  double bytesPerGB = bytesPerMB * unitMultiplier;
  std::stringstream buffer;

  if(sizeBytes < bytesPerKB) {
    buffer << sizeBytes << " bytes";
  }
  else if(sizeBytes < bytesPerMB) {
    buffer << (sizeBytes/bytesPerKB) << " KB";
  }
  else if(sizeBytes < bytesPerGB) {
    buffer << (sizeBytes/bytesPerMB) << " MB";
  }
  else {
    buffer << (sizeBytes/bytesPerGB) << " GB";
  }
  return buffer.str();
}

void printElapsedTime(timepoint start) {
  timepoint end = std::chrono::steady_clock::now();
  std::cout << elapsedTimeToStr(start,end) << std::endl;
}

std::vector<double> decodePlaintext(const std::vector<double> &x, CTEncoding enc,
                               int height, int width, int encoded_height, int encoded_width) {
  std::vector<double> dest;

  if(enc == COL_VEC && (width != 1 || height != encoded_width)) {
    std::stringstream buffer;
    buffer << "Invalid column vector encoding: real size= " << height << "x" << width << "; encoded size= " << encoded_height << "x" << encoded_width;
    throw std::invalid_argument(buffer.str());
  }
  if(enc == ROW_VEC && (height != 1 || width != encoded_height)) {
    std::stringstream buffer;
    buffer << "Invalid row vector encoding: real size= " << height << "x" << width << "; encoded size= " << encoded_height << "x" << encoded_width;
    throw std::invalid_argument(buffer.str());
  }

  if(enc == MATRIX || enc == ROW_MAT || enc == COL_MAT || enc == COL_VEC) {
    int size = height*width;
    dest = std::vector<double>(x.begin(),x.begin()+size);
  }
  else { // encoding is a row vector, which becomes the columns of the matrix
    for(int i = 0; i < width; i++) {
      // puts the left column into the destination, which corresponds to the encoded row vector
      dest.push_back(x[i*encoded_width]);
    }
  }
  return dest;
}

// computes the |expected-actual|/|expected|, where |*| denotes the 2-norm.
double diff2Norm(const std::vector<double> &expected, const std::vector<double> &actual) {
  int len = expected.size();
  if(len != actual.size()) {
    std::stringstream buffer;
    buffer << "diff2Norm inputs do not have the same size: " << len << " != " << actual.size();
    throw std::invalid_argument(buffer.str());
  }

  Vector expectedVec = fromStdVector(expected);
  Vector actualVec = fromStdVector(actual);
  Vector diffVec = expectedVec - actualVec;
  double expectedL2Norm = norm_2(expectedVec);
  double actualL2Norm = norm_2(actualVec);
  double diffL2Norm = norm_2(diffVec);

  // if the expected result is the zero vector, we can't reasonably compare norms.
  // We also can't just test if the expected vector norm is exactly 0 due to
  // decoding precision in CKKS. In other words, decode(encode(<0,0,...>))
  // may contain very small non-zero values. (Note that this has nothing to
  // do with encryption noise.) The "actual" result, which typically comes
  // from decryption a CKKS ciphertext, will have much larger coefficients.
  // For example, decoding noise for the all-0 vector may result in coefficients
  // with magnitude ~10^-30. Decryption of the all-0 vector will result in
  // coefficients ~10^-11. Since these are vastly different scales, the relative
  // norm is huge, even though these vectors both represent 0. As a result,
  // we instead fuzz the norm test: if the expected vector norm is "small enough"
  // we skip the comparison altogether. The magic constant below seems to work
  // well in practice.
  int logNormLimit = 11;
  double maxAllowedL2Norm = pow(2,-logNormLimit);
  if(expectedL2Norm <= maxAllowedL2Norm && actualL2Norm <= maxAllowedL2Norm) {
    return -1;
  }

  if(expectedL2Norm <= maxAllowedL2Norm) {
    std::cout << "WEIRD NORM SITUATION: " << expectedL2Norm << "\t" << actualL2Norm << std::endl;
  }
  if(diffL2Norm > MAX_NORM) {
    std::cout << "LogL2Norm: " << std::setprecision(8) << log2(expectedL2Norm) << std::endl;
  }
  return diffL2Norm;
}

// true if x is a power of 2, false otherwise.
bool isPow2(int x) {
  if (x < 1) {
    return false;
  }
  else if (x == 1) { // NOLINT(readability-else-after-return)
    return true;
  }
  // x > 1 and not 0 mod 2 => not a power of 2
  else if (x % 2 == 1) {
    return false;
  }
  else {
    return isPow2(x>>1);
  }
}

int polyDegreeToMaxModBits(int poly_modulus_degree) {
  if(poly_modulus_degree == 1024)       { return 27;  }
  else if(poly_modulus_degree == 2048)  { return 54;  } // NOLINT(readability-else-after-return)
  else if(poly_modulus_degree == 4096)  { return 109; }
  else if(poly_modulus_degree == 8192)  { return 218; }
  else if(poly_modulus_degree == 16384) { return 438; }
  else if(poly_modulus_degree == 32768) { return 881; }
  // extrapolating a best-fit line for the above data points:
  // mod_bits <= 0.0269*poly_modulus_degree-1.4428

  // SEAL will throw an exception when poly degree is 131072 or larger
  // (which corresponds to the 262144th cyclotomic ring)
  else if(poly_modulus_degree == 65536)  { return 1761; }
  // else if(poly_modulus_degree == 131072) { return 3524; }
  // else if(poly_modulus_degree == 262144) { return 7050; }
  else {
    std::stringstream buffer;
    buffer << "poly_modulus_degree=" << poly_modulus_degree << " not supported";
    throw std::invalid_argument(buffer.str());
  }
}

int modulusToPolyDegree(int modBits) {
  // When determining what dimension to use, we must first determine how many
  // primes need to be in our modulus (more on this below). Then we must
  // consult the following table to determine the smallest possible dimension.
  // A larger coeff_modulus implies a larger noise budget, hence more encrypted
  // computation capabilities. However, an upper bound for the total bit-length
  // of the coeff_modulus is determined by the poly_modulus_degree, as follows:
  //
  //     +----------------------------------------------------+
  //     | poly_modulus_degree | max coeff_modulus bit-length |
  //     +---------------------+------------------------------+
  //     | 1024                | 27                           |
  //     | 2048                | 54                           |
  //     | 4096                | 109                          |
  //     | 8192                | 218                          |
  //     | 16384               | 438                          |
  //     | 32768               | 881                          |
  //     +---------------------+------------------------------+
  if(modBits <= 27)       { return 1024; }
  else if(modBits <= 54)  { return 2048; } // NOLINT(readability-else-after-return)
  else if(modBits <= 109) { return 4096; }
  else if(modBits <= 218) { return 8192; }
  else if(modBits <= 438) { return 16384; }
  else if(modBits <= 881) { return 32768; }
  else if(modBits <= 1761) { return 65536; }
  // SEAL will throw an exception when poly degree is 131072 or larger
  // (which corresponds to the 262144th cyclotomic ring)
  // else if(modBits <= 3524) { return 131072; }
  // else if(modBits <= 7050) { return 262144; }
  else {
    std::stringstream buffer;
    buffer << "This computation is too big to handle right now: cannot determine a valid ring size for a " <<
              modBits << "-bit modulus";
    throw std::invalid_argument(buffer.str());
  }
}

void securityWarningBox(const std::string &str, WARN_LEVEL level) {
  int strlen = str.size();
  // set color to red (SEVERE) or yellow (WARN)
  if(level == SEVERE) {
    std::cout << std::endl << "\033[1;31m";
  }
  else {
    std::cout << std::endl << "\033[1;33m";
  }

  // print the top of the box
  for(int i = 0; i < strlen+4; i++) {
    std::cout << "*";
  }
  std::cout << std::endl;

  // print a "blank" line for the second row
  std::cout << "*";
  for(int i = 0; i < strlen+2; i++) {
    std::cout << " ";
  }
  std::cout << "*" << std::endl;

  // print the std::string itself
  std::cout << "* " << str << " *" << std::endl;

  // print a "blank" line for the second-to-last row
  std::cout << "*";
  for(int i = 0; i < strlen+2; i++) {
    std::cout << " ";
  }
  std::cout << "*" << std::endl;

  // print the bottom row of the box
  for(int i = 0; i < strlen+4; i++) {
    std::cout << "*";
  }

  // reset the color
  std::cout << "\033[0m" << std::endl << std::endl;
}

double lInfNorm(const std::vector<double> &x) {
  double xmax = 0;
  for(double i : x) {
    xmax = std::max(xmax,abs(i));
  }
  return xmax;
}

// generate a random vector of the given dimension, where each value is in the range [-maxNorm, maxNorm].
std::vector<double> randomVector(int dim, double maxNorm) {
  std::vector<double> x;
  x.reserve(dim);

  for(int i = 0; i < dim; i++) {
    // generate a random double between -maxNorm and maxNorm
    double a = -maxNorm + ((static_cast<double>(random()))/(static_cast<double>(RAND_MAX)))*(2*maxNorm);
    x.push_back(a);
  }
  return x;
}

uintmax_t streamSize(std::iostream &s) {
  std::streampos originalPos = s.tellp();
  s.seekp(0, std::ios::end);
  uintmax_t size = s.tellp();
  s.seekp(originalPos);
  return size;
}



// Extract the side-by-side plaintext from the ciphertext. Note that there is no decryption happening!
// This returns the "debug" plaintext.
Matrix ctPlaintextToMatrix(CKKSCiphertext &x) {
  return Matrix(x.height, x.width, x.getPlaintext());
}

// Extract the encrypted plaintext from the ciphertext. This actually decrypts and returns the output.
Matrix ctDecryptedToMatrix(CKKSInstance &inst, CKKSCiphertext &x) {
  return Matrix(x.height, x.width, inst.decrypt(x));
}

// Extract the debug plaintext from each ciphertext and concatenate the results side-by-side.
Matrix ctPlaintextToMatrix(std::vector<CKKSCiphertext> &xs) {
  std::vector<Matrix> mats;
  mats.reserve(xs.size());
  for(auto & x : xs) {
    mats.push_back(ctPlaintextToMatrix(x));
  }
  return matrixRowConcat(mats);
}

Vector ctPlaintextToVector(std::vector<CKKSCiphertext> &xs) {
  std::vector<double> stdvec;
  for(auto & x : xs) {
    std::vector<double> v = x.getPlaintext();
    stdvec.insert(stdvec.end(), v.begin(), v.end());
  }
  return fromStdVector(stdvec);
}

// Decrypt each ciphertext and concatenate the results side-by-side.
Matrix ctDecryptedToMatrix(CKKSInstance &inst, std::vector<CKKSCiphertext> &xs) {
  std::vector<Matrix> mats;
  mats.reserve(xs.size());
  for(auto & x : xs) {
    mats.push_back(ctDecryptedToMatrix(inst, x));
  }

  return matrixRowConcat(mats);
}

Vector ctDecryptedToVector(CKKSInstance &inst, std::vector<CKKSCiphertext> &xs) {
  std::vector<double> stdvec;
  for(const auto & x : xs) {
    std::vector<double> v = inst.decrypt(x);
    stdvec.insert(stdvec.end(), v.begin(), v.end());
  }
  return fromStdVector(stdvec);
}
