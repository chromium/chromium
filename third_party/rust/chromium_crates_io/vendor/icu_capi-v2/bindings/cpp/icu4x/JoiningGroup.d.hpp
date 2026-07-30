#ifndef ICU4X_JoiningGroup_D_HPP
#define ICU4X_JoiningGroup_D_HPP

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "diplomat_runtime.hpp"
namespace icu4x {
class JoiningGroup;
} // namespace icu4x



namespace icu4x {
namespace capi {
    enum JoiningGroup {
      JoiningGroup_NoJoiningGroup = 0,
      JoiningGroup_Ain = 1,
      JoiningGroup_Alaph = 2,
      JoiningGroup_Alef = 3,
      JoiningGroup_Beh = 4,
      JoiningGroup_Beth = 5,
      JoiningGroup_Dal = 6,
      JoiningGroup_DalathRish = 7,
      JoiningGroup_E = 8,
      JoiningGroup_Feh = 9,
      JoiningGroup_FinalSemkath = 10,
      JoiningGroup_Gaf = 11,
      JoiningGroup_Gamal = 12,
      JoiningGroup_Hah = 13,
      JoiningGroup_TehMarbutaGoal = 14,
      JoiningGroup_He = 15,
      JoiningGroup_Heh = 16,
      JoiningGroup_HehGoal = 17,
      JoiningGroup_Heth = 18,
      JoiningGroup_Kaf = 19,
      JoiningGroup_Kaph = 20,
      JoiningGroup_KnottedHeh = 21,
      JoiningGroup_Lam = 22,
      JoiningGroup_Lamadh = 23,
      JoiningGroup_Meem = 24,
      JoiningGroup_Mim = 25,
      JoiningGroup_Noon = 26,
      JoiningGroup_Nun = 27,
      JoiningGroup_Pe = 28,
      JoiningGroup_Qaf = 29,
      JoiningGroup_Qaph = 30,
      JoiningGroup_Reh = 31,
      JoiningGroup_ReversedPe = 32,
      JoiningGroup_Sad = 33,
      JoiningGroup_Sadhe = 34,
      JoiningGroup_Seen = 35,
      JoiningGroup_Semkath = 36,
      JoiningGroup_Shin = 37,
      JoiningGroup_SwashKaf = 38,
      JoiningGroup_SyriacWaw = 39,
      JoiningGroup_Tah = 40,
      JoiningGroup_Taw = 41,
      JoiningGroup_TehMarbuta = 42,
      JoiningGroup_Teth = 43,
      JoiningGroup_Waw = 44,
      JoiningGroup_Yeh = 45,
      JoiningGroup_YehBarree = 46,
      JoiningGroup_YehWithTail = 47,
      JoiningGroup_Yudh = 48,
      JoiningGroup_YudhHe = 49,
      JoiningGroup_Zain = 50,
      JoiningGroup_Fe = 51,
      JoiningGroup_Khaph = 52,
      JoiningGroup_Zhain = 53,
      JoiningGroup_BurushaskiYehBarree = 54,
      JoiningGroup_FarsiYeh = 55,
      JoiningGroup_Nya = 56,
      JoiningGroup_RohingyaYeh = 57,
      JoiningGroup_ManichaeanAleph = 58,
      JoiningGroup_ManichaeanAyin = 59,
      JoiningGroup_ManichaeanBeth = 60,
      JoiningGroup_ManichaeanDaleth = 61,
      JoiningGroup_ManichaeanDhamedh = 62,
      JoiningGroup_ManichaeanFive = 63,
      JoiningGroup_ManichaeanGimel = 64,
      JoiningGroup_ManichaeanHeth = 65,
      JoiningGroup_ManichaeanHundred = 66,
      JoiningGroup_ManichaeanKaph = 67,
      JoiningGroup_ManichaeanLamedh = 68,
      JoiningGroup_ManichaeanMem = 69,
      JoiningGroup_ManichaeanNun = 70,
      JoiningGroup_ManichaeanOne = 71,
      JoiningGroup_ManichaeanPe = 72,
      JoiningGroup_ManichaeanQoph = 73,
      JoiningGroup_ManichaeanResh = 74,
      JoiningGroup_ManichaeanSadhe = 75,
      JoiningGroup_ManichaeanSamekh = 76,
      JoiningGroup_ManichaeanTaw = 77,
      JoiningGroup_ManichaeanTen = 78,
      JoiningGroup_ManichaeanTeth = 79,
      JoiningGroup_ManichaeanThamedh = 80,
      JoiningGroup_ManichaeanTwenty = 81,
      JoiningGroup_ManichaeanWaw = 82,
      JoiningGroup_ManichaeanYodh = 83,
      JoiningGroup_ManichaeanZayin = 84,
      JoiningGroup_StraightWaw = 85,
      JoiningGroup_AfricanFeh = 86,
      JoiningGroup_AfricanNoon = 87,
      JoiningGroup_AfricanQaf = 88,
      JoiningGroup_MalayalamBha = 89,
      JoiningGroup_MalayalamJa = 90,
      JoiningGroup_MalayalamLla = 91,
      JoiningGroup_MalayalamLlla = 92,
      JoiningGroup_MalayalamNga = 93,
      JoiningGroup_MalayalamNna = 94,
      JoiningGroup_MalayalamNnna = 95,
      JoiningGroup_MalayalamNya = 96,
      JoiningGroup_MalayalamRa = 97,
      JoiningGroup_MalayalamSsa = 98,
      JoiningGroup_MalayalamTta = 99,
      JoiningGroup_HanifiRohingyaKinnaYa = 100,
      JoiningGroup_HanifiRohingyaPa = 101,
      JoiningGroup_ThinYeh = 102,
      JoiningGroup_VerticalTail = 103,
      JoiningGroup_KashmiriYeh = 104,
      JoiningGroup_ThinNoon = 105,
    };

