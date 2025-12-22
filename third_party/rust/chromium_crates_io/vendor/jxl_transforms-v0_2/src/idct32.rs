// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(unused)]
#![allow(clippy::type_complexity)]
#![allow(clippy::erasing_op)]
#![allow(clippy::identity_op)]
use crate::*;
use jxl_simd::{F32SimdVec, SimdDescriptor};

#[allow(clippy::too_many_arguments)]
#[allow(clippy::excessive_precision)]
#[inline(always)]
pub(super) fn idct_32<D: SimdDescriptor>(
    d: D,
    mut v0: D::F32Vec,
    mut v1: D::F32Vec,
    mut v2: D::F32Vec,
    mut v3: D::F32Vec,
    mut v4: D::F32Vec,
    mut v5: D::F32Vec,
    mut v6: D::F32Vec,
    mut v7: D::F32Vec,
    mut v8: D::F32Vec,
    mut v9: D::F32Vec,
    mut v10: D::F32Vec,
    mut v11: D::F32Vec,
    mut v12: D::F32Vec,
    mut v13: D::F32Vec,
    mut v14: D::F32Vec,
    mut v15: D::F32Vec,
    mut v16: D::F32Vec,
    mut v17: D::F32Vec,
    mut v18: D::F32Vec,
    mut v19: D::F32Vec,
    mut v20: D::F32Vec,
    mut v21: D::F32Vec,
    mut v22: D::F32Vec,
    mut v23: D::F32Vec,
    mut v24: D::F32Vec,
    mut v25: D::F32Vec,
    mut v26: D::F32Vec,
    mut v27: D::F32Vec,
    mut v28: D::F32Vec,
    mut v29: D::F32Vec,
    mut v30: D::F32Vec,
    mut v31: D::F32Vec,
) -> (
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
    D::F32Vec,
) {
    let mut v32 = v0 + v16;
    let mut v33 = v0 - v16;
    let mut v34 = v8 + v24;
    let mut v35 = v8 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v36 = v35 + v34;
    let mut v37 = v35 - v34;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let mut v38 = v36.mul_add(mul, v32);
    let mut v39 = v36.neg_mul_add(mul, v32);
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let mut v40 = v37.mul_add(mul, v33);
    let mut v41 = v37.neg_mul_add(mul, v33);
    let mut v42 = v4 + v12;
    let mut v43 = v12 + v20;
    let mut v44 = v20 + v28;
    let mut v45 = v4 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v46 = v45 + v43;
    let mut v47 = v45 - v43;
    let mut v48 = v42 + v44;
    let mut v49 = v42 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v50 = v49 + v48;
    let mut v51 = v49 - v48;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let mut v52 = v50.mul_add(mul, v46);
    let mut v53 = v50.neg_mul_add(mul, v46);
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let mut v54 = v51.mul_add(mul, v47);
    let mut v55 = v51.neg_mul_add(mul, v47);
    let mul = D::F32Vec::splat(d, 0.5097955791041592);
    let mut v56 = v52.mul_add(mul, v38);
    let mut v57 = v52.neg_mul_add(mul, v38);
    let mul = D::F32Vec::splat(d, 0.6013448869350453);
    let mut v58 = v54.mul_add(mul, v40);
    let mut v59 = v54.neg_mul_add(mul, v40);
    let mul = D::F32Vec::splat(d, 0.8999762231364156);
    let mut v60 = v55.mul_add(mul, v41);
    let mut v61 = v55.neg_mul_add(mul, v41);
    let mul = D::F32Vec::splat(d, 2.5629154477415055);
    let mut v62 = v53.mul_add(mul, v39);
    let mut v63 = v53.neg_mul_add(mul, v39);
    let mut v64 = v2 + v6;
    let mut v65 = v6 + v10;
    let mut v66 = v10 + v14;
    let mut v67 = v14 + v18;
    let mut v68 = v18 + v22;
    let mut v69 = v22 + v26;
    let mut v70 = v26 + v30;
    let mut v71 = v2 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v72 = v71 + v67;
    let mut v73 = v71 - v67;
    let mut v74 = v65 + v69;
    let mut v75 = v65 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v76 = v75 + v74;
    let mut v77 = v75 - v74;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let mut v78 = v76.mul_add(mul, v72);
    let mut v79 = v76.neg_mul_add(mul, v72);
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let mut v80 = v77.mul_add(mul, v73);
    let mut v81 = v77.neg_mul_add(mul, v73);
    let mut v82 = v64 + v66;
    let mut v83 = v66 + v68;
    let mut v84 = v68 + v70;
    let mut v85 = v64 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v86 = v85 + v83;
    let mut v87 = v85 - v83;
    let mut v88 = v82 + v84;
    let mut v89 = v82 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v90 = v89 + v88;
    let mut v91 = v89 - v88;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let mut v92 = v90.mul_add(mul, v86);
    let mut v93 = v90.neg_mul_add(mul, v86);
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let mut v94 = v91.mul_add(mul, v87);
    let mut v95 = v91.neg_mul_add(mul, v87);
    let mul = D::F32Vec::splat(d, 0.5097955791041592);
    let mut v96 = v92.mul_add(mul, v78);
    let mut v97 = v92.neg_mul_add(mul, v78);
    let mul = D::F32Vec::splat(d, 0.6013448869350453);
    let mut v98 = v94.mul_add(mul, v80);
    let mut v99 = v94.neg_mul_add(mul, v80);
    let mul = D::F32Vec::splat(d, 0.8999762231364156);
    let mut v100 = v95.mul_add(mul, v81);
    let mut v101 = v95.neg_mul_add(mul, v81);
    let mul = D::F32Vec::splat(d, 2.5629154477415055);
    let mut v102 = v93.mul_add(mul, v79);
    let mut v103 = v93.neg_mul_add(mul, v79);
    let mul = D::F32Vec::splat(d, 0.5024192861881557);
    let mut v104 = v96.mul_add(mul, v56);
    let mut v105 = v96.neg_mul_add(mul, v56);
    let mul = D::F32Vec::splat(d, 0.5224986149396889);
    let mut v106 = v98.mul_add(mul, v58);
    let mut v107 = v98.neg_mul_add(mul, v58);
    let mul = D::F32Vec::splat(d, 0.5669440348163577);
    let mut v108 = v100.mul_add(mul, v60);
    let mut v109 = v100.neg_mul_add(mul, v60);
    let mul = D::F32Vec::splat(d, 0.6468217833599901);
    let mut v110 = v102.mul_add(mul, v62);
    let mut v111 = v102.neg_mul_add(mul, v62);
    let mul = D::F32Vec::splat(d, 0.7881546234512502);
    let mut v112 = v103.mul_add(mul, v63);
    let mut v113 = v103.neg_mul_add(mul, v63);
    let mul = D::F32Vec::splat(d, 1.0606776859903471);
    let mut v114 = v101.mul_add(mul, v61);
    let mut v115 = v101.neg_mul_add(mul, v61);
    let mul = D::F32Vec::splat(d, 1.7224470982383342);
    let mut v116 = v99.mul_add(mul, v59);
    let mut v117 = v99.neg_mul_add(mul, v59);
    let mul = D::F32Vec::splat(d, 5.1011486186891553);
    let mut v118 = v97.mul_add(mul, v57);
    let mut v119 = v97.neg_mul_add(mul, v57);
    let mut v120 = v1 + v3;
    let mut v121 = v3 + v5;
    let mut v122 = v5 + v7;
    let mut v123 = v7 + v9;
    let mut v124 = v9 + v11;
    let mut v125 = v11 + v13;
    let mut v126 = v13 + v15;
    let mut v127 = v15 + v17;
    let mut v128 = v17 + v19;
    let mut v129 = v19 + v21;
    let mut v130 = v21 + v23;
    let mut v131 = v23 + v25;
    let mut v132 = v25 + v27;
    let mut v133 = v27 + v29;
    let mut v134 = v29 + v31;
    let mut v135 = v1 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v136 = v135 + v127;
    let mut v137 = v135 - v127;
    let mut v138 = v123 + v131;
    let mut v139 = v123 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v140 = v139 + v138;
    let mut v141 = v139 - v138;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let mut v142 = v140.mul_add(mul, v136);
    let mut v143 = v140.neg_mul_add(mul, v136);
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let mut v144 = v141.mul_add(mul, v137);
    let mut v145 = v141.neg_mul_add(mul, v137);
    let mut v146 = v121 + v125;
    let mut v147 = v125 + v129;
    let mut v148 = v129 + v133;
    let mut v149 = v121 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v150 = v149 + v147;
    let mut v151 = v149 - v147;
    let mut v152 = v146 + v148;
    let mut v153 = v146 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v154 = v153 + v152;
    let mut v155 = v153 - v152;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let mut v156 = v154.mul_add(mul, v150);
    let mut v157 = v154.neg_mul_add(mul, v150);
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let mut v158 = v155.mul_add(mul, v151);
    let mut v159 = v155.neg_mul_add(mul, v151);
    let mul = D::F32Vec::splat(d, 0.5097955791041592);
    let mut v160 = v156.mul_add(mul, v142);
    let mut v161 = v156.neg_mul_add(mul, v142);
    let mul = D::F32Vec::splat(d, 0.6013448869350453);
    let mut v162 = v158.mul_add(mul, v144);
    let mut v163 = v158.neg_mul_add(mul, v144);
    let mul = D::F32Vec::splat(d, 0.8999762231364156);
    let mut v164 = v159.mul_add(mul, v145);
    let mut v165 = v159.neg_mul_add(mul, v145);
    let mul = D::F32Vec::splat(d, 2.5629154477415055);
    let mut v166 = v157.mul_add(mul, v143);
    let mut v167 = v157.neg_mul_add(mul, v143);
    let mut v168 = v120 + v122;
    let mut v169 = v122 + v124;
    let mut v170 = v124 + v126;
    let mut v171 = v126 + v128;
    let mut v172 = v128 + v130;
    let mut v173 = v130 + v132;
    let mut v174 = v132 + v134;
    let mut v175 = v120 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v176 = v175 + v171;
    let mut v177 = v175 - v171;
    let mut v178 = v169 + v173;
    let mut v179 = v169 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v180 = v179 + v178;
    let mut v181 = v179 - v178;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let mut v182 = v180.mul_add(mul, v176);
    let mut v183 = v180.neg_mul_add(mul, v176);
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let mut v184 = v181.mul_add(mul, v177);
    let mut v185 = v181.neg_mul_add(mul, v177);
    let mut v186 = v168 + v170;
    let mut v187 = v170 + v172;
    let mut v188 = v172 + v174;
    let mut v189 = v168 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v190 = v189 + v187;
    let mut v191 = v189 - v187;
    let mut v192 = v186 + v188;
    let mut v193 = v186 * D::F32Vec::splat(d, std::f32::consts::SQRT_2);
    let mut v194 = v193 + v192;
    let mut v195 = v193 - v192;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let mut v196 = v194.mul_add(mul, v190);
    let mut v197 = v194.neg_mul_add(mul, v190);
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let mut v198 = v195.mul_add(mul, v191);
    let mut v199 = v195.neg_mul_add(mul, v191);
    let mul = D::F32Vec::splat(d, 0.5097955791041592);
    let mut v200 = v196.mul_add(mul, v182);
    let mut v201 = v196.neg_mul_add(mul, v182);
    let mul = D::F32Vec::splat(d, 0.6013448869350453);
    let mut v202 = v198.mul_add(mul, v184);
    let mut v203 = v198.neg_mul_add(mul, v184);
    let mul = D::F32Vec::splat(d, 0.8999762231364156);
    let mut v204 = v199.mul_add(mul, v185);
    let mut v205 = v199.neg_mul_add(mul, v185);
    let mul = D::F32Vec::splat(d, 2.5629154477415055);
    let mut v206 = v197.mul_add(mul, v183);
    let mut v207 = v197.neg_mul_add(mul, v183);
    let mul = D::F32Vec::splat(d, 0.5024192861881557);
    let mut v208 = v200.mul_add(mul, v160);
    let mut v209 = v200.neg_mul_add(mul, v160);
    let mul = D::F32Vec::splat(d, 0.5224986149396889);
    let mut v210 = v202.mul_add(mul, v162);
    let mut v211 = v202.neg_mul_add(mul, v162);
    let mul = D::F32Vec::splat(d, 0.5669440348163577);
    let mut v212 = v204.mul_add(mul, v164);
    let mut v213 = v204.neg_mul_add(mul, v164);
    let mul = D::F32Vec::splat(d, 0.6468217833599901);
    let mut v214 = v206.mul_add(mul, v166);
    let mut v215 = v206.neg_mul_add(mul, v166);
    let mul = D::F32Vec::splat(d, 0.7881546234512502);
    let mut v216 = v207.mul_add(mul, v167);
    let mut v217 = v207.neg_mul_add(mul, v167);
    let mul = D::F32Vec::splat(d, 1.0606776859903471);
    let mut v218 = v205.mul_add(mul, v165);
    let mut v219 = v205.neg_mul_add(mul, v165);
    let mul = D::F32Vec::splat(d, 1.7224470982383342);
    let mut v220 = v203.mul_add(mul, v163);
    let mut v221 = v203.neg_mul_add(mul, v163);
    let mul = D::F32Vec::splat(d, 5.1011486186891553);
    let mut v222 = v201.mul_add(mul, v161);
    let mut v223 = v201.neg_mul_add(mul, v161);
    let mul = D::F32Vec::splat(d, 0.5006029982351963);
    let mut v224 = v208.mul_add(mul, v104);
    let mut v225 = v208.neg_mul_add(mul, v104);
    let mul = D::F32Vec::splat(d, 0.5054709598975436);
    let mut v226 = v210.mul_add(mul, v106);
    let mut v227 = v210.neg_mul_add(mul, v106);
    let mul = D::F32Vec::splat(d, 0.5154473099226246);
    let mut v228 = v212.mul_add(mul, v108);
    let mut v229 = v212.neg_mul_add(mul, v108);
    let mul = D::F32Vec::splat(d, 0.5310425910897841);
    let mut v230 = v214.mul_add(mul, v110);
    let mut v231 = v214.neg_mul_add(mul, v110);
    let mul = D::F32Vec::splat(d, 0.5531038960344445);
    let mut v232 = v216.mul_add(mul, v112);
    let mut v233 = v216.neg_mul_add(mul, v112);
    let mul = D::F32Vec::splat(d, 0.5829349682061339);
    let mut v234 = v218.mul_add(mul, v114);
    let mut v235 = v218.neg_mul_add(mul, v114);
    let mul = D::F32Vec::splat(d, 0.6225041230356648);
    let mut v236 = v220.mul_add(mul, v116);
    let mut v237 = v220.neg_mul_add(mul, v116);
    let mul = D::F32Vec::splat(d, 0.6748083414550057);
    let mut v238 = v222.mul_add(mul, v118);
    let mut v239 = v222.neg_mul_add(mul, v118);
    let mul = D::F32Vec::splat(d, 0.7445362710022986);
    let mut v240 = v223.mul_add(mul, v119);
    let mut v241 = v223.neg_mul_add(mul, v119);
    let mul = D::F32Vec::splat(d, 0.8393496454155268);
    let mut v242 = v221.mul_add(mul, v117);
    let mut v243 = v221.neg_mul_add(mul, v117);
    let mul = D::F32Vec::splat(d, 0.9725682378619608);
    let mut v244 = v219.mul_add(mul, v115);
    let mut v245 = v219.neg_mul_add(mul, v115);
    let mul = D::F32Vec::splat(d, 1.1694399334328847);
    let mut v246 = v217.mul_add(mul, v113);
    let mut v247 = v217.neg_mul_add(mul, v113);
    let mul = D::F32Vec::splat(d, 1.4841646163141662);
    let mut v248 = v215.mul_add(mul, v111);
    let mut v249 = v215.neg_mul_add(mul, v111);
    let mul = D::F32Vec::splat(d, 2.0577810099534108);
    let mut v250 = v213.mul_add(mul, v109);
    let mut v251 = v213.neg_mul_add(mul, v109);
    let mul = D::F32Vec::splat(d, 3.4076084184687190);
    let mut v252 = v211.mul_add(mul, v107);
    let mut v253 = v211.neg_mul_add(mul, v107);
    let mul = D::F32Vec::splat(d, 10.1900081235480329);
    let mut v254 = v209.mul_add(mul, v105);
    let mut v255 = v209.neg_mul_add(mul, v105);
    (
        v224, v226, v228, v230, v232, v234, v236, v238, v240, v242, v244, v246, v248, v250, v252,
        v254, v255, v253, v251, v249, v247, v245, v243, v241, v239, v237, v235, v233, v231, v229,
        v227, v225,
    )
}

