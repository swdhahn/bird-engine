#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "audio/AudioListenerComponent.h"
#include "audio/AudioSourceComponent.h"
#include "audio/AudioSystem.h"
#include "scene/WorldObject.h"

namespace {

namespace fs = std::filesystem;

// Writes a minimal 16-bit PCM mono WAV sine wave for deterministic offline tests.
fs::path writeSineWav(const std::string& filename, float freqHz,
					  float durationSec, uint32_t sampleRate = 48000,
					  float amplitude = 0.5f) {
	const fs::path path = fs::temp_directory_path() / filename;
	const uint32_t numSamples =
		static_cast<uint32_t>(durationSec * static_cast<float>(sampleRate));
	const uint16_t numChannels = 1;
	const uint16_t bitsPerSample = 16;
	const uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
	const uint16_t blockAlign = numChannels * (bitsPerSample / 8);
	const uint32_t dataSize = numSamples * blockAlign;
	const uint32_t chunkSize = 36 + dataSize;

	std::ofstream out(path, std::ios::binary);
	auto writeU16 = [&](uint16_t v) {
		out.write(reinterpret_cast<const char*>(&v), 2);
	};
	auto writeU32 = [&](uint32_t v) {
		out.write(reinterpret_cast<const char*>(&v), 4);
	};

	out.write("RIFF", 4);
	writeU32(chunkSize);
	out.write("WAVE", 4);
	out.write("fmt ", 4);
	writeU32(16);
	writeU16(1);  // PCM
	writeU16(numChannels);
	writeU32(sampleRate);
	writeU32(byteRate);
	writeU16(blockAlign);
	writeU16(bitsPerSample);
	out.write("data", 4);
	writeU32(dataSize);

	constexpr float kTwoPi = 6.28318530718f;
	for (uint32_t i = 0; i < numSamples; ++i) {
		const float t =
			static_cast<float>(i) / static_cast<float>(sampleRate);
		const float sample = amplitude * std::sin(kTwoPi * freqHz * t);
		const int16_t pcm = static_cast<int16_t>(
			std::clamp(sample, -1.0f, 1.0f) * 32767.0f);
		out.write(reinterpret_cast<const char*>(&pcm), 2);
	}
	out.close();
	return path;
}

float measureRms(bird::AudioSystem& audio, uint64_t frames,
				 int channelFilter = -1) {
	const uint32_t ch = audio.getChannels();
	std::vector<float> buf(frames * ch, 0.0f);
	const uint64_t read = audio.readPcmFrames(buf.data(), frames);
	if (read == 0) return 0.0f;

	double sumSq = 0.0;
	uint64_t count = 0;
	for (uint64_t f = 0; f < read; ++f) {
		for (uint32_t c = 0; c < ch; ++c) {
			if (channelFilter >= 0 && static_cast<int>(c) != channelFilter)
				continue;
			const double s = buf[f * ch + c];
			sumSq += s * s;
			count++;
		}
	}
	return count > 0 ? static_cast<float>(std::sqrt(sumSq / count)) : 0.0f;
}

class AudioTest : public ::testing::Test {
   protected:
	void SetUp() override {
		tonePath_ = writeSineWav("bird_audio_test_tone.wav", 440.0f, 1.0f);
		shortPath_ = writeSineWav("bird_audio_test_short.wav", 880.0f, 0.05f);

		bird::AudioSystemConfig cfg;
		cfg.noDevice = true;
		cfg.sampleRate = 48000;
		cfg.channels = 2;
		ASSERT_TRUE(audio_.init(cfg));
	}

	void TearDown() override {
		audio_.deinit();
		std::error_code ec;
		fs::remove(tonePath_, ec);
		fs::remove(shortPath_, ec);
	}

