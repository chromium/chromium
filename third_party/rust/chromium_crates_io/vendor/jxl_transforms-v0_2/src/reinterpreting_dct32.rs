// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#![allow(clippy::type_complexity)]
#![allow(clippy::erasing_op)]
#![allow(clippy::identity_op)]
use jxl_simd::{F32SimdVec, SimdDescriptor};

#[allow(clippy::too_many_arguments)]
#[allow(clippy::excessive_precision)]
#[inline(always)]
pub(super) fn reinterpreting_dct_32<D: SimdDescriptor>(
    d: D,
    v0: D::F32Vec,
    v1: D::F32Vec,
    v2: D::F32Vec,
    v3: D::F32Vec,
    v4: D::F32Vec,
    v5: D::F32Vec,
    v6: D::F32Vec,
    v7: D::F32Vec,
    v8: D::F32Vec,
    v9: D::F32Vec,
    v10: D::F32Vec,
    v11: D::F32Vec,
    v12: D::F32Vec,
    v13: D::F32Vec,
    v14: D::F32Vec,
    v15: D::F32Vec,
    v16: D::F32Vec,
    v17: D::F32Vec,
    v18: D::F32Vec,
    v19: D::F32Vec,
    v20: D::F32Vec,
    v21: D::F32Vec,
    v22: D::F32Vec,
    v23: D::F32Vec,
    v24: D::F32Vec,
    v25: D::F32Vec,
    v26: D::F32Vec,
    v27: D::F32Vec,
    v28: D::F32Vec,
    v29: D::F32Vec,
    v30: D::F32Vec,
    v31: D::F32Vec,
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
    let v32 = v0 + v31;
    let v33 = v1 + v30;
    let v34 = v2 + v29;
    let v35 = v3 + v28;
    let v36 = v4 + v27;
    let v37 = v5 + v26;
    let v38 = v6 + v25;
    let v39 = v7 + v24;
    let v40 = v8 + v23;
    let v41 = v9 + v22;
    let v42 = v10 + v21;
    let v43 = v11 + v20;
    let v44 = v12 + v19;
    let v45 = v13 + v18;
    let v46 = v14 + v17;
    let v47 = v15 + v16;
    let v48 = v32 + v47;
    let v49 = v33 + v46;
    let v50 = v34 + v45;
    let v51 = v35 + v44;
    let v52 = v36 + v43;
    let v53 = v37 + v42;
    let v54 = v38 + v41;
    let v55 = v39 + v40;
    let v56 = v48 + v55;
    let v57 = v49 + v54;
    let v58 = v50 + v53;
    let v59 = v51 + v52;
    let v60 = v56 + v59;
    let v61 = v57 + v58;
    let v62 = v60 + v61;
    let v63 = v60 - v61;
    let v64 = v56 - v59;
    let v65 = v57 - v58;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let v66 = v64 * mul;
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let v67 = v65 * mul;
    let v68 = v66 + v67;
    let v69 = v66 - v67;
    let v70 = v68.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v69);
    let v71 = v48 - v55;
    let v72 = v49 - v54;
    let v73 = v50 - v53;
    let v74 = v51 - v52;
    let mul = D::F32Vec::splat(d, 0.5097955791041592);
    let v75 = v71 * mul;
    let mul = D::F32Vec::splat(d, 0.6013448869350453);
    let v76 = v72 * mul;
    let mul = D::F32Vec::splat(d, 0.8999762231364156);
    let v77 = v73 * mul;
    let mul = D::F32Vec::splat(d, 2.5629154477415055);
    let v78 = v74 * mul;
    let v79 = v75 + v78;
    let v80 = v76 + v77;
    let v81 = v79 + v80;
    let v82 = v79 - v80;
    let v83 = v75 - v78;
    let v84 = v76 - v77;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let v85 = v83 * mul;
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let v86 = v84 * mul;
    let v87 = v85 + v86;
    let v88 = v85 - v86;
    let v89 = v87.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v88);
    let v90 = v81.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v89);
    let v91 = v89 + v82;
    let v92 = v82 + v88;
    let v93 = v32 - v47;
    let v94 = v33 - v46;
    let v95 = v34 - v45;
    let v96 = v35 - v44;
    let v97 = v36 - v43;
    let v98 = v37 - v42;
    let v99 = v38 - v41;
    let v100 = v39 - v40;
    let mul = D::F32Vec::splat(d, 0.5024192861881557);
    let v101 = v93 * mul;
    let mul = D::F32Vec::splat(d, 0.5224986149396889);
    let v102 = v94 * mul;
    let mul = D::F32Vec::splat(d, 0.5669440348163577);
    let v103 = v95 * mul;
    let mul = D::F32Vec::splat(d, 0.6468217833599901);
    let v104 = v96 * mul;
    let mul = D::F32Vec::splat(d, 0.7881546234512502);
    let v105 = v97 * mul;
    let mul = D::F32Vec::splat(d, 1.0606776859903471);
    let v106 = v98 * mul;
    let mul = D::F32Vec::splat(d, 1.7224470982383342);
    let v107 = v99 * mul;
    let mul = D::F32Vec::splat(d, 5.1011486186891553);
    let v108 = v100 * mul;
    let v109 = v101 + v108;
    let v110 = v102 + v107;
    let v111 = v103 + v106;
    let v112 = v104 + v105;
    let v113 = v109 + v112;
    let v114 = v110 + v111;
    let v115 = v113 + v114;
    let v116 = v113 - v114;
    let v117 = v109 - v112;
    let v118 = v110 - v111;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let v119 = v117 * mul;
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let v120 = v118 * mul;
    let v121 = v119 + v120;
    let v122 = v119 - v120;
    let v123 = v121.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v122);
    let v124 = v101 - v108;
    let v125 = v102 - v107;
    let v126 = v103 - v106;
    let v127 = v104 - v105;
    let mul = D::F32Vec::splat(d, 0.5097955791041592);
    let v128 = v124 * mul;
    let mul = D::F32Vec::splat(d, 0.6013448869350453);
    let v129 = v125 * mul;
    let mul = D::F32Vec::splat(d, 0.8999762231364156);
    let v130 = v126 * mul;
    let mul = D::F32Vec::splat(d, 2.5629154477415055);
    let v131 = v127 * mul;
    let v132 = v128 + v131;
    let v133 = v129 + v130;
    let v134 = v132 + v133;
    let v135 = v132 - v133;
    let v136 = v128 - v131;
    let v137 = v129 - v130;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let v138 = v136 * mul;
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let v139 = v137 * mul;
    let v140 = v138 + v139;
    let v141 = v138 - v139;
    let v142 = v140.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v141);
    let v143 = v134.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v142);
    let v144 = v142 + v135;
    let v145 = v135 + v141;
    let v146 = v115.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v143);
    let v147 = v143 + v123;
    let v148 = v123 + v144;
    let v149 = v144 + v116;
    let v150 = v116 + v145;
    let v151 = v145 + v122;
    let v152 = v122 + v141;
    let v153 = v0 - v31;
    let v154 = v1 - v30;
    let v155 = v2 - v29;
    let v156 = v3 - v28;
    let v157 = v4 - v27;
    let v158 = v5 - v26;
    let v159 = v6 - v25;
    let v160 = v7 - v24;
    let v161 = v8 - v23;
    let v162 = v9 - v22;
    let v163 = v10 - v21;
    let v164 = v11 - v20;
    let v165 = v12 - v19;
    let v166 = v13 - v18;
    let v167 = v14 - v17;
    let v168 = v15 - v16;
    let mul = D::F32Vec::splat(d, 0.5006029982351963);
    let v169 = v153 * mul;
    let mul = D::F32Vec::splat(d, 0.5054709598975436);
    let v170 = v154 * mul;
    let mul = D::F32Vec::splat(d, 0.5154473099226246);
    let v171 = v155 * mul;
    let mul = D::F32Vec::splat(d, 0.5310425910897841);
    let v172 = v156 * mul;
    let mul = D::F32Vec::splat(d, 0.5531038960344445);
    let v173 = v157 * mul;
    let mul = D::F32Vec::splat(d, 0.5829349682061339);
    let v174 = v158 * mul;
    let mul = D::F32Vec::splat(d, 0.6225041230356648);
    let v175 = v159 * mul;
    let mul = D::F32Vec::splat(d, 0.6748083414550057);
    let v176 = v160 * mul;
    let mul = D::F32Vec::splat(d, 0.7445362710022986);
    let v177 = v161 * mul;
    let mul = D::F32Vec::splat(d, 0.8393496454155268);
    let v178 = v162 * mul;
    let mul = D::F32Vec::splat(d, 0.9725682378619608);
    let v179 = v163 * mul;
    let mul = D::F32Vec::splat(d, 1.1694399334328847);
    let v180 = v164 * mul;
    let mul = D::F32Vec::splat(d, 1.4841646163141662);
    let v181 = v165 * mul;
    let mul = D::F32Vec::splat(d, 2.0577810099534108);
    let v182 = v166 * mul;
    let mul = D::F32Vec::splat(d, 3.4076084184687190);
    let v183 = v167 * mul;
    let mul = D::F32Vec::splat(d, 10.1900081235480329);
    let v184 = v168 * mul;
    let v185 = v169 + v184;
    let v186 = v170 + v183;
    let v187 = v171 + v182;
    let v188 = v172 + v181;
    let v189 = v173 + v180;
    let v190 = v174 + v179;
    let v191 = v175 + v178;
    let v192 = v176 + v177;
    let v193 = v185 + v192;
    let v194 = v186 + v191;
    let v195 = v187 + v190;
    let v196 = v188 + v189;
    let v197 = v193 + v196;
    let v198 = v194 + v195;
    let v199 = v197 + v198;
    let v200 = v197 - v198;
    let v201 = v193 - v196;
    let v202 = v194 - v195;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let v203 = v201 * mul;
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let v204 = v202 * mul;
    let v205 = v203 + v204;
    let v206 = v203 - v204;
    let v207 = v205.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v206);
    let v208 = v185 - v192;
    let v209 = v186 - v191;
    let v210 = v187 - v190;
    let v211 = v188 - v189;
    let mul = D::F32Vec::splat(d, 0.5097955791041592);
    let v212 = v208 * mul;
    let mul = D::F32Vec::splat(d, 0.6013448869350453);
    let v213 = v209 * mul;
    let mul = D::F32Vec::splat(d, 0.8999762231364156);
    let v214 = v210 * mul;
    let mul = D::F32Vec::splat(d, 2.5629154477415055);
    let v215 = v211 * mul;
    let v216 = v212 + v215;
    let v217 = v213 + v214;
    let v218 = v216 + v217;
    let v219 = v216 - v217;
    let v220 = v212 - v215;
    let v221 = v213 - v214;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let v222 = v220 * mul;
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let v223 = v221 * mul;
    let v224 = v222 + v223;
    let v225 = v222 - v223;
    let v226 = v224.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v225);
    let v227 = v218.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v226);
    let v228 = v226 + v219;
    let v229 = v219 + v225;
    let v230 = v169 - v184;
    let v231 = v170 - v183;
    let v232 = v171 - v182;
    let v233 = v172 - v181;
    let v234 = v173 - v180;
    let v235 = v174 - v179;
    let v236 = v175 - v178;
    let v237 = v176 - v177;
    let mul = D::F32Vec::splat(d, 0.5024192861881557);
    let v238 = v230 * mul;
    let mul = D::F32Vec::splat(d, 0.5224986149396889);
    let v239 = v231 * mul;
    let mul = D::F32Vec::splat(d, 0.5669440348163577);
    let v240 = v232 * mul;
    let mul = D::F32Vec::splat(d, 0.6468217833599901);
    let v241 = v233 * mul;
    let mul = D::F32Vec::splat(d, 0.7881546234512502);
    let v242 = v234 * mul;
    let mul = D::F32Vec::splat(d, 1.0606776859903471);
    let v243 = v235 * mul;
    let mul = D::F32Vec::splat(d, 1.7224470982383342);
    let v244 = v236 * mul;
    let mul = D::F32Vec::splat(d, 5.1011486186891553);
    let v245 = v237 * mul;
    let v246 = v238 + v245;
    let v247 = v239 + v244;
    let v248 = v240 + v243;
    let v249 = v241 + v242;
    let v250 = v246 + v249;
    let v251 = v247 + v248;
    let v252 = v250 + v251;
    let v253 = v250 - v251;
    let v254 = v246 - v249;
    let v255 = v247 - v248;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let v256 = v254 * mul;
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let v257 = v255 * mul;
    let v258 = v256 + v257;
    let v259 = v256 - v257;
    let v260 = v258.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v259);
    let v261 = v238 - v245;
    let v262 = v239 - v244;
    let v263 = v240 - v243;
    let v264 = v241 - v242;
    let mul = D::F32Vec::splat(d, 0.5097955791041592);
    let v265 = v261 * mul;
    let mul = D::F32Vec::splat(d, 0.6013448869350453);
    let v266 = v262 * mul;
    let mul = D::F32Vec::splat(d, 0.8999762231364156);
    let v267 = v263 * mul;
    let mul = D::F32Vec::splat(d, 2.5629154477415055);
    let v268 = v264 * mul;
    let v269 = v265 + v268;
    let v270 = v266 + v267;
    let v271 = v269 + v270;
    let v272 = v269 - v270;
    let v273 = v265 - v268;
    let v274 = v266 - v267;
    let mul = D::F32Vec::splat(d, 0.5411961001461970);
    let v275 = v273 * mul;
    let mul = D::F32Vec::splat(d, 1.3065629648763764);
    let v276 = v274 * mul;
    let v277 = v275 + v276;
    let v278 = v275 - v276;
    let v279 = v277.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v278);
    let v280 = v271.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v279);
    let v281 = v279 + v272;
    let v282 = v272 + v278;
    let v283 = v252.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v280);
    let v284 = v280 + v260;
    let v285 = v260 + v281;
    let v286 = v281 + v253;
    let v287 = v253 + v282;
    let v288 = v282 + v259;
    let v289 = v259 + v278;
    let v290 = v199.mul_add(D::F32Vec::splat(d, std::f32::consts::SQRT_2), v283);
    let v291 = v283 + v227;
    let v292 = v227 + v284;
    let v293 = v284 + v207;
    let v294 = v207 + v285;
    let v295 = v285 + v228;
    let v296 = v228 + v286;
    let v297 = v286 + v200;
    let v298 = v200 + v287;
    let v299 = v287 + v229;
    let v300 = v229 + v288;
    let v301 = v288 + v206;
    let v302 = v206 + v289;
    let v303 = v289 + v225;
    let v304 = v225 + v278;
    (
        v62 * D::F32Vec::splat(d, 0.031250),
        v290 * D::F32Vec::splat(d, 0.031262),
        v146 * D::F32Vec::splat(d, 0.031299),
        v291 * D::F32Vec::splat(d, 0.031361),
        v90 * D::F32Vec::splat(d, 0.031449),
        v292 * D::F32Vec::splat(d, 0.031561),
        v147 * D::F32Vec::splat(d, 0.031699),
        v293 * D::F32Vec::splat(d, 0.031864),
        v70 * D::F32Vec::splat(d, 0.032055),
        v294 * D::F32Vec::splat(d, 0.032274),
        v148 * D::F32Vec::splat(d, 0.032521),
        v295 * D::F32Vec::splat(d, 0.032797),
        v91 * D::F32Vec::splat(d, 0.033103),
        v296 * D::F32Vec::splat(d, 0.033441),
        v149 * D::F32Vec::splat(d, 0.033811),
        v297 * D::F32Vec::splat(d, 0.034215),
        v63 * D::F32Vec::splat(d, 0.034654),
        v298 * D::F32Vec::splat(d, 0.035131),
        v150 * D::F32Vec::splat(d, 0.035647),
        v299 * D::F32Vec::splat(d, 0.036204),
        v92 * D::F32Vec::splat(d, 0.036806),
        v300 * D::F32Vec::splat(d, 0.037453),
        v151 * D::F32Vec::splat(d, 0.038150),
        v301 * D::F32Vec::splat(d, 0.038899),
        v69 * D::F32Vec::splat(d, 0.039705),
        v302 * D::F32Vec::splat(d, 0.040571),
        v152 * D::F32Vec::splat(d, 0.041502),
        v303 * D::F32Vec::splat(d, 0.042502),
        v88 * D::F32Vec::splat(d, 0.043578),
        v304 * D::F32Vec::splat(d, 0.044735),
        v141 * D::F32Vec::splat(d, 0.045981),
        v278 * D::F32Vec::splat(d, 0.047324),
    )
}

