﻿// -----------------------------------------------------------------------------------------
// NVEnc by rigaya
// -----------------------------------------------------------------------------------------
//
// The MIT License
//
// Copyright (c) 2014-2016 rigaya
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// ------------------------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <set>
#include <sstream>
#include <iomanip>
#include <Windows.h>
#include <shellapi.h>
#include "rgy_version.h"
#include "rgy_perf_monitor.h"
#include "rgy_caption.h"
#include "NVEncParam.h"
#include "NVEncCmd.h"
#include "NVEncFilterAfs.h"
#include "rgy_avutil.h"

tstring GetNVEncVersion() {
    static const TCHAR *const ENABLED_INFO[] = { _T("disabled"), _T("enabled") };
    tstring version;
    version += get_encoder_version();
    version += _T("\n");
    version += strsprintf(_T("  [NVENC API v%d.%d, CUDA %d.%d]\n"),
        NVENCAPI_MAJOR_VERSION, NVENCAPI_MINOR_VERSION,
        CUDART_VERSION / 1000, (CUDART_VERSION % 1000) / 10);
    version += _T(" reader: raw");
    if (ENABLE_AVI_READER) version += _T(", avi");
    if (ENABLE_AVISYNTH_READER) version += _T(", avs");
    if (ENABLE_VAPOURSYNTH_READER) version += _T(", vpy");
#if ENABLE_AVSW_READER
    version += strsprintf(_T(", avhw [%s]"), getHWDecSupportedCodecList().c_str());
#endif //#if ENABLE_AVSW_READER
    version += _T("\n");
    return version;
}

const TCHAR *cmd_short_opt_to_long(TCHAR short_opt) {
    const TCHAR *option_name = nullptr;
    switch (short_opt) {
    case _T('b'):
        option_name = _T("bframes");
        break;
    case _T('c'):
        option_name = _T("codec");
        break;
    case _T('d'):
        option_name = _T("device");
        break;
    case _T('u'):
        option_name = _T("preset");
        break;
    case _T('f'):
        option_name = _T("output-format");
        break;
    case _T('i'):
        option_name = _T("input");
        break;
    case _T('o'):
        option_name = _T("output");
        break;
    case _T('m'):
        option_name = _T("mux-option");
        break;
    case _T('v'):
        option_name = _T("version");
        break;
    case _T('h'):
    case _T('?'):
        option_name = _T("help");
        break;
    default:
        break;
    }
    return option_name;
}

bool get_list_guid_value(const guid_desc *list, const TCHAR *chr, int *value) {
    for (int i = 0; list[i].desc; i++) {
        if (0 == _tcsicmp(list[i].desc, chr)) {
            *value = list[i].value;
            return true;
        }
    }
    return false;
};

int parse_qp(int a[3], const TCHAR *str) {
    memset(a, 0, sizeof(a));
    if (   3 == _stscanf_s(str, _T("%d:%d:%d"), &a[0], &a[1], &a[2])
        || 3 == _stscanf_s(str, _T("%d/%d/%d"), &a[0], &a[1], &a[2])
        || 3 == _stscanf_s(str, _T("%d.%d.%d"), &a[0], &a[1], &a[2])
        || 3 == _stscanf_s(str, _T("%d,%d,%d"), &a[0], &a[1], &a[2])) {
        return 3;
    }
    if (   2 == _stscanf_s(str, _T("%d:%d"), &a[0], &a[1])
        || 2 == _stscanf_s(str, _T("%d/%d"), &a[0], &a[1])
        || 2 == _stscanf_s(str, _T("%d.%d"), &a[0], &a[1])
        || 2 == _stscanf_s(str, _T("%d,%d"), &a[0], &a[1])) {
        return 2;
    }
    if (1 == _stscanf_s(str, _T("%d"), &a[0])) {
        return 1;
    }
    return 0;
}

