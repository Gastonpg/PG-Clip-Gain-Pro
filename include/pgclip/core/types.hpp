#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pgclip {

using FrameIndex = std::uint64_t;

enum class CaptureState {
    Empty,
    Capturing,
    Complete,
    Partial,
    Discontinuous,
    Cancelled
};

enum class ValidationCode {
    None,
    InvalidSampleRate,
    UnsupportedChannelCount,
    EmptyRange,
    RangeOutsideCapture,
    InvalidWindow,
    InvalidHop,
    NonFiniteGain,
    UnorderedCurvePoints,
    DuplicateCurvePoint
};

struct ValidationIssue {
    ValidationCode code{ValidationCode::None};
    std::string message;

    explicit operator bool() const noexcept {
        return code != ValidationCode::None;
    }
};

struct FrameRange {
    FrameIndex start{0};
    FrameIndex end{0};
};

struct AudioFormat {
    std::uint32_t sampleRate{0};
    std::uint32_t channelCount{0};
};

struct CaptureDescriptor {
    AudioFormat format{};
    FrameIndex totalFrames{0};
    CaptureState state{CaptureState::Empty};
    bool hasDiscontinuities{false};
};

struct AnalysisConfig {
    FrameIndex windowFrames{0};
    FrameIndex hopFrames{0};
    std::uint32_t version{1};
};

struct GainPoint {
    FrameIndex frame{0};
    double gainDb{0.0};
};

struct GainCurve {
    std::vector<GainPoint> points;
};

ValidationIssue validate(const AudioFormat& format);
ValidationIssue validate(const FrameRange& range,
                         FrameIndex totalFrames);
ValidationIssue validate(const CaptureDescriptor& capture);
ValidationIssue validate(const AnalysisConfig& config);
ValidationIssue validate(const GainCurve& curve);

double gainDbToLinear(double gainDb);

} // namespace pgclip