#[inline(always)]
pub(super) fn do_idct_32<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
    stride: usize,
) {
    assert!(data.len() > 31 * stride);
    let mut v0 = D::F32Vec::load_array(d, &data[0 * stride]);
    let mut v1 = D::F32Vec::load_array(d, &data[1 * stride]);
    let mut v2 = D::F32Vec::load_array(d, &data[2 * stride]);
    let mut v3 = D::F32Vec::load_array(d, &data[3 * stride]);
    let mut v4 = D::F32Vec::load_array(d, &data[4 * stride]);
    let mut v5 = D::F32Vec::load_array(d, &data[5 * stride]);
    let mut v6 = D::F32Vec::load_array(d, &data[6 * stride]);
    let mut v7 = D::F32Vec::load_array(d, &data[7 * stride]);
    let mut v8 = D::F32Vec::load_array(d, &data[8 * stride]);
    let mut v9 = D::F32Vec::load_array(d, &data[9 * stride]);
    let mut v10 = D::F32Vec::load_array(d, &data[10 * stride]);
    let mut v11 = D::F32Vec::load_array(d, &data[11 * stride]);
    let mut v12 = D::F32Vec::load_array(d, &data[12 * stride]);
    let mut v13 = D::F32Vec::load_array(d, &data[13 * stride]);
    let mut v14 = D::F32Vec::load_array(d, &data[14 * stride]);
    let mut v15 = D::F32Vec::load_array(d, &data[15 * stride]);
    let mut v16 = D::F32Vec::load_array(d, &data[16 * stride]);
    let mut v17 = D::F32Vec::load_array(d, &data[17 * stride]);
    let mut v18 = D::F32Vec::load_array(d, &data[18 * stride]);
    let mut v19 = D::F32Vec::load_array(d, &data[19 * stride]);
    let mut v20 = D::F32Vec::load_array(d, &data[20 * stride]);
    let mut v21 = D::F32Vec::load_array(d, &data[21 * stride]);
    let mut v22 = D::F32Vec::load_array(d, &data[22 * stride]);
    let mut v23 = D::F32Vec::load_array(d, &data[23 * stride]);
    let mut v24 = D::F32Vec::load_array(d, &data[24 * stride]);
    let mut v25 = D::F32Vec::load_array(d, &data[25 * stride]);
    let mut v26 = D::F32Vec::load_array(d, &data[26 * stride]);
    let mut v27 = D::F32Vec::load_array(d, &data[27 * stride]);
    let mut v28 = D::F32Vec::load_array(d, &data[28 * stride]);
    let mut v29 = D::F32Vec::load_array(d, &data[29 * stride]);
    let mut v30 = D::F32Vec::load_array(d, &data[30 * stride]);
    let mut v31 = D::F32Vec::load_array(d, &data[31 * stride]);
    (
        v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19,
        v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
    ) = idct_32(
        d, v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18,
        v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
    );
    v0.store_array(&mut data[0 * stride]);
    v1.store_array(&mut data[1 * stride]);
    v2.store_array(&mut data[2 * stride]);
    v3.store_array(&mut data[3 * stride]);
    v4.store_array(&mut data[4 * stride]);
    v5.store_array(&mut data[5 * stride]);
    v6.store_array(&mut data[6 * stride]);
    v7.store_array(&mut data[7 * stride]);
    v8.store_array(&mut data[8 * stride]);
    v9.store_array(&mut data[9 * stride]);
    v10.store_array(&mut data[10 * stride]);
    v11.store_array(&mut data[11 * stride]);
    v12.store_array(&mut data[12 * stride]);
    v13.store_array(&mut data[13 * stride]);
    v14.store_array(&mut data[14 * stride]);
    v15.store_array(&mut data[15 * stride]);
    v16.store_array(&mut data[16 * stride]);
    v17.store_array(&mut data[17 * stride]);
    v18.store_array(&mut data[18 * stride]);
    v19.store_array(&mut data[19 * stride]);
    v20.store_array(&mut data[20 * stride]);
    v21.store_array(&mut data[21 * stride]);
    v22.store_array(&mut data[22 * stride]);
    v23.store_array(&mut data[23 * stride]);
    v24.store_array(&mut data[24 * stride]);
    v25.store_array(&mut data[25 * stride]);
    v26.store_array(&mut data[26 * stride]);
    v27.store_array(&mut data[27 * stride]);
    v28.store_array(&mut data[28 * stride]);
    v29.store_array(&mut data[29 * stride]);
    v30.store_array(&mut data[30 * stride]);
    v31.store_array(&mut data[31 * stride]);
}

