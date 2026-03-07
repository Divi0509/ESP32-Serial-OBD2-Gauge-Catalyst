const crypto = require("crypto");
const readline = require("readline");

//Serial: CC7B5C1EAB9C
function computeHMAC(key, serial) {
  // Create HMAC-SHA256
  const hmac = crypto.createHmac("sha256", key)
                     .update(serial)
                     .digest();  // returns a Buffer

  // Take first 8 bytes and convert to uppercase hex string
  return hmac.slice(0, 8).toString("hex").toUpperCase();
}

const key = "Java&Jacob";

const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout,
});

rl.question("Enter Serial Number: ", (serial) => {
  console.log("\nSerial:", serial);
  console.log("Activation Code:", computeHMAC(key, serial));
  rl.close();
});