    typedef struct JoiningGroup_option {union { JoiningGroup ok; }; bool is_ok; } JoiningGroup_option;
} // namespace capi
} // namespace

namespace icu4x {
/**
 * See the [Rust documentation for `JoiningGroup`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html) for more information.
 */
class JoiningGroup {
public:
    enum Value {
        /**
         * See the [Rust documentation for `NoJoiningGroup`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.NoJoiningGroup) for more information.
         */
        NoJoiningGroup = 0,
        /**
         * See the [Rust documentation for `Ain`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Ain) for more information.
         */
        Ain = 1,
        /**
         * See the [Rust documentation for `Alaph`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Alaph) for more information.
         */
        Alaph = 2,
        /**
         * See the [Rust documentation for `Alef`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Alef) for more information.
         */
        Alef = 3,
        /**
         * See the [Rust documentation for `Beh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Beh) for more information.
         */
        Beh = 4,
        /**
         * See the [Rust documentation for `Beth`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Beth) for more information.
         */
        Beth = 5,
        /**
         * See the [Rust documentation for `Dal`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Dal) for more information.
         */
        Dal = 6,
        /**
         * See the [Rust documentation for `DalathRish`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.DalathRish) for more information.
         */
        DalathRish = 7,
        /**
         * See the [Rust documentation for `E`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.E) for more information.
         */
        E = 8,
        /**
         * See the [Rust documentation for `Feh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Feh) for more information.
         */
        Feh = 9,
        /**
         * See the [Rust documentation for `FinalSemkath`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.FinalSemkath) for more information.
         */
        FinalSemkath = 10,
        /**
         * See the [Rust documentation for `Gaf`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Gaf) for more information.
         */
        Gaf = 11,
        /**
         * See the [Rust documentation for `Gamal`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Gamal) for more information.
         */
        Gamal = 12,
        /**
         * See the [Rust documentation for `Hah`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Hah) for more information.
         */
        Hah = 13,
        /**
         * See the [Rust documentation for `TehMarbutaGoal`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.TehMarbutaGoal) for more information.
         */
        TehMarbutaGoal = 14,
        /**
         * See the [Rust documentation for `He`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.He) for more information.
         */
        He = 15,
        /**
         * See the [Rust documentation for `Heh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Heh) for more information.
         */
        Heh = 16,
        /**
         * See the [Rust documentation for `HehGoal`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.HehGoal) for more information.
         */
        HehGoal = 17,
        /**
         * See the [Rust documentation for `Heth`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Heth) for more information.
         */
        Heth = 18,
        /**
         * See the [Rust documentation for `Kaf`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Kaf) for more information.
         */
        Kaf = 19,
        /**
         * See the [Rust documentation for `Kaph`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Kaph) for more information.
         */
        Kaph = 20,
        /**
         * See the [Rust documentation for `KnottedHeh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.KnottedHeh) for more information.
         */
        KnottedHeh = 21,
        /**
         * See the [Rust documentation for `Lam`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Lam) for more information.
         */
        Lam = 22,
        /**
         * See the [Rust documentation for `Lamadh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Lamadh) for more information.
         */
        Lamadh = 23,
        /**
         * See the [Rust documentation for `Meem`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Meem) for more information.
         */
        Meem = 24,
        /**
         * See the [Rust documentation for `Mim`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Mim) for more information.
         */
        Mim = 25,
        /**
         * See the [Rust documentation for `Noon`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Noon) for more information.
         */
        Noon = 26,
        /**
         * See the [Rust documentation for `Nun`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Nun) for more information.
         */
        Nun = 27,
        /**
         * See the [Rust documentation for `Pe`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Pe) for more information.
         */
        Pe = 28,
        /**
         * See the [Rust documentation for `Qaf`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Qaf) for more information.
         */
        Qaf = 29,
        /**
         * See the [Rust documentation for `Qaph`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Qaph) for more information.
         */
        Qaph = 30,
        /**
         * See the [Rust documentation for `Reh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Reh) for more information.
         */
        Reh = 31,
        /**
         * See the [Rust documentation for `ReversedPe`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ReversedPe) for more information.
         */
        ReversedPe = 32,
        /**
         * See the [Rust documentation for `Sad`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Sad) for more information.
         */
        Sad = 33,
        /**
         * See the [Rust documentation for `Sadhe`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Sadhe) for more information.
         */
        Sadhe = 34,
        /**
         * See the [Rust documentation for `Seen`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Seen) for more information.
         */
        Seen = 35,
        /**
         * See the [Rust documentation for `Semkath`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Semkath) for more information.
         */
        Semkath = 36,
        /**
         * See the [Rust documentation for `Shin`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Shin) for more information.
         */
        Shin = 37,
        /**
         * See the [Rust documentation for `SwashKaf`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.SwashKaf) for more information.
         */
        SwashKaf = 38,
        /**
         * See the [Rust documentation for `SyriacWaw`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.SyriacWaw) for more information.
         */
        SyriacWaw = 39,
        /**
         * See the [Rust documentation for `Tah`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Tah) for more information.
         */
        Tah = 40,
        /**
         * See the [Rust documentation for `Taw`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Taw) for more information.
         */
        Taw = 41,
        /**
         * See the [Rust documentation for `TehMarbuta`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.TehMarbuta) for more information.
         */
        TehMarbuta = 42,
        /**
         * See the [Rust documentation for `Teth`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Teth) for more information.
         */
        Teth = 43,
        /**
         * See the [Rust documentation for `Waw`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Waw) for more information.
         */
        Waw = 44,
        /**
         * See the [Rust documentation for `Yeh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Yeh) for more information.
         */
        Yeh = 45,
        /**
         * See the [Rust documentation for `YehBarree`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.YehBarree) for more information.
         */
        YehBarree = 46,
        /**
         * See the [Rust documentation for `YehWithTail`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.YehWithTail) for more information.
         */
        YehWithTail = 47,
        /**
         * See the [Rust documentation for `Yudh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Yudh) for more information.
         */
        Yudh = 48,
        /**
         * See the [Rust documentation for `YudhHe`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.YudhHe) for more information.
         */
        YudhHe = 49,
        /**
         * See the [Rust documentation for `Zain`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Zain) for more information.
         */
        Zain = 50,
        /**
         * See the [Rust documentation for `Fe`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Fe) for more information.
         */
        Fe = 51,
        /**
         * See the [Rust documentation for `Khaph`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Khaph) for more information.
         */
        Khaph = 52,
        /**
         * See the [Rust documentation for `Zhain`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Zhain) for more information.
         */
        Zhain = 53,
        /**
         * See the [Rust documentation for `BurushaskiYehBarree`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.BurushaskiYehBarree) for more information.
         */
        BurushaskiYehBarree = 54,
        /**
         * See the [Rust documentation for `FarsiYeh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.FarsiYeh) for more information.
         */
        FarsiYeh = 55,
        /**
         * See the [Rust documentation for `Nya`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.Nya) for more information.
         */
        Nya = 56,
        /**
         * See the [Rust documentation for `RohingyaYeh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.RohingyaYeh) for more information.
         */
        RohingyaYeh = 57,
        /**
         * See the [Rust documentation for `ManichaeanAleph`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanAleph) for more information.
         */
        ManichaeanAleph = 58,
        /**
         * See the [Rust documentation for `ManichaeanAyin`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanAyin) for more information.
         */
        ManichaeanAyin = 59,
        /**
         * See the [Rust documentation for `ManichaeanBeth`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanBeth) for more information.
         */
        ManichaeanBeth = 60,
        /**
         * See the [Rust documentation for `ManichaeanDaleth`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanDaleth) for more information.
         */
        ManichaeanDaleth = 61,
        /**
         * See the [Rust documentation for `ManichaeanDhamedh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanDhamedh) for more information.
         */
        ManichaeanDhamedh = 62,
        /**
         * See the [Rust documentation for `ManichaeanFive`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanFive) for more information.
         */
        ManichaeanFive = 63,
        /**
         * See the [Rust documentation for `ManichaeanGimel`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanGimel) for more information.
         */
        ManichaeanGimel = 64,
        /**
         * See the [Rust documentation for `ManichaeanHeth`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanHeth) for more information.
         */
        ManichaeanHeth = 65,
        /**
         * See the [Rust documentation for `ManichaeanHundred`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanHundred) for more information.
         */
        ManichaeanHundred = 66,
        /**
         * See the [Rust documentation for `ManichaeanKaph`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanKaph) for more information.
         */
        ManichaeanKaph = 67,
        /**
         * See the [Rust documentation for `ManichaeanLamedh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanLamedh) for more information.
         */
        ManichaeanLamedh = 68,
        /**
         * See the [Rust documentation for `ManichaeanMem`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanMem) for more information.
         */
        ManichaeanMem = 69,
        /**
         * See the [Rust documentation for `ManichaeanNun`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanNun) for more information.
         */
        ManichaeanNun = 70,
        /**
         * See the [Rust documentation for `ManichaeanOne`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanOne) for more information.
         */
        ManichaeanOne = 71,
        /**
         * See the [Rust documentation for `ManichaeanPe`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanPe) for more information.
         */
        ManichaeanPe = 72,
        /**
         * See the [Rust documentation for `ManichaeanQoph`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanQoph) for more information.
         */
        ManichaeanQoph = 73,
        /**
         * See the [Rust documentation for `ManichaeanResh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanResh) for more information.
         */
        ManichaeanResh = 74,
        /**
         * See the [Rust documentation for `ManichaeanSadhe`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanSadhe) for more information.
         */
        ManichaeanSadhe = 75,
        /**
         * See the [Rust documentation for `ManichaeanSamekh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanSamekh) for more information.
         */
        ManichaeanSamekh = 76,
        /**
         * See the [Rust documentation for `ManichaeanTaw`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanTaw) for more information.
         */
        ManichaeanTaw = 77,
        /**
         * See the [Rust documentation for `ManichaeanTen`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanTen) for more information.
         */
        ManichaeanTen = 78,
        /**
         * See the [Rust documentation for `ManichaeanTeth`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanTeth) for more information.
         */
        ManichaeanTeth = 79,
        /**
         * See the [Rust documentation for `ManichaeanThamedh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanThamedh) for more information.
         */
        ManichaeanThamedh = 80,
        /**
         * See the [Rust documentation for `ManichaeanTwenty`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanTwenty) for more information.
         */
        ManichaeanTwenty = 81,
        /**
         * See the [Rust documentation for `ManichaeanWaw`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanWaw) for more information.
         */
        ManichaeanWaw = 82,
        /**
         * See the [Rust documentation for `ManichaeanYodh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanYodh) for more information.
         */
        ManichaeanYodh = 83,
        /**
         * See the [Rust documentation for `ManichaeanZayin`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ManichaeanZayin) for more information.
         */
        ManichaeanZayin = 84,
        /**
         * See the [Rust documentation for `StraightWaw`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.StraightWaw) for more information.
         */
        StraightWaw = 85,
        /**
         * See the [Rust documentation for `AfricanFeh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.AfricanFeh) for more information.
         */
        AfricanFeh = 86,
        /**
         * See the [Rust documentation for `AfricanNoon`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.AfricanNoon) for more information.
         */
        AfricanNoon = 87,
        /**
         * See the [Rust documentation for `AfricanQaf`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.AfricanQaf) for more information.
         */
        AfricanQaf = 88,
        /**
         * See the [Rust documentation for `MalayalamBha`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.MalayalamBha) for more information.
         */
        MalayalamBha = 89,
        /**
         * See the [Rust documentation for `MalayalamJa`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.MalayalamJa) for more information.
         */
        MalayalamJa = 90,
        /**
         * See the [Rust documentation for `MalayalamLla`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.MalayalamLla) for more information.
         */
        MalayalamLla = 91,
        /**
         * See the [Rust documentation for `MalayalamLlla`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.MalayalamLlla) for more information.
         */
        MalayalamLlla = 92,
        /**
         * See the [Rust documentation for `MalayalamNga`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.MalayalamNga) for more information.
         */
        MalayalamNga = 93,
        /**
         * See the [Rust documentation for `MalayalamNna`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.MalayalamNna) for more information.
         */
        MalayalamNna = 94,
        /**
         * See the [Rust documentation for `MalayalamNnna`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.MalayalamNnna) for more information.
         */
        MalayalamNnna = 95,
        /**
         * See the [Rust documentation for `MalayalamNya`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.MalayalamNya) for more information.
         */
        MalayalamNya = 96,
        /**
         * See the [Rust documentation for `MalayalamRa`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.MalayalamRa) for more information.
         */
        MalayalamRa = 97,
        /**
         * See the [Rust documentation for `MalayalamSsa`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.MalayalamSsa) for more information.
         */
        MalayalamSsa = 98,
        /**
         * See the [Rust documentation for `MalayalamTta`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.MalayalamTta) for more information.
         */
        MalayalamTta = 99,
        /**
         * See the [Rust documentation for `HanifiRohingyaKinnaYa`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.HanifiRohingyaKinnaYa) for more information.
         */
        HanifiRohingyaKinnaYa = 100,
        /**
         * See the [Rust documentation for `HanifiRohingyaPa`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.HanifiRohingyaPa) for more information.
         */
        HanifiRohingyaPa = 101,
        /**
         * See the [Rust documentation for `ThinYeh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ThinYeh) for more information.
         */
        ThinYeh = 102,
        /**
         * See the [Rust documentation for `VerticalTail`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.VerticalTail) for more information.
         */
        VerticalTail = 103,
        /**
         * See the [Rust documentation for `KashmiriYeh`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.KashmiriYeh) for more information.
         */
        KashmiriYeh = 104,
        /**
         * See the [Rust documentation for `ThinNoon`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#associatedconstant.ThinNoon) for more information.
         */
        ThinNoon = 105,
    };

