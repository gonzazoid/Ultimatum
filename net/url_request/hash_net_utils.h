#include <string>
#include <iostream>

#include "base/values.h"
#include "crypto/secure_hash.h"
#include "url/gurl.h"

#include "net/base/net_export.h"

#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"

namespace net {

std::vector<uint8_t> GetHashOfMessage(std::string& message, const std::string& hash_func);
int GetNid(const std::string& sign_func);

EC_KEY* LoadPrivateKey(const std::string& hex, const std::string& sign_func);
EC_POINT* LoadPublicKey(const std::vector<uint8_t>& public_key, int nid);

bool VerifySign(
  const std::string& sign_function,
  const std::vector<uint8_t>& public_key_bin,
  const std::vector<uint8_t>& hash_val,
  const std::vector<uint8_t>& sig_bin
);
// TODO make them bool (successful/failed)
void SplitHash(const std::string& hash, std::string& hash_func, std::string& hash_value);
void SplitPublicKey(const std::string& public_key, std::string& sign_func, std::string& sign_hash_func, std::string& public_key_hex);
void SplitPrivateKey(const std::string& private_key, std::string& sign_func, std::string& sign_hash_func, std::string& private_key_hex);
void SplitSignedUrl(
  const GURL& url,
  std::string& sign_func_from_url,
  std::string& hash_func_from_url,
  std::string& public_key_from_url,
  std::string& label_from_url
);

std::string BuildMessageToSign(
  const std::string& hash_func,
  const std::string& hash_value,
  const std::string& nonce,
  const std::string& label,
  const std::string* related_to
);

bool ValidateNonce(
  const base::Value* message,
  std::string& nonce
  );

bool ValidateLabel(
  const base::Value* message,
  std::string& label
  );

bool ValidateHash(
  const base::Value* message,
  std::string& hash_func,
  std::string& hash_value
  );

bool ValidateSignedMessage(
  const base::Value* message,
  std::string& public_key,
  std::string& sign_func,
  std::string& sign_hash_func,
  std::string& public_key_hex,
  std::string& label,
  std::string& nonce,
  std::string& hash_func,
  std::string& hash_value,
  std::string& sig,
  std::string& related
  );

bool IsEditableMessage(const base::Value& message, const std::vector<std::string>& public_keys);
std::string GetTargetUrlForRelated(const GURL& relatedUrl);

bool VerifySignedResponse(base::Value* message, const GURL& url);
bool VerifyRelatedResponse(base::Value* message, const std::string& related_to);

NET_EXPORT std::string HexEncode(
  const uint8_t* in_binary_data,
  size_t in_binary_data_length
);

NET_EXPORT crypto::SecureHash::Algorithm GetHashAlgorithm(const std::string& hash_func);

NET_EXPORT std::string DerivePublicKeyFromPrivate(const std::string& private_key);
NET_EXPORT bool SignMessage(const std::vector<uint8_t>& bytes, const std::string& private_key, std::string& output);

NET_EXPORT base::ListValue VerifySignedResponses(const std::string& msg, const std::string& private_key, const GURL& url);
NET_EXPORT base::ListValue VerifyRelatedResponses(std::string msg, const std::string& private_key, const GURL& url);
}