	bird::AudioSystem audio_;
	fs::path tonePath_;
	fs::path shortPath_;
};

TEST_F(AudioTest, DefaultBusHierarchyAndCustomBuses) {
	EXPECT_TRUE(audio_.isInitialized());
	EXPECT_TRUE(audio_.isNoDeviceMode());
	EXPECT_EQ(audio_.getSampleRate(), 48000u);
	EXPECT_EQ(audio_.getChannels(), 2u);

	EXPECT_TRUE(audio_.hasBus(bird::AudioBus::kMaster));
	EXPECT_TRUE(audio_.hasBus(bird::AudioBus::kMusic));
	EXPECT_TRUE(audio_.hasBus(bird::AudioBus::kSFX));
	EXPECT_TRUE(audio_.hasBus(bird::AudioBus::kVoice));

	EXPECT_EQ(audio_.getBusParent(bird::AudioBus::kMaster), "");
	EXPECT_EQ(audio_.getBusParent(bird::AudioBus::kMusic),
			  bird::AudioBus::kMaster);
	EXPECT_EQ(audio_.getBusParent(bird::AudioBus::kSFX),
			  bird::AudioBus::kMaster);
	EXPECT_EQ(audio_.getBusParent(bird::AudioBus::kVoice),
			  bird::AudioBus::kMaster);

	// Custom nested bus
	EXPECT_TRUE(audio_.createBus("Footsteps", bird::AudioBus::kSFX));
	EXPECT_TRUE(audio_.hasBus("Footsteps"));
	EXPECT_EQ(audio_.getBusParent("Footsteps"), bird::AudioBus::kSFX);

	// Duplicate, empty, or invalid parent rejected
	EXPECT_FALSE(audio_.createBus("Footsteps", bird::AudioBus::kSFX));
	EXPECT_FALSE(audio_.createBus("", bird::AudioBus::kMaster));
	EXPECT_FALSE(audio_.createBus("Orphan", "DoesNotExist"));
}

TEST_F(AudioTest, BusVolumeMutePauseAndPitch) {
	bird::AudioPlayOptions opts;
	opts.bus = bird::AudioBus::kSFX;
	opts.looping = true;
	opts.volume = 1.0f;

	bird::AudioHandle h = audio_.play2D(tonePath_.string(), opts);
	ASSERT_FALSE(h.isNull());
	ASSERT_TRUE(audio_.isPlaying(h));

	// Warm up mixer and measure baseline RMS
	measureRms(audio_, 512);
	const float baseRms = measureRms(audio_, 2048);
	EXPECT_GT(baseRms, 0.1f);

	// Bus volume scaling
	audio_.setBusVolume(bird::AudioBus::kSFX, 0.25f);
	EXPECT_FLOAT_EQ(audio_.getBusVolume(bird::AudioBus::kSFX), 0.25f);
	measureRms(audio_, 512);
	const float quarterRms = measureRms(audio_, 2048);
	EXPECT_NEAR(quarterRms / baseRms, 0.25f, 0.05f);

	// Hierarchical Master * SFX scaling
	audio_.setMasterVolume(0.5f);
	EXPECT_FLOAT_EQ(audio_.getMasterVolume(), 0.5f);
	measureRms(audio_, 512);
	const float eighthRms = measureRms(audio_, 2048);
	EXPECT_NEAR(eighthRms / baseRms, 0.125f, 0.04f);

	// Mute silences output without overwriting stored volume
	audio_.setBusMuted(bird::AudioBus::kSFX, true);
	EXPECT_TRUE(audio_.isBusMuted(bird::AudioBus::kSFX));
	EXPECT_FLOAT_EQ(audio_.getBusVolume(bird::AudioBus::kSFX), 0.25f);
	measureRms(audio_, 512);
	EXPECT_LT(measureRms(audio_, 2048), 1e-4f);

	// Unmute restores output
	audio_.setBusMuted(bird::AudioBus::kSFX, false);
	measureRms(audio_, 512);
	EXPECT_GT(measureRms(audio_, 2048), 0.02f);

	// Pause stops bus audio, resume restores it
	audio_.setBusPaused(bird::AudioBus::kSFX, true);
	EXPECT_TRUE(audio_.isBusPaused(bird::AudioBus::kSFX));
	measureRms(audio_, 512);
	EXPECT_LT(measureRms(audio_, 2048), 1e-4f);

	audio_.setBusPaused(bird::AudioBus::kSFX, false);
	EXPECT_FALSE(audio_.isBusPaused(bird::AudioBus::kSFX));
	measureRms(audio_, 512);
	EXPECT_GT(measureRms(audio_, 2048), 0.02f);

	// Bus pitch control
	audio_.setBusPitch(bird::AudioBus::kMaster, 1.5f);
	audio_.setBusPitch(bird::AudioBus::kSFX, 2.0f);
	EXPECT_FLOAT_EQ(audio_.getBusPitch(bird::AudioBus::kMaster), 1.5f);
	EXPECT_FLOAT_EQ(audio_.getBusPitch(bird::AudioBus::kSFX), 2.0f);
	EXPECT_GT(measureRms(audio_, 2048), 0.02f);
}

TEST_F(AudioTest, OneShotAutoRetiresAndMusicStreams) {
	// One-shot short clip (0.05s = 2400 frames at 48kHz)
	bird::AudioHandle oneShot = audio_.playOneShot(shortPath_.string());
	ASSERT_FALSE(oneShot.isNull());
	EXPECT_EQ(audio_.getActiveSoundCount(), 1u);

	// Render past end of clip
	EXPECT_GT(measureRms(audio_, 2048), 0.05f);
	measureRms(audio_, 2048);

	audio_.tick(0.016f);
	EXPECT_FALSE(audio_.isValid(oneShot));
	EXPECT_EQ(audio_.getActiveSoundCount(), 0u);

	// Streamed looping music on Music bus
	bird::AudioHandle music =
		audio_.playMusic(shortPath_.string(), /*loop=*/true, 0.0f, 0.8f);
	ASSERT_FALSE(music.isNull());
	EXPECT_TRUE(audio_.isLooping(music));

	// Render well past one loop iteration; should still be playing
	measureRms(audio_, 4800);
	audio_.tick(0.016f);
	EXPECT_TRUE(audio_.isPlaying(music));
	EXPECT_GT(measureRms(audio_, 1024), 0.05f);

	// Fade out and stop
	audio_.stopWithFade(music, 0.02f);
	measureRms(audio_, 4800);
	audio_.tick(0.016f);
	EXPECT_FALSE(audio_.isValid(music));
}

TEST_F(AudioTest, Spatial3DPanningAndDistanceAttenuation) {
	audio_.setListenerTransform(bird::Vector3(0.0f, 0.0f, 0.0f),
								bird::Vector3(0.0f, 0.0f, -1.0f),
								bird::Vector3(0.0f, 1.0f, 0.0f));

	bird::Audio3DSettings spatial;
	spatial.attenuation = bird::AudioAttenuationModel::Inverse;
	spatial.minDistance = 1.0f;
	spatial.maxDistance = 50.0f;
	spatial.rolloff = 1.0f;

	bird::AudioPlayOptions opts;
	opts.looping = true;

	// Place sound to the right (+X)
	bird::AudioHandle h = audio_.play3D(
		tonePath_.string(), bird::Vector3(5.0f, 0.0f, 0.0f), spatial, opts);
	ASSERT_FALSE(h.isNull());
	measureRms(audio_, 512);
	const float rightSideLeftCh = measureRms(audio_, 2048, 0);
	const float rightSideRightCh = measureRms(audio_, 2048, 1);
	EXPECT_GT(rightSideRightCh, rightSideLeftCh * 1.5f);

	// Move sound to the left (-X)
	audio_.setPosition(h, bird::Vector3(-5.0f, 0.0f, 0.0f));
	measureRms(audio_, 512);
	const float leftSideLeftCh = measureRms(audio_, 2048, 0);
	const float leftSideRightCh = measureRms(audio_, 2048, 1);
	EXPECT_GT(leftSideLeftCh, leftSideRightCh * 1.5f);

	// Distance attenuation: near (1m) should be much louder than far (30m)
	audio_.setPosition(h, bird::Vector3(0.0f, 0.0f, -1.0f));
	measureRms(audio_, 512);
	const float nearRms = measureRms(audio_, 2048);

	audio_.setPosition(h, bird::Vector3(0.0f, 0.0f, -30.0f));
	measureRms(audio_, 512);
	const float farRms = measureRms(audio_, 2048);
	EXPECT_GT(nearRms, farRms * 5.0f);
}

TEST_F(AudioTest, ListenerAndSourceComponentsSyncTransforms) {
	bird::AudioSystem* prevGlobal = bird::AudioSystem::getGlobal();
	bird::AudioSystem::setGlobal(&audio_);

	{
		bird::WorldObject listenerObj;
		listenerObj.setWorldPosition(bird::Vector3(1.0f, 2.0f, 3.0f));
		auto listenerComp = bird::_own_mk<bird::AudioListenerComponent>(true);
		listenerObj.addComponent(std::move(listenerComp));
		listenerObj.init(nullptr);

		EXPECT_NEAR(audio_.getListenerPosition().x, 1.0f, 1e-4f);
		EXPECT_NEAR(audio_.getListenerPosition().y, 2.0f, 1e-4f);
		EXPECT_NEAR(audio_.getListenerPosition().z, 3.0f, 1e-4f);

		// Move listener and tick -> velocity should be computed automatically
		listenerObj.setWorldPosition(bird::Vector3(3.0f, 2.0f, 3.0f));
		audio_.tick(0.5f);
		EXPECT_NEAR(audio_.getListenerPosition().x, 3.0f, 1e-4f);
		EXPECT_NEAR(audio_.getListenerVelocity().x, 4.0f, 1e-3f);

		// AudioSourceComponent with playOnInit
		bird::WorldObject sourceObj;
		sourceObj.setWorldPosition(bird::Vector3(4.0f, 2.0f, 3.0f));
		auto sourceComp = bird::_own_mk<bird::AudioSourceComponent>(
			tonePath_.string(), bird::AudioBus::kSFX);
		bird::AudioSourceComponent* rawSource = sourceComp.get();
		rawSource->setLooping(true);
		rawSource->setPlayOnInit(true);
		sourceObj.addComponent(std::move(sourceComp));
		sourceObj.init(nullptr);

		EXPECT_TRUE(rawSource->isPlaying());
		EXPECT_EQ(audio_.getActiveSoundCount(), 1u);

		rawSource->pause();
		EXPECT_TRUE(rawSource->isPaused());
		EXPECT_FALSE(rawSource->isPlaying());

		rawSource->resume();
		EXPECT_TRUE(rawSource->isPlaying());

		sourceObj.setWorldPosition(bird::Vector3(6.0f, 2.0f, 3.0f));
		audio_.tick(0.25f);
		EXPECT_TRUE(rawSource->isPlaying());

		sourceObj.deinit();
		EXPECT_FALSE(rawSource->isPlaying());
		EXPECT_EQ(audio_.getActiveSoundCount(), 0u);

		listenerObj.deinit();
	}

	bird::AudioSystem::setGlobal(prevGlobal);
}

}  // namespace
