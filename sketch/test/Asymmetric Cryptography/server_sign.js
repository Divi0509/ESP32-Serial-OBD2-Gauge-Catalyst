import crypto from "crypto";
import fs from "fs";

//✅ Server: private key → generates signature
//✅ Client: public key + serial + signature → verifies authenticity

// === 1. Generate keypair (run once, then store securely) ===
if (!fs.existsSync("private.pem") || !fs.existsSync("public.pem")) {
  const { privateKey, publicKey } = crypto.generateKeyPairSync("ec", {
    namedCurve: "prime256v1", // secp256r1
  });
  fs.writeFileSync("private.pem", privateKey.export({ type: "pkcs8", format: "pem" }));
  fs.writeFileSync("public.pem", publicKey.export({ type: "spki", format: "pem" }));
  console.log("Generated new ECDSA key pair");
}

// === 2. Load private key ===
const privateKey = crypto.createPrivateKey(fs.readFileSync("private.pem"));

// === 3. Input: ESP32 serial number ===
const serial = "ACDE482FD123"; // Example

// === 4. Create signature ===
const sign = crypto.createSign("SHA256");
sign.update(serial);
sign.end();
const signature = sign.sign(privateKey);

// === 5. Output results ===
console.log("Serial:", serial);
console.log("Signature (base64):", signature.toString("base64"));
console.log("Public Key PEM:\n", fs.readFileSync("public.pem").toString());