    JoiningGroup(): value(Value::NoJoiningGroup) {}

    // Implicit conversions between enum and ::Value
    constexpr JoiningGroup(Value v) : value(v) {}
    constexpr operator Value() const { return value; }
    // Prevent usage as boolean value
    explicit operator bool() const = delete;

  /**
   * See the [Rust documentation for `for_char`](https://docs.rs/icu/2.2.0/icu/properties/props/trait.EnumeratedProperty.html#tymethod.for_char) for more information.
   */
  inline static icu4x::JoiningGroup for_char(char32_t ch);

  /**
   * Get the "long" name of this property value (returns empty if property value is unknown)
   *
   * See the [Rust documentation for `get`](https://docs.rs/icu/2.2.0/icu/properties/struct.PropertyNamesLongBorrowed.html#method.get) for more information.
   */
  inline std::optional<std::string_view> long_name() const;

  /**
   * Get the "short" name of this property value (returns empty if property value is unknown)
   *
   * See the [Rust documentation for `get`](https://docs.rs/icu/2.2.0/icu/properties/struct.PropertyNamesShortBorrowed.html#method.get) for more information.
   */
  inline std::optional<std::string_view> short_name() const;

  /**
   * Convert to an integer value usable with ICU4C and `CodePointMapData`
   *
   * See the [Rust documentation for `to_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#method.to_icu4c_value) for more information.
   */
  inline uint8_t to_integer_value() const;

  /**
   * Convert from an integer value from ICU4C or `CodePointMapData`
   *
   * See the [Rust documentation for `from_icu4c_value`](https://docs.rs/icu/2.2.0/icu/properties/props/struct.JoiningGroup.html#method.from_icu4c_value) for more information.
   */
  inline static std::optional<icu4x::JoiningGroup> from_integer_value(uint8_t other);

  inline static std::optional<icu4x::JoiningGroup> try_from_str(std::string_view s);

    inline icu4x::capi::JoiningGroup AsFFI() const;
    inline static icu4x::JoiningGroup FromFFI(icu4x::capi::JoiningGroup c_enum);
private:
    Value value;
};

} // namespace
#endif // ICU4X_JoiningGroup_D_HPP
