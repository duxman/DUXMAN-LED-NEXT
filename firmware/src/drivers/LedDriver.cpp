#include "drivers/LedDriver.h"

namespace {
LedDriverType parseLedType(const String &value) {
	if (value == "digital") {
		return LedDriverType::Digital;
	}
	if (value == "ws2812b") {
		return LedDriverType::Ws2812b;
	}
	if (value == "ws2811") {
		return LedDriverType::Ws2811;
	}
	if (value == "ws2813") {
		return LedDriverType::Ws2813;
	}
	if (value == "ws2815") {
		return LedDriverType::Ws2815;
	}
	if (value == "sk6812") {
		return LedDriverType::Sk6812;
	}
	if (value == "tm1814") {
		return LedDriverType::Tm1814;
	}
	return LedDriverType::Unknown;
}

LedDriverColorOrder parseColorOrder(const String &value) {
	if (value == "RGB") {
		return LedDriverColorOrder::RGB;
	}
	if (value == "GRB") {
		return LedDriverColorOrder::GRB;
	}
	if (value == "BRG") {
		return LedDriverColorOrder::BRG;
	}
	if (value == "RBG") {
		return LedDriverColorOrder::RBG;
	}
	if (value == "GBR") {
		return LedDriverColorOrder::GBR;
	}
	if (value == "BGR") {
		return LedDriverColorOrder::BGR;
	}
	if (value == "RGBW") {
		return LedDriverColorOrder::RGBW;
	}
	if (value == "GRBW") {
		return LedDriverColorOrder::GRBW;
	}
	if (value == "R") {
		return LedDriverColorOrder::R;
	}
	if (value == "G") {
		return LedDriverColorOrder::G;
	}
	if (value == "B") {
		return LedDriverColorOrder::B;
	}
	if (value == "W") {
		return LedDriverColorOrder::W;
	}
	return LedDriverColorOrder::Unknown;
}

bool isRgbwOutput(LedDriverType ledType, LedDriverColorOrder colorOrder) {
	return ledType == LedDriverType::Sk6812 || ledType == LedDriverType::Tm1814 ||
				 colorOrder == LedDriverColorOrder::RGBW || colorOrder == LedDriverColorOrder::GRBW;
}
} // namespace

void LedDriver::configure(const GpioConfig &config) {
	outputCount_ = config.outputCount > kMaxLedOutputs ? kMaxLedOutputs : config.outputCount;
	level_ = 0;

	for (uint8_t i = 0; i < kMaxLedOutputs; ++i) {
		outputs_[i] = LedDriverOutputConfig();
		outputLevels_[i] = 0;
	}

	for (uint8_t i = 0; i < outputCount_; ++i) {
		const LedOutput &source = config.outputs[i];
		LedDriverOutputConfig &target = outputs_[i];
		target.enabled = source.pin >= 0 && source.ledCount > 0;
		target.id = source.id;
		target.pin = source.pin;
		target.ledCount = source.ledCount;
		target.ledType = parseLedType(source.ledType);
		target.colorOrder = parseColorOrder(source.colorOrder);
		target.isDigital = target.ledType == LedDriverType::Digital;
		target.isRgbw = isRgbwOutput(target.ledType, target.colorOrder);
	}
}

void LedDriver::setAll(uint8_t level) {
	level_ = level;
	for (uint8_t i = 0; i < outputCount_; ++i) {
		setOutputLevel(i, level);
	}
}

void LedDriver::setOutputLevel(uint8_t outputIndex, uint8_t level) {
	if (outputIndex >= outputCount_) {
		return;
	}
	outputLevels_[outputIndex] = level;
}

const LedDriverOutputConfig &LedDriver::outputConfig(uint8_t outputIndex) const {
	static const LedDriverOutputConfig kEmpty;
	if (outputIndex >= outputCount_) {
		return kEmpty;
	}
	return outputs_[outputIndex];
}

uint8_t LedDriver::outputLevel(uint8_t outputIndex) const {
	if (outputIndex >= outputCount_) {
		return 0;
	}
	return outputLevels_[outputIndex];
}

bool LedDriver::isAddressableType(LedDriverType ledType) {
	return ledType == LedDriverType::Ws2812b || ledType == LedDriverType::Ws2811 ||
				 ledType == LedDriverType::Ws2813 || ledType == LedDriverType::Ws2815 ||
				 ledType == LedDriverType::Sk6812 || ledType == LedDriverType::Tm1814;
}