#[inline(always)]
pub(super) fn do_idct_32_rowblock<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
) {
    assert!(data.len() >= 32);
    const { assert!(32usize.is_multiple_of(D::F32Vec::LEN)) };
    let row_stride = 32 / D::F32Vec::LEN;
    let mut v0 = D::F32Vec::load_array(
        d,
        &data[row_stride * (0 % D::F32Vec::LEN) + (0 / D::F32Vec::LEN)],
    );
    let mut v1 = D::F32Vec::load_array(
        d,
        &data[row_stride * (1 % D::F32Vec::LEN) + (1 / D::F32Vec::LEN)],
    );
    let mut v2 = D::F32Vec::load_array(
        d,
        &data[row_stride * (2 % D::F32Vec::LEN) + (2 / D::F32Vec::LEN)],
    );
    let mut v3 = D::F32Vec::load_array(
        d,
        &data[row_stride * (3 % D::F32Vec::LEN) + (3 / D::F32Vec::LEN)],
    );
    let mut v4 = D::F32Vec::load_array(
        d,
        &data[row_stride * (4 % D::F32Vec::LEN) + (4 / D::F32Vec::LEN)],
    );
    let mut v5 = D::F32Vec::load_array(
        d,
        &data[row_stride * (5 % D::F32Vec::LEN) + (5 / D::F32Vec::LEN)],
    );
    let mut v6 = D::F32Vec::load_array(
        d,
        &data[row_stride * (6 % D::F32Vec::LEN) + (6 / D::F32Vec::LEN)],
    );
    let mut v7 = D::F32Vec::load_array(
        d,
        &data[row_stride * (7 % D::F32Vec::LEN) + (7 / D::F32Vec::LEN)],
    );
    let mut v8 = D::F32Vec::load_array(
        d,
        &data[row_stride * (8 % D::F32Vec::LEN) + (8 / D::F32Vec::LEN)],
    );
    let mut v9 = D::F32Vec::load_array(
        d,
        &data[row_stride * (9 % D::F32Vec::LEN) + (9 / D::F32Vec::LEN)],
    );
    let mut v10 = D::F32Vec::load_array(
        d,
        &data[row_stride * (10 % D::F32Vec::LEN) + (10 / D::F32Vec::LEN)],
    );
    let mut v11 = D::F32Vec::load_array(
        d,
        &data[row_stride * (11 % D::F32Vec::LEN) + (11 / D::F32Vec::LEN)],
    );
    let mut v12 = D::F32Vec::load_array(
        d,
        &data[row_stride * (12 % D::F32Vec::LEN) + (12 / D::F32Vec::LEN)],
    );
    let mut v13 = D::F32Vec::load_array(
        d,
        &data[row_stride * (13 % D::F32Vec::LEN) + (13 / D::F32Vec::LEN)],
    );
    let mut v14 = D::F32Vec::load_array(
        d,
        &data[row_stride * (14 % D::F32Vec::LEN) + (14 / D::F32Vec::LEN)],
    );
    let mut v15 = D::F32Vec::load_array(
        d,
        &data[row_stride * (15 % D::F32Vec::LEN) + (15 / D::F32Vec::LEN)],
    );
    let mut v16 = D::F32Vec::load_array(
        d,
        &data[row_stride * (16 % D::F32Vec::LEN) + (16 / D::F32Vec::LEN)],
    );
    let mut v17 = D::F32Vec::load_array(
        d,
        &data[row_stride * (17 % D::F32Vec::LEN) + (17 / D::F32Vec::LEN)],
    );
    let mut v18 = D::F32Vec::load_array(
        d,
        &data[row_stride * (18 % D::F32Vec::LEN) + (18 / D::F32Vec::LEN)],
    );
    let mut v19 = D::F32Vec::load_array(
        d,
        &data[row_stride * (19 % D::F32Vec::LEN) + (19 / D::F32Vec::LEN)],
    );
    let mut v20 = D::F32Vec::load_array(
        d,
        &data[row_stride * (20 % D::F32Vec::LEN) + (20 / D::F32Vec::LEN)],
    );
    let mut v21 = D::F32Vec::load_array(
        d,
        &data[row_stride * (21 % D::F32Vec::LEN) + (21 / D::F32Vec::LEN)],
    );
    let mut v22 = D::F32Vec::load_array(
        d,
        &data[row_stride * (22 % D::F32Vec::LEN) + (22 / D::F32Vec::LEN)],
    );
    let mut v23 = D::F32Vec::load_array(
        d,
        &data[row_stride * (23 % D::F32Vec::LEN) + (23 / D::F32Vec::LEN)],
    );
    let mut v24 = D::F32Vec::load_array(
        d,
        &data[row_stride * (24 % D::F32Vec::LEN) + (24 / D::F32Vec::LEN)],
    );
    let mut v25 = D::F32Vec::load_array(
        d,
        &data[row_stride * (25 % D::F32Vec::LEN) + (25 / D::F32Vec::LEN)],
    );
    let mut v26 = D::F32Vec::load_array(
        d,
        &data[row_stride * (26 % D::F32Vec::LEN) + (26 / D::F32Vec::LEN)],
    );
    let mut v27 = D::F32Vec::load_array(
        d,
        &data[row_stride * (27 % D::F32Vec::LEN) + (27 / D::F32Vec::LEN)],
    );
    let mut v28 = D::F32Vec::load_array(
        d,
        &data[row_stride * (28 % D::F32Vec::LEN) + (28 / D::F32Vec::LEN)],
    );
    let mut v29 = D::F32Vec::load_array(
        d,
        &data[row_stride * (29 % D::F32Vec::LEN) + (29 / D::F32Vec::LEN)],
    );
    let mut v30 = D::F32Vec::load_array(
        d,
        &data[row_stride * (30 % D::F32Vec::LEN) + (30 / D::F32Vec::LEN)],
    );
    let mut v31 = D::F32Vec::load_array(
        d,
        &data[row_stride * (31 % D::F32Vec::LEN) + (31 / D::F32Vec::LEN)],
    );
    (
        v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19,
        v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
    ) = idct_32(
        d, v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18,
        v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
    );
    v0.store_array(&mut data[row_stride * (0 % D::F32Vec::LEN) + (0 / D::F32Vec::LEN)]);
    v1.store_array(&mut data[row_stride * (1 % D::F32Vec::LEN) + (1 / D::F32Vec::LEN)]);
    v2.store_array(&mut data[row_stride * (2 % D::F32Vec::LEN) + (2 / D::F32Vec::LEN)]);
    v3.store_array(&mut data[row_stride * (3 % D::F32Vec::LEN) + (3 / D::F32Vec::LEN)]);
    v4.store_array(&mut data[row_stride * (4 % D::F32Vec::LEN) + (4 / D::F32Vec::LEN)]);
    v5.store_array(&mut data[row_stride * (5 % D::F32Vec::LEN) + (5 / D::F32Vec::LEN)]);
    v6.store_array(&mut data[row_stride * (6 % D::F32Vec::LEN) + (6 / D::F32Vec::LEN)]);
    v7.store_array(&mut data[row_stride * (7 % D::F32Vec::LEN) + (7 / D::F32Vec::LEN)]);
    v8.store_array(&mut data[row_stride * (8 % D::F32Vec::LEN) + (8 / D::F32Vec::LEN)]);
    v9.store_array(&mut data[row_stride * (9 % D::F32Vec::LEN) + (9 / D::F32Vec::LEN)]);
    v10.store_array(&mut data[row_stride * (10 % D::F32Vec::LEN) + (10 / D::F32Vec::LEN)]);
    v11.store_array(&mut data[row_stride * (11 % D::F32Vec::LEN) + (11 / D::F32Vec::LEN)]);
    v12.store_array(&mut data[row_stride * (12 % D::F32Vec::LEN) + (12 / D::F32Vec::LEN)]);
    v13.store_array(&mut data[row_stride * (13 % D::F32Vec::LEN) + (13 / D::F32Vec::LEN)]);
    v14.store_array(&mut data[row_stride * (14 % D::F32Vec::LEN) + (14 / D::F32Vec::LEN)]);
    v15.store_array(&mut data[row_stride * (15 % D::F32Vec::LEN) + (15 / D::F32Vec::LEN)]);
    v16.store_array(&mut data[row_stride * (16 % D::F32Vec::LEN) + (16 / D::F32Vec::LEN)]);
    v17.store_array(&mut data[row_stride * (17 % D::F32Vec::LEN) + (17 / D::F32Vec::LEN)]);
    v18.store_array(&mut data[row_stride * (18 % D::F32Vec::LEN) + (18 / D::F32Vec::LEN)]);
    v19.store_array(&mut data[row_stride * (19 % D::F32Vec::LEN) + (19 / D::F32Vec::LEN)]);
    v20.store_array(&mut data[row_stride * (20 % D::F32Vec::LEN) + (20 / D::F32Vec::LEN)]);
    v21.store_array(&mut data[row_stride * (21 % D::F32Vec::LEN) + (21 / D::F32Vec::LEN)]);
    v22.store_array(&mut data[row_stride * (22 % D::F32Vec::LEN) + (22 / D::F32Vec::LEN)]);
    v23.store_array(&mut data[row_stride * (23 % D::F32Vec::LEN) + (23 / D::F32Vec::LEN)]);
    v24.store_array(&mut data[row_stride * (24 % D::F32Vec::LEN) + (24 / D::F32Vec::LEN)]);
    v25.store_array(&mut data[row_stride * (25 % D::F32Vec::LEN) + (25 / D::F32Vec::LEN)]);
    v26.store_array(&mut data[row_stride * (26 % D::F32Vec::LEN) + (26 / D::F32Vec::LEN)]);
    v27.store_array(&mut data[row_stride * (27 % D::F32Vec::LEN) + (27 / D::F32Vec::LEN)]);
    v28.store_array(&mut data[row_stride * (28 % D::F32Vec::LEN) + (28 / D::F32Vec::LEN)]);
    v29.store_array(&mut data[row_stride * (29 % D::F32Vec::LEN) + (29 / D::F32Vec::LEN)]);
    v30.store_array(&mut data[row_stride * (30 % D::F32Vec::LEN) + (30 / D::F32Vec::LEN)]);
    v31.store_array(&mut data[row_stride * (31 % D::F32Vec::LEN) + (31 / D::F32Vec::LEN)]);
}

