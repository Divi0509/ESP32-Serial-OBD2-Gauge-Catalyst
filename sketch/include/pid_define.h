/*
struct PIDs {
  const char *label;  // Pid label show on meter
  const char *unit;   // unit for pid
  const char *pid;    // string pid 0104
  uint8_t formula;    // formula no to calculate
  int32_t min;        // lowest value
  int32_t max;        // highest value
  uint8_t skip;       // no of skip reading
  bool digit;         // want to show digit or no
  uint32_t warn;      // default warning value
};
*/
//Default factory  PIDs configuration

PIDs pidConfig[] = {
    // Gague page
    // fix PID 0-2
    {"ABP", "psi", "0133", 0, 0, 20, 4, true, 20},       // A * 0.145038
    {"SPEED", "km/h", "010D", 8, 0, 250, 1, false, 120}, // A
    {"RPM", "rev/m", "010C", 2, 0, 80, 1, true, 6},      // ((A*256)+B)/4 (/400 to use only 2nd first digit) 0-8000 rpm
    {"GEAR", " ", "221E12", 8, 1, 70, 1, false, 99},     // A

    // 1-7 pids
    {"ECT", "°c", "0105", 1, 0, 150, 3, false, 99},       // (A - 40)
    {"EOT", "°c", "015C", 1, 0, 150, 3, false, 99},       // A-40
    {"TFT", "°c", "221E1C", 9, 0, 150, 3, false, 99},     //(256 * A + B) / 16;
    {"EGR POS", "%", "0169", 10, 0, 100, 2, false, 99},   //(C*100)/255
    {"VGT POS", "%", "0171", 4, 0, 100, 2, false, 99},    //(B*100)/255
    {"BOOST", "psi", "0187", 6, 0, 40, 0, true, 35},      // ((B*256)+C)*0.145038 - ABP
    {"FRP", "psi", "016D", 7, 0, 30000, 0, false, 15000}, // ((D*256)+E)*1.45

    // Monitor 11-20
    {"RPM", "rev/m", "010C", 2, 0, 80, 1, true, 6},  
    {"TOTAL RUN", "hours", "017F", 13, 0, 99999, 4, false, 99999}, // B(2^{24})+C(2^{16})+D(2^{8})+E
    {"ODOMETER", "km", "01A6", 24, 0, 1000000, 4, false, 1000000}, // B(2^{24})+C(2^{16})+D(2^{8})+E
    {"RUNTIME", "hours", "011F", 22, 0, 65792, 4, false, 65792},   //((A*256)+B)/3600
    {"DPF DIFF", "psi", "017A", 10, 0, 100, 4, false, 99},         //(C*100)/255
    {"DPF LEVEL", "%", "017B", 11, 0, 100, 4, false, 99},          //(F*100)/255
    {"DPF TEMP", "°c", "017C", 21, 0, 1500, 1, false, 1500},       //(256*A + B )/10 - 40
    {"INJ TIMING", "deg", "015D", 16, -10, 10, 0, true, 99},       //((A*256)+B)/128 -210
    {"EGT", "°c", "2203F4", 19, 0, 800, 0, false, 800},          // A *5
    {"ALT CURR", "Amp", "220551", 23, -100, 100, 0, false, 100},   // ((A*256)+B)/64

    // Performance chart 21 - 25
    {"RPM", "rev/m", "010C", 2, 0, 80, 0, true, 6},  
    {"BOOST", "psi", "0187", 6, 0, 40, 0, true, 35},      // ((B*256)+C)*0.145038 - ABP
    {"MAF", "g/s", "0110", 17, 1, 80, 0, true, 80},     //(256A+B)/100.0
    {"FRP", "psi", "016D", 7, 0, 30000, 0, false, 15000}, // ((D*256)+E)*1.45
    {"EGT", "°c", "2203F4", 19, 0, 800, 0, false, 800}  // A *5

};
/*
  //{ "FANSPEED", "RPM", "22099F", 23, 0, 1000, 0, false, 1000 },    //(256*A + B )/4 (6.0 Litre  22099F)
 // { "ENGINEIT", "Hours", "017F", 14, 0, 100000, 5, false, 99 },  //F(2^{24})+G(2^{16})+H(2^{8})+I
  //{ "FRPC", "psi", "016D", 12, 0, 27000, 0, false, 99 },         //((A*256)+B)
  //{ "TURBO SPD", "RPM", "0174", 15, 0, 660000, 0, false, 99 },   //(B*256+C)x10

  // { "MAP", "psi", "0187", 6, 0, 60, 0, true, 35 },       // ((B*256)+C)*0.145038
  //{ "EGRT", "`F", "016B", 5, 0, 15, 1, false, 99 },             //((B-40)
  //{ "IAT", "°c", "0175", 5, 0, 220, 5, false, 99 },           // (B-40)
  //{ "EGT", "°c", "0178", 5, 0, 2000, 3, false, 99 },          // ((B-40)
  // { "EGT", "°c", "2203F4", 19, 0, 1500, 2, false, 1500 },  //A * 5

*/
