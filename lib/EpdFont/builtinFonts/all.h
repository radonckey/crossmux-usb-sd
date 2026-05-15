#pragma once

#ifdef ENABLE_CHINESE_VERSION
// Chinese build: one CJK font header per UI/reader point size (8/10/12/14/16/
// 18pt), each covering the top 3000 most-common Chinese characters (frequency-
// ranked subset of GB2312 Level-1) plus ASCII and full-width CJK punctuation.
// Raw 2-bit bitmaps — fontconvert.py's --compress is NOT used because the
// large per-group buffers fragment the heap on a 5+ font load (see
// build-cn-builtin-fonts.sh for context). Latin builtin headers are skipped;
// src/main.cpp aliases each Latin EpdFont global to the matching-size CJK
// header so font-size selection behaves like the Latin build.
#include <builtinFonts/chinese_chess_16.h>
#include <builtinFonts/notosans_cjk_8.h>
#include <builtinFonts/notosans_cjk_10.h>
#include <builtinFonts/notosans_cjk_12.h>
#include <builtinFonts/notosans_cjk_14.h>
#include <builtinFonts/notosans_cjk_16.h>
#include <builtinFonts/notosans_cjk_18.h>
#else
#include <builtinFonts/notoserif_12_bold.h>
#include <builtinFonts/notoserif_12_bolditalic.h>
#include <builtinFonts/notoserif_12_italic.h>
#include <builtinFonts/notoserif_12_regular.h>
#include <builtinFonts/notoserif_14_bold.h>
#include <builtinFonts/notoserif_14_bolditalic.h>
#include <builtinFonts/notoserif_14_italic.h>
#include <builtinFonts/notoserif_14_regular.h>
#include <builtinFonts/notoserif_16_bold.h>
#include <builtinFonts/notoserif_16_bolditalic.h>
#include <builtinFonts/notoserif_16_italic.h>
#include <builtinFonts/notoserif_16_regular.h>
#include <builtinFonts/notoserif_18_bold.h>
#include <builtinFonts/notoserif_18_bolditalic.h>
#include <builtinFonts/notoserif_18_italic.h>
#include <builtinFonts/notoserif_18_regular.h>
#include <builtinFonts/notosans_8_regular.h>
#include <builtinFonts/notosans_12_bold.h>
#include <builtinFonts/notosans_12_bolditalic.h>
#include <builtinFonts/notosans_12_italic.h>
#include <builtinFonts/notosans_12_regular.h>
#include <builtinFonts/notosans_14_bold.h>
#include <builtinFonts/notosans_14_bolditalic.h>
#include <builtinFonts/notosans_14_italic.h>
#include <builtinFonts/notosans_14_regular.h>
#include <builtinFonts/notosans_16_bold.h>
#include <builtinFonts/notosans_16_bolditalic.h>
#include <builtinFonts/notosans_16_italic.h>
#include <builtinFonts/notosans_16_regular.h>
#include <builtinFonts/notosans_18_bold.h>
#include <builtinFonts/notosans_18_bolditalic.h>
#include <builtinFonts/notosans_18_italic.h>
#include <builtinFonts/notosans_18_regular.h>
#include <builtinFonts/opendyslexic_10_bold.h>
#include <builtinFonts/opendyslexic_10_bolditalic.h>
#include <builtinFonts/opendyslexic_10_italic.h>
#include <builtinFonts/opendyslexic_10_regular.h>
#include <builtinFonts/opendyslexic_12_bold.h>
#include <builtinFonts/opendyslexic_12_bolditalic.h>
#include <builtinFonts/opendyslexic_12_italic.h>
#include <builtinFonts/opendyslexic_12_regular.h>
#include <builtinFonts/opendyslexic_14_bold.h>
#include <builtinFonts/opendyslexic_14_bolditalic.h>
#include <builtinFonts/opendyslexic_14_italic.h>
#include <builtinFonts/opendyslexic_14_regular.h>
#include <builtinFonts/opendyslexic_8_bold.h>
#include <builtinFonts/opendyslexic_8_bolditalic.h>
#include <builtinFonts/opendyslexic_8_italic.h>
#include <builtinFonts/opendyslexic_8_regular.h>
#include <builtinFonts/ubuntu_10_bold.h>
#include <builtinFonts/ubuntu_10_regular.h>
#include <builtinFonts/ubuntu_12_bold.h>
#include <builtinFonts/ubuntu_12_regular.h>
#endif