#[inline(always)]
pub(super) fn do_idct_32_trh<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
) {
    let row_stride = 16 / D::F32Vec::LEN;
    assert!(data.len() > 31 * row_stride);
    const { assert!(16usize.is_multiple_of(D::F32Vec::LEN)) };
    let mut v0 = D::F32Vec::load_array(d, &data[row_stride * 0]);
    let mut v1 = D::F32Vec::load_array(d, &data[row_stride * 2]);
    let mut v2 = D::F32Vec::load_array(d, &data[row_stride * 4]);
    let mut v3 = D::F32Vec::load_array(d, &data[row_stride * 6]);
    let mut v4 = D::F32Vec::load_array(d, &data[row_stride * 8]);
    let mut v5 = D::F32Vec::load_array(d, &data[row_stride * 10]);
    let mut v6 = D::F32Vec::load_array(d, &data[row_stride * 12]);
    let mut v7 = D::F32Vec::load_array(d, &data[row_stride * 14]);
    let mut v8 = D::F32Vec::load_array(d, &data[row_stride * 16]);
    let mut v9 = D::F32Vec::load_array(d, &data[row_stride * 18]);
    let mut v10 = D::F32Vec::load_array(d, &data[row_stride * 20]);
    let mut v11 = D::F32Vec::load_array(d, &data[row_stride * 22]);
    let mut v12 = D::F32Vec::load_array(d, &data[row_stride * 24]);
    let mut v13 = D::F32Vec::load_array(d, &data[row_stride * 26]);
    let mut v14 = D::F32Vec::load_array(d, &data[row_stride * 28]);
    let mut v15 = D::F32Vec::load_array(d, &data[row_stride * 30]);
    let mut v16 = D::F32Vec::load_array(d, &data[row_stride * 1]);
    let mut v17 = D::F32Vec::load_array(d, &data[row_stride * 3]);
    let mut v18 = D::F32Vec::load_array(d, &data[row_stride * 5]);
    let mut v19 = D::F32Vec::load_array(d, &data[row_stride * 7]);
    let mut v20 = D::F32Vec::load_array(d, &data[row_stride * 9]);
    let mut v21 = D::F32Vec::load_array(d, &data[row_stride * 11]);
    let mut v22 = D::F32Vec::load_array(d, &data[row_stride * 13]);
    let mut v23 = D::F32Vec::load_array(d, &data[row_stride * 15]);
    let mut v24 = D::F32Vec::load_array(d, &data[row_stride * 17]);
    let mut v25 = D::F32Vec::load_array(d, &data[row_stride * 19]);
    let mut v26 = D::F32Vec::load_array(d, &data[row_stride * 21]);
    let mut v27 = D::F32Vec::load_array(d, &data[row_stride * 23]);
    let mut v28 = D::F32Vec::load_array(d, &data[row_stride * 25]);
    let mut v29 = D::F32Vec::load_array(d, &data[row_stride * 27]);
    let mut v30 = D::F32Vec::load_array(d, &data[row_stride * 29]);
    let mut v31 = D::F32Vec::load_array(d, &data[row_stride * 31]);
    (
        v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19,
        v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
    ) = idct_32(
        d, v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18,
        v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
    );
    v0.store_array(&mut data[row_stride * 0]);
    v1.store_array(&mut data[row_stride * 1]);
    v2.store_array(&mut data[row_stride * 2]);
    v3.store_array(&mut data[row_stride * 3]);
    v4.store_array(&mut data[row_stride * 4]);
    v5.store_array(&mut data[row_stride * 5]);
    v6.store_array(&mut data[row_stride * 6]);
    v7.store_array(&mut data[row_stride * 7]);
    v8.store_array(&mut data[row_stride * 8]);
    v9.store_array(&mut data[row_stride * 9]);
    v10.store_array(&mut data[row_stride * 10]);
    v11.store_array(&mut data[row_stride * 11]);
    v12.store_array(&mut data[row_stride * 12]);
    v13.store_array(&mut data[row_stride * 13]);
    v14.store_array(&mut data[row_stride * 14]);
    v15.store_array(&mut data[row_stride * 15]);
    v16.store_array(&mut data[row_stride * 16]);
    v17.store_array(&mut data[row_stride * 17]);
    v18.store_array(&mut data[row_stride * 18]);
    v19.store_array(&mut data[row_stride * 19]);
    v20.store_array(&mut data[row_stride * 20]);
    v21.store_array(&mut data[row_stride * 21]);
    v22.store_array(&mut data[row_stride * 22]);
    v23.store_array(&mut data[row_stride * 23]);
    v24.store_array(&mut data[row_stride * 24]);
    v25.store_array(&mut data[row_stride * 25]);
    v26.store_array(&mut data[row_stride * 26]);
    v27.store_array(&mut data[row_stride * 27]);
    v28.store_array(&mut data[row_stride * 28]);
    v29.store_array(&mut data[row_stride * 29]);
    v30.store_array(&mut data[row_stride * 30]);
    v31.store_array(&mut data[row_stride * 31]);
}

