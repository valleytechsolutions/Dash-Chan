#include "avatars.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
struct Svg {
  std::ostream &out;
  static std::string color(uint16_t c) {
    std::ostringstream s;
    s << '#' << std::hex << std::setfill('0') << std::setw(2) << (((c >> 11) & 31) * 255 / 31)
      << std::setw(2) << (((c >> 5) & 63) * 255 / 63) << std::setw(2) << ((c & 31) * 255 / 31);
    return s.str();
  }
  void fillEllipse(int x, int y, int rx, int ry, uint16_t c) {
    out << "<ellipse cx='" << x << "' cy='" << y << "' rx='" << rx << "' ry='" << ry << "' fill='"
        << color(c) << "'/>";
  }
  void fillCircle(int x, int y, int r, uint16_t c) {
    fillEllipse(x, y, r, r, c);
  }
  void fillRoundRect(int x, int y, int w, int h, int r, uint16_t c) {
    out << "<rect x='" << x << "' y='" << y << "' width='" << w << "' height='" << h << "' rx='"
        << r << "' fill='" << color(c) << "'/>";
  }
  void fillRect(int x, int y, int w, int h, uint16_t c) {
    fillRoundRect(x, y, w, h, 0, c);
  }
  void fillTriangle(int x, int y, int x2, int y2, int x3, int y3, uint16_t c) {
    out << "<polygon points='" << x << "," << y << " " << x2 << "," << y2 << " " << x3 << "," << y3
        << "' fill='" << color(c) << "'/>";
  }
  void drawLine(int x, int y, int x2, int y2, uint16_t c) {
    out << "<path d='M" << x << " " << y << "L" << x2 << " " << y2 << "' stroke='" << color(c)
        << "' stroke-width='2' stroke-linecap='round'/>";
  }
};
int main() {
  std::ofstream f("media/companions.svg");
  Svg s{f};
  f << "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 960 620'><rect width='960' "
       "height='620' rx='24' fill='#0b1123'/><text x='40' y='44' font-family='sans-serif' "
       "font-size='20' font-weight='bold' fill='#8df0b5'>DASH-CHAN / MEET YOUR COPILOTS</text>";
  const char *names[] = {"MOCHI", "SPROUT", "ORBIT", "WISP", "EMBER"};
  const char *desc[] = {"Professional snack enthusiast.", "Collector of invisible treasure.",
                        "Big friendship signal.", "Your emotional support shadow.",
                        "Small flame. Big adventure."};
  for (int a = 0; a < 5; ++a) {
    int x = a < 3 ? 160 + 320 * a : 320 + 320 * (a - 3), y = a < 3 ? 164 : 440;
    s.fillRoundRect(x - 142, y - 98, 284, 250, 18, 0x1928);
    f << "<g transform='translate(" << x << " " << y << ") scale(1.4)'>";
    dash::drawBuddy(s, a, 0, 0, 0, 0, false);
    f << "</g><text x='" << x << "' y='" << y + 104
      << "' text-anchor='middle' font-family='sans-serif' font-size='21' font-weight='bold' "
         "fill='#f7f1e6'>"
      << names[a] << "</text><text x='" << x << "' y='" << y + 132
      << "' text-anchor='middle' font-family='sans-serif' font-size='11' fill='#abb9ce'>" << desc[a]
      << "</text>";
  }
  f << "</svg>";
}
