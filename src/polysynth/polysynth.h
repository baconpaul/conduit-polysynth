/*
 * Conduit - a project highlighting CLAP-first development
 *           and exercising the surge synth team libraries.
 *
 * Copyright 2023-2024 Paul Walker and authors in github
 *
 * This file you are viewing now is released under the
 * MIT license as described in LICENSE.md
 *
 * The assembled program which results from compiling this
 * project has GPL3 dependencies, so if you distribute
 * a binary, the combined work would be a GPL3 product.
 *
 * Roughly, that means you are welcome to copy the code and
 * ideas in the src/ directory, but perhaps not code from elsewhere
 * if you are closed source or non-GPL3. And if you do copy this code
 * you will need to replace some of the dependencies. Please see
 * the discussion in README.md for further information on what this may
 * mean for you.
 */

#ifndef CONDUIT_SRC_POLYSYNTH_POLYSYNTH_H
#define CONDUIT_SRC_POLYSYNTH_POLYSYNTH_H

#include <iostream>
#include <unordered_map>
#include <vector>
#include <map>
#include <optional>
#include "conduit-shared/debug-helpers.h"

#include <atomic>
#include <array>
#include <unordered_map>
#include <memory>
#include <random>
#include <tuple>

#include <clap/helpers/plugin.hh>

#include "conduit-shared/sse-include.h"

#include "sst/basic-blocks/params/ParamMetadata.h"
#include "sst/basic-blocks/dsp/VUPeak.h"
#include "sst/filters/HalfRateFilter.h"
#include "sst/voicemanager/voicemanager.h"

#include "sst/effects/Phaser.h"
#include "sst/effects/Flanger.h"
#include "sst/effects/Reverb1.h"

#include "conduit-shared/clap-base-class.h"
#include "voice.h"

struct MTSClient;

namespace sst::conduit::polysynth
{
/*
 * This static (defined in the cpp file) allows us to present a name, feature set,
 * url etc... and is consumed by clap-saw-demo-pluginentry.cpp
 */
static constexpr int nParams{72};

struct ModMatrixConfig;

struct ConduitPolysynthConfig
{
    static constexpr int nParams{sst::conduit::polysynth::nParams};
    static constexpr bool baseClassProvidesMonoModSupport{
        false}; // as a synth we do voice level modulation with the VM
    static constexpr bool usesSpecializedMessages{true};
    struct PatchExtension
    {
        static constexpr bool hasExtension{true};

        void initialize();
        std::unique_ptr<ModMatrixConfig> modMatrixConfig;

        bool toXml(TiXmlElement &);
        bool fromXml(TiXmlElement *);

        bool mpeMode{false};
    };
    struct DataCopyForUI
    {
        std::atomic<uint32_t> updateCount{0};
        std::atomic<bool> isProcessing{false};
        std::atomic<int> polyphony{0};

        std::atomic<float> mainVU[2];

        // s1, s2, target, depth
        using modMessage = std::tuple<int32_t, int32_t, int32_t, float>;
        std::array<modMessage, 8> modMatrixCopy;
        std::atomic<uint32_t> rescanMatrix{0};

        std::atomic<bool> isPlayingOrRecording;
        std::atomic<double> tempo;
        std::atomic<clap_beattime> bar_start;
        std::atomic<int32_t> bar_number;
        std::atomic<clap_beattime> song_pos_beats;

        std::atomic<uint16_t> tsig_num, tsig_denom;

        void populateMatrixView(const std::unique_ptr<ModMatrixConfig> &);
    };

    static const clap_plugin_descriptor *getDescription();

