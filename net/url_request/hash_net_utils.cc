#include "net/url_request/hash_net_utils.h"

#include "base/strings/string_split.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"

namespace net {

std::string HexEncode(
  const uint8_t* in_binary_data,
  size_t in_binary_data_length
) {
    static const char *hex_digits = "0123456789abcdef";

    // Create a string and give a hint to its final size (twice the size
    // of the input binary data)
    std::string hex_string;
    hex_string.reserve(in_binary_data_length * 2);

    // Run through the binary data and convert to a hex string
    UNSAFE_BUFFERS(
    std::for_each(
        in_binary_data,
        in_binary_data + in_binary_data_length,
        [&hex_string](uint8_t inputByte) {
            hex_string.push_back(hex_digits[inputByte >> 4]);
            hex_string.push_back(hex_digits[inputByte & 0x0F]);
        })
    );

    return hex_string;
}

crypto::SecureHash::Algorithm GetHashAlgorithm(const std::string& hash_func) {
  if (hash_func == "sha1") return crypto::SecureHash::SHA1;
  if (hash_func == "sha256") return crypto::SecureHash::SHA256;
  if (hash_func == "sha512") return crypto::SecureHash::SHA512;

  return crypto::SecureHash::NOT_IMPLEMENTED;
}

std::vector<uint8_t> GetHashOfMessage(std::string& message, const std::string& hash_func) {
  auto hash_function = GetHashAlgorithm(hash_func);
  if (hash_function == crypto::SecureHash::NOT_IMPLEMENTED) return {};

  auto hash_checker = std::unique_ptr<crypto::SecureHash> (
      crypto::SecureHash::Create(hash_function));
  size_t hash_length = hash_checker->GetHashLength();
  hash_checker->Update(base::as_byte_span(message));

  std::vector<uint8_t> hash = std::vector<uint8_t>(hash_length);
  hash_checker->Finish(hash);
  return hash;
}

int GetNid(const std::string& sign_func) {
  if (sign_func == "secp256r1") return NID_X9_62_prime256v1;
  if (sign_func == "secp256k1") return NID_secp256k1;
  return NID_undef;
}

EC_KEY* LoadPrivateKey(const std::string& hex, const std::string& sign_func) {
  int nid = GetNid(sign_func);
  if (nid == NID_undef) return nullptr;

  BIGNUM start;
  BIGNUM *res;

  BN_init(&start);
  res = &start;
  BN_hex2bn(&res, hex.data());

  EC_KEY *key = EC_KEY_new_by_curve_name(nid);
  int result = EC_KEY_set_private_key(key, res);

  if (1 != result) return nullptr;

  return key;
}

EC_POINT* LoadPublicKey(const std::vector<uint8_t>& public_key, int nid) {
  BN_CTX *bn_ctx;
  EC_KEY *key;
  EC_POINT *point;
  const EC_GROUP *group;

  bn_ctx = BN_CTX_new();
  key = EC_KEY_new_by_curve_name(nid);
  group = EC_KEY_get0_group(key);
  point = EC_POINT_new(group);

  int result = EC_POINT_oct2point(group, point, public_key.data(), public_key.size(), bn_ctx);
  if (1 != result) return nullptr;

  BN_CTX_free(bn_ctx);
  EC_KEY_free(key);

  return point;
}

std::string DerivePublicKeyFromPrivate(const std::string& private_key) {

  if (private_key == "") return "";
  std::string sign_func;
  std::string sign_hash_func;
  std::string private_key_hex;
  SplitPrivateKey(private_key, sign_func, sign_hash_func, private_key_hex);

  int nid = GetNid(sign_func);
  if (nid == NID_undef) return "";

  BIGNUM start;
  BIGNUM *res;
  BN_init(&start);
  res = &start;
  BN_hex2bn(&res, private_key_hex.data());

  BN_CTX *ctx;
  ctx = BN_CTX_new();
  EC_KEY *ec_key = EC_KEY_new_by_curve_name(nid);
  const EC_GROUP *group = nullptr;
  group = EC_KEY_get0_group(ec_key);
  EC_POINT *pub_key = EC_POINT_new(group);
  if (!EC_POINT_mul(group, pub_key, res, nullptr, nullptr, ctx))
       return "";

  EC_KEY_set_public_key(ec_key, pub_key);

  unsigned char *short_pbuf = nullptr;
  size_t short_length = EC_KEY_key2buf(ec_key, POINT_CONVERSION_COMPRESSED, &short_pbuf, ctx);
  std::string short_key = HexEncode(short_pbuf, short_length);

  return base::StrCat({sign_func, ".", sign_hash_func, ":", short_key});
}

bool VerifySign(
  const std::string& sign_function,
  const std::vector<uint8_t>& public_key_bin,
  const std::vector<uint8_t>& hash_val,
  const std::vector<uint8_t>& sig_bin
) {
  if (sign_function == "secp256r1") {
    bssl::UniquePtr<EC_KEY> ec_key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
    if (ec_key == nullptr) {
      return false;
    }
    auto* ec_point = LoadPublicKey(public_key_bin, NID_X9_62_prime256v1);
    EC_KEY_set_public_key(ec_key.get(), ec_point);
    int result = ECDSA_verify(0, hash_val.data(), hash_val.size(), sig_bin.data(), sig_bin.size(), ec_key.get());
    return result == 1;
  }
  if (sign_function == "secp256k1") {
    bssl::UniquePtr<EC_KEY> ec_key(EC_KEY_new_by_curve_name(NID_secp256k1));
    if (ec_key == nullptr) {
      return false;
    }
    auto* ec_point = LoadPublicKey(public_key_bin, NID_secp256k1);
    EC_KEY_set_public_key(ec_key.get(), ec_point);
    int result = ECDSA_verify(0, hash_val.data(), hash_val.size(), sig_bin.data(), sig_bin.size(), ec_key.get());
    return result == 1;
  }
  return false;
}

void SplitHash(const std::string& hash, std::string& hash_func, std::string& hash_value) {
  auto chunks = base::SplitString(hash, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  // TODO checks
  hash_func = chunks[0];
  hash_value = chunks[1];
}

void SplitPublicKey(const std::string& public_key, std::string& sign_func, std::string& sign_hash_func, std::string& public_key_hex) {
  auto chunks_with_sign_function = base::SplitString(public_key, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  sign_func = chunks_with_sign_function[0];
  SplitHash(chunks_with_sign_function[1], sign_hash_func, public_key_hex);
}

void SplitPrivateKey(const std::string& private_key, std::string& sign_func, std::string& sign_hash_func, std::string& private_key_hex) {
  if (private_key.empty()) return;
  auto chunks_with_sign_function = base::SplitString(private_key, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  sign_func = chunks_with_sign_function[0];
  SplitHash(chunks_with_sign_function[1], sign_hash_func, private_key_hex);
}

void SplitSignedUrl(
  const GURL& url,
  std::string& sign_func_from_url,
  std::string& hash_func_from_url,
  std::string& public_key_from_url,
  std::string& label_from_url
) {
  std::string host = url.GetHost();
  std::string delimiter = ".";

  auto terminator = host.find(delimiter);
  // TODO check if found ???
  hash_func_from_url = host.substr(terminator + 1);
  sign_func_from_url = host.substr(0, terminator);

  auto chunks = base::SplitString(url.path(), "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  // TODO CHECK if there is two chunks exactly??
  public_key_from_url = chunks[1];
  label_from_url = url.path().substr(chunks[1].length() + 1); // chunks[0] is empty and we count only the first slash
}

std::string BuildMessageToSign(
  const std::string& hash_func,
  const std::string& hash_value,
  const std::string& nonce,
  const std::string& label,
  const std::string* related_to
) {
  if (related_to != nullptr && !related_to->empty()) {
    return base::JoinString({hash_func, hash_value, nonce, label, *related_to}, " ");
  } else {
    return base::JoinString({hash_func, hash_value, nonce, label}, " ");
  }
}

bool ValidateNonce(
  const base::Value* message,
  std::string& nonce
  ) {
  const std::string* nonce_ = message->GetIfDict()->FindString("nonce");
  if (nonce_ == nullptr || nonce_->empty()) return false;
  if (nonce_->find_first_not_of("0123456789") != std::string::npos) return false;
  nonce = *nonce_;
  return true;
}

bool ValidateLabel(
  const base::Value* message,
  std::string& label
  ) {
  const std::string* label_ = message->GetIfDict()->FindString("label");
  if (label_ == nullptr || label_->empty()) return false;
  label = *label_;
  return true;
}

bool ValidateHash(
  const base::Value* message,
  std::string& hash_func,
  std::string& hash_value
  ) {
  const std::string* hash = message->GetIfDict()->FindString("hash");
  if (hash == nullptr || hash->empty()) return false;
  SplitHash(*hash, hash_func, hash_value);
  if (hash_func.empty() || hash_value.empty()) return false;
  if (hash_value.find_first_not_of("0123456789abcdef") != std::string::npos) return false;
  return true;
}

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
  ) {
  if (!message->is_dict()) return false;
  auto* dict = message->GetIfDict();
  const std::string* public_key_ = dict->FindString("publicKey");
  if (public_key_ == nullptr || public_key_->empty()) return false;
  public_key = *public_key_;

  SplitPublicKey(public_key, sign_func, sign_hash_func, public_key_hex);
  if (public_key_hex.empty()) return false;
  if (public_key_hex.find_first_not_of("0123456789abcdef") != std::string::npos) return false;
  if (sign_func.empty() || sign_hash_func.empty()) return false;

  if (!ValidateLabel(message, label)) return false;
  if (!ValidateNonce(message, nonce)) return false;
  if (!ValidateHash(message, hash_func, hash_value)) return false;

  const std::string* sig_ = dict->FindString("signature");

  if (sig_ == nullptr || sig_->empty()) return false;
  if (sig_->find_first_not_of("0123456789abcdef") != std::string::npos) return false;
  sig = *sig_;

  const std::string* related_ = dict->FindString("relatedTo");
  if (related_ != nullptr && !related_->empty()) related = *related_;

  return true;
}

bool IsEditableMessage(const base::Value& message, const std::string& public_key) {
  if (!message.is_dict()) return false;
  const std::string* public_key_from_message = message.GetIfDict()->FindString("publicKey");
  if (public_key_from_message == nullptr) return false;
  return *public_key_from_message == public_key;
}

std::string GetTargetUrlForRelated(const GURL& relatedUrl) {
  std::string delimiter = ".";
  auto terminator = relatedUrl.host().find(delimiter);
  if (terminator == std::string::npos) {
    // must be a hash url
    return base::StrCat({"hash://", relatedUrl.host(), relatedUrl.path()});
  } else {
    // must be a signed url
    return base::StrCat({"signed://", relatedUrl.host(), relatedUrl.path()});
  }
}

bool SignMessage(const std::vector<uint8_t>& bytes, const std::string& private_key, std::string& output) {
  if (bytes.size() == 0) return false;

  auto root = base::JSONReader::ReadAndReturnValueWithError(
      std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()),
      base::JSON_PARSE_RFC);
  if (!root.has_value() || !root->is_dict()) return false;

  std::string hash_func, hash_value, label, nonce;
  if (!ValidateHash(&root.value(), hash_func, hash_value)) return false;
  if (!ValidateLabel(&root.value(), label)) return false;

  if (!ValidateNonce(&root.value(), nonce)) return false;

  // we expect fields: hash (hashFunction:hashValue), label, nonce, relatedTo
  // hashFunction hashValue nonce label ??relatedTo
  // and we going to add fields: publicKey, signature
  std::string sign_func, sign_hash_func, private_key_hex;
  SplitPrivateKey(private_key, sign_func, sign_hash_func, private_key_hex);
  EC_KEY* ec_private_key = LoadPrivateKey(private_key_hex, sign_func);
  if (ec_private_key == nullptr) return false;

  std::string public_key = DerivePublicKeyFromPrivate(private_key);
  if (public_key.empty()) return false;
  root->GetIfDict()->Set("publicKey", public_key);

  auto* dict = root->GetIfDict();
  const std::string* related_to = dict->FindString("relatedTo");
  if (related_to != nullptr && related_to->empty()) return false;
  std::string message_to_sign = BuildMessageToSign(hash_func, hash_value, nonce, label, related_to);

  auto hash_bin = GetHashOfMessage(message_to_sign, sign_hash_func);
  std::string calculated_hash = HexEncode(hash_bin.data(), hash_bin.size());
  unsigned int sign_length = ECDSA_size(ec_private_key);
  std::vector<uint8_t> signature = {};
  signature.resize(sign_length);
  int result = ECDSA_sign(0, hash_bin.data(), hash_bin.size(), signature.data(), &sign_length, ec_private_key);
  if (result != 1) return false;
  std::string signature_hex = HexEncode(signature.data(), sign_length);
  root->GetIfDict()->Set("signature", signature_hex);

  // TODO check if successfull
  base::JSONWriter::Write(*root->GetIfDict(), &output);
  return true;
}

bool VerifySignedResponse(base::Value* message, const GURL& url) {

  std::string public_key, public_key_hex, sign_func, sign_hash_func, sig;
  std::string hash_func, hash_value;
  std::string label, nonce, related;

  if (!ValidateSignedMessage(
    message,
    public_key,
    sign_func,
    sign_hash_func,
    public_key_hex,
    label,
    nonce,
    hash_func,
    hash_value,
    sig,
    related
  )) return false;

  std::string public_key_from_url, sign_func_from_url;
  std::string label_from_url, hash_func_from_url;
  SplitSignedUrl(url, sign_func_from_url, hash_func_from_url, public_key_from_url, label_from_url);

  if (public_key_hex != public_key_from_url) return false;
  if (sign_func != sign_func_from_url) return false;
  if (sign_hash_func != hash_func_from_url) return false;
  // TODO check if hash_func is doable and hash_value is hex and has proper length

  if (label != label_from_url) return false;

  /* end of validation */

  std::vector<uint8_t> public_key_from_url_bin;
  base::HexStringToBytes(public_key_from_url, &public_key_from_url_bin);

  std::string msg_to_sign = BuildMessageToSign(hash_func_from_url, hash_value, nonce, label_from_url, &related);

  auto digest_bin = GetHashOfMessage(msg_to_sign, sign_hash_func);
  std::string calculated_hash = HexEncode(digest_bin.data(), digest_bin.size());

  // check the signature
  std::vector<uint8_t> public_key_bin;
  base::HexStringToBytes(public_key_hex, &public_key_bin);

  std::vector<uint8_t> sig_bin;
  base::HexStringToBytes(sig, &sig_bin);

  bool result = VerifySign(
    sign_func_from_url,
    public_key_from_url_bin,
    digest_bin,
    sig_bin
  );
  return result;
}

base::ListValue VerifySignedResponses(const std::string& msg, const std::string& private_key, const GURL& url) {
  /* validation */
  auto root = base::JSONReader::ReadAndReturnValueWithError(
      std::string_view(msg.data(), msg.length()),
      base::JSON_PARSE_RFC);

  base::ListValue verified_messages = {};
  if (!root.has_value()) return verified_messages;
  if (!root->is_list()) return verified_messages;

  std::string sign_func, sign_hash_func, private_key_hex;

  SplitPrivateKey(private_key, sign_func, sign_hash_func, private_key_hex);
  EC_KEY* ec_private_key = LoadPrivateKey(private_key_hex, sign_func);
  if (ec_private_key == nullptr) return verified_messages;

  std::string public_key = DerivePublicKeyFromPrivate(private_key);

  for (auto& current_message : root->GetList()) {
    bool res = VerifySignedResponse(&current_message, url);
    if (res) {
      bool is_editable = !public_key.empty() && IsEditableMessage(current_message, public_key);
      current_message.GetIfDict()->Set("editable", is_editable);
      verified_messages.Append(std::move(current_message));
    } else {
      // not verified message, do nothing
    }
  }
  return verified_messages;
}

bool VerifyRelatedResponse(base::Value* message, const std::string& related_to) {
  std::string public_key, public_key_hex, sign_func, sign_hash_func, sig;
  std::string hash_func, hash_value;
  std::string label, nonce, related;

  if (!ValidateSignedMessage(
    message,
    public_key,
    sign_func,
    sign_hash_func,
    public_key_hex,
    label,
    nonce,
    hash_func,
    hash_value,
    sig,
    related
  )) {
    return false;
  }

  if (related.empty()) return false;
  // TODO check if valid #Net url (do we have to???)
  /* end of validation */

  if (related != related_to) return false;

  std::string msg_to_sign = BuildMessageToSign(sign_hash_func, hash_value, nonce, label, &related);

  auto digest_bin = GetHashOfMessage(msg_to_sign, sign_hash_func);
  if (digest_bin.size() == 0) return false;
  std::string calculated_hash = HexEncode(digest_bin.data(), digest_bin.size());

  // check the signature
  std::vector<uint8_t> public_key_bin;
  base::HexStringToBytes(public_key_hex, &public_key_bin);

  std::vector<uint8_t> sig_bin;
  base::HexStringToBytes(sig, &sig_bin);

  bool result = VerifySign(
    sign_func,
    public_key_bin,
    digest_bin,
    sig_bin
  );
  return result;
}

base::ListValue VerifyRelatedResponses(std::string msg, const std::string& private_key, const GURL& url) {
  /* validation */
  auto root = base::JSONReader::ReadAndReturnValueWithError(
      std::string_view(msg.data(), msg.length()),
      base::JSON_PARSE_RFC);

  base::ListValue verified_messages = {};
  if (!root.has_value() || !root->is_list()) return verified_messages;

  std::string sign_func, sign_hash_func, private_key_hex;

  SplitPrivateKey(private_key, sign_func, sign_hash_func, private_key_hex);
  EC_KEY* ec_private_key = LoadPrivateKey(private_key_hex, sign_func);
  if (ec_private_key == nullptr) return verified_messages;

  std::string public_key = DerivePublicKeyFromPrivate(private_key);
  if (public_key.empty()) return verified_messages;

  std::string related_to = GetTargetUrlForRelated(url);
  // for every message
  for (auto& current_message : root->GetList()) {
    bool res = VerifyRelatedResponse(&current_message, related_to);
    if (res) {
      bool is_editable = IsEditableMessage(current_message, public_key);
      current_message.GetIfDict()->Set("editable", is_editable);
      verified_messages.Append(std::move(current_message));
    } else {
      // std::cout << "NOT VERIFIED!!!\n";
    }
  }
  return verified_messages;
}

}