#[inline(always)]
pub(super) fn do_reinterpreting_dct_32<D: SimdDescriptor>(
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
    ) = reinterpreting_dct_32(
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
pub(super) fn do_reinterpreting_dct_32_rowblock<D: SimdDescriptor>(
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
    ) = reinterpreting_dct_32(
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
pub(super) fn do_reinterpreting_dct_32_trh<D: SimdDescriptor>(
    d: D,
    data: &mut [<D::F32Vec as F32SimdVec>::UnderlyingArray],
) {
    let row_stride = 16 / D::F32Vec::LEN;
    assert!(data.len() > 31 * row_stride);
    const { assert!(16usize.is_multiple_of(D::F32Vec::LEN)) };
    let mut v0 = D::F32Vec::load_array(d, &data[row_stride * 0]);
    let mut v1 = D::F32Vec::load_array(d, &data[row_stride * 1]);
    let mut v2 = D::F32Vec::load_array(d, &data[row_stride * 2]);
    let mut v3 = D::F32Vec::load_array(d, &data[row_stride * 3]);
    let mut v4 = D::F32Vec::load_array(d, &data[row_stride * 4]);
    let mut v5 = D::F32Vec::load_array(d, &data[row_stride * 5]);
    let mut v6 = D::F32Vec::load_array(d, &data[row_stride * 6]);
    let mut v7 = D::F32Vec::load_array(d, &data[row_stride * 7]);
    let mut v8 = D::F32Vec::load_array(d, &data[row_stride * 8]);
    let mut v9 = D::F32Vec::load_array(d, &data[row_stride * 9]);
    let mut v10 = D::F32Vec::load_array(d, &data[row_stride * 10]);
    let mut v11 = D::F32Vec::load_array(d, &data[row_stride * 11]);
    let mut v12 = D::F32Vec::load_array(d, &data[row_stride * 12]);
    let mut v13 = D::F32Vec::load_array(d, &data[row_stride * 13]);
    let mut v14 = D::F32Vec::load_array(d, &data[row_stride * 14]);
    let mut v15 = D::F32Vec::load_array(d, &data[row_stride * 15]);
    let mut v16 = D::F32Vec::load_array(d, &data[row_stride * 16]);
    let mut v17 = D::F32Vec::load_array(d, &data[row_stride * 17]);
    let mut v18 = D::F32Vec::load_array(d, &data[row_stride * 18]);
    let mut v19 = D::F32Vec::load_array(d, &data[row_stride * 19]);
    let mut v20 = D::F32Vec::load_array(d, &data[row_stride * 20]);
    let mut v21 = D::F32Vec::load_array(d, &data[row_stride * 21]);
    let mut v22 = D::F32Vec::load_array(d, &data[row_stride * 22]);
    let mut v23 = D::F32Vec::load_array(d, &data[row_stride * 23]);
    let mut v24 = D::F32Vec::load_array(d, &data[row_stride * 24]);
    let mut v25 = D::F32Vec::load_array(d, &data[row_stride * 25]);
    let mut v26 = D::F32Vec::load_array(d, &data[row_stride * 26]);
    let mut v27 = D::F32Vec::load_array(d, &data[row_stride * 27]);
    let mut v28 = D::F32Vec::load_array(d, &data[row_stride * 28]);
    let mut v29 = D::F32Vec::load_array(d, &data[row_stride * 29]);
    let mut v30 = D::F32Vec::load_array(d, &data[row_stride * 30]);
    let mut v31 = D::F32Vec::load_array(d, &data[row_stride * 31]);
    (
        v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19,
        v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
    ) = reinterpreting_dct_32(
        d, v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18,
        v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
    );
    v0.store_array(&mut data[row_stride * 0]);
    v16.store_array(&mut data[row_stride * 1]);
    v1.store_array(&mut data[row_stride * 2]);
    v17.store_array(&mut data[row_stride * 3]);
    v2.store_array(&mut data[row_stride * 4]);
    v18.store_array(&mut data[row_stride * 5]);
    v3.store_array(&mut data[row_stride * 6]);
    v19.store_array(&mut data[row_stride * 7]);
    v4.store_array(&mut data[row_stride * 8]);
    v20.store_array(&mut data[row_stride * 9]);
    v5.store_array(&mut data[row_stride * 10]);
    v21.store_array(&mut data[row_stride * 11]);
    v6.store_array(&mut data[row_stride * 12]);
    v22.store_array(&mut data[row_stride * 13]);
    v7.store_array(&mut data[row_stride * 14]);
    v23.store_array(&mut data[row_stride * 15]);
    v8.store_array(&mut data[row_stride * 16]);
    v24.store_array(&mut data[row_stride * 17]);
    v9.store_array(&mut data[row_stride * 18]);
    v25.store_array(&mut data[row_stride * 19]);
    v10.store_array(&mut data[row_stride * 20]);
    v26.store_array(&mut data[row_stride * 21]);
    v11.store_array(&mut data[row_stride * 22]);
    v27.store_array(&mut data[row_stride * 23]);
    v12.store_array(&mut data[row_stride * 24]);
    v28.store_array(&mut data[row_stride * 25]);
    v13.store_array(&mut data[row_stride * 26]);
    v29.store_array(&mut data[row_stride * 27]);
    v14.store_array(&mut data[row_stride * 28]);
    v30.store_array(&mut data[row_stride * 29]);
    v15.store_array(&mut data[row_stride * 30]);
    v31.store_array(&mut data[row_stride * 31]);
}