int parse_one_option(const TCHAR *option_name, const TCHAR* strInput[], int& i, int nArgNum, InEncodeVideoParam *pParams, NV_ENC_CODEC_CONFIG *codecPrm, sArgsData *argData, ParseCmdError& err) {
    if (IS_OPTION("device")) {
        int deviceid = -1;
        if (i + 1 < nArgNum) {
            i++;
            int value = 0;
            if (1 == _stscanf_s(strInput[i], _T("%d"), &value)) {
                deviceid = value;
            }
        }
        if (deviceid < 0) {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        pParams->deviceID = deviceid;
        return 0;
    }
    if (IS_OPTION("preset")) {
        i++;
        int value = get_value_from_name(strInput[i], list_nvenc_preset_names);
        if (value >= 0) {
            pParams->preset = value;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("codec")) {
        i++;
        int value = 0;
        if (get_list_value(list_nvenc_codecs_for_opt, strInput[i], &value)) {
            pParams->codec = value;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("cqp")) {
        i++;
        int a[3] = { 0 };
        int ret = parse_qp(a, strInput[i]);
        if (ret == 0) {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        pParams->encConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CONSTQP;
        pParams->encConfig.rcParams.constQP.qpIntra  = a[0];
        pParams->encConfig.rcParams.constQP.qpInterP = (ret > 1) ? a[1] : a[ret-1];
        pParams->encConfig.rcParams.constQP.qpInterB = (ret > 2) ? a[2] : a[ret-1];
        return 0;
    }
    if (IS_OPTION("vbr")) {
        i++;
        int value = 0;
        if (1 == _stscanf_s(strInput[i], _T("%d"), &value)) {
            pParams->encConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_VBR;
            pParams->encConfig.rcParams.averageBitRate = value * 1000;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("vbrhq") || IS_OPTION("vbr2")) {
        i++;
        int value = 0;
        if (1 == _stscanf_s(strInput[i], _T("%d"), &value)) {
            pParams->encConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_VBR_HQ;
            pParams->encConfig.rcParams.averageBitRate = value * 1000;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("cbr")) {
        i++;
        int value = 0;
        if (1 == _stscanf_s(strInput[i], _T("%d"), &value)) {
            pParams->encConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
            pParams->encConfig.rcParams.averageBitRate = value * 1000;
            pParams->encConfig.rcParams.maxBitRate = value * 1000;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("cbrhq")) {
        i++;
        int value = 0;
        if (1 == _stscanf_s(strInput[i], _T("%d"), &value)) {
            pParams->encConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR_HQ;
            pParams->encConfig.rcParams.averageBitRate = value * 1000;
            pParams->encConfig.rcParams.maxBitRate = value * 1000;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("vbr-quality")) {
        i++;
        double value = 0;
        if (1 == _stscanf_s(strInput[i], _T("%lf"), &value)) {
            value = (std::max)(0.0, value);
            int value_int = (int)value;
            pParams->encConfig.rcParams.targetQuality = (uint8_t)clamp(value_int, 0, 51);
            pParams->encConfig.rcParams.targetQualityLSB = (uint8_t)clamp((int)((value - value_int) * 256.0), 0, 255);
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("dynamic-rc")) {
        if (i+1 >= nArgNum || strInput[i+1][0] == _T('-')) {
            return 0;
        }
        i++;
        bool rc_mode_defined = false;
        DynamicRCParam rcPrm;
        for (const auto &param : split(strInput[i], _T(","))) {
            auto pos = param.find_first_of(_T("="));
            if (pos != std::string::npos) {
                auto param_arg = param.substr(0, pos);
                auto param_val = param.substr(pos+1);
                param_arg = tolowercase(param_arg);
                if (param_arg == _T("start")) {
                    try {
                        rcPrm.start = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("end")) {
                    try {
                        rcPrm.end = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("cqp")) {
                    int a[3] = { 0 };
                    int ret = parse_qp(a, strInput[i]);
                    if (ret == 0) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    rcPrm.rc_mode = NV_ENC_PARAMS_RC_CONSTQP;
                    rcPrm.qp.qpIntra  = a[0];
                    rcPrm.qp.qpInterP = (ret > 1) ? a[1] : a[ret-1];
                    rcPrm.qp.qpInterB = (ret > 2) ? a[2] : a[ret-1];
                    rc_mode_defined = true;
                    continue;
                }
                int temp = 0;
                if (get_list_value(list_nvenc_rc_method_en, touppercase(param_arg).c_str(), &temp)) {
                    try {
                        rcPrm.avg_bitrate = std::stoi(param_val) * 1000;
                        rcPrm.rc_mode = (NV_ENC_PARAMS_RC_MODE)temp;
                        rc_mode_defined = true;
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("max-bitrate")) {
                    try {
                        rcPrm.max_bitrate = std::stoi(param_val) * 1000;
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("vbr-quality")) {
                    try {
                        auto value = (std::max)(0.0, std::stod(param_val));
                        int value_int = (int)value;
                        rcPrm.targetQuality = (uint8_t)clamp(value_int, 0, 51);
                        rcPrm.targetQualityLSB = (uint8_t)clamp((int)((value - value_int) * 256.0), 0, 255);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            } else {
                pos = param.find_first_of(_T(":"));
                if (pos != std::string::npos) {
                    auto param_val0 = param.substr(0, pos);
                    auto param_val1 = param.substr(pos+1);
                    try {
                        rcPrm.start = std::stoi(param_val0);
                        rcPrm.end   = std::stoi(param_val1);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            }
        }
        if (!rc_mode_defined) {
            CMD_PARSE_SET_ERR(strInput[0], _T("rate control mode unspecified!"), option_name, strInput[i]);
            return -1;
        }
        if (rcPrm.start < 0) {
            CMD_PARSE_SET_ERR(strInput[0], _T("start frame ID unspecified!"), option_name, strInput[i]);
            return -1;
        }
        if (rcPrm.end > 0 && rcPrm.start > rcPrm.end) {
            CMD_PARSE_SET_ERR(strInput[0], _T("start frame ID must be smaller than end frame ID!"), option_name, strInput[i]);
            return -1;
        }
        pParams->dynamicRC.push_back(rcPrm);
        return 0;
    }
    if (IS_OPTION("qp-init") || IS_OPTION("qp-max") || IS_OPTION("qp-min")) {
        i++;
        int a[4] = { 0 };
        if (   4 == _stscanf_s(strInput[i], _T("%d;%d:%d:%d"), &a[3], &a[0], &a[1], &a[2])
            || 4 == _stscanf_s(strInput[i], _T("%d;%d/%d/%d"), &a[3], &a[0], &a[1], &a[2])
            || 4 == _stscanf_s(strInput[i], _T("%d;%d.%d.%d"), &a[3], &a[0], &a[1], &a[2])
            || 4 == _stscanf_s(strInput[i], _T("%d;%d,%d,%d"), &a[3], &a[0], &a[1], &a[2])) {
            a[3] = a[3] ? 1 : 0;
        } else if (
               3 == _stscanf_s(strInput[i], _T("%d:%d:%d"), &a[0], &a[1], &a[2])
            || 3 == _stscanf_s(strInput[i], _T("%d/%d/%d"), &a[0], &a[1], &a[2])
            || 3 == _stscanf_s(strInput[i], _T("%d.%d.%d"), &a[0], &a[1], &a[2])
            || 3 == _stscanf_s(strInput[i], _T("%d,%d,%d"), &a[0], &a[1], &a[2])) {
            a[3] = 1;
        } else if (
               3 == _stscanf_s(strInput[i], _T("%d;%d:%d"), &a[3], &a[0], &a[1])
            || 3 == _stscanf_s(strInput[i], _T("%d;%d/%d"), &a[3], &a[0], &a[1])
            || 3 == _stscanf_s(strInput[i], _T("%d;%d.%d"), &a[3], &a[0], &a[1])
            || 3 == _stscanf_s(strInput[i], _T("%d;%d,%d"), &a[3], &a[0], &a[1])) {
            a[3] = a[3] ? 1 : 0;
            a[2] = a[1];
        } else if (
               2 == _stscanf_s(strInput[i], _T("%d:%d"), &a[0], &a[1])
            || 2 == _stscanf_s(strInput[i], _T("%d/%d"), &a[0], &a[1])
            || 2 == _stscanf_s(strInput[i], _T("%d.%d"), &a[0], &a[1])
            || 2 == _stscanf_s(strInput[i], _T("%d,%d"), &a[0], &a[1])) {
            a[3] = 1;
            a[2] = a[1];
        } else if (2 == _stscanf_s(strInput[i], _T("%d;%d"), &a[3], &a[0])) {
            a[3] = a[3] ? 1 : 0;
            a[1] = a[0];
            a[2] = a[0];
        } else if (1 == _stscanf_s(strInput[i], _T("%d"), &a[0])) {
            a[3] = 1;
            a[1] = a[0];
            a[2] = a[0];
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        NV_ENC_QP *ptrQP = nullptr;
        if (IS_OPTION("qp-init")) {
            pParams->encConfig.rcParams.enableInitialRCQP = a[3];
            ptrQP = &pParams->encConfig.rcParams.initialRCQP;
        } else if (IS_OPTION("qp-max")) {
            pParams->encConfig.rcParams.enableMaxQP = a[3];
            ptrQP = &pParams->encConfig.rcParams.maxQP;
        } else if (IS_OPTION("qp-min")) {
            pParams->encConfig.rcParams.enableMinQP = a[3];
            ptrQP = &pParams->encConfig.rcParams.minQP;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        ptrQP->qpIntra  = a[0];
        ptrQP->qpInterP = a[1];
        ptrQP->qpInterB = a[2];
        return 0;
    }
    if (IS_OPTION("gop-len")) {
        i++;
        int value = 0;
        if (0 == _tcsnccmp(strInput[i], _T("auto"), _tcslen(_T("auto")))) {
            pParams->encConfig.gopLength = 0;
        } else if (1 == _stscanf_s(strInput[i], _T("%d"), &value)) {
            pParams->encConfig.gopLength = value;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("strict-gop")) {
        pParams->encConfig.rcParams.strictGOPTarget = 1;
        return 0;
    }
    if (IS_OPTION("bframes")) {
        i++;
        int value = 0;
        if (1 == _stscanf_s(strInput[i], _T("%d"), &value)) {
            pParams->encConfig.frameIntervalP = value + 1;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("bref-mode")) {
        i++;
        int value = 0;
        if (get_list_value(list_bref_mode, strInput[i], &value)) {
            codecPrm[NV_ENC_H264].h264Config.useBFramesAsRef = (NV_ENC_BFRAME_REF_MODE)value;
            codecPrm[NV_ENC_HEVC].hevcConfig.useBFramesAsRef = (NV_ENC_BFRAME_REF_MODE)value;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("max-bitrate") || IS_OPTION("maxbitrate")) {
        i++;
        int value = 0;
        if (1 == _stscanf_s(strInput[i], _T("%d"), &value)) {
            pParams->encConfig.rcParams.maxBitRate = value * 1000;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("lookahead")) {
        i++;
        int value = 0;
        if (1 == _stscanf_s(strInput[i], _T("%d"), &value)) {
            pParams->encConfig.rcParams.enableLookahead = value > 0;
            pParams->encConfig.rcParams.lookaheadDepth = (uint16_t)clamp(value, 0, 32);
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("no-i-adapt")) {
        pParams->encConfig.rcParams.disableIadapt = 1;
        return 0;
    }
    if (IS_OPTION("no-b-adapt")) {
        pParams->encConfig.rcParams.disableBadapt = 1;
        return 0;
    }
    if (IS_OPTION("vbv-bufsize")) {
        i++;
        int value = 0;
        if (1 == _stscanf_s(strInput[i], _T("%d"), &value)) {
            pParams->encConfig.rcParams.vbvBufferSize = value * 1000;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("aq")) {
        pParams->encConfig.rcParams.enableAQ = 1;
        return 0;
    }
    if (IS_OPTION("aq-temporal")) {
        pParams->encConfig.rcParams.enableTemporalAQ = 1;
        return 0;
    }
    if (IS_OPTION("aq-strength")) {
        i++;
        int value = 0;
        if (1 == _stscanf_s(strInput[i], _T("%d"), &value)) {
            pParams->encConfig.rcParams.aqStrength = clamp(value, 0, 15);
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("disable-aq")
        || IS_OPTION("no-aq")) {
        pParams->encConfig.rcParams.enableAQ = 0;
        return 0;
    }
    if (IS_OPTION("direct")) {
        i++;
        int value = 0;
        if (get_list_value(list_bdirect, strInput[i], &value)) {
            codecPrm[NV_ENC_H264].h264Config.bdirectMode = (NV_ENC_H264_BDIRECT_MODE)value;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("adapt-transform")) {
        codecPrm[NV_ENC_H264].h264Config.adaptiveTransformMode = NV_ENC_H264_ADAPTIVE_TRANSFORM_ENABLE;
        return 0;
    }
    if (IS_OPTION("no-adapt-transform")) {
        codecPrm[NV_ENC_H264].h264Config.adaptiveTransformMode = NV_ENC_H264_ADAPTIVE_TRANSFORM_DISABLE;
        return 0;
    }
    if (IS_OPTION("ref")) {
        i++;
        try {
            int value = std::stoi(strInput[i]);
            codecPrm[NV_ENC_H264].h264Config.maxNumRefFrames = value;
            codecPrm[NV_ENC_HEVC].hevcConfig.maxNumRefFramesInDPB = value;
        } catch (...) {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("multiref-l0")) {
        i++;
        int value = 0;
        if (get_list_value(list_num_refs, strInput[i], &value)) {
            codecPrm[NV_ENC_H264].h264Config.numRefL0 = (NV_ENC_NUM_REF_FRAMES)value;
            codecPrm[NV_ENC_HEVC].hevcConfig.numRefL0 = (NV_ENC_NUM_REF_FRAMES)value;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("multiref-l1")) {
        i++;
        int value = 0;
        if (get_list_value(list_num_refs, strInput[i], &value)) {
            codecPrm[NV_ENC_H264].h264Config.numRefL1 = (NV_ENC_NUM_REF_FRAMES)value;
            codecPrm[NV_ENC_HEVC].hevcConfig.numRefL1 = (NV_ENC_NUM_REF_FRAMES)value;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("weightp")) {
        if (i+1 >= nArgNum || strInput[i+1][0] == _T('-')) {
            pParams->nWeightP = 1;
            return 0;
        }
        i++;
        if (0 == _tcscmp(strInput[i], _T("force"))) {
            pParams->nWeightP = 2;
        }
        return 0;
    }
    if (IS_OPTION("nonrefp")) {
        pParams->encConfig.rcParams.enableNonRefP = 1;
        return 0;
    }
    if (IS_OPTION("mv-precision")) {
        i++;
        int value = 0;
        if (get_list_value(list_mv_presicion, strInput[i], &value)) {
            pParams->encConfig.mvPrecision = (NV_ENC_MV_PRECISION)value;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("vpp-deinterlace")) {
        i++;
        int value = 0;
        if (get_list_value(list_deinterlace, strInput[i], &value)) {
            pParams->vpp.deinterlace = (cudaVideoDeinterlaceMode)value;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        if (pParams->vpp.deinterlace != cudaVideoDeinterlaceMode_Weave
            && (pParams->input.picstruct & RGY_PICSTRUCT_INTERLACED) == 0) {
            pParams->input.picstruct = RGY_PICSTRUCT_AUTO;
        }
        return 0;
    }
    if (IS_OPTION("vpp-resize")) {
        i++;
        int value = 0;
        if (get_list_value(list_nppi_resize, strInput[i], &value)) {
            pParams->vpp.resizeInterp = (NppiInterpolationMode)value;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("vpp-gauss")) {
        i++;
        int value = 0;
        if (get_list_value(list_nppi_gauss, strInput[i], &value)) {
            pParams->vpp.gaussMaskSize = (NppiMaskSize)value;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("vpp-unsharp")) {
        pParams->vpp.unsharp.enable = true;
        if (i+1 >= nArgNum || strInput[i+1][0] == _T('-')) {
            pParams->vpp.unsharp.radius = FILTER_DEFAULT_UNSHARP_RADIUS;
            return 0;
        }
        i++;
        for (const auto& param : split(strInput[i], _T(","))) {
            auto pos = param.find_first_of(_T("="));
            if (pos != std::string::npos) {
                auto param_arg = tolowercase(param.substr(0, pos));
                auto param_val = param.substr(pos+1);
                if (param_arg == _T("enable")) {
                    if (param_val == _T("true")) {
                        pParams->vpp.unsharp.enable = true;
                    } else if (param_val == _T("false")) {
                        pParams->vpp.unsharp.enable = false;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("radius")) {
                    try {
                        pParams->vpp.unsharp.radius = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("weight")) {
                    try {
                        pParams->vpp.unsharp.weight = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("threshold")) {
                    try {
                        pParams->vpp.unsharp.threshold = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            }
        }
        return 0;
    }
    if (IS_OPTION("vpp-edgelevel")) {
        pParams->vpp.edgelevel.enable = true;
        if (i+1 >= nArgNum || strInput[i+1][0] == _T('-')) {
            return 0;
        }
        i++;
        for (const auto& param : split(strInput[i], _T(","))) {
            auto pos = param.find_first_of(_T("="));
            if (pos != std::string::npos) {
                auto param_arg = param.substr(0, pos);
                auto param_val = param.substr(pos+1);
                param_arg = tolowercase(param_arg);
                if (param_arg == _T("enable")) {
                    if (param_val == _T("true")) {
                        pParams->vpp.edgelevel.enable = true;
                    } else if (param_val == _T("false")) {
                        pParams->vpp.edgelevel.enable = false;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("strength")) {
                    try {
                        pParams->vpp.edgelevel.strength = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("threshold")) {
                    try {
                        pParams->vpp.edgelevel.threshold = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("black")) {
                    try {
                        pParams->vpp.edgelevel.black = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("white")) {
                    try {
                        pParams->vpp.edgelevel.white = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            }
        }
        return 0;
    }
    if (IS_OPTION("vpp-delogo-select")) {
        i++;
        pParams->vpp.delogo.logoSelect = strInput[i];
        return 0;
    }
    if (IS_OPTION("vpp-delogo-add")) {
        pParams->vpp.delogo.mode = DELOGO_MODE_ADD;
        return 0;
    }
    if (IS_OPTION("vpp-delogo-pos")) {
        i++;
        int posOffsetX, posOffsetY;
        if (   2 != _stscanf_s(strInput[i], _T("%dx%d"), &posOffsetX, &posOffsetY)
            && 2 != _stscanf_s(strInput[i], _T("%d,%d"), &posOffsetX, &posOffsetY)
            && 2 != _stscanf_s(strInput[i], _T("%d/%d"), &posOffsetX, &posOffsetY)
            && 2 != _stscanf_s(strInput[i], _T("%d:%d"), &posOffsetX, &posOffsetY)) {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        pParams->vpp.delogo.posX = posOffsetX;
        pParams->vpp.delogo.posY = posOffsetY;
        return 0;
    }
    if (IS_OPTION("vpp-delogo-depth")) {
        i++;
        int depth;
        if (1 != _stscanf_s(strInput[i], _T("%d"), &depth)) {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        pParams->vpp.delogo.depth = depth;
        return 0;
    }
    if (IS_OPTION("vpp-delogo-y")) {
        i++;
        int value;
        if (1 != _stscanf_s(strInput[i], _T("%d"), &value)) {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        pParams->vpp.delogo.Y = value;
        return 0;
    }
    if (IS_OPTION("vpp-delogo-cb")) {
        i++;
        int value;
        if (1 != _stscanf_s(strInput[i], _T("%d"), &value)) {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        pParams->vpp.delogo.Cb = value;
        return 0;
    }
    if (IS_OPTION("vpp-delogo-cr")) {
        i++;
        int value;
        if (1 != _stscanf_s(strInput[i], _T("%d"), &value)) {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        pParams->vpp.delogo.Cr = value;
        return 0;
    }

    if (IS_OPTION("vpp-delogo")) {
        pParams->vpp.delogo.enable = true;

        if (i+1 >= nArgNum || strInput[i+1][0] == _T('-')) {
            return 0;
        }
        i++;
        vector<tstring> param_list;
        bool flag_comma = false;
        const TCHAR *pstr = strInput[i];
        const TCHAR *qstr = strInput[i];
        for (; *pstr; pstr++) {
            if (*pstr == _T('\"')) {
                flag_comma ^= true;
            }
            if (!flag_comma && *pstr == _T(',')) {
                param_list.push_back(tstring(qstr, pstr - qstr));
                qstr = pstr+1;
            }
        }
        param_list.push_back(tstring(qstr, pstr - qstr));

        for (const auto& param : split(strInput[i], _T(","))) {
            auto pos = param.find_first_of(_T("="));
            if (pos != std::string::npos) {
                auto param_arg = param.substr(0, pos);
                auto param_val = param.substr(pos+1);
                param_arg = tolowercase(param_arg);
                if (param_arg == _T("enable")) {
                    if (param_val == _T("true")) {
                        pParams->vpp.delogo.enable = true;
                    } else if (param_val == _T("false")) {
                        pParams->vpp.delogo.enable = false;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("file")) {
                    try {
                        pParams->vpp.delogo.logoFilePath = param_val;
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("select")) {
                    try {
                        pParams->vpp.delogo.logoSelect = param_val;
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("add")) {
                    if (param_val == _T("true")) {
                        pParams->vpp.delogo.mode = DELOGO_MODE_ADD;
                    } else if (param_val == _T("false")) {
                        pParams->vpp.delogo.mode = DELOGO_MODE_REMOVE;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("pos")) {
                    int posOffsetX, posOffsetY;
                    if (   2 != _stscanf_s(param_val.c_str(), _T("%dx%d"), &posOffsetX, &posOffsetY)
                        && 2 != _stscanf_s(param_val.c_str(), _T("%d,%d"), &posOffsetX, &posOffsetY)
                        && 2 != _stscanf_s(param_val.c_str(), _T("%d/%d"), &posOffsetX, &posOffsetY)
                        && 2 != _stscanf_s(param_val.c_str(), _T("%d:%d"), &posOffsetX, &posOffsetY)) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    pParams->vpp.delogo.posX = posOffsetX;
                    pParams->vpp.delogo.posY = posOffsetY;
                    continue;
                }
                if (param_arg == _T("depth")) {
                    try {
                        pParams->vpp.delogo.depth = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("y")) {
                    try {
                        pParams->vpp.delogo.Y = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("cb")) {
                    try {
                        pParams->vpp.delogo.Cb = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("cr")) {
                    try {
                        pParams->vpp.delogo.Cr = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("auto_nr")) {
                    if (param_val == _T("true") || param_val == _T("on")) {
                        pParams->vpp.delogo.autoNR = true;
                    } else if (param_val == _T("false") || param_val == _T("off")) {
                        pParams->vpp.delogo.autoNR = false;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("auto_fade")) {
                    if (param_val == _T("true") || param_val == _T("on")) {
                        pParams->vpp.delogo.autoFade = true;
                    } else if (param_val == _T("false") || param_val == _T("off")) {
                        pParams->vpp.delogo.autoFade = false;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("nr_area")) {
                    try {
                        pParams->vpp.delogo.NRArea = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("nr_value")) {
                    try {
                        pParams->vpp.delogo.NRValue = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("log")) {
                    if (param_val == _T("true") || param_val == _T("on")) {
                        pParams->vpp.delogo.log = true;
                    } else if (param_val == _T("false") || param_val == _T("off")) {
                        pParams->vpp.delogo.log = false;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            } else {
                pParams->vpp.delogo.logoFilePath = param;
            }
        }
        return 0;
    }
    if (IS_OPTION("vpp-knn")) {
        pParams->vpp.knn.enable = true;
        if (i+1 >= nArgNum || strInput[i+1][0] == _T('-')) {
            pParams->vpp.knn.radius = FILTER_DEFAULT_KNN_RADIUS;
            return 0;
        }
        i++;
        int radius = FILTER_DEFAULT_KNN_RADIUS;
        if (1 != _stscanf_s(strInput[i], _T("%d"), &radius)) {
            for (const auto& param : split(strInput[i], _T(","))) {
                auto pos = param.find_first_of(_T("="));
                if (pos != std::string::npos) {
                    auto param_arg = param.substr(0, pos);
                    auto param_val = param.substr(pos+1);
                    param_arg = tolowercase(param_arg);
                    if (param_arg == _T("enable")) {
                        if (param_val == _T("true")) {
                            pParams->vpp.knn.enable = true;
                        } else if (param_val == _T("false")) {
                            pParams->vpp.knn.enable = false;
                        } else {
                            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                            return -1;
                        }
                        continue;
                    }
                    if (param_arg == _T("radius")) {
                        try {
                            pParams->vpp.knn.radius = std::stoi(param_val);
                        } catch (...) {
                            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                            return -1;
                        }
                        continue;
                    }
                    if (param_arg == _T("strength")) {
                        try {
                            pParams->vpp.knn.strength = std::stof(param_val);
                        } catch (...) {
                            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                            return -1;
                        }
                        continue;
                    }
                    if (param_arg == _T("lerp")) {
                        try {
                            pParams->vpp.knn.lerpC = std::stof(param_val);
                        } catch (...) {
                            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                            return -1;
                        }
                        continue;
                    }
                    if (param_arg == _T("th_weight")) {
                        try {
                            pParams->vpp.knn.weight_threshold = std::stof(param_val);
                        } catch (...) {
                            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                            return -1;
                        }
                        continue;
                    }
                    if (param_arg == _T("th_lerp")) {
                        try {
                            pParams->vpp.knn.lerp_threshold = std::stof(param_val);
                        } catch (...) {
                            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                            return -1;
                        }
                        continue;
                    }
                    CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                    return -1;
                }
            }
        } else {
            pParams->vpp.knn.radius = radius;
        }
        return 0;
    }
    if (IS_OPTION("vpp-pmd")) {
        pParams->vpp.pmd.enable = true;
        if (i+1 >= nArgNum || strInput[i+1][0] == _T('-')) {
            return 0;
        }
        i++;
        for (const auto& param : split(strInput[i], _T(","))) {
            auto pos = param.find_first_of(_T("="));
            if (pos != std::string::npos) {
                auto param_arg = param.substr(0, pos);
                auto param_val = param.substr(pos+1);
                param_arg = tolowercase(param_arg);
                if (param_arg == _T("enable")) {
                    if (param_val == _T("true")) {
                        pParams->vpp.pmd.enable = true;
                    } else if (param_val == _T("false")) {
                        pParams->vpp.pmd.enable = false;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("apply_count")) {
                    try {
                        pParams->vpp.pmd.applyCount = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("strength")) {
                    try {
                        pParams->vpp.pmd.strength = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("threshold")) {
                    try {
                        pParams->vpp.pmd.threshold = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("useexp")) {
                    try {
                        pParams->vpp.pmd.useExp = std::stoi(param_val) != 0;
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            }
        }
        return 0;
    }

    if (IS_OPTION("vpp-deband")) {
        pParams->vpp.deband.enable = true;
        if (i+1 >= nArgNum || strInput[i+1][0] == _T('-')) {
            return 0;
        }
        i++;
        for (const auto& param : split(strInput[i], _T(","))) {
            auto pos = param.find_first_of(_T("="));
            if (pos != std::string::npos) {
                auto param_arg = param.substr(0, pos);
                auto param_val = param.substr(pos+1);
                param_arg = tolowercase(param_arg);
                if (param_arg == _T("enable")) {
                    if (param_val == _T("true")) {
                        pParams->vpp.deband.enable = true;
                    } else if (param_val == _T("false")) {
                        pParams->vpp.deband.enable = false;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("range")) {
                    try {
                        pParams->vpp.deband.range = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("thre")) {
                    try {
                        pParams->vpp.deband.threY = std::stoi(param_val);
                        pParams->vpp.deband.threCb = pParams->vpp.deband.threY;
                        pParams->vpp.deband.threCr = pParams->vpp.deband.threY;
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("thre_y")) {
                    try {
                        pParams->vpp.deband.threY = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("thre_cb")) {
                    try {
                        pParams->vpp.deband.threCb = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("thre_cr")) {
                    try {
                        pParams->vpp.deband.threCr = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("dither")) {
                    try {
                        pParams->vpp.deband.ditherY = std::stoi(param_val);
                        pParams->vpp.deband.ditherC = pParams->vpp.deband.ditherY;
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("dither_y")) {
                    try {
                        pParams->vpp.deband.ditherY = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("dither_c")) {
                    try {
                        pParams->vpp.deband.ditherC = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("sample")) {
                    try {
                        pParams->vpp.deband.sample = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("seed")) {
                    try {
                        pParams->vpp.deband.seed = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("blurfirst")) {
                    pParams->vpp.deband.blurFirst = (param_val == _T("true")) || (param_val == _T("on"));
                    continue;
                }
                if (param_arg == _T("rand_each_frame")) {
                    pParams->vpp.deband.randEachFrame = (param_val == _T("true")) || (param_val == _T("on"));
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            } else {
                if (param == _T("blurfirst")) {
                    pParams->vpp.deband.blurFirst = true;
                    continue;
                }
                if (param == _T("rand_each_frame")) {
                    pParams->vpp.deband.randEachFrame = true;
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            }
        }
        return 0;
    }
    if (IS_OPTION("vpp-afs")) {
        pParams->vpp.afs.enable = true;

        if (i+1 >= nArgNum || strInput[i+1][0] == _T('-')) {
            return 0;
        }
        i++;
        vector<tstring> param_list;
        bool flag_comma = false;
        const TCHAR *pstr = strInput[i];
        const TCHAR *qstr = strInput[i];
        for (; *pstr; pstr++) {
            if (*pstr == _T('\"')) {
                flag_comma ^= true;
            }
            if (!flag_comma && *pstr == _T(',')) {
                param_list.push_back(tstring(qstr, pstr - qstr));
                qstr = pstr+1;
            }
        }
        param_list.push_back(tstring(qstr, pstr - qstr));
        for (const auto& param : param_list) {
            auto pos = param.find_first_of(_T("="));
            if (pos != std::string::npos) {
                auto param_arg = param.substr(0, pos);
                auto param_val = param.substr(pos+1);
                param_arg = tolowercase(param_arg);
                if (param_arg == _T("ini")) {
                    if (pParams->vpp.afs.read_afs_inifile(param_val.c_str())) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("ini file does not exist."), option_name, strInput[i]);
                        return -1;
                    }
                }
                if (param_arg == _T("preset")) {
                    try {
                        int value = 0;
                        if (get_list_value(list_afs_preset, param_val.c_str(), &value)) {
                            pParams->vpp.afs.set_preset(value);
                        } else {
                            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                            return -1;
                        }
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
            }
        }
        for (const auto& param : param_list) {
            auto pos = param.find_first_of(_T("="));
            if (pos != std::string::npos) {
                auto param_arg = param.substr(0, pos);
                auto param_val = param.substr(pos+1);
                param_arg = tolowercase(param_arg);
                if (param_arg == _T("enable")) {
                    if (param_val == _T("true")) {
                        pParams->vpp.afs.enable = true;
                    } else if (param_val == _T("false")) {
                        pParams->vpp.afs.enable = false;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("top")) {
                    try {
                        pParams->vpp.afs.clip.top = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("bottom")) {
                    try {
                        pParams->vpp.afs.clip.bottom = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("left")) {
                    try {
                        pParams->vpp.afs.clip.left = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("right")) {
                    try {
                        pParams->vpp.afs.clip.right = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("method_switch")) {
                    try {
                        pParams->vpp.afs.method_switch = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("coeff_shift")) {
                    try {
                        pParams->vpp.afs.coeff_shift = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("thre_shift")) {
                    try {
                        pParams->vpp.afs.thre_shift = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("thre_deint")) {
                    try {
                        pParams->vpp.afs.thre_deint = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("thre_motion_y")) {
                    try {
                        pParams->vpp.afs.thre_Ymotion = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("thre_motion_c")) {
                    try {
                        pParams->vpp.afs.thre_Cmotion = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("level")) {
                    try {
                        pParams->vpp.afs.analyze = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("shift")) {
                    pParams->vpp.afs.shift = (param_val == _T("true")) || (param_val == _T("on"));
                    continue;
                }
                if (param_arg == _T("drop")) {
                    pParams->vpp.afs.drop = (param_val == _T("true")) || (param_val == _T("on"));
                    continue;
                }
                if (param_arg == _T("smooth")) {
                    pParams->vpp.afs.smooth = (param_val == _T("true")) || (param_val == _T("on"));
                    continue;
                }
                if (param_arg == _T("24fps")) {
                    pParams->vpp.afs.force24 = (param_val == _T("true")) || (param_val == _T("on"));
                    continue;
                }
                if (param_arg == _T("tune")) {
                    pParams->vpp.afs.tune = (param_val == _T("true")) || (param_val == _T("on"));
                    continue;
                }
                if (param_arg == _T("rff")) {
                    pParams->vpp.afs.rff = (param_val == _T("true")) || (param_val == _T("on"));
                    continue;
                }
                if (param_arg == _T("timecode")) {
                    pParams->vpp.afs.timecode = (param_val == _T("true")) || (param_val == _T("on"));
                    continue;
                }
                if (param_arg == _T("log")) {
                    pParams->vpp.afs.log = (param_val == _T("true")) || (param_val == _T("on"));
                    continue;
                }
                if (param_arg == _T("ini")) {
                    continue;
                }
                if (param_arg == _T("preset")) {
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            } else {
                if (param == _T("shift")) {
                    pParams->vpp.afs.shift = true;
                    continue;
                }
                if (param == _T("drop")) {
                    pParams->vpp.afs.drop = true;
                    continue;
                }
                if (param == _T("smooth")) {
                    pParams->vpp.afs.smooth = true;
                    continue;
                }
                if (param == _T("24fps")) {
                    pParams->vpp.afs.force24 = true;
                    continue;
                }
                if (param == _T("tune")) {
                    pParams->vpp.afs.tune = true;
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            }
        }
        return 0;
    }
    if (IS_OPTION("vpp-nnedi")) {
        pParams->vpp.nnedi.enable = true;
        if (i+1 >= nArgNum || strInput[i+1][0] == _T('-')) {
            return 0;
        }
        i++;
        for (const auto& param : split(strInput[i], _T(","))) {
            auto pos = param.find_first_of(_T("="));
            if (pos != std::string::npos) {
                auto param_arg = param.substr(0, pos);
                auto param_val = param.substr(pos+1);
                param_arg = tolowercase(param_arg);
                if (param_arg == _T("enable")) {
                    if (param_val == _T("true")) {
                        pParams->vpp.nnedi.enable = true;
                    } else if (param_val == _T("false")) {
                        pParams->vpp.nnedi.enable = false;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("field")) {
                    int value = 0;
                    if (get_list_value(list_vpp_nnedi_field, param_val.c_str(), &value)) {
                        pParams->vpp.nnedi.field = (VppNnediField)value;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("nns")) {
                    int value = 0;
                    if (get_list_value(list_vpp_nnedi_nns, param_val.c_str(), &value)) {
                        pParams->vpp.nnedi.nns = value;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("nsize")) {
                    int value = 0;
                    if (get_list_value(list_vpp_nnedi_nsize, param_val.c_str(), &value)) {
                        pParams->vpp.nnedi.nsize = (VppNnediNSize)value;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("quality")) {
                    int value = 0;
                    if (get_list_value(list_vpp_nnedi_quality, param_val.c_str(), &value)) {
                        pParams->vpp.nnedi.quality = (VppNnediQuality)value;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("prescreen")) {
                    int value = 0;
                    if (get_list_value(list_vpp_nnedi_pre_screen, param_val.c_str(), &value)) {
                        pParams->vpp.nnedi.pre_screen = (VppNnediPreScreen)value;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("errortype")) {
                    int value = 0;
                    if (get_list_value(list_vpp_nnedi_error_type, param_val.c_str(), &value)) {
                        pParams->vpp.nnedi.errortype = (VppNnediErrorType)value;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("prec")) {
                    int value = 0;
                    if (get_list_value(list_vpp_nnedi_prec, param_val.c_str(), &value)) {
                        pParams->vpp.nnedi.precision = (VppNnediPrecision)value;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("weightfile")) {
                    pParams->vpp.nnedi.weightfile = param_val.c_str();
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            } else {
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            }
        }
        return 0;
    }
    if (IS_OPTION("vpp-yadif")) {
        pParams->vpp.yadif.enable = true;
        if (i+1 >= nArgNum || strInput[i+1][0] == _T('-')) {
            return 0;
        }
        i++;
        for (const auto& param : split(strInput[i], _T(","))) {
            auto pos = param.find_first_of(_T("="));
            if (pos != std::string::npos) {
                auto param_arg = param.substr(0, pos);
                auto param_val = param.substr(pos+1);
                param_arg = tolowercase(param_arg);
                if (param_arg == _T("enable")) {
                    if (param_val == _T("true")) {
                        pParams->vpp.yadif.enable = true;
                    } else if (param_val == _T("false")) {
                        pParams->vpp.yadif.enable = false;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("mode")) {
                    int value = 0;
                    if (get_list_value(list_vpp_yadif_mode, param_val.c_str(), &value)) {
                        pParams->vpp.yadif.mode = (VppYadifMode)value;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            } else {
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            }
        }
        return 0;
    }
    if (IS_OPTION("vpp-rff")) {
        pParams->vpp.rff = true;
        return 0;
    }

    if (IS_OPTION("vpp-tweak")) {
        pParams->vpp.tweak.enable = true;
        if (i+1 >= nArgNum || strInput[i+1][0] == _T('-')) {
            return 0;
        }
        i++;
        for (const auto& param : split(strInput[i], _T(","))) {
            auto pos = param.find_first_of(_T("="));
            if (pos != std::string::npos) {
                auto param_arg = param.substr(0, pos);
                auto param_val = param.substr(pos+1);
                param_arg = tolowercase(param_arg);
                if (param_arg == _T("enable")) {
                    if (param_val == _T("true")) {
                        pParams->vpp.tweak.enable = true;
                    } else if (param_val == _T("false")) {
                        pParams->vpp.tweak.enable = false;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("brightness")) {
                    try {
                        pParams->vpp.tweak.brightness = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("contrast")) {
                    try {
                        pParams->vpp.tweak.contrast = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("gamma")) {
                    try {
                        pParams->vpp.tweak.gamma = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("saturation")) {
                    try {
                        pParams->vpp.tweak.saturation = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("hue")) {
                    try {
                        pParams->vpp.tweak.hue = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            } else {
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            }
        }
        return 0;
    }
    if (IS_OPTION("vpp-colorspace")) {
        pParams->vpp.colorspace.enable = true;
        if (i+1 >= nArgNum || strInput[i+1][0] == _T('-')) {
            return 0;
        }
        i++;
        for (const auto &param : split(strInput[i], _T(","))) {
            auto pos = param.find_first_of(_T("="));
            if (pos != std::string::npos) {
                auto parse = [](int *from, int *to, tstring param_val, const CX_DESC *list) {
                    auto from_to = split(param_val, _T(":"));
                    if (from_to.size() == 2
                        && get_list_value(list, from_to[0].c_str(), from)
                        && get_list_value(list, from_to[1].c_str(), to)) {
                        return true;
                    }
                    return false;
                };
                if (pParams->vpp.colorspace.convs.size() == 0) {
                    pParams->vpp.colorspace.convs.push_back(ColorspaceConv());
                }
                auto param_arg = param.substr(0, pos);
                auto param_val = param.substr(pos+1);
                param_arg = tolowercase(param_arg);
                if (param_arg == _T("matrix")) {
                    auto& conv = pParams->vpp.colorspace.convs.back();
                    if (conv.from.matrix != conv.to.matrix) {
                        pParams->vpp.colorspace.convs.push_back(ColorspaceConv());
                        conv = pParams->vpp.colorspace.convs.back();
                    }
                    if (!parse((int *)&conv.from.matrix, (int *)&conv.to.matrix, param_val, list_colormatrix)) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("colorprim")) {
                    auto &conv = pParams->vpp.colorspace.convs.back();
                    if (conv.from.colorprim != conv.to.colorprim) {
                        pParams->vpp.colorspace.convs.push_back(ColorspaceConv());
                        conv = pParams->vpp.colorspace.convs.back();
                    }
                    if (!parse((int *)&conv.from.colorprim, (int *)&conv.to.colorprim, param_val, list_colorprim)) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("transfer")) {
                    auto &conv = pParams->vpp.colorspace.convs.back();
                    if (conv.from.transfer != conv.to.transfer) {
                        pParams->vpp.colorspace.convs.push_back(ColorspaceConv());
                        conv = pParams->vpp.colorspace.convs.back();
                    }
                    if (!parse((int *)&conv.from.transfer, (int *)&conv.to.transfer, param_val, list_transfer)) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("range")) {
                    auto &conv = pParams->vpp.colorspace.convs.back();
                    if (conv.from.fullrange != conv.to.fullrange) {
                        pParams->vpp.colorspace.convs.push_back(ColorspaceConv());
                        conv = pParams->vpp.colorspace.convs.back();
                    }
                    if (!parse((int *)&conv.from.fullrange, (int *)&conv.to.fullrange, param_val, list_colorrange)) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("source_peak")) {
                    try {
                        pParams->vpp.colorspace.hdr2sdr.hdr_source_peak = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("approx_gamma")) {
                    auto &conv = pParams->vpp.colorspace.convs.back();
                    conv.approx_gamma = (param_val == _T("true")) || (param_val == _T("on"));
                    continue;
                }
                if (param_arg == _T("scene_ref")) {
                    auto &conv = pParams->vpp.colorspace.convs.back();
                    conv.scene_ref = (param_val == _T("true")) || (param_val == _T("on"));
                    continue;
                }
                if (param_arg == _T("hdr2sdr")) {
                    int value = 0;
                    if (get_list_value(list_vpp_hdr2sdr, param_val.c_str(), &value)) {
                        pParams->vpp.colorspace.hdr2sdr.tonemap = (HDR2SDRToneMap)value;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("ldr_nits")) {
                    try {
                        pParams->vpp.colorspace.hdr2sdr.ldr_nits = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("a")) {
                    try {
                        pParams->vpp.colorspace.hdr2sdr.hable.a = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("b")) {
                    try {
                        pParams->vpp.colorspace.hdr2sdr.hable.b = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("c")) {
                    try {
                        pParams->vpp.colorspace.hdr2sdr.hable.c = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("d")) {
                    try {
                        pParams->vpp.colorspace.hdr2sdr.hable.d = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("e")) {
                    try {
                        pParams->vpp.colorspace.hdr2sdr.hable.e = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("f")) {
                    try {
                        pParams->vpp.colorspace.hdr2sdr.hable.f = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("w")) {
                    continue;
                }
                if (param_arg == _T("transition")) {
                    try {
                        pParams->vpp.colorspace.hdr2sdr.mobius.transition = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("contrast")) {
                    try {
                        pParams->vpp.colorspace.hdr2sdr.reinhard.contrast = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("peak")) {
                    try {
                        float peak = std::stof(param_val);
                        pParams->vpp.colorspace.hdr2sdr.mobius.peak = peak;
                        pParams->vpp.colorspace.hdr2sdr.reinhard.peak = peak;
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            } else {
                if (param == _T("hdr2sdr")) {
                    pParams->vpp.colorspace.hdr2sdr.tonemap = HDR2SDR_HABLE;
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            }
        }
        return 0;
    }



    if (IS_OPTION("vpp-subburn")) {
        VppSubburn subburn;
        subburn.enable = true;
        if (i+1 >= nArgNum || strInput[i+1][0] == _T('-')) {
            pParams->vpp.subburn.push_back(subburn);
            return 0;
        }
        i++;
        vector<tstring> param_list;
        bool flag_comma = false;
        const TCHAR *pstr = strInput[i];
        const TCHAR *qstr = strInput[i];
        for (; *pstr; pstr++) {
            if (*pstr == _T('\"')) {
                flag_comma ^= true;
            }
            if (!flag_comma && *pstr == _T(',')) {
                param_list.push_back(tstring(qstr, pstr - qstr));
                qstr = pstr+1;
            }
        }
        param_list.push_back(tstring(qstr, pstr - qstr));
        for (const auto &param : param_list) {
            auto pos = param.find_first_of(_T("="));
            if (pos != std::string::npos) {
                auto param_arg = param.substr(0, pos);
                auto param_val = param.substr(pos+1);
                param_arg = tolowercase(param_arg);
                if (param_arg == _T("enable")) {
                    if (param_val == _T("true")) {
                        subburn.enable = true;
                    } else if (param_val == _T("false")) {
                        subburn.enable = false;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("track")) {
                    try {
                        subburn.trackId = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("filename")) {
                    subburn.filename = param_val;
                    continue;
                }
                if (param_arg == _T("charcode")) {
                    subburn.charcode = tchar_to_string(param_val);
                    continue;
                }
                if (param_arg == _T("shaping")) {
                    int value = 0;
                    if (get_list_value(list_vpp_ass_shaping, param_val.c_str(), &value)) {
                        subburn.assShaping = value;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("scale")) {
                    try {
                        subburn.scale = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("transparency")) {
                    try {
                        subburn.transparency_offset = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("brightness")) {
                    try {
                        subburn.brightness = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("contrast")) {
                    try {
                        subburn.contrast = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("ts_offset")) {
                    try {
                        subburn.ts_offset = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            } else {
                try {
                    subburn.trackId = std::stoi(param);
                } catch (...) {
                    subburn.filename = param;
                }
            }
        }
        pParams->vpp.subburn.push_back(subburn);
        return 0;
    }

    if (IS_OPTION("vpp-pad")) {
        pParams->vpp.pad.enable = true;
        if (i+1 >= nArgNum || strInput[i+1][0] == _T('-')) {
            return 0;
        }
        i++;
        for (const auto& param : split(strInput[i], _T(","))) {
            auto pos = param.find_first_of(_T("="));
            if (pos != std::string::npos) {
                auto param_arg = param.substr(0, pos);
                auto param_val = param.substr(pos+1);
                param_arg = tolowercase(param_arg);
                if (param_arg == _T("enable")) {
                    if (param_val == _T("true")) {
                        pParams->vpp.pad.enable = true;
                    } else if (param_val == _T("false")) {
                        pParams->vpp.pad.enable = false;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("r")) {
                    try {
                        pParams->vpp.pad.right = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("l")) {
                    try {
                        pParams->vpp.pad.left = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("t")) {
                    try {
                        pParams->vpp.pad.top = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("b")) {
                    try {
                        pParams->vpp.pad.bottom = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            } else {
                int val[4] = { 0 };
                if (   4 == _stscanf_s(strInput[i], _T("%d,%d,%d,%d"), &val[0], &val[1], &val[2], &val[3])
                    || 4 == _stscanf_s(strInput[i], _T("%d:%d:%d:%d"), &val[0], &val[1], &val[2], &val[3])) {
                    pParams->vpp.pad.left   = val[0];
                    pParams->vpp.pad.top    = val[1];
                    pParams->vpp.pad.right  = val[2];
                    pParams->vpp.pad.bottom = val[3];
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            }
        }
        return 0;
    }

    if (IS_OPTION("vpp-select-every")) {
        pParams->vpp.selectevery.enable = true;
        if (i+1 >= nArgNum || strInput[i+1][0] == _T('-')) {
            return 0;
        }
        i++;
        for (const auto& param : split(strInput[i], _T(","))) {
            auto pos = param.find_first_of(_T("="));
            if (pos != std::string::npos) {
                auto param_arg = param.substr(0, pos);
                auto param_val = param.substr(pos+1);
                param_arg = tolowercase(param_arg);
                if (param_arg == _T("enable")) {
                    if (param_val == _T("true")) {
                        pParams->vpp.selectevery.enable = true;
                    } else if (param_val == _T("false")) {
                        pParams->vpp.selectevery.enable = false;
                    } else {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("offset")) {
                    try {
                        pParams->vpp.selectevery.offset = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("step")) {
                    try {
                        pParams->vpp.selectevery.step = std::stoi(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            } else {
                try {
                    pParams->vpp.selectevery.step = std::stoi(strInput[i]);
                } catch (...) {
                    CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                    return -1;
                }
                continue;
            }
        }
        return 0;
    }
    if (IS_OPTION("vpp-perf-monitor")) {
        pParams->vpp.checkPerformance = true;
        return 0;
    }
    if (IS_OPTION("no-vpp-perf-monitor")) {
        pParams->vpp.checkPerformance = false;
        return 0;
    }
    if (IS_OPTION("ssim")) {
        pParams->ssim = true;
        return 0;
    }
    if (IS_OPTION("no-ssim")) {
        pParams->ssim = false;
        return 0;
    }
    if (IS_OPTION("psnr")) {
        pParams->psnr = true;
        return 0;
    }
    if (IS_OPTION("no-psnr")) {
        pParams->psnr = false;
        return 0;
    }
    if (IS_OPTION("cavlc")) {
        codecPrm[NV_ENC_H264].h264Config.entropyCodingMode = NV_ENC_H264_ENTROPY_CODING_MODE_CAVLC;
        return 0;
    }
    if (IS_OPTION("cabac")) {
        codecPrm[NV_ENC_H264].h264Config.entropyCodingMode = NV_ENC_H264_ENTROPY_CODING_MODE_CABAC;
        return 0;
    }
    if (IS_OPTION("bluray")) {
        pParams->bluray = TRUE;
        return 0;
    }
    if (IS_OPTION("lossless")) {
        pParams->lossless = TRUE;
        return 0;
    }
    if (IS_OPTION("no-deblock")) {
        codecPrm[NV_ENC_H264].h264Config.disableDeblockingFilterIDC = 1;
        return 0;
    }
    if (IS_OPTION("slices:h264")) {
        i++;
        try {
            int value = std::stoi(strInput[i]);
            codecPrm[NV_ENC_H264].h264Config.sliceMode = 3;
            codecPrm[NV_ENC_H264].h264Config.sliceModeData = value;
        } catch (...) {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("slices:hevc")) {
        i++;
        try {
            int value = std::stoi(strInput[i]);
            codecPrm[NV_ENC_HEVC].hevcConfig.sliceMode = 3;
            codecPrm[NV_ENC_HEVC].hevcConfig.sliceModeData = value;
        } catch (...) {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("slices")) {
        i++;
        try {
            int value = std::stoi(strInput[i]);
            codecPrm[NV_ENC_H264].h264Config.sliceMode = 3;
            codecPrm[NV_ENC_HEVC].hevcConfig.sliceMode = 3;
            codecPrm[NV_ENC_H264].h264Config.sliceModeData = value;
            codecPrm[NV_ENC_HEVC].hevcConfig.sliceModeData = value;
        } catch (...) {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("deblock")) {
        codecPrm[NV_ENC_H264].h264Config.disableDeblockingFilterIDC = 0;
        return 0;
    }
    if (IS_OPTION("aud:h264")) {
        codecPrm[NV_ENC_H264].h264Config.outputAUD = 1;
        return 0;
    }
    if (IS_OPTION("aud:hevc")) {
        codecPrm[NV_ENC_HEVC].hevcConfig.outputAUD = 1;
        return 0;
    }
    if (IS_OPTION("aud")) {
        codecPrm[NV_ENC_H264].h264Config.outputAUD = 1;
        codecPrm[NV_ENC_HEVC].hevcConfig.outputAUD = 1;
        return 0;
    }
    if (IS_OPTION("pic-struct:h264")) {
        codecPrm[NV_ENC_H264].h264Config.outputPictureTimingSEI = 1;
        return 0;
    }
    if (IS_OPTION("pic-struct:hevc")) {
        codecPrm[NV_ENC_HEVC].hevcConfig.outputPictureTimingSEI = 1;
        return 0;
    }
    if (IS_OPTION("pic-struct")) {
        codecPrm[NV_ENC_H264].h264Config.outputPictureTimingSEI = 1;
        codecPrm[NV_ENC_HEVC].hevcConfig.outputPictureTimingSEI = 1;
        return 0;
    }
    if (IS_OPTION("level") || IS_OPTION("level:h264") || IS_OPTION("level:hevc")) {
        const bool for_h264 = IS_OPTION("level") || IS_OPTION("level:h264");
        const bool for_hevc = IS_OPTION("level") || IS_OPTION("level:hevc");
        i++;
        auto getLevel = [](const CX_DESC *desc, const TCHAR *argvstr, int *levelValue) {
            int value = 0;
            bool bParsed = false;
            if (desc != nullptr) {
                if (PARSE_ERROR_FLAG != (value = get_value_from_chr(desc, argvstr))) {
                    *levelValue = value;
                    bParsed = true;
                } else {
                    double val_float = 0.0;
                    if (1 == _stscanf_s(argvstr, _T("%lf"), &val_float)) {
                        value = (int)(val_float * 10 + 0.5);
                        if (value == desc[get_cx_index(desc, value)].value) {
                            *levelValue = value;
                            bParsed = true;
                        } else {
                            value = (int)(val_float + 0.5);
                            if (value == desc[get_cx_index(desc, value)].value) {
                                *levelValue = value;
                                bParsed = true;
                            }
                        }
                    }
                }
            }
            return bParsed;
        };
        bool flag = false;
        int value = 0;
        if (for_h264 && getLevel(list_avc_level, strInput[i], &value)) {
            codecPrm[NV_ENC_H264].h264Config.level = value;
            flag = true;
        }
        if (for_hevc && getLevel(list_hevc_level, strInput[i], &value)) {
            codecPrm[NV_ENC_HEVC].hevcConfig.level = value;
            flag = true;
        }
        if (!flag) {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("profile") || IS_OPTION("profile:h264") || IS_OPTION("profile:hevc")) {
        const bool for_h264 = IS_OPTION("profile") || IS_OPTION("profile:h264");
        const bool for_hevc = IS_OPTION("profile") || IS_OPTION("profile:hevc");
        i++;
        bool flag = false;
        if (for_h264) {
            GUID zero = { 0 };
            GUID result_guid = get_guid_from_name(strInput[i], h264_profile_names);
            if (0 != memcmp(&result_guid, &zero, sizeof(result_guid))) {
                pParams->encConfig.profileGUID = result_guid;
                pParams->yuv444 = memcmp(&pParams->encConfig.profileGUID, &NV_ENC_H264_PROFILE_HIGH_444_GUID, sizeof(result_guid)) == 0;
                flag = true;
            }
        }
        if (for_hevc) {
            int result = get_value_from_name(strInput[i], h265_profile_names);
            if (-1 != result) {
                //下位16bitを使用する
                uint16_t *ptr = (uint16_t *)&codecPrm[NV_ENC_HEVC].hevcConfig.tier;
                ptr[0] = (uint16_t)result;
                if (result == NV_ENC_PROFILE_HEVC_MAIN444) {
                    pParams->yuv444 = TRUE;
                }
                if (result == NV_ENC_PROFILE_HEVC_MAIN10) {
                    codecPrm[NV_ENC_HEVC].hevcConfig.pixelBitDepthMinus8 = 2;
                    pParams->yuv444 = FALSE;
                } else if (result == NV_ENC_PROFILE_HEVC_MAIN) {
                    codecPrm[NV_ENC_HEVC].hevcConfig.pixelBitDepthMinus8 = 0;
                    pParams->yuv444 = FALSE;
                }
                flag = true;
            }
        }
        if (!flag) {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("tier")) {
        i++;
        int value = 0;
        if (get_list_value(h265_tier_names, strInput[i], &value)) {
            //上位16bitを使用する
            uint16_t *ptr = (uint16_t *)&codecPrm[NV_ENC_HEVC].hevcConfig.tier;
            ptr[1] = (uint16_t)value;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("output-depth")) {
        i++;
        int value = 0;
        if (1 == _stscanf_s(strInput[i], _T("%d"), &value)) {
            codecPrm[NV_ENC_HEVC].hevcConfig.pixelBitDepthMinus8 = clamp(value - 8, 0, 4);
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("sar") || IS_OPTION("par") || IS_OPTION("dar")) {
        i++;
        int a[2] = { 0 };
        if (   2 == _stscanf_s(strInput[i], _T("%d:%d"), &a[0], &a[1])
            || 2 == _stscanf_s(strInput[i], _T("%d/%d"), &a[0], &a[1])
            || 2 == _stscanf_s(strInput[i], _T("%d.%d"), &a[0], &a[1])
            || 2 == _stscanf_s(strInput[i], _T("%d,%d"), &a[0], &a[1])) {
            if (IS_OPTION("dar")) {
                a[0] = -a[0];
                a[1] = -a[1];
            }
            pParams->par[0] = a[0];
            pParams->par[1] = a[1];
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("cu-max")) {
        i++;
        int value = 0;
        if (get_list_value(list_hevc_cu_size, strInput[i], &value)) {
            codecPrm[NV_ENC_HEVC].hevcConfig.maxCUSize = (NV_ENC_HEVC_CUSIZE)value;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("cu-min")) {
        i++;
        int value = 0;
        if (get_list_value(list_hevc_cu_size, strInput[i], &value)) {
            codecPrm[NV_ENC_HEVC].hevcConfig.minCUSize = (NV_ENC_HEVC_CUSIZE)value;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }
    if (IS_OPTION("cuda-schedule")) {
        i++;
        int value = 0;
        if (get_list_value(list_cuda_schedule, strInput[i], &value)) {
            pParams->cudaSchedule = value;
        } else {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return -1;
        }
        return 0;
    }

    if (IS_OPTION("gpu-select")) {
        if (i+1 >= nArgNum || strInput[i+1][0] == _T('-')) {
            return 0;
        }
        i++;
        for (const auto &param : split(strInput[i], _T(","))) {
            auto pos = param.find_first_of(_T("="));
            if (pos != std::string::npos) {
                auto param_arg = param.substr(0, pos);
                auto param_val = param.substr(pos+1);
                param_arg = tolowercase(param_arg);
                if (param_arg == _T("cores")) {
                    try {
                        pParams->gpuSelect.cores = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("gen")) {
                    try {
                        pParams->gpuSelect.gen = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("ve")) {
                    try {
                        pParams->gpuSelect.ve = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                if (param_arg == _T("gpu")) {
                    try {
                        pParams->gpuSelect.gpu = std::stof(param_val);
                    } catch (...) {
                        CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                        return -1;
                    }
                    continue;
                }
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            } else {
                CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
                return -1;
            }
        }
        return 0;
    }
    if (0 == _tcscmp(option_name, _T("session-retry"))) {
        i++;
        int value = 0;
        if (1 != _stscanf_s(strInput[i], _T("%d"), &value)) {
            CMD_PARSE_SET_ERR(strInput[0], _T("Unknown value"), option_name, strInput[i]);
            return 1;
        }
        if (value < 0) {
            CMD_PARSE_SET_ERR(strInput[0], _T("Invalid value"), option_name, strInput[i]);
            return 1;
        }
        pParams->sessionRetry = value;
        return 0;
    }

    auto ret = parse_one_input_option(option_name, strInput, i, nArgNum, &pParams->input, argData, err);
    if (ret >= 0) return ret;

    ret = parse_one_common_option(option_name, strInput, i, nArgNum, &pParams->common, argData, err);
    if (ret >= 0) return ret;

    ret = parse_one_ctrl_option(option_name, strInput, i, nArgNum, &pParams->ctrl, argData, err);
    if (ret >= 0) return ret;

    tstring mes = _T("Unknown option: --");
    mes += option_name;
    CMD_PARSE_SET_ERR(strInput[0], (TCHAR *)mes.c_str(), NULL, strInput[i]);
    return -1;
}
#undef IS_OPTION

int parse_cmd(InEncodeVideoParam *pParams, NV_ENC_CODEC_CONFIG *codecPrm, int nArgNum, const TCHAR **strInput, ParseCmdError& err, bool ignore_parse_err) {
    sArgsData argsData;

    for (int i = 1; i < nArgNum; i++) {
        if (strInput[i] == nullptr) {
            return -1;
        }
        const TCHAR *option_name = nullptr;
        if (strInput[i][0] == _T('-')) {
            if (strInput[i][1] == _T('-')) {
                option_name = &strInput[i][2];
            } else if (strInput[i][2] == _T('\0')) {
                if (nullptr == (option_name = cmd_short_opt_to_long(strInput[i][1]))) {
                    CMD_PARSE_SET_ERR(strInput[0], strsprintf(_T("Unknown options: \"%s\""), strInput[i]).c_str(), nullptr, nullptr);
                    return -1;
                }
            } else {
                if (ignore_parse_err) continue;
                CMD_PARSE_SET_ERR(strInput[0], strsprintf(_T("Invalid options: \"%s\""), strInput[i]).c_str(), nullptr, nullptr);
                return -1;
            }
        }

        if (nullptr == option_name) {
            if (ignore_parse_err) continue;
            CMD_PARSE_SET_ERR(strInput[0], strsprintf(_T("Unknown option: \"%s\""), strInput[i]).c_str(), nullptr, nullptr);
            return -1;
        }
        auto sts = parse_one_option(option_name, strInput, i, nArgNum, pParams, codecPrm, &argsData, err);
        if (!ignore_parse_err && sts != 0) {
            return sts;
        }
    }

    return 0;
}

int parse_cmd(InEncodeVideoParam *pParams, NV_ENC_CODEC_CONFIG *codecPrm, const char *cmda, ParseCmdError& err, bool ignore_parse_err) {
    if (cmda == nullptr) {
        return 0;
    }
    std::wstring cmd = char_to_wstring(cmda);
    int argc = 0;
    auto argvw = CommandLineToArgvW(cmd.c_str(), &argc);
    if (argc <= 1) {
        return 0;
    }
    vector<tstring> argv_tstring;
    for (int i = 0; i < argc; i++) {
        argv_tstring.push_back(wstring_to_tstring(argvw[i]));
    }
    LocalFree(argvw);

    vector<TCHAR *> argv_tchar;
    for (int i = 0; i < argc; i++) {
        argv_tchar.push_back((TCHAR *)argv_tstring[i].data());
    }
    argv_tchar.push_back(_T(""));
    const TCHAR **strInput = (const TCHAR **)argv_tchar.data();
    int ret = parse_cmd(pParams, codecPrm, argc, strInput, err, ignore_parse_err);
    return ret;
}

#pragma warning (push)
#pragma warning (disable: 4127)
tstring gen_cmd(const InEncodeVideoParam *pParams, const NV_ENC_CODEC_CONFIG codecPrmArg[2], bool save_disabled_prm) {
    std::basic_stringstream<TCHAR> cmd;
    InEncodeVideoParam encPrmDefault;
    NV_ENC_CODEC_CONFIG codecPrmDefault[2];
    codecPrmDefault[NV_ENC_H264] = DefaultParamH264();
    codecPrmDefault[NV_ENC_HEVC] = DefaultParamHEVC();

    NV_ENC_CODEC_CONFIG codecPrm[2];
    if (codecPrmArg != nullptr) {
        memcpy(codecPrm, codecPrmArg, sizeof(codecPrm));
    } else {
        memcpy(codecPrm, codecPrmDefault, sizeof(codecPrm));
        codecPrm[pParams->codec] = pParams->encConfig.encodeCodecConfig;
    }

#define OPT_FLOAT(str, opt, prec) if ((pParams->opt) != (encPrmDefault.opt)) cmd << _T(" ") << (str) << _T(" ") << std::setprecision(prec) << (pParams->opt);
#define OPT_NUM(str, opt) if ((pParams->opt) != (encPrmDefault.opt)) cmd << _T(" ") << (str) << _T(" ") << (int)(pParams->opt);
#define OPT_NUM_HEVC(str, codec, opt) if ((codecPrm[NV_ENC_HEVC].hevcConfig.opt) != (codecPrmDefault[NV_ENC_HEVC].hevcConfig.opt)) cmd << _T(" ") << (str) << ((save_disabled_prm) ? codec : _T("")) << _T(" ") << (int)(codecPrm[NV_ENC_HEVC].hevcConfig.opt);
#define OPT_NUM_H264(str, codec, opt) if ((codecPrm[NV_ENC_H264].h264Config.opt) != (codecPrmDefault[NV_ENC_H264].h264Config.opt)) cmd << _T(" ") << (str) << ((save_disabled_prm) ? codec : _T("")) << _T(" ") << (int)(codecPrm[NV_ENC_H264].h264Config.opt);
#define OPT_GUID(str, opt, list) if ((pParams->opt) != (encPrmDefault.opt)) cmd << _T(" ") << (str) << _T(" ") << get_name_from_guid((pParams->opt), list);
#define OPT_GUID_HEVC(str, codec, opt, list) if ((codecPrm[NV_ENC_HEVC].hevcConfig.opt) != (codecPrmDefault[NV_ENC_HEVC].hevcConfig.opt)) cmd << _T(" ") << (str) << ((save_disabled_prm) ? codec : _T("")) << _T(" ") << get_name_from_value((codecPrm[NV_ENC_HEVC].hevcConfig.opt), list);
#define OPT_LST(str, opt, list) if ((pParams->opt) != (encPrmDefault.opt)) cmd << _T(" ") << (str) << _T(" ") << get_chr_from_value(list, (pParams->opt));
#define OPT_LST_HEVC(str, codec, opt, list) if ((codecPrm[NV_ENC_HEVC].hevcConfig.opt) != (codecPrmDefault[NV_ENC_HEVC].hevcConfig.opt)) cmd << _T(" ") << (str) << ((save_disabled_prm) ? codec : _T("")) << _T(" ") << get_chr_from_value(list, (codecPrm[NV_ENC_HEVC].hevcConfig.opt));
#define OPT_LST_H264(str, codec, opt, list) if ((codecPrm[NV_ENC_H264].h264Config.opt) != (codecPrmDefault[NV_ENC_H264].h264Config.opt)) cmd << _T(" ") << (str) << ((save_disabled_prm) ? codec : _T("")) << _T(" ") << get_chr_from_value(list, (codecPrm[NV_ENC_H264].h264Config.opt));
#define OPT_QP(str, qp, enable, force) { \
    if ((force) || (enable) \
    || (pParams->qp.qpIntra) != (encPrmDefault.qp.qpIntra) \
    || (pParams->qp.qpInterP) != (encPrmDefault.qp.qpInterP) \
    || (pParams->qp.qpInterB) != (encPrmDefault.qp.qpInterB)) { \
        if (enable) { \
            cmd << _T(" ") << (str) << _T(" "); \
        } else { \
            cmd << _T(" ") << (str) << _T(" 0;"); \
        } \
        if ((pParams->qp.qpIntra) == (pParams->qp.qpInterP) && (pParams->qp.qpIntra) == (pParams->qp.qpInterB)) { \
            cmd << (int)(pParams->qp.qpIntra); \
        } else { \
            cmd << (int)(pParams->qp.qpIntra) << _T(":") << (int)(pParams->qp.qpInterP) << _T(":") << (int)(pParams->qp.qpInterB); \
        } \
    } \
}
#define OPT_BOOL(str_true, str_false, opt) if ((pParams->opt) != (encPrmDefault.opt)) cmd << _T(" ") << ((pParams->opt) ? (str_true) : (str_false));
#define OPT_BOOL_HEVC(str_true, str_false, codec, opt) \
    if ((codecPrm[NV_ENC_HEVC].hevcConfig.opt) != (codecPrmDefault[NV_ENC_HEVC].hevcConfig.opt)) { \
        cmd << _T(" "); \
        if ((codecPrm[NV_ENC_HEVC].hevcConfig.opt)) { \
            if (_tcslen(str_true)) { cmd << (str_true) << ((save_disabled_prm) ? (codec) : _T("")); } \
        } else { \
            if (_tcslen(str_false)) { cmd << (str_false) << ((save_disabled_prm) ? (codec) : _T("")); } \
        } \
    }
#define OPT_BOOL_H264(str_true, str_false, codec, opt) \
    if ((codecPrm[NV_ENC_H264].h264Config.opt) != (codecPrmDefault[NV_ENC_H264].h264Config.opt)) { \
        cmd << _T(" "); \
        if ((codecPrm[NV_ENC_H264].h264Config.opt)) { \
            if (_tcslen(str_true)) { cmd << (str_true) << ((save_disabled_prm) ? (codec) : _T("")); }\
        } else { \
            if (_tcslen(str_false)) { cmd << (str_false) << ((save_disabled_prm) ? (codec) : _T("")); }\
        } \
    }
#define OPT_TCHAR(str, opt) if ((pParams->opt) && _tcslen(pParams->opt)) cmd << _T(" ") << str << _T(" ") << (pParams->opt);
#define OPT_TSTR(str, opt) if (pParams->opt.length() > 0) cmd << _T(" ") << str << _T(" ") << pParams->opt.c_str();
#define OPT_CHAR(str, opt) if ((pParams->opt) && _tcslen(pParams->opt)) cmd << _T(" ") << str << _T(" ") << char_to_tstring(pParams->opt);
#define OPT_STR(str, opt) if (pParams->opt.length() > 0) cmd << _T(" ") << str << _T(" ") << char_to_tstring(pParams->opt).c_str();
#define OPT_CHAR_PATH(str, opt) if ((pParams->opt) && _tcslen(pParams->opt)) cmd << _T(" ") << str << _T(" \"") << (pParams->opt) << _T("\"");
#define OPT_STR_PATH(str, opt) if (pParams->opt.length() > 0) cmd << _T(" ") << str << _T(" \"") << (pParams->opt.c_str()) << _T("\"");

    OPT_NUM(_T("-d"), deviceID);
    cmd << _T(" -c ") << get_chr_from_value(list_nvenc_codecs_for_opt, pParams->codec);
    if ((pParams->preset) != (encPrmDefault.preset)) cmd << _T(" -u ") << get_name_from_value(pParams->preset, list_nvenc_preset_names);

    cmd << gen_cmd(&pParams->input, &encPrmDefault.input, save_disabled_prm);

    if (save_disabled_prm) {
        switch (pParams->encConfig.rcParams.rateControlMode) {
        case NV_ENC_PARAMS_RC_CBR:
        case NV_ENC_PARAMS_RC_CBR_HQ:
        case NV_ENC_PARAMS_RC_VBR:
        case NV_ENC_PARAMS_RC_VBR_HQ: {
            OPT_QP(_T("--cqp"), encConfig.rcParams.constQP, true, true);
        } break;
        case NV_ENC_PARAMS_RC_CONSTQP:
        default: {
            cmd << _T(" --vbr ") << pParams->encConfig.rcParams.averageBitRate / 1000;
        } break;
        }
    }
    switch (pParams->encConfig.rcParams.rateControlMode) {
    case NV_ENC_PARAMS_RC_CBR: {
        cmd << _T(" --cbr ") << pParams->encConfig.rcParams.averageBitRate / 1000;
    } break;
    case NV_ENC_PARAMS_RC_CBR_HQ: {
        cmd << _T(" --cbrhq ") << pParams->encConfig.rcParams.averageBitRate / 1000;
    } break;
    case NV_ENC_PARAMS_RC_VBR: {
        cmd << _T(" --vbr ") << pParams->encConfig.rcParams.averageBitRate / 1000;
    } break;
    case NV_ENC_PARAMS_RC_VBR_HQ: {
        cmd << _T(" --vbrhq ") << pParams->encConfig.rcParams.averageBitRate / 1000;
    } break;
    case NV_ENC_PARAMS_RC_CONSTQP:
    default: {
        OPT_QP(_T("--cqp"), encConfig.rcParams.constQP, true, true);
    } break;
    }
    if (pParams->encConfig.rcParams.rateControlMode != NV_ENC_PARAMS_RC_CONSTQP || save_disabled_prm) {
        OPT_NUM(_T("--vbv-bufsize"), encConfig.rcParams.vbvBufferSize / 1000);
        if ((pParams->encConfig.rcParams.targetQuality) != (encPrmDefault.encConfig.rcParams.targetQuality)
            || (pParams->encConfig.rcParams.targetQualityLSB) != (encPrmDefault.encConfig.rcParams.targetQualityLSB)) {
            float val = pParams->encConfig.rcParams.targetQuality + pParams->encConfig.rcParams.targetQualityLSB / 256.0f;
            cmd << _T(" --vbr-quality ") << std::fixed << std::setprecision(2) << val;
        }
        OPT_NUM(_T("--max-bitrate"), encConfig.rcParams.maxBitRate / 1000);
    }
    if (pParams->encConfig.rcParams.enableInitialRCQP || save_disabled_prm) {
        OPT_QP(_T("--qp-init"), encConfig.rcParams.initialRCQP, pParams->encConfig.rcParams.enableInitialRCQP, false);
    }
    if (pParams->encConfig.rcParams.enableMinQP || save_disabled_prm) {
        OPT_QP(_T("--qp-min"), encConfig.rcParams.minQP, pParams->encConfig.rcParams.enableMinQP, false);
    }
    if (pParams->encConfig.rcParams.enableMaxQP || save_disabled_prm) {
        OPT_QP(_T("--qp-max"), encConfig.rcParams.maxQP, pParams->encConfig.rcParams.enableMaxQP, false);
    }
    if (pParams->encConfig.rcParams.enableLookahead || save_disabled_prm) {
        OPT_NUM(_T("--lookahead"), encConfig.rcParams.lookaheadDepth);
    }
    OPT_BOOL(_T("--no-i-adapt"), _T(""), encConfig.rcParams.disableIadapt);
    OPT_BOOL(_T("--no-b-adapt"), _T(""), encConfig.rcParams.disableBadapt);
    OPT_BOOL(_T("--strict-gop"), _T(""), encConfig.rcParams.strictGOPTarget);
    if (pParams->encConfig.gopLength == 0) {
        cmd << _T(" --gop-len auto");
    } else {
        OPT_NUM(_T("--gop-len"), encConfig.gopLength);
    }
    OPT_NUM(_T("-b"), encConfig.frameIntervalP-1);
    OPT_BOOL(_T("--weightp"), _T(""), nWeightP);
    OPT_BOOL(_T("--nonrefp"), _T(""), encConfig.rcParams.enableNonRefP);
    OPT_BOOL(_T("--aq"), _T("--no-aq"), encConfig.rcParams.enableAQ);
    OPT_BOOL(_T("--aq-temporal"), _T(""), encConfig.rcParams.enableTemporalAQ);
    OPT_NUM(_T("--aq-strength"), encConfig.rcParams.aqStrength);
    OPT_LST(_T("--mv-precision"), encConfig.mvPrecision, list_mv_presicion);
    if (pParams->par[0] > 0 && pParams->par[1] > 0) {
        cmd << _T(" --sar ") << pParams->par[0] << _T(":") << pParams->par[1];
    } else if (pParams->par[0] < 0 && pParams->par[1] < 0) {
        cmd << _T(" --dar ") << -1 * pParams->par[0] << _T(":") << -1 * pParams->par[1];
    }
    OPT_BOOL(_T("--lossless"), _T(""), lossless);

    if (pParams->codec == NV_ENC_HEVC || save_disabled_prm) {
        OPT_LST_HEVC(_T("--level"), _T(":hevc"), level, list_hevc_level);
        OPT_GUID_HEVC(_T("--profile"), _T(":hevc"), tier & 0xffff, h265_profile_names);
        OPT_LST_HEVC(_T("--tier"), _T(":hevc"), tier >> 16, h265_tier_names);
        OPT_NUM_HEVC(_T("--ref"), _T(""), maxNumRefFramesInDPB);
        OPT_NUM_HEVC(_T("--multiref-l0"), _T(""), numRefL0);
        OPT_NUM_HEVC(_T("--multiref-l1"), _T(""), numRefL1);
        OPT_LST_HEVC(_T("--bref-mode"), _T(""), useBFramesAsRef, list_bref_mode);
        if (codecPrm[NV_ENC_HEVC].hevcConfig.pixelBitDepthMinus8 != codecPrmDefault[NV_ENC_HEVC].hevcConfig.pixelBitDepthMinus8) {
            cmd << _T(" --output-depth ") << codecPrm[NV_ENC_HEVC].hevcConfig.pixelBitDepthMinus8 + 8;
        }
        OPT_NUM_HEVC(_T("--slices"), _T(":hevc"), sliceModeData);
        OPT_BOOL_HEVC(_T("--aud"), _T(""), _T(":hevc"), outputAUD);
        OPT_BOOL_HEVC(_T("--pic-struct"), _T(""), _T(":hevc"), outputPictureTimingSEI);
        OPT_LST_HEVC(_T("--cu-max"), _T(""), maxCUSize, list_hevc_cu_size);
        OPT_LST_HEVC(_T("--cu-min"), _T(""), minCUSize, list_hevc_cu_size);
    }
    if (pParams->codec == NV_ENC_H264 || save_disabled_prm) {
        OPT_LST_H264(_T("--level"), _T(":h264"), level, list_avc_level);
        OPT_GUID(_T("--profile"), encConfig.profileGUID, h264_profile_names);
        OPT_NUM_H264(_T("--ref"), _T(""), maxNumRefFrames);
        OPT_NUM_H264(_T("--multiref-l0"), _T(""), numRefL0);
        OPT_NUM_H264(_T("--multiref-l1"), _T(""), numRefL1);
        OPT_LST_H264(_T("--bref-mode"), _T(""), useBFramesAsRef, list_bref_mode);
        OPT_LST_H264(_T("--direct"), _T(""), bdirectMode, list_bdirect);
        OPT_LST_H264(_T("--adapt-transform"), _T(""), adaptiveTransformMode, list_adapt_transform);
        OPT_NUM_H264(_T("--slices"), _T(":h264"), sliceModeData);
        OPT_BOOL_H264(_T("--aud"), _T(""), _T(":h264"), outputAUD);
        OPT_BOOL_H264(_T("--pic-struct"), _T(""), _T(":h264"), outputPictureTimingSEI);
        if ((codecPrm[NV_ENC_H264].h264Config.entropyCodingMode) != (codecPrmDefault[NV_ENC_H264].h264Config.entropyCodingMode)) {
            cmd << _T(" --") << get_chr_from_value(list_entropy_coding, codecPrm[NV_ENC_H264].h264Config.entropyCodingMode);
        }
        OPT_BOOL(_T("--bluray"), _T(""), bluray);
        OPT_BOOL_H264(_T("--no-deblock"), _T("--deblock"), _T(""), disableDeblockingFilterIDC);
    }

    cmd << gen_cmd(&pParams->common, &encPrmDefault.common, save_disabled_prm);

#define ADD_FLOAT(str, opt, prec) if ((pParams->opt) != (encPrmDefault.opt)) tmp << _T(",") << (str) << _T("=") << std::setprecision(prec) << (pParams->opt);
#define ADD_NUM(str, opt) if ((pParams->opt) != (encPrmDefault.opt)) tmp << _T(",") << (str) << _T("=") << (pParams->opt);
#define ADD_LST(str, opt, list) if ((pParams->opt) != (encPrmDefault.opt)) tmp << _T(",") << (str) << _T("=") << get_chr_from_value(list, (pParams->opt));
#define ADD_BOOL(str, opt) if ((pParams->opt) != (encPrmDefault.opt)) tmp << _T(",") << (str) << _T("=") << ((pParams->opt) ? (_T("true")) : (_T("false")));
#define ADD_CHAR(str, opt) if ((pParams->opt) && _tcslen(pParams->opt)) tmp << _T(",") << (str) << _T("=") << (pParams->opt);
#define ADD_PATH(str, opt) if ((pParams->opt) && _tcslen(pParams->opt)) tmp << _T(",") << (str) << _T("=\"") << (pParams->opt) << _T("\"");
#define ADD_STR(str, opt) if (pParams->opt.length() > 0) tmp << _T(",") << (str) << _T("=") << (pParams->opt.c_str());

    OPT_LST(_T("--vpp-deinterlace"), vpp.deinterlace, list_deinterlace);
    OPT_BOOL(_T("--vpp-rff"), _T(""), vpp.rff);
    OPT_LST(_T("--vpp-resize"), vpp.resizeInterp, list_nppi_resize);

    std::basic_stringstream<TCHAR> tmp;
    if (pParams->vpp.afs != encPrmDefault.vpp.afs) {
        tmp.str(tstring());
        if (!pParams->vpp.afs.enable && save_disabled_prm) {
            tmp << _T(",enable=false");
        }
        if (pParams->vpp.afs.enable || save_disabled_prm) {
            ADD_NUM(_T("top"), vpp.afs.clip.top);
            ADD_NUM(_T("bottom"), vpp.afs.clip.bottom);
            ADD_NUM(_T("left"), vpp.afs.clip.left);
            ADD_NUM(_T("right"), vpp.afs.clip.right);
            ADD_NUM(_T("method_switch"), vpp.afs.method_switch);
            ADD_NUM(_T("coeff_shift"), vpp.afs.coeff_shift);
            ADD_NUM(_T("thre_shift"), vpp.afs.thre_shift);
            ADD_NUM(_T("thre_deint"), vpp.afs.thre_deint);
            ADD_NUM(_T("thre_motion_y"), vpp.afs.thre_Ymotion);
            ADD_NUM(_T("thre_motion_c"), vpp.afs.thre_Cmotion);
            ADD_NUM(_T("level"), vpp.afs.analyze);
            ADD_BOOL(_T("shift"), vpp.afs.shift);
            ADD_BOOL(_T("drop"), vpp.afs.drop);
            ADD_BOOL(_T("smooth"), vpp.afs.smooth);
            ADD_BOOL(_T("24fps"), vpp.afs.force24);
            ADD_BOOL(_T("tune"), vpp.afs.tune);
            ADD_BOOL(_T("rff"), vpp.afs.rff);
            ADD_BOOL(_T("timecode"), vpp.afs.timecode);
            ADD_BOOL(_T("log"), vpp.afs.log);
        }
        if (!tmp.str().empty()) {
            cmd << _T(" --vpp-afs ") << tmp.str().substr(1);
        } else if (pParams->vpp.afs.enable) {
            cmd << _T(" --vpp-afs");
        }
    }
    if (pParams->vpp.nnedi != encPrmDefault.vpp.nnedi) {
        tmp.str(tstring());
        if (!pParams->vpp.nnedi.enable && save_disabled_prm) {
            tmp << _T(",enable=false");
        }
        if (pParams->vpp.nnedi.enable || save_disabled_prm) {
            ADD_LST(_T("field"), vpp.nnedi.field, list_vpp_nnedi_field);
            ADD_LST(_T("nns"), vpp.nnedi.nns, list_vpp_nnedi_nns);
            ADD_LST(_T("nsize"), vpp.nnedi.nsize, list_vpp_nnedi_nsize);
            ADD_LST(_T("quality"), vpp.nnedi.quality, list_vpp_nnedi_quality);
            ADD_LST(_T("prec"), vpp.nnedi.precision, list_vpp_nnedi_prec);
            ADD_LST(_T("prescreen"), vpp.nnedi.pre_screen, list_vpp_nnedi_pre_screen);
            ADD_LST(_T("errortype"), vpp.nnedi.errortype, list_vpp_nnedi_error_type);
            ADD_PATH(_T("weightfile"), vpp.nnedi.weightfile.c_str());
        }
        if (!tmp.str().empty()) {
            cmd << _T(" --vpp-nnedi ") << tmp.str().substr(1);
        } else if (pParams->vpp.nnedi.enable) {
            cmd << _T(" --vpp-nnedi");
        }
    }
    if (pParams->vpp.yadif != encPrmDefault.vpp.yadif) {
        tmp.str(tstring());
        if (!pParams->vpp.yadif.enable && save_disabled_prm) {
            tmp << _T(",enable=false");
        }
        if (pParams->vpp.yadif.enable || save_disabled_prm) {
            ADD_LST(_T("mode"), vpp.yadif.mode, list_vpp_yadif_mode);
        }
        if (!tmp.str().empty()) {
            cmd << _T(" --vpp-yadif ") << tmp.str().substr(1);
        } else if (pParams->vpp.yadif.enable) {
            cmd << _T(" --vpp-yadif");
        }
    }
    if (pParams->vpp.knn != encPrmDefault.vpp.knn) {
        tmp.str(tstring());
        if (!pParams->vpp.knn.enable && save_disabled_prm) {
            tmp << _T(",enable=false");
        }
        if (pParams->vpp.knn.enable || save_disabled_prm) {
            ADD_NUM(_T("radius"), vpp.knn.radius);
            ADD_FLOAT(_T("strength"), vpp.knn.strength, 3);
            ADD_FLOAT(_T("lerp"), vpp.knn.lerpC, 3);
            ADD_FLOAT(_T("th_weight"), vpp.knn.weight_threshold, 3);
            ADD_FLOAT(_T("th_lerp"), vpp.knn.lerp_threshold, 3);
        }
        if (!tmp.str().empty()) {
            cmd << _T(" --vpp-knn ") << tmp.str().substr(1);
        } else if (pParams->vpp.knn.enable) {
            cmd << _T(" --vpp-knn");
        }
    }
    if (pParams->vpp.pmd != encPrmDefault.vpp.pmd) {
        tmp.str(tstring());
        if (!pParams->vpp.pmd.enable && save_disabled_prm) {
            tmp << _T(",enable=false");
        }
        if (pParams->vpp.pmd.enable || save_disabled_prm) {
            ADD_NUM(_T("apply_count"), vpp.pmd.applyCount);
            ADD_FLOAT(_T("strength"), vpp.pmd.strength, 3);
            ADD_FLOAT(_T("threshold"), vpp.pmd.threshold, 3);
            ADD_NUM(_T("useexp"), vpp.pmd.useExp);
        }
        if (!tmp.str().empty()) {
            cmd << _T(" --vpp-pmd ") << tmp.str().substr(1);
        } else if (pParams->vpp.pmd.enable) {
            cmd << _T(" --vpp-pmd");
        }
    }
    OPT_LST(_T("--vpp-gauss"), vpp.gaussMaskSize, list_nppi_gauss);
    if (pParams->vpp.unsharp != encPrmDefault.vpp.unsharp) {
        tmp.str(tstring());
        if (!pParams->vpp.unsharp.enable && save_disabled_prm) {
            tmp << _T(",enable=false");
        }
        if (pParams->vpp.unsharp.enable || save_disabled_prm) {
            ADD_NUM(_T("radius"), vpp.unsharp.radius);
            ADD_FLOAT(_T("weight"), vpp.unsharp.weight, 3);
            ADD_FLOAT(_T("threshold"), vpp.unsharp.threshold, 3);
        }
        if (!tmp.str().empty()) {
            cmd << _T(" --vpp-unsharp ") << tmp.str().substr(1);
        } else if (pParams->vpp.unsharp.enable) {
            cmd << _T(" --vpp-unsharp");
        }
    }
    if (pParams->vpp.edgelevel != encPrmDefault.vpp.edgelevel) {
        tmp.str(tstring());
        if (!pParams->vpp.edgelevel.enable && save_disabled_prm) {
            tmp << _T(",enable=false");
        }
        if (pParams->vpp.edgelevel.enable || save_disabled_prm) {
            ADD_FLOAT(_T("strength"), vpp.edgelevel.strength, 3);
            ADD_FLOAT(_T("threshold"), vpp.edgelevel.threshold, 3);
            ADD_FLOAT(_T("black"), vpp.edgelevel.black, 3);
            ADD_FLOAT(_T("white"), vpp.edgelevel.white, 3);
        }
        if (!tmp.str().empty()) {
            cmd << _T(" --vpp-edgelevel ") << tmp.str().substr(1);
        } else if (pParams->vpp.edgelevel.enable) {
            cmd << _T(" --vpp-edgelevel");
        }
    }
    if (pParams->vpp.tweak != encPrmDefault.vpp.tweak) {
        tmp.str(tstring());
        if (!pParams->vpp.tweak.enable && save_disabled_prm) {
            tmp << _T(",enable=false");
        }
        if (pParams->vpp.tweak.enable || save_disabled_prm) {
            ADD_FLOAT(_T("brightness"), vpp.tweak.brightness, 3);
            ADD_FLOAT(_T("contrast"), vpp.tweak.contrast, 3);
            ADD_FLOAT(_T("gamma"), vpp.tweak.gamma, 3);
            ADD_FLOAT(_T("saturation"), vpp.tweak.saturation, 3);
            ADD_FLOAT(_T("hue"), vpp.tweak.hue, 3);
        }
        if (!tmp.str().empty()) {
            cmd << _T(" --vpp-tweak ") << tmp.str().substr(1);
        } else if (pParams->vpp.tweak.enable) {
            cmd << _T(" --vpp-tweak");
        }
    }
    for (int i = 0; i < (int)pParams->vpp.subburn.size(); i++) {
        if (pParams->vpp.subburn[i] != VppSubburn()) {
            tmp.str(tstring());
            if (!pParams->vpp.subburn[i].enable && save_disabled_prm) {
                tmp << _T(",enable=false");
            }
            if (pParams->vpp.subburn[i].enable || save_disabled_prm) {
                ADD_NUM(_T("track"), vpp.subburn[i].trackId);
                ADD_PATH(_T("filename"), vpp.subburn[i].filename.c_str());
                ADD_STR(_T("charcode"), vpp.subburn[i].charcode);
                ADD_LST(_T("shaping"), vpp.subburn[i].assShaping, list_vpp_ass_shaping);
                ADD_FLOAT(_T("scale"), vpp.subburn[i].scale, 4);
                ADD_FLOAT(_T("ts_offset"), vpp.subburn[i].ts_offset, 4);
                ADD_FLOAT(_T("transparency"), vpp.subburn[i].transparency_offset, 4);
                ADD_FLOAT(_T("brightness"), vpp.subburn[i].brightness, 4);
                ADD_FLOAT(_T("contrast"), vpp.subburn[i].contrast, 4);
            }
            if (!tmp.str().empty()) {
                cmd << _T(" --vpp-subburn ") << tmp.str().substr(1);
            } else if (pParams->vpp.subburn[i].enable) {
                cmd << _T(" --vpp-subburn");
            }
        }
    }
    if (pParams->vpp.colorspace != encPrmDefault.vpp.colorspace) {
        tmp.str(tstring());
        if (!pParams->vpp.colorspace.enable && save_disabled_prm) {
            tmp << _T(",enable=false");
        }
        if (pParams->vpp.colorspace.enable || save_disabled_prm) {
            for (int i = 0; i < pParams->vpp.colorspace.convs.size(); i++) {
                auto from = pParams->vpp.colorspace.convs[i].from;
                auto to = pParams->vpp.colorspace.convs[i].to;
                if (from.matrix != to.matrix) {
                    tmp << _T(",matrix=");
                    tmp << get_cx_desc(list_colormatrix, from.matrix);
                    tmp << _T(":");
                    tmp << get_cx_desc(list_colormatrix, to.matrix);
                }
                if (from.colorprim != to.colorprim) {
                    tmp << _T(",colorprim=");
                    tmp << get_cx_desc(list_colorprim, from.colorprim);
                    tmp << _T(":");
                    tmp << get_cx_desc(list_colorprim, to.colorprim);
                }
                if (from.transfer != to.transfer) {
                    tmp << _T(",transfer=");
                    tmp << get_cx_desc(list_transfer, from.transfer);
                    tmp << _T(":");
                    tmp << get_cx_desc(list_transfer, to.transfer);
                }
                if (from.fullrange != to.fullrange) {
                    tmp << _T(",range=");
                    tmp << get_cx_desc(list_colorrange, from.fullrange);
                    tmp << _T(":");
                    tmp << get_cx_desc(list_colorrange, to.fullrange);
                }
                ADD_BOOL(_T("approx_gamma"), vpp.colorspace.convs[i].approx_gamma);
                ADD_BOOL(_T("scene_ref"), vpp.colorspace.convs[i].scene_ref);
                ADD_LST(_T("hdr2sdr"), vpp.colorspace.hdr2sdr.tonemap, list_vpp_hdr2sdr);
                ADD_FLOAT(_T("ldr_nits"), vpp.colorspace.hdr2sdr.ldr_nits, 1);
                ADD_FLOAT(_T("source_peak"), vpp.colorspace.hdr2sdr.hdr_source_peak, 1);
                ADD_FLOAT(_T("a"), vpp.colorspace.hdr2sdr.hable.a, 3);
                ADD_FLOAT(_T("b"), vpp.colorspace.hdr2sdr.hable.b, 3);
                ADD_FLOAT(_T("c"), vpp.colorspace.hdr2sdr.hable.c, 3);
                ADD_FLOAT(_T("d"), vpp.colorspace.hdr2sdr.hable.d, 3);
                ADD_FLOAT(_T("e"), vpp.colorspace.hdr2sdr.hable.e, 3);
                ADD_FLOAT(_T("f"), vpp.colorspace.hdr2sdr.hable.f, 3);
                ADD_FLOAT(_T("transition"), vpp.colorspace.hdr2sdr.mobius.transition, 3);
                ADD_FLOAT(_T("peak"), vpp.colorspace.hdr2sdr.mobius.peak, 3);
                ADD_FLOAT(_T("contrast"), vpp.colorspace.hdr2sdr.reinhard.contrast, 3);
            }
        }
        if (!tmp.str().empty()) {
            cmd << _T(" --vpp-colorspace ") << tmp.str().substr(1);
        } else if (pParams->vpp.colorspace.enable) {
            cmd << _T(" --vpp-colorspace");
        }
    }
    if (pParams->vpp.pad != encPrmDefault.vpp.pad) {
        tmp.str(tstring());
        if (!pParams->vpp.pad.enable && save_disabled_prm) {
            tmp << _T(",enable=false");
        }
        if (pParams->vpp.pad.enable || save_disabled_prm) {
            ADD_NUM(_T("r"), vpp.pad.right);
            ADD_NUM(_T("l"), vpp.pad.left);
            ADD_NUM(_T("t"), vpp.pad.top);
            ADD_NUM(_T("b"), vpp.pad.bottom);
        }
        if (!tmp.str().empty()) {
            cmd << _T(" --vpp-pad ") << tmp.str().substr(1);
        } else if (pParams->vpp.pad.enable) {
            cmd << _T(" --vpp-pad");
        }
    }
    if (pParams->vpp.deband != encPrmDefault.vpp.deband) {
        tmp.str(tstring());
        if (!pParams->vpp.deband.enable && save_disabled_prm) {
            tmp << _T(",enable=false");
        }
        if (pParams->vpp.deband.enable || save_disabled_prm) {
            ADD_NUM(_T("range"), vpp.deband.range);
            if (pParams->vpp.deband.threY == pParams->vpp.deband.threCb
                && pParams->vpp.deband.threY == pParams->vpp.deband.threCr) {
                ADD_NUM(_T("thre"), vpp.deband.threY);
            } else {
                ADD_NUM(_T("thre_y"), vpp.deband.threY);
                ADD_NUM(_T("thre_cb"), vpp.deband.threCb);
                ADD_NUM(_T("thre_cr"), vpp.deband.threCr);
            }
            if (pParams->vpp.deband.ditherY == pParams->vpp.deband.ditherC) {
                ADD_NUM(_T("dither"), vpp.deband.ditherY);
            } else {
                ADD_NUM(_T("dither_y"), vpp.deband.ditherY);
                ADD_NUM(_T("dither_c"), vpp.deband.ditherC);
            }
            ADD_NUM(_T("sample"), vpp.deband.sample);
            ADD_BOOL(_T("blurfirst"), vpp.deband.blurFirst);
            ADD_BOOL(_T("rand_each_frame"), vpp.deband.randEachFrame);
        }
        if (!tmp.str().empty()) {
            cmd << _T(" --vpp-deband ") << tmp.str().substr(1);
        } else if (pParams->vpp.deband.enable) {
            cmd << _T(" --vpp-deband");
        }
    }
    if (pParams->vpp.delogo != encPrmDefault.vpp.delogo) {
        tmp.str(tstring());
        if (!pParams->vpp.delogo.enable && save_disabled_prm) {
            tmp << _T(",enable=false");
        }
        if (pParams->vpp.delogo.enable || save_disabled_prm) {
            ADD_PATH(_T("file"), vpp.delogo.logoFilePath.c_str());
            ADD_PATH(_T("select"), vpp.delogo.logoSelect.c_str());
            if (pParams->vpp.delogo.posX != encPrmDefault.vpp.delogo.posX
                || pParams->vpp.delogo.posY != encPrmDefault.vpp.delogo.posY) {
                tmp << _T(",pos=") << pParams->vpp.delogo.posX << _T("x") << pParams->vpp.delogo.posY;
            }
            ADD_NUM(_T("depth"), vpp.delogo.depth);
            ADD_NUM(_T("y"),  vpp.delogo.Y);
            ADD_NUM(_T("cb"), vpp.delogo.Cb);
            ADD_NUM(_T("cr"), vpp.delogo.Cr);
            ADD_BOOL(_T("add"), vpp.delogo.mode);
            ADD_BOOL(_T("auto_fade"), vpp.delogo.autoFade);
            ADD_BOOL(_T("auto_nr"), vpp.delogo.autoNR);
            ADD_NUM(_T("nr_area"), vpp.delogo.NRArea);
            ADD_NUM(_T("nr_value"), vpp.delogo.NRValue);
            ADD_BOOL(_T("log"), vpp.delogo.log);
        }
        if (!tmp.str().empty()) {
            cmd << _T(" --vpp-delogo ") << tmp.str().substr(1);
        }
    }
    if (pParams->vpp.selectevery != encPrmDefault.vpp.selectevery) {
        tmp.str(tstring());
        if (!pParams->vpp.selectevery.enable && save_disabled_prm) {
            tmp << _T(",enable=false");
        }
        if (pParams->vpp.selectevery.enable || save_disabled_prm) {
            ADD_NUM(_T("step"), vpp.selectevery.step);
            ADD_NUM(_T("offset"), vpp.selectevery.offset);
        }
        if (!tmp.str().empty()) {
            cmd << _T(" --vpp-select-every ") << tmp.str().substr(1);
        }
    }
    OPT_BOOL(_T("--vpp-perf-monitor"), _T("--no-vpp-perf-monitor"), vpp.checkPerformance);

    OPT_BOOL(_T("--ssim"), _T(""), ssim);
    OPT_BOOL(_T("--psnr"), _T(""), psnr);

    OPT_LST(_T("--cuda-schedule"), cudaSchedule, list_cuda_schedule);
    if (pParams->gpuSelect != encPrmDefault.gpuSelect) {
        tmp.str(tstring());
        ADD_FLOAT(_T("cores"), gpuSelect.cores, 6);
        ADD_FLOAT(_T("gen"), gpuSelect.gen, 3);
        ADD_FLOAT(_T("ve"), gpuSelect.ve, 3);
        ADD_FLOAT(_T("gpu"), gpuSelect.gpu, 3);
        if (!tmp.str().empty()) {
            cmd << _T(" --gpu-select ") << tmp.str().substr(1);
        }
    }
    OPT_NUM(_T("--session-retry"), sessionRetry);

    cmd << gen_cmd(&pParams->ctrl, &encPrmDefault.ctrl, save_disabled_prm);

    return cmd.str();
}
#pragma warning (pop)

#undef CMD_PARSE_SET_ERR