'use strict';

var BN = require('bn.js');
var CryptoJS = require('crypto-js');

var SRP_N_HEX = 'AC6BDB41324A9A9BF166DE5E1389582FAF72B6651987EE07FC3192943DB56050A37329CBB4A099ED8193E0757767A13DD52312AB4B03310DCD7F48A9DA04FD50E8083969EDB767B0CF6095179A163AB3661A05FBD5FAAAE82918A9962F0B93B855F97993EC975EEAA80D740ADBF4FF747359D041D5C33EA71D281E446B14773BCA97B43A23FB801676BD207A436C6481F1D2B9078717461A5B9D32E688F87748544523B524B0D57D5EA77A2775D2ECFA032CFBDBF52FB3786160279004E57AE6AF874E7303CE53299CCC041C7BC308D82A5698F3A8D0C38271AE35F8E9DBFBB694B5C803D89F7AE435DE236D525F54759B65E372FCD68EF20FA7111F9E4AFF73';
var SRP_LENGTH = 256;
var N = new BN(SRP_N_HEX, 16);
var G = new BN(2);

function bytesToWordArray(bytes) {
  var words = [];
  var i;
  for (i = 0; i < bytes.length; i += 1) {
    words[i >>> 2] = (words[i >>> 2] || 0) | (bytes[i] << (24 - (i % 4) * 8));
  }
  return CryptoJS.lib.WordArray.create(words, bytes.length);
}

function wordArrayToBytes(wordArray) {
  var bytes = [];
  var i;
  for (i = 0; i < wordArray.sigBytes; i += 1) {
    bytes.push((wordArray.words[i >>> 2] >>> (24 - (i % 4) * 8)) & 255);
  }
  return bytes;
}

function utf8Bytes(value) {
  return wordArrayToBytes(CryptoJS.enc.Utf8.parse(String(value)));
}

function concatBytes() {
  var result = [];
  var i;
  for (i = 0; i < arguments.length; i += 1) {
    result = result.concat(arguments[i]);
  }
  return result;
}

function sha256() {
  return wordArrayToBytes(CryptoJS.SHA256(bytesToWordArray(concatBytes.apply(null, arguments))));
}

function bytesToHex(bytes) {
  return bytes.map(function(byte) { return ('0' + byte.toString(16)).slice(-2); }).join('');
}

function hexToBytes(hex) {
  var normalized = String(hex || '').replace(/^0x/i, '');
  var result = [];
  var i;
  if (normalized.length % 2) normalized = '0' + normalized;
  for (i = 0; i < normalized.length; i += 2) result.push(parseInt(normalized.slice(i, i + 2), 16));
  return result;
}

function bytesToBase64(bytes) {
  return CryptoJS.enc.Base64.stringify(bytesToWordArray(bytes));
}

function base64ToBytes(value) {
  return wordArrayToBytes(CryptoJS.enc.Base64.parse(String(value || '')));
}

function bnFromBytes(bytes) {
  return new BN(bytesToHex(bytes) || '00', 16);
}

function bnToBytes(value, length) {
  var bytes = hexToBytes(value.toString(16));
  while (length && bytes.length < length) bytes.unshift(0);
  return bytes;
}

function padToN(value) {
  var bytes = BN.isBN(value) ? bnToBytes(value) : value.slice();
  while (bytes.length < SRP_LENGTH) bytes.unshift(0);
  return bytes;
}

function randomBytes(length, fallbackEntropy) {
  var result = new Uint8Array(length);
  var cryptoObject = typeof crypto !== 'undefined' ? crypto : null;
  if (cryptoObject && cryptoObject.getRandomValues) {
    cryptoObject.getRandomValues(result);
    return Array.prototype.slice.call(result);
  }
  if (!fallbackEntropy) throw new Error('SECURE_RANDOM_UNAVAILABLE');
  var output = [];
  var counter = 0;
  while (output.length < length) {
    output = output.concat(sha256(utf8Bytes(String(fallbackEntropy) + '|' + counter)));
    counter += 1;
  }
  return output.slice(0, length);
}

function derivePassword(password, salt, iterations, protocol) {
  var passwordHash = sha256(utf8Bytes(password));
  var input = passwordHash;
  if (protocol === 's2k_fo') input = utf8Bytes(bytesToHex(passwordHash));
  if (protocol !== 's2k' && protocol !== 's2k_fo') throw new Error('Unsupported Apple SRP protocol');
  return wordArrayToBytes(CryptoJS.PBKDF2(bytesToWordArray(input), bytesToWordArray(salt), {
    keySize: 256 / 32,
    iterations: iterations,
    hasher: CryptoJS.algo.SHA256
  }));
}

function xorBytes(left, right) {
  return left.map(function(byte, index) { return byte ^ right[index]; });
}

function modPow(base, exponent, modulus) {
  return base.toRed(BN.red(modulus)).redPow(exponent).fromRed();
}

function createSrpProof(accountName, password, challenge, privateBytes) {
  var a = bnFromBytes(privateBytes || randomBytes(32));
  var A = modPow(G, a, N);
  var paddedA = padToN(A);
  var salt = base64ToBytes(challenge.salt);
  var serverPublicBytes = base64ToBytes(challenge.b);
  var B = bnFromBytes(serverPublicBytes);
  var passwordKey = derivePassword(password, salt, Number(challenge.iteration), String(challenge.protocol || 's2k'));
  var identityHash = sha256(utf8Bytes(':'), passwordKey);
  var x = bnFromBytes(sha256(salt, identityHash));
  var k = bnFromBytes(sha256(padToN(N), padToN(G)));
  var u = bnFromBytes(sha256(paddedA, padToN(serverPublicBytes)));
  var gx = modPow(G, x, N);
  var base = B.sub(k.mul(gx)).umod(N);
  var exponent = a.add(u.mul(x));
  var shared = modPow(base, exponent, N);
  var sessionKey = sha256(bnToBytes(shared));
  var proof = sha256(
    xorBytes(sha256(padToN(N)), sha256(padToN(G))),
    sha256(utf8Bytes(accountName)),
    salt,
    paddedA,
    serverPublicBytes,
    sessionKey
  );
  var serverProof = sha256(paddedA, proof, sessionKey);
  return {
    publicA: bytesToBase64(paddedA),
    m1: bytesToBase64(proof),
    m2: bytesToBase64(serverProof)
  };
}

function createSrpClient(fallbackEntropy) {
  var privateBytes = randomBytes(32, fallbackEntropy);
  var publicValue = modPow(G, bnFromBytes(privateBytes), N);
  return {
    privateBytes: privateBytes,
    publicA: bytesToBase64(padToN(publicValue))
  };
}

module.exports = {
  base64ToBytes: base64ToBytes,
  bytesToBase64: bytesToBase64,
  createSrpClient: createSrpClient,
  createSrpProof: createSrpProof,
  derivePassword: derivePassword,
  sha256: sha256,
  utf8Bytes: utf8Bytes
};
