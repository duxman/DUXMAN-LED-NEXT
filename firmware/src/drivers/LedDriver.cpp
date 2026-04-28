/*
 * duxman-led next - v0.3.7-beta
 * Licensed under the Apache License 2.0
 * File: firmware/src/drivers/LedDriver.cpp
 * Last commit: ec3d96f - 2026-04-28
 */

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
	setAllColor((static_cast<uint32_t>(level) << 16) |
							(static_cast<uint32_t>(level) << 8) |
							static_cast<uint32_t>(level));
}

void LedDriver::setAllColor(uint32_t color) {
	level_ = colorLevel(color);
	for (uint8_t i = 0; i < outputCount_; ++i) {
		setOutputColor(i, color);
	}
}

void LedDriver::setOutputLevel(uint8_t outputIndex, uint8_t level) {
	setOutputColor(outputIndex,
								 (static_cast<uint32_t>(level) << 16) |
								 (static_cast<uint32_t>(level) << 8) |
								 static_cast<uint32_t>(level));
}

void LedDriver::setOutputColor(uint8_t outputIndex, uint32_t color) {
	if (outputIndex >= outputCount_) {
		return;
	}
	outputLevels_[outputIndex] = colorLevel(color);
}

void LedDriver::setPixelColor(uint8_t outputIndex, uint16_t pixelIndex, uint32_t color) {
	(void)pixelIndex;
	setOutputColor(outputIndex, color);
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

bool LedDriver::supportsPerPixelColor(uint8_t outputIndex) const {
	if (outputIndex >= outputCount_) {
		return false;
	}

	const LedDriverOutputConfig &output = outputConfig(outputIndex);
	return output.enabled && !output.isDigital && isAddressableType(output.ledType);
}

uint8_t LedDriver::colorLevel(uint32_t color) {
	const uint8_t red = static_cast<uint8_t>((color >> 16) & 0xFF);
	const uint8_t green = static_cast<uint8_t>((color >> 8) & 0xFF);
	const uint8_t blue = static_cast<uint8_t>(color & 0xFF);
	return max(red, max(green, blue));
}

bool LedDriver::isAddressableType(LedDriverType ledType) {
	return ledType == LedDriverType::Ws2812b || ledType == LedDriverType::Ws2811 ||
				 ledType == LedDriverType::Ws2813 || ledType == LedDriverType::Ws2815 ||
				 ledType == LedDriverType::Sk6812 || ledType == LedDriverType::Tm1814;
}