#[inline(always)]
pub(super) fn do_idct_32_trq<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
) {
    let row_stride = 8 / D::F32Vec::LEN;
    assert!(data.len() > 31 * row_stride);
    const { assert!(8usize.is_multiple_of(D::F32Vec::LEN)) };
    let mut v0 = D::F32Vec::load_array(d, &data[row_stride * 0]);
    let mut v1 = D::F32Vec::load_array(d, &data[row_stride * 4]);
    let mut v2 = D::F32Vec::load_array(d, &data[row_stride * 8]);
    let mut v3 = D::F32Vec::load_array(d, &data[row_stride * 12]);
    let mut v4 = D::F32Vec::load_array(d, &data[row_stride * 16]);
    let mut v5 = D::F32Vec::load_array(d, &data[row_stride * 20]);
    let mut v6 = D::F32Vec::load_array(d, &data[row_stride * 24]);
    let mut v7 = D::F32Vec::load_array(d, &data[row_stride * 28]);
    let mut v8 = D::F32Vec::load_array(d, &data[row_stride * 1]);
    let mut v9 = D::F32Vec::load_array(d, &data[row_stride * 5]);
    let mut v10 = D::F32Vec::load_array(d, &data[row_stride * 9]);
    let mut v11 = D::F32Vec::load_array(d, &data[row_stride * 13]);
    let mut v12 = D::F32Vec::load_array(d, &data[row_stride * 17]);
    let mut v13 = D::F32Vec::load_array(d, &data[row_stride * 21]);
    let mut v14 = D::F32Vec::load_array(d, &data[row_stride * 25]);
    let mut v15 = D::F32Vec::load_array(d, &data[row_stride * 29]);
    let mut v16 = D::F32Vec::load_array(d, &data[row_stride * 2]);
    let mut v17 = D::F32Vec::load_array(d, &data[row_stride * 6]);
    let mut v18 = D::F32Vec::load_array(d, &data[row_stride * 10]);
    let mut v19 = D::F32Vec::load_array(d, &data[row_stride * 14]);
    let mut v20 = D::F32Vec::load_array(d, &data[row_stride * 18]);
    let mut v21 = D::F32Vec::load_array(d, &data[row_stride * 22]);
    let mut v22 = D::F32Vec::load_array(d, &data[row_stride * 26]);
    let mut v23 = D::F32Vec::load_array(d, &data[row_stride * 30]);
    let mut v24 = D::F32Vec::load_array(d, &data[row_stride * 3]);
    let mut v25 = D::F32Vec::load_array(d, &data[row_stride * 7]);
    let mut v26 = D::F32Vec::load_array(d, &data[row_stride * 11]);
    let mut v27 = D::F32Vec::load_array(d, &data[row_stride * 15]);
    let mut v28 = D::F32Vec::load_array(d, &data[row_stride * 19]);
    let mut v29 = D::F32Vec::load_array(d, &data[row_stride * 23]);
    let mut v30 = D::F32Vec::load_array(d, &data[row_stride * 27]);
    let mut v31 = D::F32Vec::load_array(d, &data[row_stride * 31]);
    (
        v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19,
        v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
    ) = idct_32(
        d, v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18,
        v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
    );
    v0.store_array(&mut data[row_stride * 0]);
    v1.store_array(&mut data[row_stride * 1]);
    v2.store_array(&mut data[row_stride * 2]);
    v3.store_array(&mut data[row_stride * 3]);
    v4.store_array(&mut data[row_stride * 4]);
    v5.store_array(&mut data[row_stride * 5]);
    v6.store_array(&mut data[row_stride * 6]);
    v7.store_array(&mut data[row_stride * 7]);
    v8.store_array(&mut data[row_stride * 8]);
    v9.store_array(&mut data[row_stride * 9]);
    v10.store_array(&mut data[row_stride * 10]);
    v11.store_array(&mut data[row_stride * 11]);
    v12.store_array(&mut data[row_stride * 12]);
    v13.store_array(&mut data[row_stride * 13]);
    v14.store_array(&mut data[row_stride * 14]);
    v15.store_array(&mut data[row_stride * 15]);
    v16.store_array(&mut data[row_stride * 16]);
    v17.store_array(&mut data[row_stride * 17]);
    v18.store_array(&mut data[row_stride * 18]);
    v19.store_array(&mut data[row_stride * 19]);
    v20.store_array(&mut data[row_stride * 20]);
    v21.store_array(&mut data[row_stride * 21]);
    v22.store_array(&mut data[row_stride * 22]);
    v23.store_array(&mut data[row_stride * 23]);
    v24.store_array(&mut data[row_stride * 24]);
    v25.store_array(&mut data[row_stride * 25]);
    v26.store_array(&mut data[row_stride * 26]);
    v27.store_array(&mut data[row_stride * 27]);
    v28.store_array(&mut data[row_stride * 28]);
    v29.store_array(&mut data[row_stride * 29]);
    v30.store_array(&mut data[row_stride * 30]);
    v31.store_array(&mut data[row_stride * 31]);
}