    struct SpecializedMessage
    {
        struct ModRowMessage
        {
            int32_t row;
            int32_t s1, s2, tgt;
            float depth;
        };
        struct MPEConfig
        {
            bool active;
            int range{24};
        };
        std::variant<ModRowMessage, MPEConfig> payload;
    };
    using specializedMessage_t = SpecializedMessage;
};

struct PhaserConfig;
struct FlangerConfig;
struct Reverb1Config;

using PhaserFX = sst::effects::phaser::Phaser<PhaserConfig>;
using FlangerFX = sst::effects::flanger::Flanger<FlangerConfig>;
using ReverbFX = sst::effects::reverb1::Reverb1<Reverb1Config>;

struct ConduitPolysynth
    : sst::conduit::shared::ClapBaseClass<ConduitPolysynth, ConduitPolysynthConfig>
{
    static constexpr int max_voices = 64;
    ConduitPolysynth(const clap_host *host);
    ~ConduitPolysynth();

    bool activate(double sampleRate, uint32_t minFrameCount,
                  uint32_t maxFrameCount) noexcept override;

    enum paramIds : uint32_t
    {
        // Oscillators - in the 1000 range
        // Saw Oscillator
        pmSawActive = 1100,
        pmSawUnisonCount,
        pmSawUnisonSpread,
        pmSawCoarse,
        pmSawFine,
        pmSawLevel,

        // Pulse Oscillator
        pmPWActive = 1200,
        pmPWWidth,
        pmPWFrequencyDiv,
        pmPWCoarse,
        pmPWFine,
        pmPWLevel,

        // Sine Oscillator
        pmSinActive = 1300,
        pmSinFrequencyDiv,
        pmSinCoarse,
        pmSinLevel,

        // Noise Oscillator
        pmNoiseActive = 1400,
        pmNoiseColor,
        pmNoiseLevel,

        // Filters - in the 2000 range
        pmLPFActive = 2000,
        pmLPFCutoff,
        pmLPFResonance,
        pmLPFFilterMode,
        pmLPFKeytrack,

        pmSVFActive = 2100,
        pmSVFCutoff,
        pmSVFResonance,
        pmSVFFilterMode,
        pmSVFKeytrack,

        pmWSActive = 2200,
        pmWSDrive,
        pmWSBias,
        pmWSMode,

        pmFilterRouting = 2300,
        pmFilterFeedback,

        // Envelopes - in the 8000 range
        pmEnvA = 8000, // +10 for FEG
        pmEnvD,
        pmEnvS,
        pmEnvR, // so don't use within 8020 or so

        pmFegToLPFCutoff = 8040,
        pmFegToSVFCutoff,

        pmAegVelocitySens = 8050,
        pmAegPreFilterGain,

        // LFOs in the 9000 range
        pmLFOActive = 9000, // + 100 for LFO2
        pmLFORate,
        pmLFODeform,
        pmLFOAmplitude,
        pmLFOShape,
        pmLFOTempoSync,

        // Output in the 10k range
        pmVoicePan = 10000,
        pmVoiceLevel,

        // fx up in the 20k range
        pmModFXActive = 20000,
        pmModFXType,
        pmModFXPreset,
        pmModFXRate,
        pmModFXRateTemposync,
        pmModFXMix,

        pmRevFXActive = 20025,
        pmRevFXPreset,
        pmRevFXTime,
        pmRevFXMix,

        // and finally the main level
        pmOutputLevel = 20100,

        // Special parameter indicating no modulation target
        pmNoModTarget = 0x0100BEEF
    };

    static constexpr int offPmFeg{10};
    static constexpr int offPmLFO2{100};
    static constexpr int n_lfos{2};

  public:
    /*
     * Many CLAP plugins will want input and output audio and note ports, although
     * the spec doesn't require this. Here as a simple synth we set up a single s
     * stereo output and a single midi / clap_note input.
     */
    bool implementsAudioPorts() const noexcept override { return true; }
    uint32_t audioPortsCount(bool isInput) const noexcept override { return isInput ? 0 : 1; }
    bool audioPortsInfo(uint32_t index, bool isInput,
                        clap_audio_port_info *info) const noexcept override;

    bool implementsNotePorts() const noexcept override { return true; }
    uint32_t notePortsCount(bool isInput) const noexcept override { return isInput ? 1 : 0; }
    bool notePortsInfo(uint32_t index, bool isInput,
                       clap_note_port_info *info) const noexcept override;

    /*
     * VoiceInfo is an optional (currently draft) extension where you advertise
     * polyphony information. Crucially here though it allows you to advertise that
     * you can support overlapping notes, which in conjunction with CLAP_DIALECT_NOTE
     * and the Bitwig voice stack modulator lets you stack this little puppy!
     */
    bool implementsVoiceInfo() const noexcept override { return true; }
    bool voiceInfoGet(clap_voice_info *info) noexcept override
    {
        info->voice_capacity = max_voices;
        info->voice_count = max_voices;
        info->flags = CLAP_VOICE_INFO_SUPPORTS_OVERLAPPING_NOTES;
        return true;
    }

    /*
     * process is the meat of the operation. It does obvious things like trigger
     * voices but also handles all the polyphonic modulation and so on. Please see the
     * comments in the cpp file to understand it and the helper functions we have
     * delegated to.
     */
    clap_process_status process(const clap_process *process) noexcept override;
    void handleInboundEvent(const clap_event_header_t *evt);
    void pushParamsToVoices();
    void activateVoice(PolysynthVoice &v, int port_index, int channel, int key, int noteid,
                       double velocity);

    /*
     * In addition to ::process, the plugin should implement ::paramsFlush. ::paramsFlush will be
     * called when processing isn't active (no audio being generated, etc...) but the host or UI
     * wants to update a value - usually a parameter value. In effect it looks like a version
     * of process with no audio buffers.
     */
    void paramsFlush(const clap_input_events *in, const clap_output_events *out) noexcept override;

    /*
     * start and stop processing are called when you start and stop obviously.
     * We update an atomic bool so our UI can go ahead and draw processing state
     * and also flush param changes when there is no processing queue.
     */
    bool startProcessing() noexcept override
    {
        uiComms.dataCopyForUI.isProcessing = true;
        uiComms.dataCopyForUI.updateCount++;
        return true;
    }
    void stopProcessing() noexcept override
    {
        uiComms.dataCopyForUI.isProcessing = false;
        uiComms.dataCopyForUI.updateCount++;
    }

    uint32_t getAsVst3SupportedNodeExpressions() override { return AS_VST3_NOTE_EXPRESSION_ALL; }

    std::default_random_engine gen;
    std::uniform_real_distribution<float> urd;

    void onStateRestored() override;

  protected:
    std::unique_ptr<juce::Component> createEditor() override;

    friend struct PolysynthVoice;

  private:
    typedef std::unordered_map<int, int> PatchPluginExtension;

    uint16_t blockPos{0};
    void renderVoices();
    float output alignas(16)[2][PolysynthVoice::blockSize];
    float outputOS alignas(16)[2][PolysynthVoice::blockSizeOS];
    sst::filters::HalfRate::HalfRateFilter hr_dn;

    // Voice Management
    struct VMConfig
    {
        static constexpr size_t maxVoiceCount{max_voices};
        using voice_t = PolysynthVoice;
    };

  public:
    std::function<void(PolysynthVoice *)> voiceEndCallback;
    void setVoiceEndCallback(std::function<void(PolysynthVoice *)> f) { voiceEndCallback = f; }

    constexpr int32_t beginVoiceCreationTransaction(uint16_t port, uint16_t channel, uint16_t key,
                                                    int32_t noteId, float velocity)
    {
        return 1;
    }
    void endVoiceCreationTransaction(uint16_t port, uint16_t channel, uint16_t key, int32_t noteId,
                                     float velocity)
    {
    }
    int32_t initializeMultipleVoices(
        std::array<PolysynthVoice *, VMConfig::maxVoiceCount> &voiceInitWorkingBuffer,
        uint16_t port, uint16_t channel, uint16_t key, int32_t noteId, float velocity, float retune)
    {
        voiceInitWorkingBuffer[0] = initializeVoice(port, channel, key, noteId, velocity, retune);
        return 1;
    }
    PolysynthVoice *initializeVoice(uint16_t port, uint16_t channel, uint16_t key, int32_t noteId,
                                    float velocity, float retune);
    void releaseVoice(PolysynthVoice *v, float velocity);
    void retriggerVoiceWithNewNoteID(PolysynthVoice *v, int32_t noteid, float velocity)
    {
        CNDOUT << "retriggerVoice" << std::endl;
    }

    void setVoiceMIDIPitchBend(PolysynthVoice *v, uint16_t pb14bit)
    {
        auto bv = (pb14bit - 8192) / 8192.f;
        v->pitchBendWheel = bv * 2; // just hardcode a pitch bend depth of 2
        v->recalcPitch();
    }

    void setVoiceMIDIMPEChannelPitchBend(PolysynthVoice *v, uint16_t pb14bit)
    {
        auto bv = (pb14bit - 8192) / 8192.f;
        v->mpePitchBend = bv;
        v->recalcPitch();
    }

    void setVoicePolyphonicParameterModulation(PolysynthVoice *v, uint32_t parameter, double value)
    {
        v->applyExternalMod(parameter, value);
    }

    void setNoteExpression(PolysynthVoice *v, int32_t expression, double value)
    {
        v->receiveNoteExpression(expression, value);
    }

    void setPolyphonicAftertouch(PolysynthVoice *v, int8_t pat)
    {
        v->applyPolyphonicAftertouch(pat);
    }

    void setChannelPressure(PolysynthVoice *v, int8_t pres) { v->applyChannelPressure(pres); }

    void setMIDI1CC(PolysynthVoice *v, int8_t cc, int8_t val) { v->applyMIDI1CC(cc, val); }

    void handleSpecializedFromUI(const FromUI &r);

    void allSoundsOff() {}
    void allNotesOff() {}

    MTSClient *mtsClient{nullptr};

    std::unique_ptr<PhaserFX> phaserFX;
    std::unique_ptr<FlangerFX> flangerFX;
    std::unique_ptr<ReverbFX> reverbFX;

    sst::basic_blocks::dsp::VUPeak mainVU;

  private:
    using voiceManager_t = sst::voicemanager::VoiceManager<VMConfig, ConduitPolysynth>;
    voiceManager_t voiceManager;

    std::array<PolysynthVoice, max_voices> voices;
    std::vector<std::tuple<int, int, int, int>> terminatedVoices; // that's PCK ID
};

struct ModMatrixConfig
{
    enum Sources
    {
        NONE = 600,
        LFO1 = 10057,
        LFO2,

