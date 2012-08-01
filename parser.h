#include <string>
using namespace std;

#define BD '%'

wstring parse (wchar_t* str) {
  int word_length = wcslen(str);
  wstring result;
  int i = 0;
  while (i < word_length) {
    result += str[i++];
    if (i == word_length) {
      result += BD;
      break;
    }
    if (str[i-1] == 66 || str[i-1] == 98) { // b
      if (str[i] == 614 || str[i] == 104 || str[i] == 118) { // bH, bh, bv
	result += str[i++];
	if (i == word_length) {
	  result += BD;
	  break;
	}
	if (str[i-1] == 104 && str[i] == 104) // bhh
	  result += str[i++];
	else if (str[i-1] == 118 && str[i] == 119) // bvw
	  result += str[i++];
      }
    }
    else if (str[i-1] == 67 || str[i-1] == 99) { // c
      if (str[i] == 104 || str[i] == 121) { // ch, cy
	result += str[i++];
	if (i == word_length) {
	  result += BD;
	  break;
	}
	if (str[i-1] == 104 && str[i] == 104) // chh
	  result += str[i++];
      }
    }
    else if (str[i-1] == 68 || str[i-1] == 100) { // d
      if (str[i] == 98 || str[i] == 100 || str[i] == 614 || str[i] == 119 || str[i] == 122) { // db, dd, dH, dw, dz
	result += str[i++];
	if (i == word_length) {
	  result += BD;
	  break;
	}
	if (str[i-1] == 98 && str[i] == 98) // dbb
	  result += str[i++];
      }
    }
    else if (str[i-1] == 70 || str[i-1] == 102) { // f
      if (str[i] == 119) // fw
	result += str[i++];
    }
    else if (str[i-1] == 71 || str[i-1] == 103) { // g
      if (str[i] == 98 || str[i] == 103 || str[i] == 104 || str[i] == 614 || str[i] == 114 || str[i] == 118 || str[i] == 119 || str[i] == 121) { // gb, gg, gh, gH, gr, gv, gw, gy
	result += str[i++];
	if (i == word_length) {
	  result += BD;
	  break;
	}
	if ((str[i-1] == 98 || str[i-1] == 114) && str[i] == 98) // gbb, grb
	  result += str[i++];
	else if ((str[i-1] == 104 || str[i-1] == 119) && str[i] == 614) // ghH, gwH
	  result += str[i++];
      }
    }
    else if (str[i-1] == 72 || str[i-1] == 104) { // h
      if (str[i] == 119 || str[i] == 771) { // hw, h~
	result += str[i++];
	if (i == word_length) {
	  result += BD;
	  break;
	}
	if (str[i] == 771 && str[i] == 119) // h~w
	  result += str[i++];
      }
    }
    else if (str[i-1] == 74 || str[i-1] == 106) { // j
      if (str[i] == 614 || str[i] == 121)// jH, jy
	result += str[i++];
    }
    else if (str[i-1] == 75 || str[i-1] == 107) { // k
      if (str[i] == 102 || str[i] == 104 || str[i] == 112 || str[i] == 119 || str[i] == 120 || str[i] == 121) { // kf, kh, kp, kw, kx, ky
	result += str[i++];
	if (i == word_length) {
	  result += BD;
	  break;
	}
	if (str[i-1] == 112 && str[i] == 112) // kpp
	  result += str[i++];
	else if (str[i-1] == 119 && str[i] == 104) // kwh
	  result += str[i++];
      }
    }
    else if (str[i-1] == 76 || str[i-1] == 108) { // l
      if (str[i] == 119) // lw
	result += str[i++];
    }
    else if (str[i-1] == 78 || str[i-1] == 110) { // n
      if (str[i] == 119 || str[i] == 121) // nw, ny
	result += str[i++];
    }
    else if (str[i-1] == 7748 || str[i-1] == 7749) { // n.
      if (str[i] == 109)// n.m
	result += str[i++];
    }
    else if (str[i-1] == 80 || str[i-1] == 112) { // p
      if (str[i] == 102 || str[i] == 104) { // pf, ph
	result += str[i++];
	if (i == word_length) {
	  result += BD;
	  break;
	}
	if (str[i-1] == 104 && (str[i] == 104 || str[i] == 119)) // phh, phw
	  result += str[i++];
      }
    }
    else if (str[i-1] == 82 || str[i-1] == 114) { // r
      if (str[i] == 119 || str[i] == 121) // rw, ry
	result += str[i++];
    }
    else if (str[i-1] == 83 || str[i-1] == 115) { // s
      if (str[i] == 104 || str[i] == 119) { // sh, sw
	result += str[i++];
	if (i == word_length) {
	  result += BD;
	  break;
	}
	if (str[i] == 104 && str[i] == 119) // shw
	  result += str[i++];
      }
    }
    else if (str[i-1] == 84 || str[i-1] == 116) { // t
      if (str[i] == 104 || str[i] == 102 || str[i] == 115) // th, tf, ts
	result += str[i++];
    }
    else if (str[i-1] == 90 || str[i-1] == 122) { // z
      if (str[i] == 104) // zh
	result += str[i++];
    }
    if (str[i] == 768 || str[i] == 772 || str[i] == 769 || str[i] == 771) // grave, macron, acute, tilde
      result += str[i++];
    result += BD;
  }
  return result;
}