        AEG = 10157,
        FEG,

        Velocity = 17000,
        ReleaseVelocity,
        ModWheel,
        PolyAT,
        ChannelAT,

        MPETimbre = 18000,
        MPEPressure
    };

    std::map<Sources, std::pair<std::string, std::string>> sourceNames{
        {NONE, {"-", ""}},
        {LFO1, {"LFO1", "LFOs"}},
        {LFO2, {"LFO2", "LFOs"}},
        {AEG, {"AEG", "Envelopes"}},
        {FEG, {"FEG", "Envelopes"}},

        {Velocity, {"Velocity", "MIDI"}},
        {ReleaseVelocity, {"Release Velocity", "MIDI"}},
        {ModWheel, {"ModWheel", "MIDI"}},
        {PolyAT, {"Polyphonic Aftertouch", "MIDI"}},
        {ChannelAT, {"Channel AfterTouch", "MIDI"}},
        {MPETimbre, {"Timbre", "MPE"}},
        {MPEPressure, {"Pressure", "MPE"}}};

    struct EntryDescription
    {
        Sources source;
        Sources via;
        float depth;
        ConduitPolysynth::paramIds target;
    };
    static constexpr int nModSlots{8};

    std::array<EntryDescription, nModSlots> routings;

    ModMatrixConfig()
    {
        for (auto &e : routings)
        {
            e.source = NONE;
            e.via = NONE;
            e.target = ConduitPolysynth::pmNoModTarget;
            e.depth = 0.f;
        }
    }
};
} // namespace sst::conduit::polysynth

#endif
